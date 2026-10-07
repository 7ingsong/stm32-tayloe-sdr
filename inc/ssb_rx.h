#ifndef _SSB_RX
#define _SSB_RX
#include <stdint.h>

/*
 * Standalone SSB receiver on the I2S DAC: ADC I/Q (64 kHz) -> decimate to 8 kHz ->
 * complex band-pass 300..2800 Hz -> real part = audio -> resample to I2S (48913 Hz), both channels.
 * Mirror of ssb_tx.c; tune with the LO (CMD_SET_FREQ): a USB station sits 300..2800 Hz above it.
 */

// 0 = USB, 1 = LSB (which one is which also depends on the mixer's I/Q wiring)
#ifndef SSB_RX_LSB
#define SSB_RX_LSB 0
#endif

// Volume at boot (changeable with CMD_VOLUME): output = 16-bit audio sample * volume in the 24-bit I2S word.
// 8 puts a full-scale signal at -30 dBFS; each doubling is +6 dB, 0 mutes.
#ifndef SSB_RX_VOLUME_DEFAULT
#define SSB_RX_VOLUME_DEFAULT 16
#endif

// Queue n ADC DMA words (I low half-word, Q high); the buffer must stay valid until ssb_rx_poll() drained it
void ssb_rx_process_adc(const uint32_t *buf, int n);
// Demodulate the next small chunk of the queued block; call every main-loop pass
void ssb_rx_poll(void);

void ssb_rx_set_volume(uint8_t volume);
uint8_t ssb_rx_get_volume(void);

#endif
