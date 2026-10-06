#include "ssb_tx.h"
#include "ssb_tx_taps.h"

// FIR histories are stored twice (x[i] and x[i + N]) so the newest-first window is always contiguous
typedef struct {
    int16_t *buf;
    int n;
    int pos;
} fir_hist_t;

static int16_t a_hist[2 * A_TAPS], b_hist[2 * B_TAPS], c_hist[2 * C_TAPS];
static int16_t d_hist_i[2 * D_TAPS], d_hist_q[2 * D_TAPS];

static fir_hist_t a = {a_hist, A_TAPS, 0};
static fir_hist_t b = {b_hist, B_TAPS, 0};
static fir_hist_t c = {c_hist, C_TAPS, 0};
static fir_hist_t d_i = {d_hist_i, D_TAPS, 0};
static fir_hist_t d_q = {d_hist_q, D_TAPS, 0};

static int a_count, b_count;

static inline const int16_t *push(fir_hist_t *h, int16_t v) {
    h->pos = (h->pos == 0 ? h->n : h->pos) - 1;
    h->buf[h->pos] = v;
    h->buf[h->pos + h->n] = v;
    return &h->buf[h->pos]; // x[k] = sample from k steps ago
}

static inline int32_t dot(const int16_t *taps, const int16_t *x, int n) {
    int32_t acc = 0;
    for (int k = 0; k < n; k++) {
        acc += taps[k] * x[k];
    }
    return acc;
}

static inline int16_t sat16(int32_t v) {
    return v > 32767 ? 32767 : (v < -32768 ? -32768 : (int16_t)v);
}

static inline uint32_t to_dac(int32_t v) {
    v = v * SSB_TX_GAIN / 16 + 2048; // int16 range -> 12-bit around mid-scale
    return v > 4095 ? 4095 : (v < 0 ? 0 : (uint32_t)v);
}

void ssb_tx_process(const uint16_t *mic, int n, uint32_t *dac) {
    for (int i = 0; i < n; i++) {
        const int16_t *xa = push(&a, (int16_t)(((int32_t)mic[i] - 2048) << 4));
        if (++a_count < 4) continue;
        a_count = 0;

        const int16_t *xb = push(&b, sat16(dot(taps_a, xa, A_TAPS) >> 15)); // 16 kHz
        if (++b_count < 2) continue;
        b_count = 0;

        const int16_t *xc = push(&c, sat16(dot(taps_b, xb, B_TAPS) >> 15)); // 8 kHz audio
        int16_t si = sat16(dot(taps_c_re, xc, C_TAPS) >> 14);              // analytic signal, USB
        int16_t sq = sat16(dot(taps_c_im, xc, C_TAPS) >> 14);
#if SSB_TX_LSB
        sq = -sq;
#endif
        const int16_t *xi = push(&d_i, si);
        const int16_t *xq = push(&d_q, sq);

        // This 8 kHz sample completes the 8 mic samples i-7..i: emit their 8 interpolated I/Q words
        uint32_t *out = &dac[i - 7];
        for (int p = 0; p < D_PHASES; p++) {
            const int16_t *h = &taps_d[p * D_TAPS];
            out[p] = to_dac(dot(h, xi, D_TAPS) >> 15) | (to_dac(dot(h, xq, D_TAPS) >> 15) << 16);
        }
    }
}
