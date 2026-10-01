#include "transport.h"

#include "fifo.h"
#include "misc.h"
#include "usb_istr.h"
#include "usbd.h"
#include "utils.h"

uint8_t usb_tx_buffer[USB_TX_FIFO_SIZE];
uint8_t usb_rx_buffer[USB_RX_FIFO_SIZE];

fifo_t usb_tx;
fifo_t usb_rx;

int usb_connected;
int usb_transmitting;

static int toggle2 = 0;

void OnUsbTransmitted() {
    toggle2 ^= 1;

    uint8_t usb_buf[MAX_USB_PACKET_SIZE];
    usb_transmitting = 0;
    __disable_irq();
    int n = fifo_read(&usb_tx, usb_buf, sizeof(usb_buf));
    __enable_irq();
    if (n > 0) {
        usb_transmitting = 1;
        CDC_Send_DATA(usb_buf, n);
    }
}

void OnUsbReceived(uint8_t* buf, int n) {
    __disable_irq();
    fifo_write(&usb_rx, buf, n);
    __enable_irq();
}

void OnUsbUnconnected() { usb_connected = 0; }

void OnUsbConfigured() {
    usb_connected = 1;
    OnUsbTransmitted();
}

void transport_send(uint8_t* data, int len) {
    __disable_irq();
    if (len > fifo_get_free_space(&usb_tx)) {
        /* Not enough room: drop this whole frame rather than letting
         * fifo_write() evict oldest bytes, which can land mid-frame and
         * corrupt an already-queued, not-yet-transmitted packet. */
        usb_tx.overflow += len;
        __enable_irq();
        return;
    }
    fifo_write(&usb_tx, data, len);
    __enable_irq();
    if (!usb_transmitting) {
        OnUsbTransmitted();
    }
}

void transport_init(void) {
    usb_connected = 0;
    usb_transmitting = 0;

    Set_System();
    Set_USBClock();
    USB_Interrupts_Config();
    USB_Init();

    fifo_init(&usb_tx, usb_tx_buffer, sizeof(usb_tx_buffer));
    fifo_init(&usb_rx, usb_rx_buffer, sizeof(usb_rx_buffer));
}

int transport_recv(uint8_t* buf, int max_len) {
    __disable_irq();
    int n = fifo_read(&usb_rx, buf, max_len);
    __enable_irq();
    return n;
}
uint32_t transport_get_rx_overflow(void) { return fifo_get_overflow(&usb_rx); }
uint32_t transport_get_tx_overflow(void) { return fifo_get_overflow(&usb_tx); }

uint32_t transport_get_rx_free_space(void) { return fifo_get_free_space(&usb_rx); }
uint32_t transport_get_tx_free_space(void) { return fifo_get_free_space(&usb_tx); }

uint32_t transport_get_rx_size(void) { return fifo_get_size(&usb_rx); }
uint32_t transport_get_tx_size(void) { return fifo_get_size(&usb_tx); }
