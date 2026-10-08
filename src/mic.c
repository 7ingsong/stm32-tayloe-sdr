#include "mic.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_tim.h"
#include "stm32f10x.h"

static uint16_t samples[MIC_N_SAMPLES];

__attribute__((weak)) void on_mic(uint16_t *buf, int n) {}

void mic_init() {
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC3, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // TIM3 CC1 fires once per 64 kHz period. Its output pin would be PA6, which stays an analog
    // input (I channel), so enabling the channel only produces the internal trigger.
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 1125 / 2;
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);

    // DMA2 Channel5 = ADC3. Its IRQ is shared with the DAC (DMA2_Channel4_5), so the
    // half/full flags are polled in mic_dispatch() instead.
    DMA_InitTypeDef DMA_InitStructure;
    DMA_DeInit(DMA2_Channel5);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC3->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)samples;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = MIC_N_SAMPLES;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA2_Channel5, &DMA_InitStructure);

    // ADC clock is already PCLK2/6 = 12 MHz (adc.c)
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_CC1;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC3, &ADC_InitStructure);
    ADC_RegularChannelConfig(ADC3, ADC_Channel_3, 1, ADC_SampleTime_71Cycles5); // 7 us, fits the 15.6 us period
}

void mic_start() {
    DMA_ClearFlag(DMA2_FLAG_HT5 | DMA2_FLAG_TC5);
    DMA_Cmd(DMA2_Channel5, ENABLE);
    ADC_DMACmd(ADC3, ENABLE);

    ADC_Cmd(ADC3, ENABLE);
    ADC_ResetCalibration(ADC3);
    while (ADC_GetResetCalibrationStatus(ADC3));
    ADC_StartCalibration(ADC3);
    while (ADC_GetCalibrationStatus(ADC3));

    ADC_ExternalTrigConvCmd(ADC3, ENABLE);
}

void mic_stop() {
    ADC_ExternalTrigConvCmd(ADC3, DISABLE);
    ADC_Cmd(ADC3, DISABLE);
    ADC_DMACmd(ADC3, DISABLE);
    DMA_Cmd(DMA2_Channel5, DISABLE);
}

void mic_dispatch() {
    if (DMA_GetFlagStatus(DMA2_FLAG_HT5) != RESET) {
        DMA_ClearFlag(DMA2_FLAG_HT5);
        on_mic(&samples[0], MIC_N_SAMPLES / 2);
    }
    if (DMA_GetFlagStatus(DMA2_FLAG_TC5) != RESET) {
        DMA_ClearFlag(DMA2_FLAG_TC5);
        on_mic(&samples[MIC_N_SAMPLES / 2], MIC_N_SAMPLES / 2);
    }
}
