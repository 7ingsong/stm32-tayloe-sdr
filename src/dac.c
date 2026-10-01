#include "dac.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dac.h"
#include "stm32f10x_dma.h"
#include "stm32f10x.h"
#include "misc.h"
#include "utils.h"

#define IQ_PENDING_HALF0 0x01
#define IQ_PENDING_HALF1 0x02

static DMA_InitTypeDef DMA_InitStructure;
static TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
static DAC_InitTypeDef DAC_InitStructure;

// [[maybe_unused]] static uint32_t idx = 0;  
// [[maybe_unused]] static const uint16_t sine_12bit[DAC_N_SAMPLES] = {
//                       2047, 2447, 2831, 3185, 3498, 3750, 3939, 4056, 4095, 4056,
//                       3939, 3750, 3495, 3185, 2831, 2447, 2047, 1647, 1263, 909, 
//                       599, 344, 155, 38, 0, 38, 155, 344, 599, 909, 1263, 1647};


[[maybe_unused]] static uint32_t samples[DAC_N_SAMPLES];

static volatile uint8_t pending_mask = 0;

__attribute__((weak)) void on_dac(uint32_t *buf, int n) {
}


void DMA2_Channel4_5_IRQHandler(void) {
    if (DMA_GetITStatus(DMA2_IT_HT4)!= RESET) {
        pending_mask |= IQ_PENDING_HALF0;
        DMA_ClearITPendingBit(DMA2_IT_HT4);        
    }

    if (DMA_GetITStatus(DMA2_IT_TC4)!= RESET) {
        pending_mask |= IQ_PENDING_HALF1;
        DMA_ClearITPendingBit(DMA2_IT_TC4);
    }
}

void dac_init() {
    // for (idx = 0; idx < DAC_N_SAMPLES; idx++) {
    //     samples[idx] = (sine_12bit[idx] << 16) + (sine_12bit[idx]);
    // }

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

       
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Channel4_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);    

    
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure); 
    TIM_TimeBaseStructure.TIM_Period = 1125-1; // 72e6/1125 = 64000 HZ
    TIM_TimeBaseStructure.TIM_Prescaler = 1-1;//9-1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;    
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_SelectOutputTrigger(TIM2, TIM_TRGOSource_Update);

    
    DAC_InitStructure.DAC_Trigger = DAC_Trigger_T2_TRGO;
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
    DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Disable;
    DAC_Init(DAC_Channel_1, &DAC_InitStructure);
    DAC_Init(DAC_Channel_2, &DAC_InitStructure);

    DMA_DeInit(DMA2_Channel4);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(DAC->DHR12RD);
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)samples;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = DAC_N_SAMPLES;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA2_Channel4, &DMA_InitStructure);

    DMA_ITConfig(DMA2_Channel4, DMA_IT_HT | DMA_IT_TC, ENABLE);
}

void dac_start() {
    DMA_Cmd(DMA2_Channel4, ENABLE);

    DAC_Cmd(DAC_Channel_1, ENABLE);
    DAC_Cmd(DAC_Channel_2, ENABLE);

    DAC_DMACmd(DAC_Channel_2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

void dac_stop() {
    TIM_Cmd(TIM2, DISABLE);
    DAC_DMACmd(DAC_Channel_2, DISABLE);

    DAC_Cmd(DAC_Channel_1, DISABLE);
    DAC_Cmd(DAC_Channel_2, DISABLE);

    DMA_Cmd(DMA2_Channel4, DISABLE);
}

void dac_dispatch() {

    uint8_t mask;

    __disable_irq();
    mask = pending_mask;
    pending_mask = 0;
    __enable_irq();


    if (mask & IQ_PENDING_HALF0) {
        on_dac(&samples[0], DAC_N_SAMPLES/2);
    }

    if (mask & IQ_PENDING_HALF1) {
        on_dac(&samples[DAC_N_SAMPLES/2], DAC_N_SAMPLES/2);
    }
}