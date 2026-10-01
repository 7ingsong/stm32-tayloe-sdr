#ifndef __FLASH__
#define __FLASH__

#define CONFIG_FLASH_ADDR (uint32_t)0x0807F800

void Flash_Write(void *data, int size);
void Flash_Read(void *data, int size);

#endif