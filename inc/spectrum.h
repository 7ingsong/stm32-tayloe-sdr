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

void spectrum_init(void);                                // after ili9488_init(); blocks ~0.2 s clearing the screen
void spectrum_capture(const uint32_t *adc, int n);       // every ADC DMA half-buffer (I low half-word, Q high)
void spectrum_poll(void);                                // every main-loop pass; one short step per call

#endif
