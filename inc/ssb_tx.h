#ifndef _SSB_TX
#define _SSB_TX
#include <stdint.h>

/*
 * Standalone SSB transmitter: microphone (ADC3, 64 kHz) -> phasing-method SSB -> DAC I/Q words (64 kHz).
 * Same idea as radio_iq_tx.grc, in fixed point: decimate to 8 kHz, complex band-pass 300..2800 Hz
 * (only positive frequencies survive = USB), interpolate back to 64 kHz.
 */

// 0 = USB, 1 = LSB (which one appears on air also depends on the exciter's I/Q wiring)
#ifndef SSB_TX_LSB
#define SSB_TX_LSB 0
#endif

// DAC code = I or Q * SSB_TX_GAIN around mid-scale. A real tone keeps half its amplitude in each
// of I and Q after the band-pass, so 2 maps a full-scale mic signal back to roughly full scale.
#ifndef SSB_TX_GAIN
#define SSB_TX_GAIN 2
#endif

// n mic samples (12-bit right-aligned, n a multiple of 8) -> n DAC words (I low half, Q high half)
void ssb_tx_process(const uint16_t *mic, int n, uint32_t *dac);

#endif
