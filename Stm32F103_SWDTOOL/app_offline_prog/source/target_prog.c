#if !defined(BUILD_BOOT)
/**
  * @file  target_prog.c
  * @brief 目标烧录实现 (Keil FLM 算法 + SWD)
  */
#include <string.h>
#include "target_prog.h"
#include "swd_host.h"
#include "flash_algo.h"
#include "storage.h"
#include "crc.h"
#include "bsp.h"
#include "board.h"
#include "log.h"

#if USB_HAS_CDC
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
/* ---- GD32 FMC 产品 ID ---- */
#define FMC_PID        0x40022100u

/* 按 board.h 的 TARGET_CHIP 选择烧录算法 */
#if TARGET_CHIP == TARGET_CHIP_GD32F1X0
  #define ACTIVE_ALGO   (&FLASH_ALGO_GD32F1X0)
#elif TARGET_CHIP == TARGET_CHIP_GD32F103
  #define ACTIVE_ALGO   (&FLASH_ALGO_GD32F103)
#else
  #define ACTIVE_ALGO   (&FLASH_ALGO_STM32F1)
#endif

/* ---- 目标身份校验 (按 board.h 的 TARGET_CHIP 配置) ---- */
static uint8_t target_id_check(uint32_t idcode)
{
    uint32_t dev = idcode & 0xFFFu;
    uint32_t rev = (idcode >> 16) & 0xFFFFu;

#if TARGET_CHIP == TARGET_CHIP_GD32F103
    /* GD32F103: DEV_ID 0x410 (与 STM32F103 中容量相同)。
       再用 FMC_PID 确认一次, 读到 0x41424333 可判定为 GD32;
       读不到也放行 (有些 GD32 掩码不同), 避免误拒。               */
    uint32_t pid = 0;
    (void)rev;
    if (dev != 0x410u) return TP_ERR_IDCODE;
    (void)SWD_Mem_Read32(FMC_PID, &pid);   /* 仅用于诊断, 不作为拒绝条件 */
    return TP_OK;
#elif TARGET_CHIP == TARGET_CHIP_GD32F1X0
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
    uint8_t r;
    uint32_t v = 0;

#if DBG_TOOLS
    /* 以下全部是排查用的诊断 (引脚电平/时钟网络/原始波形/引脚组合扫描) */
    {
    uint8_t idle, io_rel, io_hold, ck_rel, ck_hold, a2b, b2a;
    uint8_t i, c;

    SWD_PinDiag(&idle, &io_rel, &io_hold, &ck_rel, &ck_hold);
    Log_Str("SWD io id="); Log_Dec(idle);
    Log_Str(" r1=");  Log_Dec(io_rel);
    Log_Str(" r100="); Log_Dec(io_hold);
    Log_Str(" ck r1="); Log_Dec(ck_rel);
    Log_Str(" r100="); Log_Dec(ck_hold);
    Log_Str("\n");
    SWD_ClockDiag(&a2b, &b2a);
    Log_Str("SWD cknet PA3->PB13="); Log_Dec(a2b);
    Log_Str(" PB13->PA3="); Log_Dec(b2a);
    Log_Str("\n");

    /* 原始监听: 看目标是否真的把 SWDIO 拉低过 (全 F = 完全没驱动) */
    Log_Str("SWD raw pre="); Log_Hex(SWD_ListenRaw(0));
    Log_Str(" post="); Log_Hex(SWD_ListenRaw(1));
    Log_Str("\n");
    /* SWDIO 自回环: 发送脚(PA4)与采样脚(PA7)是否真并在一起 (期望 echo=1) */
    Log_Str("SWD echo="); Log_Dec(SWD_SelfEcho());
    Log_Str("\n");

    /* 组合表: 时钟脚 / SWDIO 发送 / SWDIO 采样
     *   c0 文档接法 (PA3 时钟, PA4 发, PA7 采)
     *   c1 备用时钟 PB13
     *   c2 SWDIO 收发对调
     *   c3 SWCLK 与 SWDIO 对调 (排查四针口标反)
     *   c4 SWCLK 与 SWDIO 全对调
     *   c5 PB13 时钟 + SWDIO 对调                                    */
    {
        typedef struct {
            GPIO_TypeDef *clk; uint16_t clk_pin;
            GPIO_TypeDef *dop; uint16_t do_pin;
            GPIO_TypeDef *dip; uint16_t di_pin;
        } pin_cfg_t;
        static const pin_cfg_t cfg[6] = {
            { SWCLK_GPIO,       SWCLK_PIN,       SWDIO_OUT_GPIO, SWDIO_OUT_PIN, SWDIO_IN_GPIO,  SWDIO_IN_PIN  },
            { SWD_CLK_ALT_GPIO, SWD_CLK_ALT_PIN, SWDIO_OUT_GPIO, SWDIO_OUT_PIN, SWDIO_IN_GPIO,  SWDIO_IN_PIN  },
            { SWCLK_GPIO,       SWCLK_PIN,       SWDIO_IN_GPIO,  SWDIO_IN_PIN,  SWDIO_OUT_GPIO, SWDIO_OUT_PIN },
            { SWDIO_OUT_GPIO,   SWDIO_OUT_PIN,   SWCLK_GPIO,     SWCLK_PIN,     SWDIO_IN_GPIO,  SWDIO_IN_PIN  },
            { SWDIO_OUT_GPIO,   SWDIO_OUT_PIN,   SWDIO_IN_GPIO,  SWDIO_IN_PIN,  SWCLK_GPIO,     SWCLK_PIN     },
            { SWD_CLK_ALT_GPIO, SWD_CLK_ALT_PIN, SWDIO_IN_GPIO,  SWDIO_IN_PIN,  SWDIO_OUT_GPIO, SWDIO_OUT_PIN },
        };
        static const uint32_t sp[2] = { 200u, 1000u };

        for (c = 0; c < 6u; c++) {
            SWD_SetDioPins(cfg[c].dop, cfg[c].do_pin, cfg[c].dip, cfg[c].di_pin);
            SWD_SetClockPin(cfg[c].clk, cfg[c].clk_pin);
            for (i = 0; i < 2u; i++) {
                SWD_SetSpeed(sp[i]);
                v = 0;
                r = SWD_Connect(&v);
                Log_Str("SWD c"); Log_Dec(c);
                Log_Str(" s"); Log_Dec(i);
                Log_Str(" r="); Log_Dec(r);
                Log_Str(" a="); Log_Dec(g_swd_dbg_ack);
                Log_Str(" d="); Log_Hex(v);
                Log_Str("\n");
                if (r == SWD_OK) break;
            }
            if (r == SWD_OK) break;
        }
    }
    SWD_SetSpeed(SWD_DELAY_LOOP);
    SWD_SetClockPin(SWCLK_GPIO, SWCLK_PIN);
    SWD_SetDioPins(SWDIO_OUT_GPIO, SWDIO_OUT_PIN, SWDIO_IN_GPIO, SWDIO_IN_PIN);
    }
#else
    r = SWD_Connect(&v);
#endif
    *dpidr = v;
    g_status.swd_err = r;
    g_status.target_dpidr = v;
    if (r == SWD_OK) {
        if (SWD_Mem_Read32(DBG_IDCODE, idcode) == SWD_OK)
            g_status.target_idcode = *idcode;
#if DBG_TOOLS
        {
            uint32_t dh = 0;
            if (SWD_Mem_Read32(0xE000EDF0u, &dh) == SWD_OK) {
                Log_Str("DHCSR=");
                Log_Hex(dh);
                Log_Str("\n");
            }
        }
        {
            /* 目标是否在跑: 间隔 300ms 采样 GPIOA/B/C 的 ODR, 有变化即说明代码在动 */
            uint32_t o1[3], o2[3];
            uint8_t  i;
            for (i = 0; i < 3u; i++)
                (void)SWD_Mem_Read32(0x4001080Cu + (uint32_t)i * 0x400u, &o1[i]);
            BSP_DelayMs(300);
            for (i = 0; i < 3u; i++)
                (void)SWD_Mem_Read32(0x4001080Cu + (uint32_t)i * 0x400u, &o2[i]);
            Log_Str("ODR1="); Log_Hex(o1[2]); Log_Hex(o1[1]); Log_Hex(o1[0]);
            Log_Str(" ODR2="); Log_Hex(o2[2]); Log_Hex(o2[1]); Log_Hex(o2[0]);
            Log_Str((o1[0] != o2[0] || o1[1] != o2[1] || o1[2] != o2[2]) ?
                    " RUN\n" : " STATIC\n");
        }
#endif
        SWD_ReleaseNRST();
    }
    SWD_GPIO_Release();
    return r;
}

