#ifndef _SPECTRUM
#define _SPECTRUM
#include <stdint.h>

/*
 * Spectrum + waterfall of the receiver input (ADC I/Q, 64 kHz, +-32 kHz around the LO) on the ILI9488.
 * Portrait 320x480: spectrum in the top SPECTRUM_HEIGHT rows (3-bit colour, redrawn in stripes by DMA),
 * waterfall below (18-bit colour, one new row per frame, moved by the panel's hardware vertical scroll).
 * Everything runs in small steps from spectrum_poll() so the main loop (and the I2S audio) never stalls.
 */

#define SPECTRUM_HEIGHT 160

// Take one ADC block in this many for a frame: 8 ms blocks -> ~20 frames/s
#ifndef SPECTRUM_CAPTURE_EVERY
#define SPECTRUM_CAPTURE_EVERY 6
#endif

// Smoothing over time, asymmetric so signals show up at once but noise doesn't flicker: each frame a bin
// moves 1/2^SHIFT of the way to its new level, with a separate SHIFT for rising and falling (0 = jump).
// These are the boot values; CMD_SPECTRUM changes all three at runtime.
#ifndef SPECTRUM_RISE_SHIFT
#define SPECTRUM_RISE_SHIFT 1
#endif
#ifndef SPECTRUM_FALL_SHIFT
#define SPECTRUM_FALL_SHIFT 2
#endif
// Smoothing over frequency: 1 = average neighbouring bins 1-2-1 (smoother, peaks a bit wider)
#ifndef SPECTRUM_SMOOTH_BINS
#define SPECTRUM_SMOOTH_BINS 1
#endif

#define SPECTRUM_SHIFT_MAX 7

typedef struct {
    uint8_t rise_shift;   // 0..SPECTRUM_SHIFT_MAX
    uint8_t fall_shift;   // 0..SPECTRUM_SHIFT_MAX
    uint8_t smooth_bins;  // 0 or 1
} spectrum_smoothing_t;

void spectrum_set_smoothing(spectrum_smoothing_t s); // out-of-range values are clamped
spectrum_smoothing_t spectrum_get_smoothing(void);

void spectrum_init(void);                                // after ili9488_init(); blocks ~0.2 s clearing the screen
void spectrum_capture(const uint32_t *adc, int n);       // every ADC DMA half-buffer (I low half-word, Q high)
void spectrum_poll(void);                                // every main-loop pass; one short step per call

#endif
