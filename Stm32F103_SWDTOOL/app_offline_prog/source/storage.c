/**
  * @file  storage.c
  * @brief 目标固件镜像存储: 内部 flash 擦写 (STM32F103 FPEC, 寄存器级)
  */
#include "storage.h"

#define FLASH_KEY1   0x45670123u
#define FLASH_KEY2   0xCDEF89ABu

static void flash_unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

static void flash_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static uint8_t flash_wait(void)
{
    uint32_t to = 200000u;
    while ((FLASH->SR & FLASH_SR_BSY) && --to) { }
    if (FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) {
        FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
        return STORAGE_ERR_FLASH;
    }
    return to ? STORAGE_OK : STORAGE_ERR_FLASH;
}

static uint8_t flash_erase_page(uint32_t addr)
{
    uint8_t r;
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = addr;
    FLASH->CR |= FLASH_CR_STRT;
    r = flash_wait();
    FLASH->CR &= ~FLASH_CR_PER;
    return r;
}

static uint8_t flash_write_half(uint32_t addr, uint16_t hw)
{
    uint8_t r;
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint16_t *)addr = hw;
    r = flash_wait();
    FLASH->CR &= ~FLASH_CR_PG;
    if (r == STORAGE_OK && *(volatile uint16_t *)addr != hw) r = STORAGE_ERR_FLASH;
    return r;
}

/* ------------------------------------------------ 对外接口 --------------- */
const img_hdr_t *Storage_Header(void)
{
    return (const img_hdr_t *)IMG_STORE_ADDR;
}

uint8_t Storage_ImageValid(void)
{
    const img_hdr_t *h = Storage_Header();
    if (h->magic != IMG_MAGIC) return 0;
    if (h->length == 0 || h->length > IMG_MAX_PAYLOAD) return 0;
    if (h->load_addr < 0x08000000u) return 0;
    if ((h->load_addr & 0xFF000000u) != 0x08000000u) return 0;
    return 1;
}

uint8_t Storage_Erase(void)
{
    uint32_t addr;
    uint8_t r = STORAGE_OK;
    flash_unlock();
    for (addr = IMG_STORE_ADDR; addr < IMG_STORE_ADDR + IMG_STORE_SIZE;
         addr += FLASH_PAGE_SIZE) {
        if (flash_erase_page(addr) != STORAGE_OK) { r = STORAGE_ERR_FLASH; break; }
    }
    flash_lock();
    return r;
}

uint8_t Storage_Write(uint32_t offset, const uint8_t *data, uint32_t len)
{
    uint32_t addr = IMG_STORE_ADDR + offset;
    uint8_t r = STORAGE_OK;

    if (offset + len > IMG_STORE_SIZE) return STORAGE_ERR_ARG;

    flash_unlock();
    while (len && r == STORAGE_OK) {
        uint16_t hw = *data++;
        if (len > 1u) { hw |= (uint16_t)(*data++) << 8; len -= 2u; }
        else          { hw |= 0xFF00u;                  len -= 1u; }
        r = flash_write_half(addr, hw);
        addr += 2u;
    }
    flash_lock();
    return r;
}

uint32_t Storage_PayloadAddr(void)
{
    return IMG_STORE_ADDR + sizeof(img_hdr_t);
}

const uint8_t *Storage_PayloadPtr(void)
{
    return (const uint8_t *)Storage_PayloadAddr();
}
