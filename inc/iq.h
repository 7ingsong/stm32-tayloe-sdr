#ifndef _IQ
#define _IQ

#include <stdint.h>

#define LO_FREQ_DEFAULT 10000000
#define LO_FREQ_MIN     1400000   // si5351_CalcIQ() range for 90° I/Q outputs
#define LO_FREQ_MAX     100000000

void iq_init();
void iq_dispatch();
void iq_set_frequency(uint32_t frequency);
uint32_t iq_get_frequency(void);
uint8_t iq_get_ptt(void);
void iq_set_ptt(uint8_t on); // on-board radio: 1 = transmit (mic SSB), 0 = receive

#endif