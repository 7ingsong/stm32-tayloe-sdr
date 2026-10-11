#include "spectrum.h"
#include "spectrum_tables.h"
#include "ili9488.h"
#include "fonts.h"
#include "iq.h"
#include "ui.h"

#define W        ILI9488_WIDTH
#define HEADER   32                                // text rows above the spectrum: frequency, mode, step
#define SPEC_ROWS (SPECTRUM_HEIGHT - HEADER)        // 128, a multiple of STRIPE
#define WF_TOP   SPECTRUM_HEIGHT
#define WF_ROWS  (ILI9488_HEIGHT - SPECTRUM_HEIGHT)
#define STRIPE   4 // spectrum rows per DMA transfer

// Levels are log2(power) in 1/16 steps (q4): 1 dB = 16 / 3.0103 = 5.3 q4
#define DB_Q4(db)       ((db) * 16 * 100 / 301)
#define SPECTRUM_DB     80         // dB from the bottom to the top of the spectrum: 8 grid steps of 10 dB
#define SPECTRUM_RANGE  DB_Q4(SPECTRUM_DB)
#define ROWS_PER_10DB   (SPEC_ROWS * 10 / SPECTRUM_DB) // 16
#define WATERFALL_RANGE DB_Q4(40)  // dB from black to white in the waterfall
#define FLOOR_BELOW     DB_Q4(10)  // the bottom edge sits this far below the average bin (noise floor)

static enum { IDLE, HEADER_DRAW, FFT, LEVELS, SPECTRUM, WATERFALL, SCROLL } state = IDLE;

static int32_t re[FFT_N], im[FFT_N];
static uint16_t bin_level[FFT_N];  // smoothed q4 level per bin, already in display order (-fs/2 .. +fs/2)
static uint8_t column_height[W];
static uint8_t stripe[STRIPE * W / 2]; // RGB111: 2 pixels per byte
static uint8_t wf_row[W * 3];          // RGB666
static int32_t floor_q4 = DB_Q4(80);

static spectrum_smoothing_t smoothing = {SPECTRUM_RISE_SHIFT, SPECTRUM_FALL_SHIFT, SPECTRUM_SMOOTH_BINS, SPECTRUM_DC_REMOVE};

static int capture_count, fft_stage, stripe_y;
static const int marker_x = W / 2; // red line: the dialled frequency, always in the middle

/*
 * The display is centred on the dialled frequency, not on the LO: the FFT bins are read shifted by the digital
 * offset (tune.h), so the picture scrolls smoothly while tuning and the red line stays put; the DC that marks
 * the LO is taken out (spectrum_capture). It shows +-24 kHz (150 Hz per pixel, dotted grid every 6 kHz): the LO
 * is at most 8 kHz below the dialled frequency, so that span always lies within the +-32 kHz around it.
 */
#define SPAN_HZ 48000
#define BIN_HZ  (64000 / FFT_N)
static volatile int32_t offset_hz_now;

void spectrum_set_offset(int32_t offset_hz) {
    offset_hz_now = offset_hz;
}

// Red marker into a 3-bit row, unless a spectrum bar already covers that pixel
static void put_marker(uint8_t *row) {
    uint8_t *b = &row[marker_x / 2];
    if (marker_x & 1) {
        if ((*b & 0x07) != C3_GREEN) *b = (uint8_t)((*b & 0x38) | C3_RED);
    } else {
        if ((*b >> 3) != C3_GREEN) *b = (uint8_t)((*b & 0x07) | C3_RED << 3);
    }
}

/* ---- Header: frequency, mode and tuning step, redrawn (in stripes, 3-bit colour) only when they change ---- */

typedef struct {
    char text[12];
    const FontDef *font;
    int16_t x, y;
    uint8_t color;
} label_t;

static label_t labels[3];
static uint32_t shown_freq = 0xFFFFFFFF;
static uint8_t shown_mode = 0xFF;
static const char *shown_step;

