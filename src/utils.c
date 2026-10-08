#include "utils.h"
#include <stm32f10x_rcc.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_flash.h>
#include "config.h"
volatile uint32_t ms_ticks = 0;  // Counter for milliseconds

void SysTick_Handler(void) {
    ms_ticks++;  // Increment counter every millisecond
}


void systick_init(){
    SysTick_Config(SystemCoreClock / 1000);
}

void clock_init(){
    // 72 Mhz
    RCC_DeInit();
    RCC_HSEConfig(RCC_HSE_Bypass); // use RCC_HSE_ON with external crystal
    while (RCC_WaitForHSEStartUp() == ERROR);

    RCC_HCLKConfig(RCC_SYSCLK_Div1); /// HCLK = SYSCLK
    RCC_PCLK2Config(RCC_HCLK_Div1); /// PCLK2 = HCLK
    RCC_PCLK1Config(RCC_HCLK_Div2); /// PCLK1 = HCLK/2
    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9); /// PLLCLK = 8MHz * 9 = 72 MHz

    // 72 MHz needs 2 flash wait states. Don't rely on SystemInit() for this: it only sets them when the HSE
    // is already running, which after power-on it isn't (the Si5351 CLK2 is programmed later, in main).
    // With 0 wait states the core fetched garbage right after the switch and locked up in a HardFault.
    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
    FLASH_SetLatency(FLASH_Latency_2);

    RCC_PLLCmd(ENABLE); // Enable PLL
    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET); /// Wait till PLL is ready
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while(RCC_GetSYSCLKSource() != 0x08); /// Wait till PLL is used as system clock source  

    systick_init();
}


void delay_ms(uint32_t ms) {
    uint32_t target = ms_ticks + ms;
    while (ms_ticks < target);
}

uint32_t get_ticks_ms(){
    return ms_ticks;
}

void led_init() {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Pin = MCU_LED;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_WriteBit(GPIOB, MCU_LED, 0);
}

void led_control(int on) { GPIO_WriteBit(GPIOB, MCU_LED, on); }

