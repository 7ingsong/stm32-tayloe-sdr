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

// Mic gain at boot (changeable with CMD_MIC_GAIN): DAC code = I or Q * gain / 128 around mid-scale.
// A real tone keeps half its amplitude in each of I and Q after the band-pass, so 16 maps a full-scale
// mic signal back to roughly full scale. Each doubling is +6 dB, 0 transmits silence.
#ifndef SSB_TX_GAIN_DEFAULT
#define SSB_TX_GAIN_DEFAULT 16
#endif

// n mic samples (12-bit right-aligned, n a multiple of 8) -> n DAC words (I low half, Q high half)
void ssb_tx_process(const uint16_t *mic, int n, uint32_t *dac);

void ssb_tx_set_gain(uint8_t gain);
uint8_t ssb_tx_get_gain(void);

#endif