// "dd.ddd.ddd", leading zero of the MHz part blanked; 11 chars at 100 MHz
static void format_frequency(uint32_t f, char *s) {
    uint32_t mhz = f / 1000000, khz = f / 1000 % 1000, hz = f % 1000;
    int i = 0;
    if (mhz >= 100) s[i++] = '0' + mhz / 100;
    s[i++] = mhz >= 10 ? '0' + mhz / 10 % 10 : ' ';
    s[i++] = '0' + mhz % 10;
    s[i++] = '.';
    s[i++] = '0' + khz / 100;
    s[i++] = '0' + khz / 10 % 10;
    s[i++] = '0' + khz % 10;
    s[i++] = '.';
    s[i++] = '0' + hz / 100;
    s[i++] = '0' + hz / 10 % 10;
    s[i++] = '0' + hz % 10;
    s[i] = 0;
}

static int header_changed(void) {
    return ui_get_frequency() != shown_freq || iq_get_ptt() != shown_mode || ui_get_step_name() != shown_step;
}

static void build_header(void) {
    static const char *const mode_text[] = {"RX", "TX", "DX"};
    static const uint8_t mode_color[] = {C3_GREEN, C3_RED, C3_YELLOW};
    shown_freq = ui_get_frequency();
    shown_mode = iq_get_ptt();
    shown_step = ui_get_step_name();

    format_frequency(shown_freq, labels[0].text);
    labels[0].font = &Font_16x26; labels[0].x = 2;   labels[0].y = 3;  labels[0].color = C3_WHITE;
    labels[1].text[0] = mode_text[shown_mode][0]; labels[1].text[1] = mode_text[shown_mode][1]; labels[1].text[2] = 0;
    labels[1].font = &Font_11x18; labels[1].x = 196; labels[1].y = 7;  labels[1].color = mode_color[shown_mode];
    int i = 0;
    for (const char *p = shown_step; *p && i < 11; p++) labels[2].text[i++] = *p;
    labels[2].text[i] = 0;
    labels[2].font = &Font_7x10;  labels[2].x = 236; labels[2].y = 11; labels[2].color = C3_CYAN;
}

static void put_pixel(uint8_t *row, int x, uint8_t color) {
    if (x < 0 || x >= W) return;
    uint8_t *b = &row[x / 2];
    *b = (x & 1) ? (uint8_t)((*b & 0x38) | color) : (uint8_t)((*b & 0x07) | color << 3);
}

// Row y (in the labels' coordinates) of every label that crosses it, into a 3-bit row
static void draw_labels(uint8_t *row, int y, const label_t *lbs, int n) {
    for (int l = 0; l < n; l++) {
        const label_t *lb = &lbs[l];
        int fy = y - lb->y;
        if (fy < 0 || fy >= lb->font->FontHeight) continue;
        int x = lb->x;
        for (const char *c = lb->text; *c; c++, x += lb->font->FontWidth) {
            uint16_t bits = lb->font->data[(*c - 32) * lb->font->FontHeight + fy];
            for (int fx = 0; bits && fx < lb->font->FontWidth; fx++, bits <<= 1) {
                if (bits & 0x8000) put_pixel(row, x + fx, lb->color);
            }
        }
    }
}

static void fill_header_stripe(int y0) {
    uint8_t *row = stripe;
    for (int y = y0; y < y0 + STRIPE; y++, row += W / 2) {
        uint8_t bg = y == HEADER - 1 ? (C3_BLUE << 3 | C3_BLUE) : 0; // thin line under the header
        for (int i = 0; i < W / 2; i++) row[i] = bg;
        draw_labels(row, y, labels, 3);
    }
}
static uint16_t wf_line = WF_TOP; // GRAM line currently shown at the top of the waterfall

enum { BG_PLAIN, BG_DOTS, BG_GRID };
static uint8_t background[3][W / 2];
static uint8_t tallest;

/*
 * Scale: dotted vertical lines on round frequencies (every 5 kHz, labelled every 10 kHz along the top), so they
 * scroll with the signals; dotted horizontal lines every 10 dB, labelled in dBFS on the left (0 dBFS: a complex
 * tone at the ADC's full scale). The bottom edge follows the noise floor in 10 dB steps, so the labels stay round.
 */
