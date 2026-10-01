#ifndef __CONFIG__
#define __CONFIG__

#include <stm32f10x.h>
#include <stm32f10x_gpio.h>

#define MCU_LED GPIO_Pin_12

#define FPGA_RST GPIO_Pin_8
#define FPGA_RESET_B GPIO_Pin_6
#define FPGA_CDONE GPIO_Pin_7

#define IQ_SPI_SS GPIO_Pin_15
#define IQ_SPI_MOSI GPIO_Pin_5
#define IQ_SPI_MISO GPIO_Pin_4
#define IQ_SPI_CLK GPIO_Pin_3

#define FPGA_SPI_SS GPIO_Pin_2
#define FPGA_SPI_SDI GPIO_Pin_5
#define FPGA_SPI_SDO GPIO_Pin_4
#define FPGA_SPI_CLK GPIO_Pin_3

#define RF_SPI_SS GPIO_Pin_12
#define RF_SPI_SDI GPIO_Pin_15
#define RF_SPI_SDO GPIO_Pin_14
#define RF_SPI_CLK GPIO_Pin_13

#define RF_RSTN GPIO_Pin_10
#define RF_IRQ GPIO_Pin_9

#define IQ_SPI_DMA_RX_CHANNEL DMA1_Channel2
#define IQ_SPI_DMA_RX_IRQn DMA1_Channel2_IRQn
// SPI1 TX is DMA1 Channel3 on STM32F1
#define IQ_SPI_DMA_TX_CHANNEL DMA1_Channel3
#define IQ_SPI_DMA_TX_IRQn DMA1_Channel3_IRQn

#endif