#ifndef _I2S
#define _I2S
#include <stdint.h>

/*
 * I2S3 (SPI3), master mode, no MCLK output.
 *   PA15 - I2S3_WS  (LRCLK)
 *   PB3  - I2S3_CK  (BCLK)
 *   PB5  - I2S3_SD  (SDATA)
 *
 * Philips standard, 24-bit data in a 32-bit channel frame, stereo.
 * I2S clock = SYSCLK = 72 MHz, Fs = 72e6 / (64 * 23) = 48913 Hz for I2S_AudioFreq_48k.
 *
 * Buffer layout (uint16_t, DMA half-words): L_hi, L_lo, R_hi, R_lo, ...
 * Use i2s_sample24() to get a signed 24-bit sample from a hi/lo pair.
 */

// 1 - master receive (external ADC -> MCU), 0 - master transmit (MCU -> external DAC)
#ifndef I2S_RX
#define I2S_RX 0
#endif

#define I2S_N_SAMPLES 1024 // half-words, 4 per stereo frame
#define I2S_FS 48913       // 72e6 / (64 * 23), see I2S_AudioFreq_48k in i2s.c

void i2s_init();
void i2s_dispatch();
void i2s_start();
void i2s_stop();
void on_i2s(uint16_t *buf, int n);

static inline void i2s_put24(uint16_t *p, int32_t s) {
    p[0] = (uint16_t)((uint32_t)s >> 8);
    p[1] = (uint16_t)((uint32_t)s << 8);
}

static inline int32_t i2s_sample24(const uint16_t *p) {
    return (int32_t)(((uint32_t)p[0] << 16) | p[1]) >> 8;
}

#endif