#define GRID_HZ     5000
#define N_FLABELS   6
#define N_DBLABELS  (SPECTRUM_DB / 10 - 1)
static uint8_t vline[W / 8];             // bitmap: a vertical grid line in this column
static label_t scale_labels[N_FLABELS + N_DBLABELS];
static int n_scale_labels;
static int32_t ref_q4;                   // level of a full-scale tone: 0 dBFS
static int bottom_db = -100;             // dBFS at the bottom edge, a multiple of 10

static uint16_t log2_q4(uint64_t p);

static uint8_t background_pixel(int type, int x) {
    if (type == BG_GRID && (x & 3) == 0) return C3_BLUE;                         // horizontal grid line, dotted
    if (type != BG_PLAIN && (vline[x / 8] >> (x & 7) & 1)) return C3_BLUE;       // vertical grid, dotted
    return C3_BLACK;
}

static void build_backgrounds(void) {
    for (int type = 0; type < 3; type++) {
        for (int x = 0; x < W; x += 2) {
            background[type][x / 2] = (uint8_t)(background_pixel(type, x) << 3 | background_pixel(type, x + 1));
        }
    }
}

void spectrum_set_smoothing(spectrum_smoothing_t s) {
    smoothing.rise_shift = s.rise_shift > SPECTRUM_SHIFT_MAX ? SPECTRUM_SHIFT_MAX : s.rise_shift;
    smoothing.fall_shift = s.fall_shift > SPECTRUM_SHIFT_MAX ? SPECTRUM_SHIFT_MAX : s.fall_shift;
    smoothing.smooth_bins = s.smooth_bins ? 1 : 0;
    smoothing.dc_remove = s.dc_remove ? 1 : 0;
}

spectrum_smoothing_t spectrum_get_smoothing(void) {
    return smoothing;
}

// Text of a frequency in MHz with kHz: "9.995", "10.010"
static void format_mhz_khz(uint32_t f, char *s) {
    uint32_t mhz = f / 1000000, khz = f / 1000 % 1000;
    int i = 0;
    if (mhz >= 100) s[i++] = '0' + mhz / 100;
    if (mhz >= 10) s[i++] = '0' + mhz / 10 % 10;
    s[i++] = '0' + mhz % 10;
    s[i++] = '.';
    s[i++] = '0' + khz / 100;
    s[i++] = '0' + khz / 10 % 10;
    s[i++] = '0' + khz % 10;
    s[i] = 0;
}

// Once per frame: grid lines and labels for the current dialled frequency and bottom level
static void build_scale(void) {
    uint32_t dial = iq_get_frequency();
    int32_t first = (int32_t)(dial - SPAN_HZ / 2) / GRID_HZ * GRID_HZ;
    for (int i = 0; i < W / 8; i++) vline[i] = 0;
    n_scale_labels = 0;
    for (int32_t f = first; f <= (int32_t)(dial + SPAN_HZ / 2) + GRID_HZ; f += GRID_HZ) {
        int x = W / 2 + (int)((int64_t)(f - (int32_t)dial) * W / SPAN_HZ);
        if (x < 0 || x >= W) continue;
        vline[x / 8] |= (uint8_t)(1 << (x & 7));
        if (f % (2 * GRID_HZ) == 0 && n_scale_labels < N_FLABELS) {
            label_t *lb = &scale_labels[n_scale_labels];
            format_mhz_khz((uint32_t)f, lb->text);
            int len = 0;
            while (lb->text[len]) len++;
            lb->font = &Font_7x10;
            lb->x = (int16_t)(x - len * Font_7x10.FontWidth / 2);
            if (lb->x < 0 || lb->x + len * Font_7x10.FontWidth > W) continue; // would be cut at the edge
            lb->y = 2;
            lb->color = C3_WHITE;
            n_scale_labels++;
        }
    }
    build_backgrounds();
    // dB labels just under each horizontal line (not the top one: the frequency labels are there)
    for (int k = 1; k <= N_DBLABELS; k++) {
        label_t *lb = &scale_labels[n_scale_labels++];
        int db = bottom_db + SPECTRUM_DB - 10 * k, a = db < 0 ? -db : db, i = 0;
        if (db < 0) lb->text[i++] = '-';
        if (a >= 100) lb->text[i++] = '0' + a / 100;
        if (a >= 10) lb->text[i++] = '0' + a / 10 % 10;
        lb->text[i++] = '0' + a % 10;
        lb->text[i] = 0;
        lb->font = &Font_7x10;
        lb->x = 2;
        lb->y = (int16_t)(k * ROWS_PER_10DB + 2);
        lb->color = C3_CYAN;
    }
}

