/**
  * @file  target_prog.c
  * @brief STM32F103 目标烧录实现
  */
#include "target_prog.h"
#include "swd_host.h"
#include "storage.h"
#include "proto.h"
#include "crc.h"
#include "bsp.h"
#include "board.h"

/* ---- Cortex-M 内核寄存器 ---- */
#define DHCSR        0xE000EDF0u
#define AIRCR        0xE000ED0Cu
/* ---- STM32F1 DBGMCU ---- */
#define DBG_IDCODE   0xE0042000u
#define DBG_CR       0xE0042004u
#define DBG_IWDG_STOP  0x00000100u
#define DBG_WWDG_STOP  0x00000200u
/* ---- STM32F1 FPEC ---- */
#define FPEC_KEYR    0x40022004u
#define FPEC_SR      0x4002200Cu
#define FPEC_CR      0x40022010u
#define FPEC_KEY1    0x45670123u
#define FPEC_KEY2    0xCDEF89ABu
#define SR_BSY       0x01u
#define SR_PGERR     0x04u
#define SR_WRPRTERR  0x10u
#define SR_EOP       0x20u
#define CR_PG        0x01u
#define CR_MER       0x04u
#define CR_STRT      0x40u
#define CR_LOCK      0x80u

#define ERASE_TIMEOUT_MS   3000u
#define WRITE_TIMEOUT_MS   100u

/* ---- 目标身份校验 (按 board.h 的 TARGET_CHIP 配置) ---- */
static uint8_t target_id_check(uint32_t idcode)
{
    uint32_t dev = idcode & 0xFFFu;
    uint32_t rev = (idcode >> 16) & 0xFFFFu;

#if TARGET_CHIP == TARGET_CHIP_GD32F1X0
    /* GD32F1x0 (GD32F130/150): DEV_ID 0x410, REV_ID 0x1303 */
    if (dev != 0x410u) return TP_ERR_IDCODE;
    if (rev != 0x1303u) return TP_ERR_IDCODE;    /* 防止误烧 STM32F1 */
    return TP_OK;
#else
    /* STM32F1: 中容量/低容量/大容量/互联型/XL 系列 */
    (void)rev;
    if (dev == 0x410u || dev == 0x412u || dev == 0x414u ||
        dev == 0x418u || dev == 0x420u || dev == 0x428u || dev == 0x430u)
        return TP_OK;
    return TP_ERR_IDCODE;
#endif
}

/* 读 FPEC SR 并等待 BSY 清零; 返回 TP_xxx */
static uint8_t fpec_wait(uint32_t timeout_ms, uint32_t *sr_out)
{
    uint32_t t0 = BSP_Millis();
    uint32_t sr = 0;
    for (;;) {
        Proto_Poll();        /* 长操作中保持 USB 协议可响应 */
        if (SWD_Mem_Read32(FPEC_SR, &sr) != SWD_OK) return TP_ERR_CONNECT;
        if (!(sr & SR_BSY)) break;
        if ((BSP_Millis() - t0) > timeout_ms) return TP_ERR_WRITE;
    }
    if (sr_out) *sr_out = sr;
    return TP_OK;
}

static uint8_t fpec_clear_flags(void)
{
    return (SWD_Mem_Write32(FPEC_SR, SR_EOP | SR_PGERR | SR_WRPRTERR) == SWD_OK)
           ? TP_OK : TP_ERR_CONNECT;
}

uint8_t TargetProg_Detect(void)
{
    uint32_t dpidr, idcode;
    uint8_t r;

    r = SWD_Connect(&dpidr);
    g_status.swd_err = r;
    if (r != SWD_OK) {
        SWD_GPIO_Release();
        return TP_ERR_CONNECT;
    }
    SWD_ReleaseNRST();      /* 探测完毕, 让目标正常跑 */
    g_status.target_dpidr = dpidr;

    if (SWD_Mem_Read32(DBG_IDCODE, &idcode) == SWD_OK)
        g_status.target_idcode = idcode;

    return TP_OK;    /* 保持连接, 由调用者决定后续 */
}

uint8_t TargetProg_Probe(uint32_t *dpidr, uint32_t *idcode)
{
    uint8_t r = SWD_Connect(dpidr);
    g_status.swd_err = r;
    g_status.target_dpidr = *dpidr;
    if (r == SWD_OK) {
        if (SWD_Mem_Read32(DBG_IDCODE, idcode) == SWD_OK)
            g_status.target_idcode = *idcode;
        SWD_ReleaseNRST();
    }
    SWD_GPIO_Release();
    return r;
}

