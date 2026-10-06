#include "audio.h"
#include "audio_taps.h"
#include "i2s.h"

/*
 * ADC and I2S both run from the 72 MHz clock: 64000 Hz = 72e6/1125, I2S = 72e6/1472.
 * So each I2S frame advances the input by exactly 1472/1125 samples and the buffer level never drifts.
 */
#define RATE_IN_DIV  1125 // 72e6 / 64000
#define RATE_OUT_DIV 1472 // 72e6 / 48913 (64 * 23)

#define RING_SIZE 2048 // 64 kHz samples, power of two
#define RING_MASK (RING_SIZE - 1)
#define PRIME     640  // ~10 ms of input buffered before playback (ADC delivers 512 per 8 ms block)

static int16_t ring[RING_SIZE];
static uint32_t ring_w;   // samples written
static uint32_t ring_r;   // samples consumed by the resampler
static uint32_t phase;    // 0..RATE_IN_DIV-1, fractional input position
static int playing;

// Called from the main loop with each ADC DMA half-buffer: low half-word = ADC1 (I), high = ADC2 (Q)
void audio_process_adc(const uint32_t *buf, int n) {
    for (int i = 0; i < n; i++) {
        int32_t q = (int32_t)(buf[i] >> 16) - 2048; // Complex to Imag
        ring[ring_w++ & RING_MASK] = (int16_t)(q << 4);
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
        playing = 0; // underrun (ADC stopped) or overrun: re-prime
        return 0;
    }

    const int16_t *h = &taps_r[(phase * R_PHASES / RATE_IN_DIV) * R_TAPS];
    uint32_t newest = (ring_r - 1) & RING_MASK;
    int32_t acc = 0;
    if (newest >= R_TAPS - 1) {
        // Window doesn't wrap (almost always): walk the ring directly, no masking per tap
        const int16_t *x = &ring[newest];
        for (int k = 0; k < R_TAPS; k++) {
            acc += h[k] * x[-k];
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
            s = resample_next() * AUDIO_GAIN;
            if (s > 0x7FFFFF) s = 0x7FFFFF;
            if (s < -0x800000) s = -0x800000;
        }

        i2s_put24(&buf[i], s);     // L
        i2s_put24(&buf[i + 2], s); // R
    }
}
