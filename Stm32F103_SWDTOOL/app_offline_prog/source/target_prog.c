/**
  * @file  target_prog.c
  * @brief 目标烧录实现 (Keil FLM 算法 + SWD)
  */
#include "target_prog.h"
#include "swd_host.h"
#include "flash_algo.h"
#include "storage.h"
#include "crc.h"
#include "bsp.h"
#include "board.h"

#if USB_MODE == USB_MODE_CDC
  #include "proto.h"
  #define USBD_POLL()    Proto_Poll()     /* CDC: 处理上位机命令 */
#else
  #define USBD_POLL()    ((void)0)       /* MSC: USB 中断已处理, 主循环不需轮询 */
#endif

/* ---- Cortex-M 内核寄存器 ---- */
#define DHCSR        0xE000EDF0u
#define AIRCR        0xE000ED0Cu
/* ---- STM32F1/GD32F1 DBGMCU ---- */
#define DBG_IDCODE   0xE0042000u
#define DBG_CR       0xE0042004u
#define DBG_IWDG_STOP  0x00000100u
#define DBG_WWDG_STOP  0x00000200u

/* 按 board.h 的 TARGET_CHIP 选择烧录算法 */
#if TARGET_CHIP == TARGET_CHIP_GD32F1X0
  #define ACTIVE_ALGO   (&FLASH_ALGO_GD32F1X0)
#else
  #define ACTIVE_ALGO   (&FLASH_ALGO_STM32F1)
#endif

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

/* 调一次烧录算法函数并解释结果 (FA_xxx -> TP_xxx) */
static uint8_t algo_call(const flash_algo_t *algo, uint32_t entry,
                         uint32_t a1, uint32_t a2, uint32_t a3,
                         uint32_t timeout_ms, uint8_t fa_map)
{
    uint32_t ret = 0;
    uint8_t r = FlashAlgo_Call(algo, entry, a1, a2, a3, 0, timeout_ms, &ret);
    if (r == FA_ERR_SWD)     return TP_ERR_CONNECT;
    if (r == FA_ERR_TIMEOUT) return fa_map;
    if (r == FA_ERR_RETVAL)  return fa_map;   /* ret!=0: 擦/写失败 (ret=3 常表示写保护) */
    return TP_OK;
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
    uint32_t v;
    uint8_t  r;
    uint32_t crc;

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

    /* ---- 2. 下载烧录算法到目标 RAM 并初始化 ---- */
    {
        const flash_algo_t *algo = ACTIVE_ALGO;

        if (FlashAlgo_Load(algo) != FA_OK) return TP_ERR_CONNECT;

        r = algo_call(algo, algo->init, load, 0, 1,
                      algo->prog_timeout_ms + 500u, TP_ERR_ALGO);
        if (r != TP_OK) return r;

        /* ---- 3. 擦除 ---- */
        if (algo->erase_chip != 0xFFFFFFFFu) {
            r = algo_call(algo, algo->erase_chip, 0, 0, 0,
                          algo->erase_timeout_ms + 2000u, TP_ERR_ERASE);
        } else {
            /* 无全片擦除: 按扇区逐个擦 */
            uint32_t sec;
            r = TP_OK;
            for (sec = load & ~(algo->sector_size - 1u);
                 sec < load + len && r == TP_OK;
                 sec += algo->sector_size) {
                r = algo_call(algo, algo->erase_sector, sec, 0, 0,
                              algo->erase_timeout_ms + 500u, TP_ERR_ERASE);
            }
        }
        if (r != TP_OK) return r;

        /* ---- 4. 按页编程 ---- */
        {
            const uint8_t *payload = Storage_PayloadPtr();
            uint32_t buf_words = algo->program_buffer_size / 4u;

            for (addr = 0; addr < len; addr += algo->program_buffer_size) {
                uint32_t chunk = len - addr;
                if (chunk > algo->program_buffer_size) chunk = algo->program_buffer_size;

                /* 数据 -> 目标 RAM 页缓冲区 (不足 4 字节尾部补 0xFF) */
                {
                    uint32_t wbuf[8];           /* 32B 一批写, 减少栈占用 */
                    uint32_t sent = 0;
                    while (sent < chunk) {
                        uint32_t i, n = (chunk - sent > sizeof(wbuf))
                                        ? sizeof(wbuf) : (chunk - sent);
                        uint32_t nw = (n + 3u) / 4u;
                        for (i = 0; i < nw; i++) {
                            uint32_t off = sent + i * 4u;
                            uint32_t w = 0xFFFFFFFFu;
                            uint32_t b;
                            for (b = 0; b < 4u && off + b < chunk; b++)
                                ((uint8_t *)&w)[b] = payload[addr + off + b];
                            wbuf[i] = w;
                        }
                        if (SWD_Mem_WriteBlock(algo->program_buffer + sent,
                                               wbuf, nw) != SWD_OK) {
                            if (err_addr) *err_addr = load + addr;
                            return TP_ERR_CONNECT;
                        }
                        sent += n;
                    }
                    (void)buf_words;
                }

                r = algo_call(algo, algo->program_page, load + addr, chunk,
                              algo->program_buffer,
                              algo->prog_timeout_ms + 500u, TP_ERR_WRITE);
                if (r != TP_OK) {
                    if (err_addr) *err_addr = load + addr;
                    return r;
                }

                g_status.progress = addr + chunk;   /* 每页汇报 */
                USBD_POLL();                        /* 保持 USB 响应 */
            }
        }

        /* UnInit (尽力而为) */
        (void)algo_call(algo, algo->uninit, 3, 0, 0,
                        algo->prog_timeout_ms + 500u, TP_OK);
    }
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
