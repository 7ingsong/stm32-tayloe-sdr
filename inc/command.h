#ifndef __COMMAND__
#define __COMMAND__
#include <stdint.h>

#define FRAME_MAGIC_0 0xA5
#define FRAME_MAGIC_1 0x5A
#define PACKAGE_HEADER_SIZE 8
#define FRAME_HEADER_SIZE 6
#define FRAME_MAX_PAYLOAD 256

enum {
    CMD_PING = 0x01,
    CMD_INFO = 0x02,
    CMD_EEPROM_READ = 0x10,
    CMD_EEPROM_WRITE = 0x11,
    CMD_FPGA_CONFIG = 0x20,
    // CMD_IQ_STREAM = 0x30,
    CMD_IQ_STREAM_TX = 0x31,
    CMD_IQ_STREAM_TX_INFO = 0x32,
    CMD_IQ_STREAM_TX_START = 0x33,
    CMD_IQ_STREAM_TX_STOP = 0x34,
    CMD_IQ_STREAM_RX_START = 0x35,
    CMD_IQ_STREAM_RX_STOP = 0x36,
};

enum {
    RESP_ACK = 0x80,
    RESP_ERR = 0x81,
    RESP_INFO = 0x82,
    RESP_EEPROM_READ = 0x90,
    RESP_EEPROM_WRITE = 0x91,
    RESP_FPGA_CONFIG = 0xA0,
    // RESP_IQ_STREAM = 0xB0,
    RESP_IQ_STREAM_RX = 0xB1,
    RESP_IQ_STREAM_TX_INFO = 0xB2,
};

enum {
    ERR_BAD_MAGIC = 1,
    ERR_BAD_LENGTH = 2,
    ERR_BAD_CRC = 3,
    ERR_BAD_COMMAND = 4,
    ERR_BAD_PAYLOAD = 5,
    ERR_FIFO_OVERFLOW = 6,
    ERR_EEPROM = 7,
    ERR_FPGA = 8,
    ERR_BAD_IMAGE = 9,
};

typedef struct __attribute__((packed)) {
    uint8_t cmd;
    uint8_t seq;
    uint16_t len;
} command_header_t;

typedef struct __attribute__((packed)) {
    command_header_t command;
    uint16_t crc;
    uint8_t payload[FRAME_MAX_PAYLOAD];
} frame_t;

typedef void (*dispatch_frame_t)(const frame_t* frame);

void command_send(uint8_t resp_cmd, uint8_t seq, const uint8_t* payload, uint16_t len);
void command_send_error(uint8_t seq, uint8_t err, uint8_t detail);
void command_dispatch(const dispatch_frame_t dispatch_frame);
uint16_t checksum16(uint16_t init, uint8_t* data, int length);

#endif