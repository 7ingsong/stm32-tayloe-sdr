#ifndef _DSP
#define _DSP
#include <stdint.h>

/*
 * Fixed-point FIR helpers shared by ssb_tx.c and ssb_rx.c (no FPU on the F103).
 * Samples are int16, taps Q15/Q14 int16, sums in int32 (tools/gen_ssb_taps.py checks they can't overflow).
 */

// History stored twice (x[i] and x[i + n]) so the newest-first window is always contiguous
typedef struct {
    int16_t *buf; // 2 * n entries
    int n;
    int pos;
} fir_hist_t;

static inline const int16_t *fir_push(fir_hist_t *h, int16_t v) {
    h->pos = (h->pos == 0 ? h->n : h->pos) - 1;
    h->buf[h->pos] = v;
    h->buf[h->pos + h->n] = v;
    return &h->buf[h->pos]; // x[k] = sample from k steps ago
}

// n must be a multiple of 4: unrolled because a taken branch per tap costs as much as the multiply
static inline int32_t fir_dot(const int16_t *taps, const int16_t *x, int n) {
    int32_t acc = 0;
    for (int k = 0; k < n; k += 4) {
        acc += taps[k] * x[k] + taps[k + 1] * x[k + 1] + taps[k + 2] * x[k + 2] + taps[k + 3] * x[k + 3];
    }
    return acc;
}

static inline int16_t sat16(int32_t v) {
    return v > 32767 ? 32767 : (v < -32768 ? -32768 : (int16_t)v);
}

#endif
