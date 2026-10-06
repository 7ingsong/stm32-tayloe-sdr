#ifndef _AUDIO
#define _AUDIO
#include <stdint.h>

/*
 * Listen to the receiver on the I2S DAC, like radio_iq_rx.grc without the host:
 * ADC Q channel (Complex to Imag) at 64 kHz -> polyphase resampler -> I2S (48913 Hz), both channels.
 */

// Output = 16-bit sample * AUDIO_GAIN in the 24-bit I2S word; 8 puts a full-scale ADC signal at -30 dBFS
#ifndef AUDIO_GAIN
#define AUDIO_GAIN 8
#endif

void audio_process_adc(const uint32_t *buf, int n);

#endif