static uint8_t tp_run_impl(uint32_t *err_addr)
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

    /* 防止把 Intel HEX 文本当成固件烧进去 (例如直接拖 .hex 进 U 盘) */
    {
        const uint8_t *p = Storage_PayloadPtr();
        if (len >= 8u && p[0] == (uint8_t)':') {
            uint32_t i;
            uint8_t is_txt = 1;
            for (i = 1; i < len && i < 256u; i++) {
                uint8_t c = p[i];
                if (c == '\r' || c == '\n') break;
                if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
                      (c >= 'a' && c <= 'f'))) { is_txt = 0; break; }
            }
            if (is_txt) {
                Log_Str("IMG IS HEX TEXT\n");
                return TP_ERR_HEX;
            }
        }
    }

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

uint8_t TargetProg_Run(uint32_t *err_addr)
{
    uint8_t r = tp_run_impl(err_addr);

    /* 失败时别把目标丢在停机态, 否则看起来像"芯片没运行": 复位让它重新跑(旧)固件 */
    if (r != TP_OK && r != TP_ERR_CONNECT && r != TP_ERR_NOIMAGE && r != TP_ERR_HEX) {
        (void)SWD_Mem_Write32(DHCSR, 0xA05F0000u);
        (void)SWD_Mem_Write32(AIRCR, 0x05FA0004u);
        BSP_DelayMs(20);
    }
    return r;
}