void spectrum_init(void) {
    // 0 dBFS: a complex tone of amplitude 2047 through the window (scaled >> 7 as in spectrum_capture), unscaled FFT
    int64_t wsum = 0;
    for (int i = 0; i < FFT_N; i++) wsum += fft_window[i];
    uint64_t mag = (uint64_t)(2047 * wsum / 128);
    ref_q4 = log2_q4(mag * mag);
    build_scale();
    ili9488_fill_rect(0, 0, ILI9488_WIDTH, ILI9488_HEIGHT, 0, 0, 0);
    // Vertical scroll: top SPECTRUM_HEIGHT lines fixed, the rest scrolls (VSCRDEF), starting at WF_TOP (VSCRSADD)
    uint8_t def[] = {0, WF_TOP, WF_ROWS >> 8, WF_ROWS & 0xFF, 0, 0};
    ili9488_command(0x33, def, sizeof(def));
    uint8_t start[] = {WF_TOP >> 8, WF_TOP & 0xFF};
    ili9488_command(0x37, start, sizeof(start));
}

// Windowed samples go straight to their bit-reversed slots, so the in-place FFT needs no reorder pass
volatile uint8_t spectrum_debug_pause;

void spectrum_capture(const uint32_t *adc, int n) {
    if (state != IDLE || spectrum_debug_pause || n < FFT_N || ++capture_count < SPECTRUM_CAPTURE_EVERY) {
        return;
    }
    capture_count = 0;
    // Optionally remove the DC (ADC offset, LO leakage), see SPECTRUM_DC_REMOVE; else just the ADC mid-scale
    int32_t mean_i = 2048, mean_q = 2048;
    if (smoothing.dc_remove) {
        mean_i = 0;
        mean_q = 0;
        for (int i = 0; i < FFT_N; i++) {
            mean_i += (int32_t)(adc[i] & 0xFFFF);
            mean_q += (int32_t)(adc[i] >> 16);
        }
        mean_i /= FFT_N;
        mean_q /= FFT_N;
    }
    for (int i = 0; i < FFT_N; i++) {
        int32_t w = fft_window[i];
        int32_t si = (int32_t)(adc[i] & 0xFFFF) - mean_i;
        int32_t sq = (int32_t)(adc[i] >> 16) - mean_q;
        re[fft_bitrev[i]] = (si * w) >> 7; // 12-bit x Q15 -> ~20 bits: room for the FFT's x256 growth in int32
        im[fft_bitrev[i]] = (sq * w) >> 7;
    }
    fft_stage = 0;
    state = FFT;
}

// One radix-2 decimation-in-time stage (128 butterflies)
static void fft_run_stage(int s) {
    int half = 1 << s, step = FFT_N / (2 * half);
    for (int k = 0; k < FFT_N; k += 2 * half) {
        for (int j = 0; j < half; j++) {
            int a = k + j, b = a + half;
            int32_t wr = fft_cos[j * step], wi = -fft_sin[j * step];
            int32_t tr = (int32_t)(((int64_t)re[b] * wr - (int64_t)im[b] * wi) >> 15);
            int32_t ti = (int32_t)(((int64_t)re[b] * wi + (int64_t)im[b] * wr) >> 15);
            re[b] = re[a] - tr;
            im[b] = im[a] - ti;
            re[a] += tr;
            im[a] += ti;
        }
    }
}

