#include "flash.h"
#include <stm32f10x_flash.h>
#include <string.h>
#include <stdint.h>
#include "stm32f10x_flash.h"

void Flash_Write(void *data, int size) {
    FLASH_Unlock();

    FLASH_ErasePage(CONFIG_FLASH_ADDR);

    uint32_t *p = (uint32_t *)data;
    for (uint32_t i = 0; i < size/4; i++) {
        FLASH_ProgramWord(CONFIG_FLASH_ADDR + i*4, p[i]);
    }

    FLASH_Lock();
}

void Flash_Read(void *data, int size) {
    memcpy(data, (void*)CONFIG_FLASH_ADDR, size);
}


