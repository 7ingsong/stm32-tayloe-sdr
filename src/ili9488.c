#include "ili9488.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_dma.h"
#include "stm32f10x.h"
#include "utils.h"

#define CS_PIN GPIO_Pin_12
#define DC_PIN GPIO_Pin_14

#define CS_LOW()   (GPIOB->BRR = CS_PIN)
#define CS_HIGH()  (GPIOB->BSRR = CS_PIN)
#define DC_CMD()   (GPIOB->BRR = DC_PIN)
#define DC_DATA()  (GPIOB->BSRR = DC_PIN)

static ili9488_format_t format = ILI9488_RGB666;
static int dma_active;

// Wait until the last byte has really left the shift register (TXE alone is one byte early)
static void spi_drain(void) {
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET);
}

int ili9488_busy(void) {
    if (!dma_active) {
        return 0;
    }
    if (DMA_GetFlagStatus(DMA1_FLAG_TC5) == RESET) {
        return 1;
    }
    spi_drain();
    DMA_Cmd(DMA1_Channel5, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_TC5);
    CS_HIGH();
    dma_active = 0;
    return 0;
}

static void wait_idle(void) {
    while (ili9488_busy());
}

static void spi_send(const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
        SPI2->DR = data[i];
    }
    spi_drain();
}

void ili9488_command(uint8_t cmd, const uint8_t *params, uint16_t n) {
    wait_idle();
    CS_LOW();
    DC_CMD();
    spi_send(&cmd, 1);
    if (n) {
        DC_DATA();
        spi_send(params, n);
    }
    CS_HIGH();
}

#define CMD(c, ...) do { static const uint8_t p_[] = {__VA_ARGS__}; ili9488_command(c, p_, sizeof(p_)); } while (0)

static void spi_init(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15; // SCK, MOSI
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = CS_PIN | DC_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    CS_HIGH();

    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2; // PCLK1 36 MHz / 2 = 18 MHz
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
    SPI_Cmd(SPI2, ENABLE);
}

void ili9488_set_format(ili9488_format_t f) {
    if (f != format) {
        uint8_t p = (uint8_t)f;
        ili9488_command(0x3A, &p, 1); // COLMOD: interface pixel format
        format = f;
    }
}

void ili9488_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t col[] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    uint8_t row[] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};
    ili9488_command(0x2A, col, 4); // CASET
    ili9488_command(0x2B, row, 4); // PASET
    ili9488_command(0x2C, 0, 0);   // RAMWR: pixel data follows
}

// RAMWR data continues as long as D/C stays high, even across CS toggles (RAMWRC is not needed)
void ili9488_write_dma(const uint8_t *data, uint16_t len) {
    wait_idle();
    CS_LOW();
    DC_DATA();

    DMA_InitTypeDef DMA_InitStructure;
    DMA_DeInit(DMA1_Channel5); // SPI2_TX
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)data;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = len;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Low;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);
    dma_active = 1;
    DMA_Cmd(DMA1_Channel5, ENABLE);
}

void ili9488_write(const uint8_t *data, uint16_t len) {
    ili9488_write_dma(data, len);
    wait_idle();
}

void ili9488_init(void) {
    spi_init();

    ili9488_command(0x01, 0, 0); // SWRESET (the RESET pin is not driven)
    delay_ms(150);
    ili9488_command(0x11, 0, 0); // sleep out
    delay_ms(150);

    CMD(0xE0, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F); // gamma +
    CMD(0xE1, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F); // gamma -
    CMD(0xC0, 0x17, 0x15);       // power control 1
    CMD(0xC1, 0x41);             // power control 2
    CMD(0xC5, 0x00, 0x12, 0x80); // VCOM
#if ILI9488_LANDSCAPE
    CMD(0x36, 0x28);             // MADCTL: MV (row/column exchange) + BGR
#else
    CMD(0x36, 0x48);             // MADCTL: MX + BGR
#endif
    CMD(0x3A, ILI9488_RGB666);   // 18-bit pixels over SPI
    format = ILI9488_RGB666;
    CMD(0xB0, 0x00);             // interface mode: SDO not used
    CMD(0xB1, 0xA0);             // frame rate ~60 Hz
    CMD(0xB4, 0x02);             // 2-dot inversion
    CMD(0xB6, 0x02, 0x02, 0x3B); // display function
    CMD(0xE9, 0x00);             // set image function: 24-bit data bus off
    CMD(0xF7, 0xA9, 0x51, 0x2C, 0x82); // adjust control 3
    ili9488_command(0x29, 0, 0);         // display on
    delay_ms(20);
}

void ili9488_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b) {
    static uint8_t chunk[16 * 3]; // small on purpose: RAM is tight, and this is only used at startup
    for (int i = 0; i < 16; i++) {
        chunk[3 * i] = r;
        chunk[3 * i + 1] = g;
        chunk[3 * i + 2] = b;
    }
    ili9488_set_format(ILI9488_RGB666);
    ili9488_set_window(x, y, x + w - 1, y + h - 1);
    for (uint32_t left = (uint32_t)w * h; left > 0;) {
        uint16_t n = left > 16 ? 16 : left;
        ili9488_write(chunk, n * 3);
        left -= n;
    }
}
