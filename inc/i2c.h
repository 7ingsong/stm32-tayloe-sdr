#ifndef __I2C__
#define __I2C__
#include <stdint.h>

void I2C1_Config(void);
void I2C1_GPIO_Config(void);
void I2C_WriteByte(uint8_t devAddr, uint8_t regAddr, uint8_t data);
uint8_t I2C_ReadByte(uint8_t devAddr, uint8_t regAddr);
void I2C_Write(uint8_t address, uint8_t* data, uint16_t size);

#endif