/* ================================================================
 *                        流式烧录
 * ================================================================ */

/* 目标 Flash 容量寄存器 (单位 KB) */
#define FLASH_SIZE_REG   0x1FFFF7E0u

uint32_t g_target_flash_kb;

static struct {
    uint8_t  active;
    uint32_t load;
    uint32_t len;
    uint32_t crc;           /* 期望的载荷 CRC32 */
    uint32_t received;
    uint32_t page_size;     /* 每页编程字节数 (= program_buffer_size) */
    uint32_t page_off;
    uint32_t addr;          /* 当前页起始地址 */
    uint8_t  page_buf[1024];
} s_strm;

/* 把 page_buf 里的一整页写进目标 Flash */
static uint8_t strm_program_page(void)
{
    const flash_algo_t *algo = ACTIVE_ALGO;
    uint32_t sent = 0;
    uint8_t  r;

    while (sent < s_strm.page_size) {
        uint32_t wbuf[8];
        uint32_t n  = (s_strm.page_size - sent > sizeof(wbuf))
                      ? (uint32_t)sizeof(wbuf) : (s_strm.page_size - sent);
        uint32_t nw = (n + 3u) / 4u;
        uint32_t i;

        for (i = 0; i < nw; i++) {
            uint32_t off = sent + i * 4u;
            uint32_t w = 0xFFFFFFFFu;
            uint32_t b;
            for (b = 0; b < 4u && off + b < s_strm.page_size; b++)
                ((uint8_t *)&w)[b] = s_strm.page_buf[off + b];
            wbuf[i] = w;
        }
        if (SWD_Mem_WriteBlock(algo->program_buffer + sent, wbuf, nw) != SWD_OK)
            return TP_ERR_CONNECT;
        sent += n;
    }

    r = algo_call(algo, algo->program_page, s_strm.addr, s_strm.page_size,
                  algo->program_buffer, algo->prog_timeout_ms + 500u, TP_ERR_WRITE);
    if (r != TP_OK) return r;

    s_strm.addr    += s_strm.page_size;
    s_strm.page_off = 0;
    return TP_OK;
}

