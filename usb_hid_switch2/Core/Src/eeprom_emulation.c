/*
 * eeprom_emulation.c
 *
 *  Created on: Aug 9, 2025
 *      Author: rntsa
 */

#include "eeprom_emulation.h"
#include "stm32f0xx_hal.h"
#include <stdio.h>

static uint32_t find_last_written_address(void);
static void erase_storage_page(void);

uint8_t Flash_Append(uint16_t data)
{
    uint32_t address = find_last_written_address();

    // Page full? Erase and restart
    if (address >= (FLASH_STORAGE_ADDRESS + FLASH_PAGE_SIZE)) {
        erase_storage_page();
        address = FLASH_STORAGE_ADDRESS;
    }

    HAL_FLASH_Unlock();
    HAL_StatusTypeDef result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, data);
    HAL_FLASH_Lock();

    return (result == HAL_OK) ? 0 : 1;
}

static uint32_t find_last_written_address(void)
{
    uint32_t address = FLASH_STORAGE_ADDRESS;
    while (address < (FLASH_STORAGE_ADDRESS + FLASH_PAGE_SIZE))
    {
        uint16_t value = *(volatile uint16_t*)address;
        if (value == 0xFFFF)
            break;
        address += 2;
    }
    return address;
}

uint16_t Flash_ReadLastValue(void)
{
    uint32_t address = find_last_written_address();

    if (address == FLASH_STORAGE_ADDRESS)
        return 0xFFFF;  // Nothing written yet

    uint16_t last_value = *(volatile uint16_t*)(address - 2);

    // Corrupt data check
    if (last_value == 0 || last_value == 1 || last_value == 2)
    {
        return last_value;
    }
    else
    {
        Flash_Append(0); // corrupt
    }
    return 0;
}

static void erase_storage_page(void)
{
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = FLASH_STORAGE_ADDRESS;
    erase.NbPages = 1;

    HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();
}

void cycle_state_and_reset(void)
{
	uint16_t currentState = Flash_ReadLastValue();
	uint16_t nextState = (currentState == 0xFFFF) ? 0 : ((currentState + 1) % 3);

    printf("Switching from mode %u to %u...\r\n", currentState, nextState);

    if (Flash_Append(nextState) == 0) // Check for success
    {
        printf("New state saved. Resetting device now.\r\n");
        HAL_Delay(100);
        HAL_NVIC_SystemReset();
    }
    else
    {
        printf("FATAL ERROR: Failed to write new state! System halted.\r\n");
        HAL_NVIC_SystemReset();
    }
}