uint8_t TargetProg_Run(uint32_t *err_addr)
{
    const img_hdr_t *h = Storage_Header();
    uint32_t addr, len, load;
    uint32_t v, sr;
    uint8_t  r;
    uint32_t crc;
    uint32_t t0;

    if (err_addr) *err_addr = 0;
    if (!Storage_ImageValid()) return TP_ERR_NOIMAGE;

    load = h->load_addr;
    len  = h->length;

    /* ---- 1. 连接并停机 ---- */
    g_status.swd_err = 0;
    r = SWD_Connect(&v);
    g_status.swd_err = r;
    if (r != SWD_OK) { SWD_GPIO_Release(); return TP_ERR_CONNECT; }
    g_status.target_dpidr = v;

    if (SWD_Mem_Read32(DBG_IDCODE, &v) != SWD_OK) return TP_ERR_CONNECT;
    g_status.target_idcode = v;
    if (target_id_check(v) != TP_OK) return TP_ERR_IDCODE;

    /* 冻结看门狗, 停机 */
    if (SWD_Mem_Read32(DBG_CR, &v) == SWD_OK)
        (void)SWD_Mem_Write32(DBG_CR, v | DBG_IWDG_STOP | DBG_WWDG_STOP);
    if (SWD_Mem_Write32(DHCSR, 0xA05F0003u) != SWD_OK) return TP_ERR_HALT;
    SWD_ReleaseNRST();      /* 停机已生效, 释放复位 (固件不会跑起来) */

    /* ---- 2. 解锁 FPEC ---- */
    if (SWD_Mem_Write32(FPEC_KEYR, FPEC_KEY1) != SWD_OK) return TP_ERR_CONNECT;
    if (SWD_Mem_Write32(FPEC_KEYR, FPEC_KEY2) != SWD_OK) return TP_ERR_CONNECT;
    if (SWD_Mem_Read32(FPEC_CR, &v) != SWD_OK)   return TP_ERR_CONNECT;
    if (v & CR_LOCK) return TP_ERR_UNLOCK;

    if (fpec_clear_flags() != TP_OK) return TP_ERR_CONNECT;

    /* ---- 3. 全片擦除 ---- */
    t0 = BSP_Millis();
    if (SWD_Mem_Read32(FPEC_CR, &v) != SWD_OK) return TP_ERR_CONNECT;
    if (SWD_Mem_Write32(FPEC_CR, (v | CR_MER)) != SWD_OK) return TP_ERR_CONNECT;
    if (SWD_Mem_Write32(FPEC_CR, (v | CR_MER | CR_STRT)) != SWD_OK) return TP_ERR_CONNECT;
    r = fpec_wait(ERASE_TIMEOUT_MS, &sr);
    if (r != TP_OK) return TP_ERR_ERASE;
    if (sr & SR_WRPRTERR) { (void)fpec_clear_flags(); return TP_ERR_PROTECTED; }
    if (sr & SR_PGERR)    { (void)fpec_clear_flags(); return TP_ERR_ERASE; }
    if (SWD_Mem_Write32(FPEC_CR, 0) != SWD_OK) return TP_ERR_CONNECT;
    (void)fpec_clear_flags();
    (void)t0;

    /* ---- 4. 半字编程 ---- */
    if (SWD_Mem_Write32(FPEC_CR, CR_PG) != SWD_OK) return TP_ERR_CONNECT;
    for (addr = 0; addr < len; addr += 2u) {
        uint16_t hw = (uint16_t)(Storage_PayloadPtr()[addr] |
                      ((addr + 1u < len) ? ((uint16_t)Storage_PayloadPtr()[addr + 1u] << 8)
                                         : 0xFF00u));
        if (SWD_Mem_Write16(load + addr, hw) != SWD_OK) {
            if (err_addr) *err_addr = load + addr;
            return TP_ERR_CONNECT;
        }
        r = fpec_wait(WRITE_TIMEOUT_MS, &sr);
        if (r != TP_OK || (sr & (SR_PGERR | SR_WRPRTERR))) {
            if (err_addr) *err_addr = load + addr;
            (void)fpec_clear_flags();
            if (SWD_Mem_Write32(FPEC_CR, 0) != SWD_OK) { }
            return (sr & SR_WRPRTERR) ? TP_ERR_PROTECTED : TP_ERR_WRITE;
        }
        if ((addr & 0x3FFu) == 0) {
            g_status.progress = addr;   /* 每 1KB 汇报 */
            Proto_Poll();               /* 保持 USB 协议可响应 */
        }
    }
    if (SWD_Mem_Write32(FPEC_CR, CR_LOCK) != SWD_OK) { }
    g_status.progress = len;

    /* ---- 5. 回读 CRC32 校验 ---- */
    {
        uint32_t words = (len + 3u) / 4u;
        uint32_t off = 0;
        uint32_t buf[32];                        /* 128B 一批 */
        crc = 0xFFFFFFFFu;
        while (off < words) {
            uint32_t n = (words - off > 32u) ? 32u : (words - off);
            uint32_t i, remain = len - off * 4u;
            if (SWD_Mem_ReadBlock(load + off * 4u, buf, n) != SWD_OK)
                return TP_ERR_CONNECT;
            for (i = 0; i < n && remain; i++, remain = (remain >= 4u) ? remain - 4u : 0) {
                uint32_t take = (remain >= 4u) ? 4u : remain;
                crc = CRC32_Update(crc, (const uint8_t *)&buf[i], take);
            }
            off += n;
        }
        crc ^= 0xFFFFFFFFu;
    }
    if (crc != h->crc32) return TP_ERR_VERIFY;

    /* ---- 6. 复位目标, 运行新固件 ---- */
    (void)SWD_Mem_Write32(DHCSR, 0xA05F0000u);   /* 解除停机 */
    (void)SWD_Mem_Write32(AIRCR, 0x05FA0004u);   /* SYSRESETREQ */
    BSP_DelayMs(50);

    return TP_OK;
}
