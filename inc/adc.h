#ifndef _PERIPHERAL
#define _PERIPHERAL
#include <stdint.h>

#define ADC_N_SAMPLES 1024

void adc_init();
void on_adc(uint32_t *, int);
void adc_dispatch();
void adc_start();
void adc_stop();

#endif