/* 结束时无论成败都让目标跑起来 (成功=跑新固件, 失败=跑原有内容) */
static void strm_reset_run(void)
{
    (void)SWD_Mem_Write32(DHCSR, 0xA05F0000u);   /* 解除停机 */
    (void)SWD_Mem_Write32(AIRCR, 0x05FA0004u); /* SYSRESETREQ */
    BSP_DelayMs(50);
    SWD_GPIO_Release();
}

uint8_t TargetStream_Begin(uint32_t total_len, uint32_t crc32,
                           uint32_t load_addr, uint32_t *err_addr)
{
    const flash_algo_t *algo = ACTIVE_ALGO;
    uint32_t v;
    uint8_t  r;

    if (err_addr) *err_addr = 0;
    s_strm.active = 0;
    g_target_flash_kb = 0;

    if (total_len == 0 || (load_addr & 0xFF000000u) != 0x08000000u ||
        (load_addr & 3u) != 0u)
        return TP_ERR_PARAM;

    /* ---- 连接 + 停机 ---- */
    g_status.swd_err = 0;
    r = SWD_Connect(&v);
    g_status.swd_err = r;
    if (r != SWD_OK) { SWD_GPIO_Release(); return TP_ERR_CONNECT; }
    g_status.target_dpidr = v;

    if (SWD_Mem_Read32(DBG_IDCODE, &v) != SWD_OK) return TP_ERR_CONNECT;
    g_status.target_idcode = v;
    if (target_id_check(v) != TP_OK) { SWD_GPIO_Release(); return TP_ERR_IDCODE; }

    if (SWD_Mem_Read32(DBG_CR, &v) == SWD_OK)
        (void)SWD_Mem_Write32(DBG_CR, v | DBG_IWDG_STOP | DBG_WWDG_STOP);
    if (SWD_Mem_Write32(DHCSR, 0xA05F0003u) != SWD_OK)
        return TP_ERR_HALT;
    SWD_ReleaseNRST();

    /* ---- 容量检查: 读目标 Flash 大小 (KB) ---- */
    if (SWD_Mem_Read32(FLASH_SIZE_REG, &v) == SWD_OK) {
        g_target_flash_kb = v & 0xFFFFu;
        if (g_target_flash_kb > 4096u) g_target_flash_kb = 0;   /* 明显无效 */
    }
    if (g_target_flash_kb != 0 &&
        (load_addr - 0x08000000u) + total_len > g_target_flash_kb * 1024u) {
        strm_reset_run();           /* 目标保持原状继续运行 */
        return TP_ERR_SIZE;
    }

    /* ---- 下载算法 + 初始化 + 擦除 ---- */
    if (FlashAlgo_Load(algo) != FA_OK) { strm_reset_run(); return TP_ERR_CONNECT; }
    if (algo->program_buffer_size > sizeof(s_strm.page_buf)) {
        strm_reset_run();
        return TP_ERR_ALGO;
    }

    r = algo_call(algo, algo->init, load_addr, 0, 1,
                  algo->prog_timeout_ms + 500u, TP_ERR_ALGO);
    if (r != TP_OK) { strm_reset_run(); return r; }

    if (algo->erase_chip != 0xFFFFFFFFu) {
        r = algo_call(algo, algo->erase_chip, 0, 0, 0,
                      algo->erase_timeout_ms + 2000u, TP_ERR_ERASE);
    } else {
        uint32_t sec;
        r = TP_OK;
        for (sec = load_addr & ~(algo->sector_size - 1u);
             sec < load_addr + total_len && r == TP_OK;
             sec += algo->sector_size) {
            r = algo_call(algo, algo->erase_sector, sec, 0, 0,
                          algo->erase_timeout_ms + 500u, TP_ERR_ERASE);
        }
    }
    if (r != TP_OK) { strm_reset_run(); return r; }

    /* ---- 就绪 ---- */
    s_strm.load      = load_addr;
    s_strm.len       = total_len;
    s_strm.crc       = crc32;
    s_strm.received  = 0;
    s_strm.page_size = algo->program_buffer_size;
    s_strm.page_off  = 0;
    s_strm.addr      = load_addr;
    s_strm.active    = 1;
    g_status.progress = 0;
    return TP_OK;
}

