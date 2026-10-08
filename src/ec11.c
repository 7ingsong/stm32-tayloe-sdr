#include "ec11.h"
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_exti.h>
#include <stm32f10x.h>
#include <misc.h>

/*
 * EC11 encoder (schematic: EC-A = PA1, EC-B = PA2, push switch EC-D = PA0; common pins to GND,
 * 100 nF on each line for debouncing). Internal pull-ups; one count per detent on the falling edge of A.
 */

static volatile int steps = 0;

void EC11_Init(){
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource1);

    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = EXTI_Line1;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void EXTI1_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line1) == SET) {
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2)) {
            steps--;
        } else {
            steps++;
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

int EC11_TakeSteps(void) {
    __disable_irq();
    int s = steps;
    steps = 0;
    __enable_irq();
    return s;
}

int EC11_ButtonDown(void) {
    return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_RESET; // switch to GND, pulled up
}
