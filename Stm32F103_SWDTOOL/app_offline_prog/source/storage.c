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

/* 一次擦除/编程最长约 40ms。该循环每圈约 8 个周期, 72MHz 下
 * 2,000,000 次 ≈ 220ms, 留足余量。
 * (原 200000 次 ≈ 22ms, 正好卡在页擦除典型 20ms / 最大 40ms 的边缘) */
#define FLASH_TIMEOUT_LOOP   2000000u

static void flash_clear_flags(void)
{
    /* EOP/PGERR/WRPRTERR 均为写 1 清零 */
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}

/* 等待一次擦除/编程真正结束。
 * 先等 BSY 清零 (页擦除最长约 40ms, 必须给足超时):
 * 原实现只等 200000 圈 (约 22ms), 若擦除尚未结束就收尾,
 * 紧接着的编程会被硬件丢弃, 扇区保持 0xFF —— 这正是"文件开头
 * 512 字节丢失"的直接原因。
 * 之后短暂等 EOP 确认操作完成, 再清 PG/PER 位。 */
static uint8_t flash_wait(void)
{
    uint32_t to = FLASH_TIMEOUT_LOOP;
    while ((FLASH->SR & FLASH_SR_BSY) && --to) { }
    if (to == 0) return STORAGE_ERR_FLASH;

    /* EOP 正常会立即置位; 个别兼容芯片不置 EOP 时最多再等约 2ms, 不据此判错 */
    to = 20000u;
    while (!(FLASH->SR & FLASH_SR_EOP) && --to) { }

    if (FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) {
        flash_clear_flags();
        return STORAGE_ERR_FLASH;
    }
    FLASH->SR = FLASH_SR_EOP;
    return STORAGE_OK;
}

/* 整页回读确认已擦净 (全 0xFF) */
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
    flash_clear_flags();        /* 清掉上一次操作残留的 EOP/错误标志 */
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = addr;
    FLASH->CR |= FLASH_CR_STRT;
    r = flash_wait();
    FLASH->CR &= ~FLASH_CR_PER;
    /* 擦除必须真的擦净, 否则后面往这一页编程会全部失败 */
    if (r == STORAGE_OK && !flash_page_blank(addr)) r = STORAGE_ERR_FLASH;
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

uint8_t Storage_ErasePage(uint32_t offset)
{
    uint8_t r;
    if (offset >= IMG_STORE_SIZE) return STORAGE_ERR_ARG;
    flash_unlock();
    r = flash_erase_page(IMG_STORE_ADDR + offset);
    flash_lock();
    return r;
}

uint8_t Storage_UpdateHeader(const img_hdr_t *hdr)
{
    uint8_t r;
    flash_unlock();
    /* 只擦除第 0 页 (镜像头独占, 数据从第 1 页开始) */
    r = flash_erase_page(IMG_STORE_ADDR);
    if (r == STORAGE_OK) {
        const uint8_t *p = (const uint8_t *)hdr;
        uint32_t len = sizeof(img_hdr_t);
        uint32_t addr = IMG_STORE_ADDR;
        while (len && r == STORAGE_OK) {
            uint16_t hw = *p++;
            if (len > 1u) { hw |= (uint16_t)(*p++) << 8; len -= 2u; }
            else          { hw |= 0xFF00u;               len -= 1u; }
            r = flash_write_half(addr, hw);
            addr += 2u;
        }
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
