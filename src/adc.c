#include "adc.h"

#include <string.h>
#include <stdio.h>

#include "stm32f10x_conf.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "misc.h"
#include "command.h"
#include "transport.h"
#include "utils.h"

#define IQ_PENDING_HALF0 0x01
#define IQ_PENDING_HALF1 0x02

static volatile uint8_t pending_mask = 0;

static uint32_t samples[ADC_N_SAMPLES];

static DMA_InitTypeDef DMA_InitStructure;
static ADC_InitTypeDef ADC_InitStructure;

void DMA1_Channel1_IRQHandler(void){
    if (DMA_GetITStatus(DMA1_IT_HT1)) {
        pending_mask |= IQ_PENDING_HALF0;
        DMA_ClearITPendingBit(DMA1_IT_HT1);
    }

    if (DMA_GetITStatus(DMA1_IT_TC1)) {
        pending_mask |= IQ_PENDING_HALF1;
        DMA_ClearITPendingBit(DMA1_IT_TC1);
    }
}

__attribute__((weak)) void on_adc(uint32_t *buf, int n){}


void adc_dispatch(void) {
    uint8_t mask_rx;

    __disable_irq();
    mask_rx = pending_mask;
    pending_mask = 0;
    __enable_irq();


    if (mask_rx & IQ_PENDING_HALF0) {
        on_adc(&samples[0], ADC_N_SAMPLES/2);
    }

    if (mask_rx & IQ_PENDING_HALF1) {
        on_adc(&samples[ADC_N_SAMPLES/2], ADC_N_SAMPLES/2);
    }
}

void adc_init(){
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_3);
    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 3;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&samples[0];
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = ADC_N_SAMPLES;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    DMA_ITConfig(DMA1_Channel1, DMA_IT_HT | DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    RCC_ADCCLKConfig(RCC_PCLK2_Div6); // 12 MHZ 
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_InitStructure.ADC_Mode = ADC_Mode_RegSimult;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_6, 1, ADC_SampleTime_71Cycles5); // 12e6/(71.5+12.5) = 142857HZ

    ADC_Init(ADC2, &ADC_InitStructure);
    ADC_RegularChannelConfig(ADC2, ADC_Channel_7, 1, ADC_SampleTime_71Cycles5);
    ADC_ExternalTrigConvCmd(ADC2, ENABLE);

    ADC_TempSensorVrefintCmd(ENABLE);
}

void adc_start() {
    ADC_DMACmd(ADC1, ENABLE);
    
    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));

    ADC_Cmd(ADC2, ENABLE);
    ADC_ResetCalibration(ADC2);
    while(ADC_GetResetCalibrationStatus(ADC2));
    ADC_StartCalibration(ADC2);
    while(ADC_GetCalibrationStatus(ADC2));

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

void adc_stop() {
    ADC_SoftwareStartConvCmd(ADC1, DISABLE);
    ADC_Cmd(ADC2, DISABLE);
    ADC_Cmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);
}
