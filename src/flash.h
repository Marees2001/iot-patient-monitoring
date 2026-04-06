#ifndef FLASH_H
#define FLASH_H

#include <Arduino.h>
#include <stm32f1xx_hal_flash.h>

// ── Flash storage config ──────────────────────────────────────────────────────
#define FLASH_STORAGE_PAGE  63
#define FLASH_STORAGE_ADDR  (0x08000000U + (FLASH_STORAGE_PAGE * 0x400U))

#define FLASH_MAGIC  0xDEADBEEFU

// ─────────────────────────────────────────────────────────────────────────────
// Every struct stored with this library MUST begin with magic as first field:
//
//   struct MyConfig {
//       uint32_t magic;      // ← always first, set to FLASH_MAGIC
//       ...your fields...
//   };
// ─────────────────────────────────────────────────────────────────────────────


// ── Erase one flash page ──────────────────────────────────────────────────────
bool erase_flash_page(uint32_t pageAddress) {
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase   = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = pageAddress;
    eraseInit.NbPages     = 1;

    uint32_t          pageError = 0;
    HAL_StatusTypeDef status    = HAL_FLASHEx_Erase(&eraseInit, &pageError);

    HAL_FLASH_Lock();

    if (status != HAL_OK) {
        Serial.print("ERROR: Flash erase failed at 0x");
        Serial.println(pageAddress, HEX);
        return false;
    }
    return true;
}

// ── Write any struct to flash ─────────────────────────────────────────────────
bool write_to_flash(const void* data, size_t size, uint32_t address = FLASH_STORAGE_ADDR) {
    const uint8_t* src = (const uint8_t*)data;

    HAL_FLASH_Unlock();

    for (size_t i = 0; i < size; i += 2) {
        uint16_t halfWord;

        if ((i + 1) < size) {
            halfWord = (uint16_t)src[i] | ((uint16_t)src[i + 1] << 8);
        } else {
            halfWord = (uint16_t)src[i] | 0xFF00U;  // pad last odd byte
        }

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              address + i,
                              halfWord) != HAL_OK) {
            HAL_FLASH_Lock();
            Serial.print("ERROR: Flash write failed at 0x");
            Serial.println(address + i, HEX);
            return false;
        }
    }

    HAL_FLASH_Lock();
    return true;
}

// ── Read any struct from flash ────────────────────────────────────────────────
bool read_from_flash(void* out, size_t size, uint32_t address = FLASH_STORAGE_ADDR) {
    memcpy(out, (const void*)address, size);

    // Validate magic (must be first field of struct)
    if (*(uint32_t*)out != FLASH_MAGIC) {
        Serial.println("WARN: No valid data in flash.");
        return false;
    }

    return true;
}

// ── Save any struct (erase → write) ──────────────────────────────────────────
bool save_to_flash(const void* data, size_t size, uint32_t address = FLASH_STORAGE_ADDR) {
    uint32_t pageAddress = address & ~(0x400U - 1U);  // align to 1KB page

    if (!erase_flash_page(pageAddress)) return false;

    if (!write_to_flash(data, size, address)) {
        Serial.println("ERROR: Flash write failed!");
        return false;
    }

    Serial.println("Flash: data saved OK.");
    return true;
}

#endif // FLASH_H