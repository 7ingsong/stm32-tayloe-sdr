#ifndef __UTILS__
#define __UTILS__
#include <stdint.h>

void delay_ms(uint32_t ms);
uint32_t get_ticks_ms();
void clock_init();
void led_init();
void led_control(int on);

#endif