#include "i2c.h"
#include <stm32f10x_i2c.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x.h>

#define I2Cx I2C1

void I2C1_GPIO_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    // Enable GPIOB and AFIO clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    // Configure PB6 (I2C1_SCL) and PB7 (I2C1_SDA) as Alternate Function Open-Drain
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void I2C1_Config(void) {
    I2C_InitTypeDef I2C_InitStructure;

    I2C_DeInit(I2C1);

    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 400000;  // 400 kHz: Si5351 and SSD1306 both support fast mode

    I2C_Init(I2C1, &I2C_InitStructure);

    I2C_Cmd(I2C1, ENABLE);    
}


void I2C_WriteByte(uint8_t devAddr, uint8_t regAddr, uint8_t data)
{
    while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY));

    I2C_GenerateSTART(I2Cx, ENABLE);
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT)); // EV5

    I2C_Send7bitAddress(I2Cx, devAddr << 1, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)); // EV6

    I2C_SendData(I2Cx, regAddr);
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED)); // EV8

    I2C_SendData(I2Cx, data);
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED)); // EV8

    I2C_GenerateSTOP(I2Cx, ENABLE);
}

uint8_t I2C_ReadByte(uint8_t devAddr, uint8_t regAddr)
{
    uint8_t data;

    while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY) == SET);

    // Write register address
    I2C_GenerateSTART(I2Cx, ENABLE);
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT)); // EV5

    I2C_Send7bitAddress(I2Cx, devAddr << 1, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)); // EV6

    I2C_SendData(I2Cx, regAddr);
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED)); // EV8

    // Restart and read
    I2C_GenerateSTART(I2Cx, ENABLE);
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT)); // EV5

    I2C_Send7bitAddress(I2Cx, devAddr << 1, I2C_Direction_Receiver);
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)); // EV6

    // Disable ACK and prepare STOP for 1-byte read
    I2C_AcknowledgeConfig(I2Cx, DISABLE);
    I2C_GenerateSTOP(I2Cx, ENABLE);

    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED)); // EV7
    data = I2C_ReceiveData(I2Cx);

    return data;
}



// ~1 ms of polling at 72 MHz: a byte takes 25 us at 400 kHz, so this only trips on a missing/stuck device
#define I2C_TIMEOUT 20000

static int wait_event(uint32_t event) {
    for (uint32_t t = 0; t < I2C_TIMEOUT; t++) {
        if (I2C_CheckEvent(I2Cx, event)) {
            return 0;
        }
    }
    return 1;
}

// Returns 0 on success, 1 if the device didn't answer (e.g. the OLED is not connected) instead of hanging
int I2C_Write(uint8_t address, const uint8_t* data, uint16_t size) {
    for (uint32_t t = 0; I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY); t++) {
        if (t >= I2C_TIMEOUT) {
            return 1;
        }
    }

    I2C_GenerateSTART(I2Cx, ENABLE);
    if (wait_event(I2C_EVENT_MASTER_MODE_SELECT)) goto fail;

    I2C_Send7bitAddress(I2Cx, address << 1, I2C_Direction_Transmitter);
    if (wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) goto fail; // NACK on the address ends up here

    for (uint16_t i = 0; i < size; i++) {
        I2C_SendData(I2Cx, data[i]);
        if (wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) goto fail;
    }

    I2C_GenerateSTOP(I2Cx, ENABLE);
    return 0;

fail:
    I2C_GenerateSTOP(I2Cx, ENABLE);
    I2C_ClearFlag(I2Cx, I2C_FLAG_AF);
    return 1;
}
