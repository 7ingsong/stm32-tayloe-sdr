#include "iq.h"
#include "dac.h"
#include "adc.h"
#include "utils.h"
#include "command.h"
#include "fifo.h"
#include "transport.h"
#include "stm32f10x.h"

typedef struct __attribute__((packed)) {
    uint32_t overflow;
    uint32_t tx;
} iq_stream_response_t;

typedef struct __attribute__((packed)) {
    uint16_t bs;    
    uint16_t free_space;
    uint16_t consumtion_fail;
    uint16_t dac_overflow;
    uint16_t tx_usb_overflow;
    uint16_t rx_usb_overflow;
} usb_stream_info_t;

static usb_stream_info_t resp_iq_stream_tx_info = {};

static fifo_t fifo_dac;
static uint8_t fifo_buffer_dac[1024*10+1];

static fifo_t fifo_adc;
[[maybe_unused]]static uint8_t fifo_buffer_adc[1024*10+1];

[[maybe_unused]]static uint8_t half = 0;

static int toggle = 0;

void on_adc(uint32_t *buf, int n){
    int size = n * sizeof(uint32_t);
    fifo_write(&fifo_adc, (uint8_t*)buf, size);

    led_control(toggle);
    toggle^=1;
}


void on_dac(uint32_t *buf, int n) {
    int size = n * sizeof(uint32_t);
    int filled_space = fifo_get_filled(&fifo_dac);
    if (filled_space >= size) {
        fifo_read(&fifo_dac, (uint8_t*)buf, size);
    }else{
        resp_iq_stream_tx_info.consumtion_fail++;
    }

    // led_control(half);
    half^=1;
}


void iq_init() {

    fifo_init(&fifo_dac, (uint8_t*)fifo_buffer_dac, sizeof(fifo_buffer_dac));
    fifo_init(&fifo_adc, (uint8_t*)fifo_buffer_adc, sizeof(fifo_buffer_adc));

    transport_init();
    
    dac_init();
    adc_init();
}

static void handle_ping(const frame_t* frame) {
    uint8_t payload[]={'P','O','N','G'};
    command_send(RESP_ACK, frame->command.seq, payload, sizeof(payload));
}

void command_handler(const frame_t* frame) {
    switch (frame->command.cmd) {
        case CMD_PING:
            handle_ping(frame);
            break;
        case CMD_IQ_STREAM_TX:
            fifo_write(&fifo_dac, frame->payload, frame->command.len);
            break;

        case CMD_IQ_STREAM_TX_START:
            dac_start();

            command_send(RESP_ACK, frame->command.seq, 0, 0);
            break;
        case CMD_IQ_STREAM_TX_STOP:
            dac_stop();

            command_send(RESP_ACK, frame->command.seq, 0, 0);
            break;

        case CMD_IQ_STREAM_RX_START:
            adc_start();
            command_send(RESP_ACK, frame->command.seq, 0, 0);
            break;
        case CMD_IQ_STREAM_RX_STOP:
            adc_stop();
            command_send(RESP_ACK, frame->command.seq, 0, 0);
            break;

        case CMD_IQ_STREAM_TX_INFO:
            fifo_write(&fifo_dac, frame->payload, frame->command.len);
            resp_iq_stream_tx_info.bs = FRAME_MAX_PAYLOAD;
            resp_iq_stream_tx_info.free_space = fifo_get_free_space(&fifo_dac);
            resp_iq_stream_tx_info.dac_overflow = fifo_get_overflow(&fifo_dac);
            resp_iq_stream_tx_info.tx_usb_overflow = transport_get_tx_overflow();
            resp_iq_stream_tx_info.rx_usb_overflow = transport_get_rx_overflow();
            
            command_send(RESP_IQ_STREAM_TX_INFO, frame->command.seq, (const uint8_t*)&resp_iq_stream_tx_info, sizeof(resp_iq_stream_tx_info));
            break;

        default:
            command_send_error(frame->command.seq, ERR_BAD_COMMAND, frame->command.cmd);
            break;
    }
}

void iq_dispatch() {
    dac_dispatch();
    adc_dispatch();

    static uint8_t iq_usb_stream_seq = 0;
    uint8_t buf[FRAME_MAX_PAYLOAD];
    int filled = fifo_get_filled(&fifo_adc);
    if (filled>=FRAME_MAX_PAYLOAD){
        fifo_read(&fifo_adc, buf, FRAME_MAX_PAYLOAD);
        command_send(RESP_IQ_STREAM_RX, iq_usb_stream_seq++, buf, FRAME_MAX_PAYLOAD);
    }

    command_dispatch(command_handler);
}