// log2 in 1/16 steps (q4), linear between powers of two
static uint16_t log2_q4(uint64_t p) {
    if (p == 0) {
        return 0;
    }
    int msb = 63 - __builtin_clzll(p);
    uint32_t frac = msb >= 4 ? (uint32_t)(p >> (msb - 4)) & 15 : (uint32_t)(p << (4 - msb)) & 15;
    return (uint16_t)(msb * 16 + frac);
}

// Level of bin b, optionally averaged with its neighbours (1-2-1)
static int32_t smoothed_bin(int b) {
    if (!smoothing.smooth_bins) {
        return bin_level[b];
    }
    int32_t l = bin_level[b > 0 ? b - 1 : b], r = bin_level[b < FFT_N - 1 ? b + 1 : b];
    return (l + 2 * bin_level[b] + r) / 4;
}

static void compute_levels(void) {
    int32_t sum = 0;
    for (int b = 0; b < FFT_N; b++) {
        int k = (b + FFT_N / 2) & (FFT_N - 1); // fftshift: negative frequencies on the left
        uint64_t p = (uint64_t)((int64_t)re[k] * re[k]) + (uint64_t)((int64_t)im[k] * im[k]);
        int32_t level = log2_q4(p);
        int32_t delta = level - (int32_t)bin_level[b];
        bin_level[b] += delta / (1 << (delta > 0 ? smoothing.rise_shift : smoothing.fall_shift)); // fast up, slow down
        sum += bin_level[b];
    }
    // What is left of the DC spike (it wanders a little within one capture) spreads over the window's main lobe:
    // draw the LO bin and its neighbours as a straight line between the bins either side
    if (smoothing.dc_remove) {
        int32_t l = bin_level[FFT_N / 2 - 2], r = bin_level[FFT_N / 2 + 2];
        for (int d = -1; d <= 1; d++) {
            bin_level[FFT_N / 2 + d] = (uint16_t)(l + (r - l) * (d + 2) / 4);
        }
    }
    int32_t target = sum / FFT_N - FLOOR_BELOW; // most bins are noise: track it so no level setup is needed
    floor_q4 += (target - floor_q4) / 8;
    // Spectrum bottom edge: the tracked floor rounded to 10 dB, with 2 dB of hysteresis so it doesn't flicker
    int floor_db = (floor_q4 - ref_q4) * 301 / 1600;
    while (floor_db < bottom_db - 2) bottom_db -= 10;
    while (floor_db > bottom_db + 12) bottom_db += 10;
    int32_t bottom_q4 = ref_q4 + DB_Q4(bottom_db);
    build_scale();

    tallest = 0;
    int32_t offset = offset_hz_now;
    for (int x = 0; x < W; x++) {
        // Frequency of this column relative to the LO, then its (fftshifted) bin position with 8 fractional bits;
        // columns fall between bins: interpolate instead of repeating a bin as a step
        int32_t hz = (x - W / 2) * SPAN_HZ / W + offset;
        int32_t pos = FFT_N / 2 * 256 + hz * 256 / BIN_HZ;
        int32_t level;
        if (pos < 0 || pos > FFT_N * 256) {
            level = 0; // beyond the I/Q band around the LO (doesn't happen with the offsets tune.c picks)
        } else {
            // the last bin (+31.75 kHz) and the first (-32 kHz = +32 kHz) are neighbours
            int b = (pos >> 8) & (FFT_N - 1), frac = pos & 255;
            int32_t lo = smoothed_bin(b), hi = smoothed_bin((b + 1) & (FFT_N - 1));
            level = lo + (hi - lo) * frac / 256;
        }
        int32_t v = level - floor_q4;
        int32_t h = (level - bottom_q4) * SPEC_ROWS / SPECTRUM_RANGE;
        column_height[x] = h < 0 ? 0 : (h >= SPEC_ROWS ? SPEC_ROWS - 1 : h);
        if (column_height[x] > tallest) tallest = column_height[x];
        int32_t c = v * WF_PALETTE / WATERFALL_RANGE;
        c = c < 0 ? 0 : (c >= WF_PALETTE ? WF_PALETTE - 1 : c);
        wf_row[3 * x] = wf_palette[3 * c];
        wf_row[3 * x + 1] = wf_palette[3 * c + 1];
        wf_row[3 * x + 2] = wf_palette[3 * c + 2];
    }
}

