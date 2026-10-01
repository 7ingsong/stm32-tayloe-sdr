#include "command.h"

#include <stm32f10x.h>
#include <string.h>

#include "transport.h"
#include "utils.h"
typedef struct __attribute__((packed)) {
    uint8_t magic0;
    uint8_t magic1;
    frame_t frame;
} packet_t;

typedef enum {
    RX_WAIT_MAGIC_0 = 0,
    RX_WAIT_MAGIC_1,
    RX_READ_HEADER,
    RX_READ_PAYLOAD,
} rx_parser_state_t;

rx_parser_state_t parser_state = RX_WAIT_MAGIC_0;
uint16_t header_pos = 0;
uint16_t payload_pos = 0;
uint8_t header_buf[FRAME_HEADER_SIZE];
frame_t rx_frame;
uint32_t usb_rx_overflow = 0;

static uint16_t frame_checksum(uint8_t cmd, uint8_t seq, uint16_t len, const uint8_t* data) {
    command_header_t command;
    uint16_t sum;

    command.cmd = cmd;
    command.seq = seq;
    command.len = len;

    sum = checksum16(0, (uint8_t*)&command, sizeof(command));

    if (len > 0) {
        sum = checksum16(sum, (uint8_t*)data, len);
    }
    return sum;
}

void command_send(uint8_t resp_cmd, uint8_t seq, const uint8_t* payload, uint16_t len) {
    packet_t pkt;

    if (len > FRAME_MAX_PAYLOAD) {
        return;
    }

    // __disable_irq();
    uint16_t crc = frame_checksum(resp_cmd, seq, len, payload);

    pkt.magic0 = FRAME_MAGIC_0;
    pkt.magic1 = FRAME_MAGIC_1;
    pkt.frame.command.cmd = resp_cmd;
    pkt.frame.command.seq = seq;
    pkt.frame.command.len = len;
    pkt.frame.crc = crc;

    if (len > 0) {
        memcpy(pkt.frame.payload, payload, len);
    }
    // __enable_irq();

    transport_send((uint8_t*)&pkt, PACKAGE_HEADER_SIZE + len);
}

void command_send_error(uint8_t seq, uint8_t err, uint8_t detail) {
    uint8_t payload[2];

    payload[0] = err;
    payload[1] = detail;
    command_send(RESP_ERR, seq, payload, sizeof(payload));
}

static void parser_reset() {
    parser_state = RX_WAIT_MAGIC_0;
    header_pos = 0;
    payload_pos = 0;
    rx_frame.command.len = 0;
}

void command_dispatch(const dispatch_frame_t dispatch_frame) {
    static uint8_t chunk[64];
    uint8_t data;
    int chunk_len;

    if (usb_rx_overflow != transport_get_rx_overflow()) {
        command_send_error(0, ERR_FIFO_OVERFLOW, 0);
        usb_rx_overflow = transport_get_rx_overflow();
    }

    while ((chunk_len = transport_recv(chunk, sizeof(chunk))) > 0) {
        for (int chunk_pos = 0; chunk_pos < chunk_len; chunk_pos++) {
            data = chunk[chunk_pos];
            switch (parser_state) {
                case RX_WAIT_MAGIC_0:
                    if (data == FRAME_MAGIC_0) {
                        parser_state = RX_WAIT_MAGIC_1;
                    }
                    break;

                case RX_WAIT_MAGIC_1:
                    if (data == FRAME_MAGIC_1) {
                        parser_state = RX_READ_HEADER;
                        header_pos = 0;
                    } else if (data != FRAME_MAGIC_0) {
                        parser_state = RX_WAIT_MAGIC_0;
                    }
                    break;

                case RX_READ_HEADER:
                    header_buf[header_pos++] = data;
                    if (header_pos == sizeof(header_buf)) {
                        rx_frame.command.cmd = header_buf[0];
                        rx_frame.command.seq = header_buf[1];
                        rx_frame.command.len = ((uint16_t)header_buf[3] << 8) | header_buf[2];
                        rx_frame.crc = ((uint16_t)header_buf[5] << 8) | header_buf[4];

                        if (rx_frame.command.len > FRAME_MAX_PAYLOAD) {
                            command_send_error(rx_frame.command.seq, ERR_BAD_LENGTH,
                                               rx_frame.command.len & 0xFF);
                            parser_reset();
                            break;
                        }

                        payload_pos = 0;
                        parser_state = RX_READ_PAYLOAD;
                        if (rx_frame.command.len == 0) {
                            if (frame_checksum(rx_frame.command.cmd, rx_frame.command.seq, 0,
                                               rx_frame.payload) != rx_frame.crc) {
                                command_send_error(rx_frame.command.seq, ERR_BAD_CRC,
                                                   rx_frame.command.cmd);
                            } else {
                                dispatch_frame(&rx_frame);
                            }
                            parser_reset();
                        }
                    }
                    break;

                case RX_READ_PAYLOAD:
                    rx_frame.payload[payload_pos++] = data;
                    if (payload_pos == rx_frame.command.len) {
                        if (frame_checksum(rx_frame.command.cmd, rx_frame.command.seq,
                                           rx_frame.command.len, rx_frame.payload) != rx_frame.crc) {
                            command_send_error(rx_frame.command.seq, ERR_BAD_CRC, rx_frame.command.cmd);
                        } else {
                            dispatch_frame(&rx_frame);
                        }
                        parser_reset();
                    }
                    break;
            }
        }
    }
}

uint16_t checksum16(uint16_t init, uint8_t* data, int length) {
    uint16_t sum = init;
    for (uint16_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}