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
// On-board radio mode (CMD_PTT): RX = SSB receiver on I2S, DAC silent; TX = mic SSB on the DAC, I2S silent;
// DUPLEX = both at once
enum { RADIO_RX = 0, RADIO_TX = 1, RADIO_DUPLEX = 2 };
uint8_t iq_get_ptt(void);
void iq_set_ptt(uint8_t mode); // larger values are clamped to RADIO_DUPLEX
void iq_tx_poll(void);         // modulates the next small chunk of the queued mic block; call often

#endif