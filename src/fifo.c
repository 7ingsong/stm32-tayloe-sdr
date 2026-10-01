#include "fifo.h"

#include <stm32f10x.h>
#include <string.h>

void fifo_init(fifo_t* fifo, uint8_t* buffer, int size) {
    fifo->head = 0;
    fifo->tail = 0;
    fifo->overflow = 0;
    fifo->buffer = buffer;
    fifo->size = size;
}

uint32_t fifo_get_overflow(fifo_t* fifo) { return fifo->overflow; }

int fifo_get_free_space(fifo_t* fifo) {
    int filled = fifo_get_filled(fifo);
    return fifo->size - 1 - filled; /* capacity is size-1 */
}

int fifo_get_filled(fifo_t* fifo) {
    int filled = (fifo->head + fifo->size - fifo->tail) % fifo->size;
    return filled;
}

int fifo_get_size(fifo_t* fifo) {
    return fifo->size-1;
}


void fifo_write(fifo_t* fifo, const uint8_t* buf, int n) {
    if (n <= 0) return;

    int size = fifo->size;
    /* current fill */
    int filled = (fifo->head + size - fifo->tail) % size;
    int free_space = size - 1 - filled; /* capacity is size-1 */

    int overwritten = 0;
    if (n > free_space) {
        overwritten = n - free_space;
        fifo->overflow += overwritten;
        /* advance tail to drop oldest bytes */
        fifo->tail = (fifo->tail + overwritten) % size;
    }

    /* write in up to two memcpy chunks to handle wrap */
    int first = n;
    if (first > size - fifo->head) first = size - fifo->head;
    memcpy(&fifo->buffer[fifo->head], buf, first);
    if (n - first > 0) {
        memcpy(&fifo->buffer[0], buf + first, n - first);
    }

    fifo->head = (fifo->head + n) % size;
}

int fifo_read(fifo_t* fifo, uint8_t* dst, int max_len) {
    int size = fifo->size;
    int filled = (fifo->head + size - fifo->tail) % size;
    int n = max_len < filled ? max_len : filled;

    if (n <= 0) return 0;

    /* read in up to two memcpy chunks to handle wrap */
    int first = n;
    if (first > size - fifo->tail) first = size - fifo->tail;
    memcpy(dst, &fifo->buffer[fifo->tail], first);
    if (n - first > 0) {
        memcpy(dst + first, &fifo->buffer[0], n - first);
    }

    fifo->tail = (fifo->tail + n) % size;

    return n;
}
