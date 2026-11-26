#if !defined(BUILD_BOOT)
/**
  * @file  storage.c
  * @brief 目标固件镜像存储: 内部 flash 擦写 (STM32F103 FPEC, 寄存器级)
  */
#include "storage.h"
#include "bsp.h"

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
    while ((FLASH->SR & FLASH_SR_BSY) && --to) {
        /* 页擦除约 40ms, 连续擦除上百页 (U盘写大固件) 会超过 3.2s 的看门狗;
           擦写都发生在 USB 中断里, 主循环没机会喂狗, 这里必须自己喂 */
        WDT_Feed();
    }
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
static uint32_t payload_offset(void);

const img_hdr_t *Storage_Header(void)
{
    return (const img_hdr_t *)IMG_STORE_ADDR;
}

uint8_t Storage_ImageValid(void)
{
    const img_hdr_t *h = Storage_Header();
    if (h->magic != IMG_MAGIC) return 0;
    if (h->length == 0 || h->length > IMG_MAX_PAYLOAD) return 0;
    if (h->length > IMG_STORE_SIZE - IMG_DATA_OFFSET - payload_offset()) return 0;
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

/* 第 0 页布局 (与 vfs.c 一致): 镜像头 32B + 文件名 64B + 首簇号 2B */
#define IMG_FW_NAME_OFF           32u
#define IMG_FW_NAME_LEN           64u
#define IMG_FW_FIRST_CLUSTER_OFF  96u
#define IMG_FIRST_DATA_CLUSTER    2u        /* FAT 数据区首簇, 对应载荷偏移 0 */

/* CDC 上传的镜像写入文件名, 使 U 盘里看到的文件名与 U 盘写入方式一致 */
void Storage_SetImageName(const char *name, uint32_t len)
{
    uint8_t  buf[IMG_FW_NAME_LEN];
    uint32_t i;

    if (len > IMG_FW_NAME_LEN - 1u) len = IMG_FW_NAME_LEN - 1u;
    for (i = 0; i < len; i++) {
        uint8_t c = (uint8_t)name[i];
        buf[i] = (c >= 0x20u && c < 0x7Fu) ? c : (uint8_t)'_';   /* 非 ASCII 用 '_' 代替 */
    }
    while (i < IMG_FW_NAME_LEN) buf[i++] = 0u;
    (void)Storage_Write(IMG_FW_NAME_OFF, buf, IMG_FW_NAME_LEN);
}

/* U 盘写入时文件不一定从簇 2 开始 (FAT 里有旧分配时 Windows 会往后放),
 * 载荷起点 = (首簇号 - 2) * 512; 空白(0xFFFF) 或非法值按 0 处理 */
static uint32_t payload_offset(void)
{
    uint16_t c = *(volatile const uint16_t *)(IMG_STORE_ADDR + IMG_FW_FIRST_CLUSTER_OFF);
    uint32_t off;

    if (c < IMG_FIRST_DATA_CLUSTER) return 0u;
    off = (uint32_t)(c - IMG_FIRST_DATA_CLUSTER) * 512u;
    if (off > IMG_STORE_SIZE - IMG_DATA_OFFSET) return 0u;
    return off;
}

uint32_t Storage_PayloadAddr(void)
{
    return IMG_STORE_ADDR + IMG_DATA_OFFSET + payload_offset();   /* 与 VFS(U 盘) 数据区偏移一致 */
}

const uint8_t *Storage_PayloadPtr(void)
{
    return (const uint8_t *)Storage_PayloadAddr();
}

/* ------------------------------------------------ 烧录流水记录区 ---------
 * 独立 2KB (RECORD_BASE), 不占用镜像区, 随离线固件更新而擦除:
 *   页 A (RECORD_BASE):
 *     [0]      魔术字 0xA55A (已初始化标志)
 *     [1..511] 流水槽位: 每成功烧录一次写一个半字 0x0000
 *   页 B (RECORD_BASE + 1KB):
 *     [0] 魔术字 0xA55A
 *     [1] 翻页次数 (页 A 记满后 +1, 然后清空页 A)
 * 累计成功次数 = 翻页次数 * 511 + 页 A 已用槽数
 * 一个 1KB 页可覆盖 511 次, 页 B 每 511 次才擦一次, 磨损很小。 */
#define REC_MAGIC      0xA55Au
#define REC_PAGE_B     (RECORD_BASE + FLASH_PAGE_SIZE)
#define REC_SLOT_MAX   (FLASH_PAGE_SIZE / 2u - 1u)   /* 511 */

static uint16_t rec_rd(uint32_t addr)
{
    return *(volatile const uint16_t *)addr;
}

/* 页 A 已用的流水槽数 (遇到第一个非 0x0000 就停) */
static uint32_t rec_marks(void)
{
    uint32_t i;
    for (i = 1; i <= REC_SLOT_MAX; i++) {
        if (rec_rd(RECORD_BASE + i * 2u) != 0x0000u) break;
    }
    return i - 1u;
}

uint8_t Storage_RecordErase(void)
{
    uint8_t r;
    flash_unlock();
    r = flash_erase_page(RECORD_BASE);
    if (r == STORAGE_OK) r = flash_erase_page(REC_PAGE_B);
    flash_lock();
    return r;
}

uint32_t Storage_RecordCount(void)
{
    uint32_t rolls = 0;

    if (rec_rd(RECORD_BASE) != REC_MAGIC) return 0;
    if (rec_rd(REC_PAGE_B) == REC_MAGIC) rolls = rec_rd(REC_PAGE_B + 2u);
    return rolls * REC_SLOT_MAX + rec_marks();
}

uint8_t Storage_RecordBurn(void)
{
    uint32_t marks;
    uint16_t rolls = 0;
    uint8_t  r = STORAGE_OK;

    flash_unlock();

    /* 首次使用: 两页都写魔术字, 保证后续计数可辨识 */
    if (rec_rd(RECORD_BASE) != REC_MAGIC) {
        r = flash_erase_page(RECORD_BASE);
        if (r == STORAGE_OK) r = flash_write_half(RECORD_BASE, REC_MAGIC);
        if (r == STORAGE_OK && rec_rd(REC_PAGE_B) != REC_MAGIC) {
            r = flash_erase_page(REC_PAGE_B);
            if (r == STORAGE_OK) r = flash_write_half(REC_PAGE_B, REC_MAGIC);
            if (r == STORAGE_OK) r = flash_write_half(REC_PAGE_B + 2u, 0x0000u);
        }
        if (r != STORAGE_OK) { flash_lock(); return r; }
    }

    marks = rec_marks();
    if (marks >= REC_SLOT_MAX) {
        /* 页 A 记满: 先把页 B 的翻页次数 +1 (掉电也不会丢已满的这一页),
           再清空页 A 重新开始流水 */
        if (rec_rd(REC_PAGE_B) == REC_MAGIC) rolls = rec_rd(REC_PAGE_B + 2u);
        if (rolls == 0xFFFFu) { flash_lock(); return STORAGE_ERR_FLASH; }
        r = flash_erase_page(REC_PAGE_B);
        if (r == STORAGE_OK) r = flash_write_half(REC_PAGE_B, REC_MAGIC);
        if (r == STORAGE_OK) r = flash_write_half(REC_PAGE_B + 2u, (uint16_t)(rolls + 1u));
        if (r == STORAGE_OK) r = flash_erase_page(RECORD_BASE);
        if (r == STORAGE_OK) r = flash_write_half(RECORD_BASE, REC_MAGIC);
        /* 本次这一笔也要记在刚清空的页 A 里, 否则进位那一次会少算 */
        if (r == STORAGE_OK) r = flash_write_half(RECORD_BASE + 2u, 0x0000u);
    } else {
        r = flash_write_half(RECORD_BASE + (marks + 1u) * 2u, 0x0000u);
    }

    flash_lock();
    return r;
}
#endif
