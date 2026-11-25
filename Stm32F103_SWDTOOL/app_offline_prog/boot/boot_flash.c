#if defined(BUILD_BOOT)
/**
  * @file  boot_flash.c
  * @brief boot 内部使用的最小 flash 擦写 (STM32F103 FPEC, 寄存器级)
  *
  *   只操作应用区 (APP_BASE..) 和升级标志页 (BOOT_FLAG_ADDR),
  *   任何情况下都不会擦写 boot 代码区, 因此升级失败也不会变砖。
  */
#include <string.h>
#include "boot.h"
#include "crc.h"

#define FLASH_KEY1   0x45670123u
#define FLASH_KEY2   0xCDEF89ABu

#define FLASH_TIMEOUT_LOOP   2000000u

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

static void flash_clear_flags(void)
{
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}

/* 等擦除/编程结束: 页擦除最长约 40ms, 超时必须给足 */
static uint8_t flash_wait(void)
{
    uint32_t to = FLASH_TIMEOUT_LOOP;

    while ((FLASH->SR & FLASH_SR_BSY) && --to) { }
    if (to == 0) return 1;

    to = 20000u;
    while (!(FLASH->SR & FLASH_SR_EOP) && --to) { }

    if (FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) {
        flash_clear_flags();
        return 1;
    }
    FLASH->SR = FLASH_SR_EOP;
    return 0;
}

static uint8_t flash_page_blank(uint32_t addr)
{
    uint32_t i;
    for (i = 0; i < FLASH_PAGE_SIZE; i += 4u) {
        if (*(volatile uint32_t *)(addr + i) != 0xFFFFFFFFu) return 0;
    }
    return 1;
}

static uint8_t flash_erase_page(uint32_t addr)
{
    uint8_t r;

    flash_clear_flags();
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = addr;
    FLASH->CR |= FLASH_CR_STRT;
    r = flash_wait();
    FLASH->CR &= ~FLASH_CR_PER;

    if (r == 0 && !flash_page_blank(addr)) r = 1;
    return r;
}

static uint8_t flash_write_half(uint32_t addr, uint16_t hw)
{
    uint8_t r;

    flash_clear_flags();
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint16_t *)addr = hw;
    r = flash_wait();
    FLASH->CR &= ~FLASH_CR_PG;

    if (r == 0 && *(volatile uint16_t *)addr != hw) r = 1;
    return r;
}

/* ------------------------------------------------ 应用区操作 ------------ */
uint8_t BootFlash_EraseApp(uint32_t len)
{
    uint32_t addr;
    uint8_t r = 0;

    if (len == 0 || len > APP_SIZE) return 1;

    flash_unlock();
    for (addr = APP_BASE; addr < APP_BASE + len; addr += FLASH_PAGE_SIZE) {
        if (flash_erase_page(addr) != 0) { r = 1; break; }
    }
    flash_lock();
    return r;
}

uint8_t BootFlash_Write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint8_t r = 0;

    if (len == 0) return 1;
    if (addr < APP_BASE || addr + len > APP_BASE + APP_SIZE) return 1;

    flash_unlock();
    while (len && r == 0) {
        uint16_t hw = *data++;
        if (len > 1u) { hw |= (uint16_t)(*data++) << 8; len -= 2u; }
        else          { hw |= 0xFF00u;                  len -= 1u; }
        r = flash_write_half(addr, hw);
        addr += 2u;
    }
    flash_lock();
    return r;
}

uint32_t BootFlash_Crc32(uint32_t addr, uint32_t len)
{
    return CRC32_Update(0xFFFFFFFFu, (const uint8_t *)addr, len) ^ 0xFFFFFFFFu;
}

/* ------------------------------------------------ 升级标志页 ------------ */
uint8_t BootFlag_Read(boot_flag_t *f)
{
    memcpy(f, (const void *)BOOT_FLAG_ADDR, sizeof(*f));
    if (f->magic != BOOT_FLAG_MAGIC || f->flag != BOOT_FLAG_VALID) return 0;
    return 1;
}

uint8_t BootFlag_Write(const boot_flag_t *f)
{
    const uint8_t *p = (const uint8_t *)f;
    uint32_t addr = BOOT_FLAG_ADDR;
    uint32_t len = sizeof(*f);
    uint8_t r;

    flash_unlock();
    r = flash_erase_page(BOOT_FLAG_ADDR);
    while (len && r == 0) {
        uint16_t hw = *p++;
        if (len > 1u) { hw |= (uint16_t)(*p++) << 8; len -= 2u; }
        else          { hw |= 0xFF00u;               len -= 1u; }
        r = flash_write_half(addr, hw);
        addr += 2u;
    }
    flash_lock();

    /* 回读确认, 避免"标志没写进去却回 OK" */
    if (r == 0 && memcmp((const void *)BOOT_FLAG_ADDR, f, sizeof(*f)) != 0) r = 1;
    return r;
}

uint8_t Boot_AppValid(void)
{
    boot_flag_t f;

    if (!BootFlag_Read(&f)) return 0;
    if (f.len == 0 || f.len > APP_SIZE) return 0;
    return (BootFlash_Crc32(APP_BASE, f.len) == f.crc32) ? 1u : 0u;
}
#endif
