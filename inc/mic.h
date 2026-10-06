#ifndef _MIC
#define _MIC
#include <stdint.h>

/*
 * Microphone (via preamp) on PA3 = ADC3_IN3, sampled at 64 kHz in sync with the I/Q ADC and the DAC:
 * ADC3 is triggered by TIM3 CC1, TIM3 being the 64 kHz timer from adc.c.
 * DMA2 Channel5 fills a circular buffer of 12-bit right-aligned samples (0..4095, mid-scale ~2048).
 */

#define MIC_N_SAMPLES 1024 // two halves of 512 samples = 8 ms each

void mic_init();   // after adc_init(): reuses TIM3
void mic_start();
void mic_stop();
void mic_dispatch();
void on_mic(uint16_t *buf, int n);

#endif