uint8_t TargetStream_Data(uint32_t offset, const uint8_t *data,
                          uint16_t len, uint32_t *err_addr)
{
    if (err_addr) *err_addr = 0;
    if (!s_strm.active) return TP_ERR_PARAM;
    if (offset != s_strm.received || offset + len > s_strm.len) return TP_ERR_PARAM;

    while (len != 0) {
        uint32_t space = s_strm.page_size - s_strm.page_off;
        uint32_t n = (len < space) ? (uint32_t)len : space;

        memcpy(&s_strm.page_buf[s_strm.page_off], data, n);
        s_strm.page_off += n;
        data += n;
        len  = (uint16_t)(len - n);
        s_strm.received += n;

        if (s_strm.page_off == s_strm.page_size) {
            uint8_t r = strm_program_page();
            if (r != TP_OK) {
                if (err_addr) *err_addr = s_strm.addr;
                return r;
            }
        }
    }

    g_status.progress = s_strm.received;
    return TP_OK;
}

uint8_t TargetStream_End(uint32_t *err_addr)
{
    const flash_algo_t *algo = ACTIVE_ALGO;
    uint32_t crc;
    uint8_t  r;

    if (err_addr) *err_addr = 0;
    if (!s_strm.active) return TP_ERR_PARAM;

    if (s_strm.received != s_strm.len) {
        s_strm.active = 0;
        strm_reset_run();
        return TP_ERR_PARAM;
    }

    /* 尾部不足一页: 0xFF 补齐后写入 (Flash 擦除态) */
    if (s_strm.page_off != 0) {
        memset(&s_strm.page_buf[s_strm.page_off], 0xFF,
               s_strm.page_size - s_strm.page_off);
        s_strm.page_off = s_strm.page_size;
        r = strm_program_page();
        if (r != TP_OK) {
            if (err_addr) *err_addr = s_strm.addr;
            s_strm.active = 0;
            strm_reset_run();
            return r;
        }
    }

    (void)algo_call(algo, algo->uninit, 3, 0, 0,
                    algo->prog_timeout_ms + 500u, TP_OK);

    /* ---- 回读 CRC32 校验 ---- */
    {
        uint32_t words = (s_strm.len + 3u) / 4u;
        uint32_t off = 0;
        uint32_t buf[32];
        crc = 0xFFFFFFFFu;
        while (off < words) {
            uint32_t n = (words - off > 32u) ? 32u : (words - off);
            uint32_t i, remain = s_strm.len - off * 4u;
            if (SWD_Mem_ReadBlock(s_strm.load + off * 4u, buf, n) != SWD_OK) {
                s_strm.active = 0;
                SWD_GPIO_Release();
                return TP_ERR_CONNECT;
            }
            for (i = 0; i < n && remain; i++,
                 remain = (remain >= 4u) ? remain - 4u : 0) {
                uint32_t take = (remain >= 4u) ? 4u : remain;
                crc = CRC32_Update(crc, (const uint8_t *)&buf[i], take);
            }
            off += n;
        }
        crc ^= 0xFFFFFFFFu;
    }

    s_strm.active = 0;
    g_status.progress = s_strm.len;

    if (crc != s_strm.crc) {
        strm_reset_run();
        return TP_ERR_VERIFY;
    }

    strm_reset_run();
    return TP_OK;
}

