#ifndef _DAC
#define _DAC
#include <stdint.h>

#define DAC_N_SAMPLES 1024

void dac_init();
void dac_dispatch();
void dac_start();
void dac_stop();
void on_dac(uint32_t *buf, int n);

#endif
