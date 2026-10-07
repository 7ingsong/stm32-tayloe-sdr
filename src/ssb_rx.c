#include "ssb_rx.h"
#include "ssb_taps.h"
#include "dsp.h"
#include "i2s.h"

/* ---- Demodulator: 64 kHz I/Q -> 8 kHz audio ---- */

static int16_t ai_hist[2 * A_TAPS], aq_hist[2 * A_TAPS];
static int16_t bi_hist[2 * B_TAPS], bq_hist[2 * B_TAPS];
static int16_t ci_hist[2 * C_TAPS], cq_hist[2 * C_TAPS];

static fir_hist_t ai = {ai_hist, A_TAPS, 0}, aq = {aq_hist, A_TAPS, 0};
static fir_hist_t bi = {bi_hist, B_TAPS, 0}, bq = {bq_hist, B_TAPS, 0};
static fir_hist_t ci = {ci_hist, C_TAPS, 0}, cq = {cq_hist, C_TAPS, 0};

static int a_count, b_count;

/* ---- 8 kHz audio ring, read by the I2S resampler ---- */

/*
 * The ADC and I2S both run from the 72 MHz clock: 8000 Hz = 72e6/9000, I2S = 72e6/1472.
 * Each I2S frame advances the audio by exactly 1472/9000 samples, so the buffer level never drifts.
 */
#define RATE_IN_DIV  9000 // 72e6 / 8000
#define RATE_OUT_DIV 1472 // 72e6 / 48913 (64 * 23)

#define RING_SIZE 512 // power of two
#define RING_MASK (RING_SIZE - 1)
#define PRIME     96  // 12 ms buffered before playback (the demodulator delivers 64 samples per 8 ms ADC block)

static int16_t ring[RING_SIZE];
static uint32_t ring_w;   // samples written
static uint32_t ring_r;   // samples consumed by the resampler
static uint32_t phase;    // 0..RATE_IN_DIV-1, fractional input position
static int playing;

static uint8_t volume = SSB_RX_VOLUME_DEFAULT;

void ssb_rx_set_volume(uint8_t v) {
    volume = v;
}

uint8_t ssb_rx_get_volume(void) {
    return volume;
}

// Called from the main loop with each ADC DMA half-buffer: low half-word = ADC1 (I), high = ADC2 (Q)
void ssb_rx_process_adc(const uint32_t *buf, int n) {
    for (int i = 0; i < n; i++) {
        const int16_t *xai = fir_push(&ai, (int16_t)(((int32_t)(buf[i] & 0xFFFF) - 2048) << 4));
        const int16_t *xaq = fir_push(&aq, (int16_t)(((int32_t)(buf[i] >> 16) - 2048) << 4));
        if (++a_count < 4) continue;
        a_count = 0;

        const int16_t *xbi = fir_push(&bi, sat16(fir_dot(taps_a, xai, A_TAPS) >> 15)); // 16 kHz
        const int16_t *xbq = fir_push(&bq, sat16(fir_dot(taps_a, xaq, A_TAPS) >> 15));
        if (++b_count < 2) continue;
        b_count = 0;

        const int16_t *xci = fir_push(&ci, sat16(fir_dot(taps_b, xbi, B_TAPS) >> 15)); // 8 kHz
        const int16_t *xcq = fir_push(&cq, sat16(fir_dot(taps_b, xbq, B_TAPS) >> 15));

        // Real part of the complex band-pass: Re{(hr + j*hi)(I + j*Q)} = hr*I - hi*Q; LSB uses conjugate taps
        int32_t re = fir_dot(taps_c_re, xci, C_TAPS);
        int32_t im = fir_dot(taps_c_im, xcq, C_TAPS);
#if SSB_RX_LSB
        int32_t audio = (re + im) >> 14;
#else
        int32_t audio = (re - im) >> 14;
#endif
        ring[ring_w++ & RING_MASK] = sat16(audio);
    }
}

static int32_t resample_next(void) {
    phase += RATE_OUT_DIV;
    while (phase >= RATE_IN_DIV) {
        phase -= RATE_IN_DIV;
        ring_r++;
    }

    int32_t fill = (int32_t)(ring_w - ring_r);
    if (fill < 0 || fill > RING_SIZE - R_TAPS) {
        playing = 0; // underrun or overrun: re-prime
        return 0;
    }

    const int16_t *h = &taps_r[(phase * R_PHASES / RATE_IN_DIV) * R_TAPS];
    uint32_t newest = (ring_r - 1) & RING_MASK;
    int32_t acc = 0;
    if (newest >= R_TAPS - 1) {
        // Window doesn't wrap (almost always): walk the ring directly, no masking per tap
        const int16_t *x = &ring[newest];
        for (int k = 0; k < R_TAPS; k += 4) {
            acc += h[k] * x[-k] + h[k + 1] * x[-k - 1] + h[k + 2] * x[-k - 2] + h[k + 3] * x[-k - 3];
        }
    } else {
        for (int k = 0; k < R_TAPS; k++) {
            acc += h[k] * ring[(newest - k) & RING_MASK];
        }
    }
    return acc >> 15;
}

// I2S DMA half-buffer: n half-words, 4 per stereo frame (L_hi, L_lo, R_hi, R_lo)
void on_i2s(uint16_t *buf, int n) {
    for (int i = 0; i < n; i += 4) {
        int32_t s = 0;

        if (!playing && (int32_t)(ring_w - ring_r) >= PRIME) {
            ring_r = ring_w - PRIME;
            phase = 0;
            playing = 1;
        }
        if (playing) {
            s = resample_next() * volume;
            if (s > 0x7FFFFF) s = 0x7FFFFF;
            if (s < -0x800000) s = -0x800000;
        }

        i2s_put24(&buf[i], s);     // L
        i2s_put24(&buf[i + 2], s); // R
    }
}
