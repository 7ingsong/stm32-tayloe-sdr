#include "ui.h"
#include "ec11.h"
#include "ssd1306.h"
#include "fonts.h"
#include "iq.h"
#include "utils.h"

#define RETUNE_INTERVAL_MS 20 // a retune is ~30 Si5351 register writes; don't flood I2C on a fast spin
#define DEBOUNCE_MS        20

static const uint32_t step_hz[] = {10, 100, 1000, 10000, 100000, 1000000};
static const char *const step_name[] = {"10Hz", "100Hz", "1kHz", "10kHz", "100kHz", "1MHz"};
// Character under the step's digit in the "dd.ddd.ddd" frequency string
static const uint8_t step_char[] = {8, 7, 5, 4, 3, 1};
#define N_STEPS (sizeof(step_hz) / sizeof(step_hz[0]))

static uint32_t freq;
static int step = 2; // 1 kHz
static int retune_pending;
static uint32_t last_retune_ms;

static int button_raw, button_state;
static uint32_t button_changed_ms;

static int display_ok, sending;
static int panel_on = 1;
static int render_step = -1; // >= 0 while a frame is being drawn piecewise
static char freq_text[12];
static uint32_t shown_freq;
static int shown_step = -1, shown_ptt = -1;

// "dd.ddd.ddd", leading zeros of the MHz part blanked; 11 chars for 100 MHz
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

/*
 * Drawing a whole frame takes ~1 ms (pixel by pixel), enough to make the I2S refill late on a busy loop pass.
 * So a frame is drawn one piece per call: clear, each frequency digit, underline, status line.
 * Returns 1 when the frame is complete.
 */
static int render_piece(void) {
    int n_chars = 0;
    while (freq_text[n_chars]) n_chars++;

    if (render_step == 0) {
        format_frequency(freq, freq_text);
        ssd1306_Fill(Black);
    } else if (render_step <= n_chars) {
        int i = render_step - 1;
        ssd1306_SetCursor(i * Font_11x18.FontWidth, 0);
        ssd1306_WriteChar(freq_text[i], Font_11x18, White);
    } else if (render_step == n_chars + 1) {
        // Underline the digit the encoder changes (shifted one char right for 100 MHz)
        int x = (step_char[step] + (freq >= 100000000)) * Font_11x18.FontWidth;
        for (int dx = 0; dx < Font_11x18.FontWidth - 1; dx++) {
            ssd1306_DrawPixel(x + dx, 18, White);
            ssd1306_DrawPixel(x + dx, 19, White);
        }
        ssd1306_SetCursor(0, 21);
        ssd1306_WriteString(iq_get_ptt() ? "TX" : "RX", Font_7x10, White);
    } else {
        ssd1306_SetCursor(4 * Font_7x10.FontWidth, 21);
        ssd1306_WriteString("STEP ", Font_7x10, White);
        ssd1306_WriteString(step_name[step], Font_7x10, White);
        return 1;
    }
    render_step++;
    return 0;
}

void ui_init(void) {
    EC11_Init();
    freq = iq_get_frequency();

    delay_ms(50); // OLED power-up
    display_ok = ssd1306_Init() == 0; // no display: keep running, just skip drawing
    if (display_ok) {
        render_step = 0;
        while (!render_piece());
        render_step = -1;
        ssd1306_UpdateScreen();
        shown_freq = freq;
        shown_step = step;
        shown_ptt = iq_get_ptt();
    }
}

void ui_poll(void) {
    uint32_t now = get_ticks_ms();

    // Push: next tuning step (debounced)
    int down = EC11_ButtonDown();
    if (down != button_raw) {
        button_raw = down;
        button_changed_ms = now;
    } else if (button_raw != button_state && now - button_changed_ms >= DEBOUNCE_MS) {
        button_state = button_raw;
        if (button_state) {
            step = (step + 1) % N_STEPS;
        }
    }

    // Turn: tune
    int n = EC11_TakeSteps();
    if (n != 0) {
#if UI_ENCODER_REVERSE
        n = -n;
#endif
        int64_t f = (int64_t)freq + (int64_t)n * step_hz[step];
        if (f < LO_FREQ_MIN) f = LO_FREQ_MIN;
        if (f > LO_FREQ_MAX) f = LO_FREQ_MAX;
        freq = (uint32_t)f;
        retune_pending = 1;
    }
    if (retune_pending && now - last_retune_ms >= RETUNE_INTERVAL_MS) {
        iq_set_frequency(freq);
        last_retune_ms = now;
        retune_pending = 0;
    } else if (!retune_pending) {
        freq = iq_get_frequency(); // follow CMD_SET_FREQ from the host
    }

    // Display: one small I2C chunk per pass; redraw only when something changed
    if (!display_ok) {
        return;
    }

    // The OLED scans its rows ~300 times a second and its charge pump pulls that current from 3.3 V:
    // the ripple reaches the mic preamp as a hum comb (312 Hz and harmonics, measured +17 dB). Blank it on TX.
    if (iq_get_ptt() == panel_on) {
        panel_on = !iq_get_ptt();
        ssd1306_SetPower(panel_on);
        shown_ptt = -1; // redraw when it comes back
    }
    if (!panel_on) {
        return;
    }

    if (sending) {
        sending = !ssd1306_PollUpdate();
    } else if (render_step >= 0) {
        if (render_piece()) {
            render_step = -1;
            ssd1306_StartUpdate();
            sending = 1;
        }
    } else if (freq != shown_freq || step != shown_step || iq_get_ptt() != shown_ptt) {
        shown_freq = freq;
        shown_step = step;
        shown_ptt = iq_get_ptt();
        render_step = 0;
    }
}
