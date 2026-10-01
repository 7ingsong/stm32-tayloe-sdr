#ifndef __FLASH__
#define __FLASH__

#include <stdint.h>

typedef struct {
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t overflow;
    uint8_t* buffer;
    int size;
} fifo_t;

void fifo_init(fifo_t* fifo, uint8_t* buffer, int size);
int fifo_read(fifo_t* fifo, uint8_t* dst, int max_len);
void fifo_write(fifo_t* fifo, const uint8_t* buf, int n);
int fifo_get_free_space(fifo_t* fifo);
int fifo_get_filled(fifo_t* fifo);
uint32_t fifo_get_overflow(fifo_t* fifo);
int fifo_get_size(fifo_t* fifo);

#endif