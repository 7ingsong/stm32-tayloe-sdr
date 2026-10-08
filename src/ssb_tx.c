#include "ssb_tx.h"
#include "ssb_taps.h"
#include "dsp.h"

static int16_t a_hist[2 * A_TAPS], b_hist[2 * B_TAPS], c_hist[2 * C_TAPS];
static int16_t d_hist_i[2 * D_TAPS], d_hist_q[2 * D_TAPS];

static fir_hist_t a = {a_hist, A_TAPS, 0};
static fir_hist_t b = {b_hist, B_TAPS, 0};
static fir_hist_t c = {c_hist, C_TAPS, 0};
static fir_hist_t d_i = {d_hist_i, D_TAPS, 0};
static fir_hist_t d_q = {d_hist_q, D_TAPS, 0};

static int a_count, b_count;

static uint8_t gain = SSB_TX_GAIN_DEFAULT;

void ssb_tx_set_gain(uint8_t g) {
    gain = g;
}

uint8_t ssb_tx_get_gain(void) {
    return gain;
}

static inline uint32_t to_dac(int32_t v) {
    v = v * gain / 128 + 2048; // int16 range -> 12-bit around mid-scale
    return v > 4095 ? 4095 : (v < 0 ? 0 : (uint32_t)v);
}

void ssb_tx_process(const uint16_t *mic, int n, uint32_t *dac) {
    for (int i = 0; i < n; i++) {
        const int16_t *xa = fir_push(&a, (int16_t)(((int32_t)mic[i] - 2048) << 4));
        if (++a_count < 4) continue;
        a_count = 0;

        const int16_t *xb = fir_push(&b, sat16(fir_dot(taps_a, xa, A_TAPS) >> 15)); // 16 kHz
        if (++b_count < 2) continue;
        b_count = 0;

        const int16_t *xc = fir_push(&c, sat16(fir_dot(taps_b, xb, B_TAPS) >> 15)); // 8 kHz audio
        int16_t si = sat16(fir_dot(taps_c_re, xc, C_TAPS) >> 14);              // analytic signal, USB
        int16_t sq = sat16(fir_dot(taps_c_im, xc, C_TAPS) >> 14);
#if SSB_TX_LSB
        sq = -sq;
#endif
        const int16_t *xi = fir_push(&d_i, si);
        const int16_t *xq = fir_push(&d_q, sq);

        // This 8 kHz sample completes the 8 mic samples i-7..i: emit their 8 interpolated I/Q words
        uint32_t *out = &dac[i - 7];
        for (int p = 0; p < D_PHASES; p++) {
            const int16_t *h = &taps_d[p * D_TAPS];
            out[p] = to_dac(fir_dot(h, xi, D_TAPS) >> 15) | (to_dac(fir_dot(h, xq, D_TAPS) >> 15) << 16);
        }
    }
}