/*
 * Rendering cost matters (it competes with the radio DSP): instead of computing every pixel, each row starts
 * from one of three precomputed backgrounds (plain, vertical-grid dots, horizontal grid line, all with the LO
 * line) and only columns tall enough to reach the row get painted green. Rows above the tallest column are a copy.
 */
// y0 counts rows of the spectrum area (0 = its top, just under the header)
static void fill_stripe(int y0) {
    uint8_t *p = stripe;
    for (int y = y0; y < y0 + STRIPE; y++, p += W / 2) {
        const uint8_t *bg = background[y % ROWS_PER_10DB == 0 ? BG_GRID : (y & 3) == 0 ? BG_DOTS : BG_PLAIN];
        int threshold = SPEC_ROWS - y; // a column reaches this row if its height is at least this
        if (tallest < threshold) {
            for (int i = 0; i < W / 2; i++) p[i] = bg[i];
            draw_labels(p, y, scale_labels, n_scale_labels);
            put_marker(p);
            continue;
        }
        for (int i = 0; i < W / 2; i++) {
            uint8_t v = bg[i];
            if (column_height[2 * i] >= threshold) v = (uint8_t)((v & 0x07) | C3_GREEN << 3);
            if (column_height[2 * i + 1] >= threshold) v = (uint8_t)((v & 0x38) | C3_GREEN);
            p[i] = v;
        }
        draw_labels(p, y, scale_labels, n_scale_labels);
        put_marker(p);
    }
}

// Debug: spectrum_debug_pause = 1 over SWD stops new frames (the current one finishes), to tell display
// interference from other noise

void spectrum_poll(void) {
    if (ili9488_busy() || (spectrum_debug_pause && state == IDLE)) {
        return;
    }
    switch (state) {
    case IDLE:
        if (header_changed()) {
            build_header();
            ili9488_set_format(ILI9488_RGB111);
            ili9488_set_window(0, 0, W - 1, HEADER - 1);
            stripe_y = 0;
            state = HEADER_DRAW;
        }
        break;
    case HEADER_DRAW:
        fill_header_stripe(stripe_y);
        ili9488_write_dma(stripe, sizeof(stripe));
        stripe_y += STRIPE;
        if (stripe_y >= HEADER) {
            state = IDLE;
        }
        break;
    case FFT:
        fft_run_stage(fft_stage);
        if (++fft_stage == FFT_LOG2N) {
            state = LEVELS;
        }
        break;
    case LEVELS:
        compute_levels();
        ili9488_set_format(ILI9488_RGB111);
        ili9488_set_window(0, HEADER, W - 1, SPECTRUM_HEIGHT - 1);
        stripe_y = 0;
        state = SPECTRUM;
        break;
    case SPECTRUM:
        fill_stripe(stripe_y);
        ili9488_write_dma(stripe, sizeof(stripe));
        stripe_y += STRIPE;
        if (stripe_y >= SPEC_ROWS) {
            state = WATERFALL;
        }
        break;
    case WATERFALL:
        // The new row goes just above the current top of the scrolling area, then becomes the top
        wf_line = wf_line == WF_TOP ? ILI9488_HEIGHT - 1 : wf_line - 1;
        ili9488_set_format(ILI9488_RGB666);
        ili9488_set_window(0, wf_line, W - 1, wf_line);
        ili9488_write_dma(wf_row, sizeof(wf_row));
        state = SCROLL;
        break;
    case SCROLL: {
        uint8_t start[] = {wf_line >> 8, wf_line & 0xFF};
        ili9488_command(0x37, start, sizeof(start)); // VSCRSADD
        state = IDLE;
        break;
    }
    }
}