/* ---------------- 整片擦除 ---------------- */
uint8_t TargetEraseChip(void)
{
    const flash_algo_t *algo = ACTIVE_ALGO;
    uint32_t v;
    uint8_t  r;

    g_target_flash_kb = 0;
    g_status.swd_err = 0;

    r = SWD_Connect(&v);
    g_status.swd_err = r;
    if (r != SWD_OK) { SWD_GPIO_Release(); return TP_ERR_CONNECT; }
    g_status.target_dpidr = v;

    if (SWD_Mem_Read32(DBG_IDCODE, &v) != SWD_OK) {
        SWD_GPIO_Release();
        return TP_ERR_CONNECT;
    }
    g_status.target_idcode = v;
    if (target_id_check(v) != TP_OK) { SWD_GPIO_Release(); return TP_ERR_IDCODE; }

    if (SWD_Mem_Read32(DBG_CR, &v) == SWD_OK)
        (void)SWD_Mem_Write32(DBG_CR, v | DBG_IWDG_STOP | DBG_WWDG_STOP);
    if (SWD_Mem_Write32(DHCSR, 0xA05F0003u) != SWD_OK) {
        SWD_GPIO_Release();
        return TP_ERR_HALT;
    }
    SWD_ReleaseNRST();

    if (SWD_Mem_Read32(FLASH_SIZE_REG, &v) == SWD_OK) {
        g_target_flash_kb = v & 0xFFFFu;
        if (g_target_flash_kb > 4096u) g_target_flash_kb = 0;
    }

    if (FlashAlgo_Load(algo) != FA_OK) { strm_reset_run(); return TP_ERR_CONNECT; }
    r = algo_call(algo, algo->init, 0x08000000u, 0, 1,
                  algo->prog_timeout_ms + 500u, TP_ERR_ALGO);
    if (r != TP_OK) { strm_reset_run(); return r; }

    if (algo->erase_chip != 0xFFFFFFFFu) {
        r = algo_call(algo, algo->erase_chip, 0, 0, 0,
                      algo->erase_timeout_ms + 2000u, TP_ERR_ERASE);
    } else if (g_target_flash_kb != 0) {
        uint32_t sec, end = 0x08000000u + g_target_flash_kb * 1024u;
        r = TP_OK;
        for (sec = 0x08000000u; sec < end && r == TP_OK; sec += algo->sector_size)
            r = algo_call(algo, algo->erase_sector, sec, 0, 0,
                          algo->erase_timeout_ms + 500u, TP_ERR_ERASE);
    } else {
        r = TP_ERR_ERASE;
    }

    (void)algo_call(algo, algo->uninit, 3, 0, 0,
                    algo->prog_timeout_ms + 500u, TP_OK);

    /* 空白校验: 首字与末字都应是 0xFFFFFFFF */
    if (r == TP_OK) {
        uint32_t w0, w1;
        if (SWD_Mem_Read32(0x08000000u, &w0) != SWD_OK) {
            r = TP_ERR_CONNECT;
        } else if (w0 != 0xFFFFFFFFu) {
            r = TP_ERR_ERASE;
        } else if (g_target_flash_kb != 0) {
            if (SWD_Mem_Read32(0x08000000u + g_target_flash_kb * 1024u - 4u, &w1) != SWD_OK)
                r = TP_ERR_CONNECT;
            else if (w1 != 0xFFFFFFFFu)
                r = TP_ERR_ERASE;
        }
    }

    strm_reset_run();
    return r;
}
#endif
