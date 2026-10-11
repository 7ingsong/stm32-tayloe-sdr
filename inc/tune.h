#ifndef _TUNE
#define _TUNE
#include <stdint.h>

/*
 * Spur-free tuning. The receiver hears products of LO harmonics with harmonics of the board clocks (8 MHz MCU /
 * CLK2, 25 MHz Si5351 crystal: all multiples of 1 MHz), e.g. 4 x 10.001 MHz - 5 x 8 MHz = 4 kHz. They sit on
 * multiples of gcd(LO, 1 MHz) from the LO, folded by the 64 kHz sampling: with the LO a little off a round
 * frequency they fill the band with a comb (measured up to +50 dB at 10.001 MHz), whatever the PLL settings.
 * With the LO on a multiple of 8 kHz they all land on multiples of 8 kHz from the LO (folded onto 0, +-8,
 * +-16, +-24 kHz): only a few lines, no comb. So the LO only goes on such frequencies, the nearest one that keeps
 * the SSB band off 0 Hz, and the remaining few kHz to the dialled frequency are made up digitally by an NCO in
 * the receiver and transmitter. (An LO on a multiple of 40 kHz leaves slightly fewer lines, but the offset then
 * reaches +-20 kHz, often onto the stronger +-16 kHz lines, and the display loses more of its edge.)
 */

#define TUNE_LO_GRID    8000  // LO step: products land on multiples of 8 kHz only
#define TUNE_MAX_OFFSET 32000 // largest digital shift; the SSB band must also fit inside the +-32 kHz I/Q band

typedef struct {
    uint32_t n, q, div;   // LO = 25 MHz * n / (q * div)
    int32_t offset_hz;    // dialled frequency - LO
    int32_t nco_step;     // offset_hz / 64 kHz * 2^32: NCO step at the 64 kHz I/Q rate
} tune_t;

// passband_lo/hi: the SSB audio band relative to the dialled frequency (USB: +300..+2800, LSB: -2800..-300);
// it must not straddle 0 Hz after the shift. Returns 1 and fills *t, or 0 if no point was found.
int tune_choose(uint32_t frequency, int32_t passband_lo, int32_t passband_hi, tune_t *t);

#endif
