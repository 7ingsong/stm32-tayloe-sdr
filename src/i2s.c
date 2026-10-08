#include "i2s.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_dma.h"
#include "stm32f10x.h"
#include "misc.h"

#define I2S_PENDING_HALF0 0x01
#define I2S_PENDING_HALF1 0x02

#if I2S_RX
#define I2S_DMA_CHANNEL DMA2_Channel1 // SPI3_RX
#define I2S_DMA_IRQn    DMA2_Channel1_IRQn
#define I2S_DMA_IT_HT   DMA2_IT_HT1
#define I2S_DMA_IT_TC   DMA2_IT_TC1
#define I2S_DMA_REQ     SPI_I2S_DMAReq_Rx
#else
#define I2S_DMA_CHANNEL DMA2_Channel2 // SPI3_TX
#define I2S_DMA_IRQn    DMA2_Channel2_IRQn
#define I2S_DMA_IT_HT   DMA2_IT_HT2
#define I2S_DMA_IT_TC   DMA2_IT_TC2
#define I2S_DMA_REQ     SPI_I2S_DMAReq_Tx
#endif

static uint16_t samples[I2S_N_SAMPLES];

static volatile uint8_t pending_mask = 0;

// Half-buffers the main loop didn't refill before the DMA came back to them (heard as clicks)
volatile uint32_t i2s_overruns = 0;

__attribute__((weak)) void on_i2s(uint16_t *buf, int n) {}

#if I2S_RX
void DMA2_Channel1_IRQHandler(void) {
#else
void DMA2_Channel2_IRQHandler(void) {
#endif
    if (DMA_GetITStatus(I2S_DMA_IT_HT) != RESET) {
        if (pending_mask & I2S_PENDING_HALF0) i2s_overruns++;
        pending_mask |= I2S_PENDING_HALF0;
        DMA_ClearITPendingBit(I2S_DMA_IT_HT);
    }

    if (DMA_GetITStatus(I2S_DMA_IT_TC) != RESET) {
        if (pending_mask & I2S_PENDING_HALF1) i2s_overruns++;
        pending_mask |= I2S_PENDING_HALF1;
        DMA_ClearITPendingBit(I2S_DMA_IT_TC);
    }
}

void i2s_init() {
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);

    // PA15 (JTDI), PB3 (JTDO), PB4 (NJTRST) are JTAG after reset; keep SWD on PA13/PA14
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    // WS
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // CK
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // SD
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
#if I2S_RX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
#else
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
#endif
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = I2S_DMA_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    I2S_InitTypeDef I2S_InitStructure;
    SPI_I2S_DeInit(SPI3);
#if I2S_RX
    I2S_InitStructure.I2S_Mode = I2S_Mode_MasterRx;
#else
    I2S_InitStructure.I2S_Mode = I2S_Mode_MasterTx;
#endif
    I2S_InitStructure.I2S_Standard = I2S_Standard_Phillips;
    I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_24b;
    I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    I2S_InitStructure.I2S_AudioFreq = I2S_AudioFreq_48k; // 72e6/(64*23) = 48913 Hz
    I2S_InitStructure.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init(SPI3, &I2S_InitStructure);

    DMA_InitTypeDef DMA_InitStructure;
    DMA_DeInit(I2S_DMA_CHANNEL);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(SPI3->DR);
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)samples;
#if I2S_RX
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
#else
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
#endif
    DMA_InitStructure.DMA_BufferSize = I2S_N_SAMPLES;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(I2S_DMA_CHANNEL, &DMA_InitStructure);

    DMA_ITConfig(I2S_DMA_CHANNEL, DMA_IT_HT | DMA_IT_TC, ENABLE);
}

void i2s_start() {
#if !I2S_RX
    // TX starts from the beginning of the buffer, fill it before enabling DMA
    on_i2s(samples, I2S_N_SAMPLES);
    pending_mask = 0;
#endif
    DMA_Cmd(I2S_DMA_CHANNEL, ENABLE);
    SPI_I2S_DMACmd(SPI3, I2S_DMA_REQ, ENABLE);
    I2S_Cmd(SPI3, ENABLE);
}

void i2s_stop() {
    I2S_Cmd(SPI3, DISABLE);
    SPI_I2S_DMACmd(SPI3, I2S_DMA_REQ, DISABLE);
    DMA_Cmd(I2S_DMA_CHANNEL, DISABLE);
}

void i2s_dispatch() {
    uint8_t mask;

    __disable_irq();
    mask = pending_mask;
    pending_mask = 0;
    __enable_irq();

    // DMA position: if it is already back inside the half we are about to refill, we were too late
    if (mask & I2S_PENDING_HALF0) {
        if (I2S_N_SAMPLES - DMA_GetCurrDataCounter(I2S_DMA_CHANNEL) < I2S_N_SAMPLES/2) i2s_overruns++;
        on_i2s(&samples[0], I2S_N_SAMPLES/2);
    }

    if (mask & I2S_PENDING_HALF1) {
        if (I2S_N_SAMPLES - DMA_GetCurrDataCounter(I2S_DMA_CHANNEL) >= I2S_N_SAMPLES/2) i2s_overruns++;
        on_i2s(&samples[I2S_N_SAMPLES/2], I2S_N_SAMPLES/2);
    }
}
