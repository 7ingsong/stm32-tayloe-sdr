#ifndef __TRANSPORT__
#define __TRANSPORT__
#include <stdint.h>

#define MAX_USB_PACKET_SIZE 63

#define USB_RX_FIFO_SIZE (1024*3 + 1)
#define USB_TX_FIFO_SIZE (1024*3 + 1)

void transport_init(void);
void transport_send(uint8_t* data, int len);
int transport_recv(uint8_t* data, int max_len);
uint32_t transport_get_rx_overflow(void);
uint32_t transport_get_tx_overflow(void);

uint32_t transport_get_rx_free_space(void);
uint32_t transport_get_tx_free_space(void);

uint32_t transport_get_rx_size(void);
uint32_t transport_get_tx_size(void);


#endif