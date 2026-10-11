#ifndef _NCO
#define _NCO
#include <stdint.h>

/*
 * Numerically controlled oscillator for digital fine tuning: rotates I/Q by a phase that advances by `step`
 * per sample (step = f / fs * 2^32). The receiver rotates by -offset so the dialled frequency lands on 0 Hz;
 * the transmitter rotates by +offset so its baseband lands on the dialled frequency (see tune.c).
 */
#define NCO_TABLE 1024
extern const int16_t nco_sin[NCO_TABLE];

// (i + jq) * e^{j*phase}; i, q up to int16 range, results may exceed it by up to sqrt(2)
static inline void nco_rotate(int32_t *i, int32_t *q, uint32_t phase) {
    uint32_t k = phase >> 22;
    int32_t s = nco_sin[k], c = nco_sin[(k + NCO_TABLE / 4) & (NCO_TABLE - 1)];
    int32_t ri = (*i * c - *q * s) >> 15;
    int32_t rq = (*i * s + *q * c) >> 15;
    *i = ri;
    *q = rq;
}

#endif
