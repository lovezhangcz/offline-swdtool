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
#define DBG_IDCODE_F0 0x40015800u   /* STM32F0 (Cortex-M0) DBGMCU_IDCODE */
#define DBG_APB1_FZ_F0 0x40015808u  /* STM32F0 看门狗冻结位 (APB1_FZ) */
#define DPIDR_M0(dp)   (((dp) & 0x0FFFFFFFu) == 0x0BC11477u)  /* Cortex-M0 DP */
#define DBG_IWDG_STOP  0x00000100u
#define DBG_WWDG_STOP  0x00000200u
/* ---- STM32F4/APM32F4 DBGMCU (冻结位在 APB1_FZ) ---- */
#define DBG_APB1_FZ      0xE0042008u
#define FZ_IWDG_STOP     (1u << 8)
#define FZ_WWDG_STOP     (1u << 9)
/* ---- GD32 FMC 产品 ID ---- */
#define FMC_PID        0x40022100u

/* ---- STM32F1/GD32/APM32 FPEC (读保护 RDP 操作, 参考 blackmagic stm32f1.c) ---- */
#define FPEC_BASE      0x40022000u
#define FPEC_KEYR      (FPEC_BASE + 0x04u)
#define FPEC_OPTKEYR   (FPEC_BASE + 0x08u)
#define FPEC_SR        (FPEC_BASE + 0x0Cu)
#define FPEC_CR        (FPEC_BASE + 0x10u)
#define FPEC_OBR       (FPEC_BASE + 0x1Cu)
#define FPEC_KEY1      0x45670123u
#define FPEC_KEY2      0xCDEF89ABu
#define FPEC_CR_OPTPG  (1u << 4)
#define FPEC_CR_OPTER  (1u << 5)
#define FPEC_CR_STRT   (1u << 6)
#define FPEC_CR_OPTWRE (1u << 9)
#define FPEC_SR_BSY    (1u << 0)
#define FPEC_OBR_RDPRT (1u << 1)
#define OBP_RDP        0x1FFFF800u
#define RDP_UNLOCK_KEY 0x5AA5u    /* 无保护 (值0xA5 + 补码0x5A) */
#define RDP_LOCK_VAL   0x0000u    /* RDP Level 1 (任何 != 0xA5 的低字节值) */

/* 镜像 trailer 携带的保护配置位 (与上位机 aesbin.FLAG_* 对应) */
#define IMG_FLAG_UNLOCK 0x01u   /* 烧录前自动解除读保护 (整片擦除) */
#define IMG_FLAG_LOCK   0x02u   /* 烧录并校验通过后上锁 RDP Level 1 */

/* 按 board.h 的 TARGET_CHIP 选择烧录算法 (AUTO 时由 target_id_check 运行期选定) */
#if TARGET_CHIP == TARGET_CHIP_AUTO
static const flash_algo_t *s_active_algo = &FLASH_ALGO_STM32F1;   /* 未识别前回退 */
  #define ACTIVE_ALGO   (s_active_algo)
#elif TARGET_CHIP == TARGET_CHIP_GD32F1X0
  #define ACTIVE_ALGO   (&FLASH_ALGO_GD32F1X0)
#elif TARGET_CHIP == TARGET_CHIP_GD32F303
  #define ACTIVE_ALGO   (&FLASH_ALGO_GD32F303)
#elif TARGET_CHIP == TARGET_CHIP_GD32F103
  #define ACTIVE_ALGO   (&FLASH_ALGO_GD32F103)
#else
  #define ACTIVE_ALGO   (&FLASH_ALGO_STM32F1)
#endif

/* ---- 目标身份校验 / 芯片识别 ---- */
static uint8_t target_id_check(uint32_t idcode)
{
    uint32_t dev = idcode & 0xFFFu;
    uint32_t rev = (idcode >> 16) & 0xFFFFu;

#if TARGET_CHIP == TARGET_CHIP_AUTO
    /* DPIDR 版本字段在 bit31:28: 0x1=DPv1 (M3), 0x2=DPv2 (M4) */
    uint32_t pid = 0;
    uint8_t  is_m4 = ((g_status.target_dpidr >> 28) == 0x2u);
    const char *name;

    if (dev == 0x414u && is_m4) {                 /* GD32F303 */
        s_active_algo = &FLASH_ALGO_GD32F303;
        name = "GD32F303";
    } else if (dev == 0x012u && is_m4) {          /* 极海 APM32 M4 芯片
                                                     (实测 IDCODE=0x001DA012) */
        /* 实测该芯片是 F1 风格外设映射, 不是 STM32F4 风格:
             RCC    @0x40021000 (F4 应 @0x40023800, 实测读 0)
             FMC    @0x40022000 (F4 应 @0x40023C00, 实测读 0)
             GPIOC  @0x40011000 (CRL 复位值 0x44444444)
             DBGMCU_CR @0xE0042004 有效 (F1 风格冻结位)
             0x1FFF0000 (F4 系统存储区) 访问总线错误
           => 用 F1 算法 (FMC@0x40022000, 1KB 页迭代对 1KB/2KB 物理页都安全) */
        s_active_algo = &FLASH_ALGO_STM32F1;
        name = "APM32-M4(F1FMC)";
    } else if ((dev == 0x413u || dev == 0x419u) && is_m4) {
        /* 真 STM32F405/407/415/417 (0x413) 与 F42x/43x (0x419):
           FMC 与 APM32F40X 寄存器兼容, 共用同一算法 */
        s_active_algo = &FLASH_ALGO_APM32F40X;
        name = "STM32F4";
    } else if (dev == 0x414u) {                   /* STM32F103 大容量 / GD32F103 HD */
        s_active_algo = &FLASH_ALGO_STM32F1;
        name = "STM32F1-HD";
    } else if (dev == 0x410u) {
        /* GD32F103 与 GD32F1x0 的 DEV_ID/REV_ID 完全相同 (0x410/0x1303),
           只能靠 FMC_PID 区分 GD32 与 ST;
           GD32 的 PID 高字节固定为 'A' (0x41):
             GD32F103 : 0x41424333   GD32F130 : 0x41384330
           两者 FMC 兼容、1KB 页, 共用 GD32F1x0 算法 (均已实测) */
        (void)SWD_Mem_Read32(FMC_PID, &pid);
        if ((pid >> 24) == 0x41u) {
            s_active_algo = &FLASH_ALGO_GD32F1X0;
            name = "GD32F10x/F1x0";
        } else {                                  /* STM32F103 中容量 */
            s_active_algo = &FLASH_ALGO_STM32F1;
            name = "STM32F1";
        }
    } else if (dev == 0x412u || dev == 0x418u || dev == 0x420u ||
               dev == 0x428u || dev == 0x430u) {  /* STM32F1 其它容量 */
        s_active_algo = &FLASH_ALGO_STM32F1;
        name = "STM32F1";
    } else if (dev == 0x440u || dev == 0x442u) {  /* STM32F030 (Cortex-M0)
                                                     DEV_ID 0x440, F030xC=0x442 */
        s_active_algo = &FLASH_ALGO_STM32F0;
        name = "STM32F030";
    } else {
        return TP_ERR_IDCODE;                     /* 未支持的芯片 */
    }
    Log_Str("CHIP "); Log_Str(name);
    Log_Str(" PID="); Log_Hex(pid); Log_Str("\n");
    (void)rev;
    return TP_OK;
#elif TARGET_CHIP == TARGET_CHIP_GD32F103
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
#elif TARGET_CHIP == TARGET_CHIP_GD32F303
    /* GD32F303: DEV_ID 0x414 (与 STM32F103 大容量同值), REV_ID 0x2104;
       用 FMC_PID 诊断确认, 但不作为拒绝条件 (掩码可能不同)          */
    uint32_t pid = 0;
    (void)rev;
    if (dev != 0x414u) return TP_ERR_IDCODE;
    (void)SWD_Mem_Read32(FMC_PID, &pid);
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

/* 停机并确认 S_HALT 真的置位: DHCSR 写入偶发不生效 (SWD 写通路临界),
 * 不确认就往下走会在目标仍运行时改 PC/寄存器, 导致"算法返回怪值" */
static uint8_t halt_target(void)
{
    uint32_t dh = 0;
    uint8_t i;

    for (i = 0; i < 8u; i++) {
        (void)SWD_Mem_Write32(DHCSR, 0xA05F0003u);
        if (SWD_Mem_Read32(DHCSR, &dh) != SWD_OK) break;
        if (dh & (1u << 17)) {                  /* S_HALT */
            if (i) { Log_Str("HALT ok try="); Log_Dec(i); Log_Str("\n"); }
            return TP_OK;
        }
        BSP_DelayMs(5);
    }
    Log_Str("HALT fail dh="); Log_Hex(dh); Log_Str("\n");
    return TP_ERR_HALT;
}

/* 冻结目标看门狗: F1/GD32 写 DBGMCU_CR; F4/APM32F4 写 DBGMCU_APB1_FZ */
static void freeze_wdog(const flash_algo_t *algo)
{
    uint32_t v;
    if (algo == &FLASH_ALGO_APM32F40X) {
        if (SWD_Mem_Read32(DBG_APB1_FZ, &v) == SWD_OK)
            (void)SWD_Mem_Write32(DBG_APB1_FZ, v | FZ_IWDG_STOP | FZ_WWDG_STOP);
    } else if (algo == &FLASH_ALGO_STM32F0) {   /* F0: APB1_FZ @0x40015808, 位同 F1 */
        if (SWD_Mem_Read32(DBG_APB1_FZ_F0, &v) == SWD_OK)
            (void)SWD_Mem_Write32(DBG_APB1_FZ_F0, v | DBG_IWDG_STOP | DBG_WWDG_STOP);
    } else {
        if (SWD_Mem_Read32(DBG_CR, &v) == SWD_OK)
            (void)SWD_Mem_Write32(DBG_CR, v | DBG_IWDG_STOP | DBG_WWDG_STOP);
    }
}

/* 连接 + 读 IDCODE + 身份校验
   目标刚烧完/刚复位时, DP 能连但 AP 读会偶发失败, 整体重试 4 次 (间隔 100ms) */
static uint8_t target_attach(void)
{
    uint32_t v;
    uint8_t  r, i;

    g_status.swd_err = 0;
    for (i = 0; i < 4u; i++) {
        r = SWD_Connect(&v);
        g_status.swd_err = r;
        if (r == SWD_OK) {
            g_status.target_dpidr = v;
            /* Cortex-M0 (STM32F0, DPIDR 0x0BC11477) 的 DBGMCU 在 0x40015800,
               与 F1/F4 的 0xE0042000 不同, 按 DP 区分 */
            {
                uint32_t dbg = ((v & 0x0FFFFFFFu) == 0x0BC11477u) ? DBG_IDCODE_F0 : DBG_IDCODE;
                if (SWD_Mem_Read32(dbg, &v) == SWD_OK) {
                    g_status.target_idcode = v;
                    return target_id_check(v);
                }
            }
        }
        SWD_GPIO_Release();
        if (i < 3u) BSP_DelayMs(100);
    }
    return TP_ERR_CONNECT;
}

/* ================= 读保护 (RDP) 解除 / 上锁 =================
   RDP Level1 时无法 halt / 读写 flash, 但 DP 可连、AHB 外设(FPEC)可访问,
   因此解除保护在 target_attach 之前通过 FPEC 完成 (参考 blackmagic stm32f1.c)。 */

/* 等待 FPEC 空闲 */
static uint8_t fpec_wait(uint32_t ms)
{
    uint32_t t0 = BSP_Millis(), sr = 0;
    for (;;) {
        if (SWD_Mem_Read32(FPEC_SR, &sr) != SWD_OK) return TP_ERR_CONNECT;
        if (!(sr & FPEC_SR_BSY)) return TP_OK;
        if ((BSP_Millis() - t0) > ms) return TP_ERR_ALGO;
    }
}

/* 解锁主 flash 控制 + option byte 写使能 (OPTWRE) */
static void fpec_unlock(void)
{
    (void)SWD_Mem_Write32(FPEC_KEYR, FPEC_KEY1);
    (void)SWD_Mem_Write32(FPEC_KEYR, FPEC_KEY2);
    (void)SWD_Mem_Write32(FPEC_OPTKEYR, FPEC_KEY1);
    (void)SWD_Mem_Write32(FPEC_OPTKEYR, FPEC_KEY2);
}

/* 当前是否读保护 (OBR.RDPRT) */
static uint8_t rdp_is_protected(void)
{
    uint32_t obr = 0;
    if (SWD_Mem_Read32(FPEC_OBR, &obr) != SWD_OK) return 0;
    return (obr & FPEC_OBR_RDPRT) ? 1u : 0u;
}

/* 擦除 option byte 页 (F1 只能整页擦, 之后各 option 回擦除态) */
static uint8_t rdp_option_erase(void)
{
    (void)SWD_Mem_Write32(FPEC_CR, FPEC_CR_OPTER | FPEC_CR_OPTWRE);
    (void)SWD_Mem_Write32(FPEC_CR, FPEC_CR_STRT | FPEC_CR_OPTER | FPEC_CR_OPTWRE);
    return fpec_wait(3000u);
}

/* 写 RDP 半字 (硬件自动生成补码) */
static uint8_t rdp_write(uint16_t val)
{
    (void)SWD_Mem_Write32(FPEC_CR, FPEC_CR_OPTPG | FPEC_CR_OPTWRE);
    if (SWD_Mem_Write16(OBP_RDP, val) != SWD_OK) return TP_ERR_CONNECT;
    return fpec_wait(1000u);
}

/* 解除读保护 (Level1->0): 擦除 option + 写无保护键, 硬件整片擦除, 复位后生效。
   返回前已复位目标并延时, 之后可重新 target_attach 烧录。 */
static uint8_t rdp_unlock(void)
{
    uint8_t r;
    fpec_unlock();
    r = rdp_option_erase();
    if (r != TP_OK) return r;
    r = rdp_write(RDP_UNLOCK_KEY);
    if (r != TP_OK) return r;
    (void)SWD_Mem_Write32(AIRCR, 0x05FA0004u);   /* SYSRESETREQ: RDP0 生效 + 整片擦除 */
    BSP_DelayMs(100);
    SWD_GPIO_Release();
    BSP_DelayMs(50);
    Log_Str("RDP unlocked\n");
    return TP_OK;
}

/* 上锁 RDP Level1: 擦除 option + 写保护值 (不擦除主 flash)。复位后生效(由调用者复位)。 */
static uint8_t rdp_lock(void)
{
    uint8_t r;
    fpec_unlock();
    r = rdp_option_erase();
    if (r != TP_OK) return r;
    r = rdp_write(RDP_LOCK_VAL);
    if (r != TP_OK) return r;
    Log_Str("RDP locked(L1)\n");
    return TP_OK;
}

/* 调一次烧录算法函数并解释结果 (FA_xxx -> TP_xxx) */
static uint8_t algo_call(const flash_algo_t *algo, uint32_t entry,
                         uint32_t a1, uint32_t a2, uint32_t a3,
                         uint32_t timeout_ms, uint8_t fa_map)
{
    uint32_t ret = 0;
    uint8_t r = FlashAlgo_Call(algo, entry, a1, a2, a3, 0, timeout_ms, &ret);
    if (r == FA_ERR_SWD) {
        Log_Str("ALG swd e="); Log_Hex(entry); Log_Str("\n");
        return TP_ERR_CONNECT;
    }
    if (r == FA_ERR_TIMEOUT || r == FA_ERR_RETVAL) {
        /* 调试线索: 哪一步、超时还是返回值非 0 */
        Log_Str("ALG e=");  Log_Hex(entry);
        Log_Str(" r=");     Log_Hex(r);
        Log_Str(" ret=");   Log_Hex(ret);
        Log_Str("\n");
        return fa_map;
    }
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

    if (SWD_Mem_Read32(DPIDR_M0(dpidr) ? DBG_IDCODE_F0 : DBG_IDCODE, &idcode) == SWD_OK)
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
        if (SWD_Mem_Read32(DPIDR_M0(v) ? DBG_IDCODE_F0 : DBG_IDCODE, idcode) == SWD_OK)
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
    uint8_t  r;
    uint32_t crc;
    uint8_t  img_flags = 0;

    if (err_addr) *err_addr = 0;
    if (!Storage_ImageValid()) return TP_ERR_NOIMAGE;
    load = h->load_addr;
    len  = h->length;
    /* 镜像 trailer 携带的保护配置: reserved[0]=trailer长度, 需>=5 才含 flags 字节;
       flags 在 trailer[4] (chip_code 之后第 1 字节), 与上位机 aesbin 对应 */
    if (h->reserved[0] >= 5u)
        img_flags = Storage_PayloadPtr()[h->length + 4u];

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

    /* 自动解除读保护: RDP Level1 时无法 halt/读写 flash, 必须在 target_attach 之前
       通过 FPEC 擦除 option byte 解除 (整片擦除), 复位后再走正常烧录流程 */
    if (img_flags & IMG_FLAG_UNLOCK) {
        uint32_t dp = 0;
        if (SWD_Connect(&dp) == SWD_OK) {
            if (rdp_is_protected()) {
                Log_Str("RDP unlock...\n");
                (void)rdp_unlock();
            }
        }
        SWD_GPIO_Release();
    }

    /* ---- 0. 先复位一次, 让目标处于"刚复位"状态 ----
       目标跑着旧固件时, 烧录算法的 Init/擦除偶发返回错误 (同一块板
       第二次点击失败就是这个原因); 复位后立即连的稳定性明显更好。 */
    r = target_attach();
    if (r != TP_OK) { SWD_GPIO_Release(); return r; }
    (void)SWD_Mem_Write32(DHCSR, 0xA05F0000u);   /* 解除停机 */
    (void)SWD_Mem_Write32(AIRCR, 0x05FA0004u);   /* SYSRESETREQ */
    BSP_DelayMs(60);
    SWD_GPIO_Release();
    BSP_DelayMs(40);

    /* ---- 1. 连接并停机 ---- */
    r = target_attach();
    if (r != TP_OK) { SWD_GPIO_Release(); return r; }

    /* 冻结看门狗, 停机 (并确认真的停住) */
    freeze_wdog(ACTIVE_ALGO);
    if ((r = halt_target()) != TP_OK) return r;
    SWD_ReleaseNRST();      /* 停机已生效, 释放复位 (固件不会跑起来) */

    /* ---- 2. 下载烧录算法到目标 RAM 并初始化 ---- */
    {
        const flash_algo_t *algo = ACTIVE_ALGO;

        if (FlashAlgo_Load(algo) != FA_OK) return TP_ERR_CONNECT;

        /* Keil 算法约定: Init 的第 3 个参数 fnc 声明接下来的操作
           (1=Erase 2=Program 3=Verify), 切换阶段必须 UnInit+重新 Init,
           否则有的算法 (STM32F4/APM32F4) ProgramPage 会静默不干活 */
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
        (void)algo_call(algo, algo->uninit, 1, 0, 0,
                        algo->prog_timeout_ms + 500u, TP_OK);
        if (r != TP_OK) return r;

        /* 切到编程阶段: 重新 Init(fnc=2) */
        r = algo_call(algo, algo->init, load, 0, 2,
                      algo->prog_timeout_ms + 500u, TP_ERR_ALGO);
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

        /* UnInit(fnc=2) 收尾编程阶段 (尽力而为) */
        (void)algo_call(algo, algo->uninit, 2, 0, 0,
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

    /* 烧录并校验通过: 按需上锁 RDP Level1 (不擦除主 flash), 复位后生效 */
    if (img_flags & IMG_FLAG_LOCK) {
        if (rdp_lock() != TP_OK)
            Log_Str("RDP lock fail\n");
    }

    /* ---- 6. 复位目标, 运行新固件 (上锁也在复位后生效) ---- */
    (void)SWD_Mem_Write32(DHCSR, 0xA05F0000u);   /* 解除停机 */
    (void)SWD_Mem_Write32(AIRCR, 0x05FA0004u);   /* SYSRESETREQ */
    BSP_DelayMs(50);
    SWD_GPIO_Release();                          /* 释放 SWD 引脚, 便于下一次连接 */

    return TP_OK;
}

uint8_t TargetProg_Run(uint32_t *err_addr)
{
    uint8_t r = tp_run_impl(err_addr);

    /* 偶发失败 (目标供电边缘、克隆料 FMC/算法实现差异):
       表现为"同一块板第二次点击烧录失败, 重开上位机又好"。
       这里复位目标后整轮重试一次, 与人工再点一次的效果一致。
       无镜像/HEX/容量/参数/身份不符/读保护 属于确定性错误, 不重试。 */
    if (r == TP_ERR_ERASE || r == TP_ERR_WRITE || r == TP_ERR_VERIFY ||
        r == TP_ERR_ALGO  || r == TP_ERR_HALT  || r == TP_ERR_UNLOCK) {
        Log_Str("PROG RETRY\n");
        (void)SWD_Mem_Write32(DHCSR, 0xA05F0000u);   /* 解除停机 */
        (void)SWD_Mem_Write32(AIRCR, 0x05FA0004u);   /* SYSRESETREQ */
        BSP_DelayMs(80);
        SWD_GPIO_Release();
        BSP_DelayMs(50);
        r = tp_run_impl(err_addr);
    }

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
    r = target_attach();
    if (r != TP_OK) { SWD_GPIO_Release(); return r; }
    algo = ACTIVE_ALGO;                 /* AUTO: 识别后取对应算法 */

    freeze_wdog(algo);
    if ((r = halt_target()) != TP_OK) {
        strm_reset_run();
        return r;
    }
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

    /* 切到编程阶段: UnInit(fnc=1) 后重新 Init(fnc=2) */
    (void)algo_call(algo, algo->uninit, 1, 0, 0,
                    algo->prog_timeout_ms + 500u, TP_OK);
    r = algo_call(algo, algo->init, load_addr, 0, 2,
                  algo->prog_timeout_ms + 500u, TP_ERR_ALGO);
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

    (void)algo_call(algo, algo->uninit, 2, 0, 0,
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

/* 擦除后的回读: 整片擦除期间 Flash 控制器长时间忙, 之后 AP 读会短暂失败,
   延时 50ms 并重新连接后再读 (F303 512KB 实测需要) */
static uint8_t flash_read_retry(uint32_t addr, uint32_t *val)
{
    uint8_t i;
    for (i = 0; i < 5u; i++) {
        if (SWD_Mem_Read32(addr, val) == SWD_OK) return TP_OK;
        BSP_DelayMs(50);
        (void)target_attach();
    }
    return TP_ERR_CONNECT;
}

/* ---------------- 整片擦除 ---------------- */
uint8_t TargetEraseChip(void)
{
    const flash_algo_t *algo = ACTIVE_ALGO;
    uint32_t v;
    uint8_t  r;

    g_target_flash_kb = 0;
    g_status.swd_err = 0;

    r = target_attach();
    if (r != TP_OK) { Log_Str("EA attach fail\n"); SWD_GPIO_Release(); return r; }
    algo = ACTIVE_ALGO;                 /* AUTO: 识别后取对应算法 */

    freeze_wdog(algo);
    if ((r = halt_target()) != TP_OK) {
        Log_Str("EA halt fail\n");
        SWD_GPIO_Release();
        return r;
    }
    SWD_ReleaseNRST();

    if (SWD_Mem_Read32(FLASH_SIZE_REG, &v) == SWD_OK) {
        g_target_flash_kb = v & 0xFFFFu;
        if (g_target_flash_kb > 4096u) g_target_flash_kb = 0;
    }

    if (FlashAlgo_Load(algo) != FA_OK) {
        Log_Str("EA load fail\n");
        strm_reset_run(); return TP_ERR_CONNECT;
    }
    r = algo_call(algo, algo->init, 0x08000000u, 0, 1,
                  algo->prog_timeout_ms + 500u, TP_ERR_ALGO);
    if (r != TP_OK) {
        Log_Str("EA init fail\n");
        strm_reset_run(); return r;
    }

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

    (void)algo_call(algo, algo->uninit, 1, 0, 0,
                    algo->prog_timeout_ms + 500u, TP_OK);

    /* 空白校验: 首字与末字都应是 0xFFFFFFFF */
    if (r == TP_OK) {
        uint32_t w0 = 0, w1 = 0;
        r = flash_read_retry(0x08000000u, &w0);
        if (r == TP_OK && w0 != 0xFFFFFFFFu) {
            Log_Str("EA w0="); Log_Hex(w0); Log_Str("\n");
            r = TP_ERR_ERASE;
        }
        if (r == TP_OK && g_target_flash_kb != 0) {
            r = flash_read_retry(0x08000000u + g_target_flash_kb * 1024u - 4u, &w1);
            if (r == TP_OK && w1 != 0xFFFFFFFFu) {
                Log_Str("EA w1="); Log_Hex(w1); Log_Str("\n");
                r = TP_ERR_ERASE;
            }
        }
        if (r != TP_OK) Log_Str("EA verify fail\n");
    }

    strm_reset_run();
    return r;
}
#endif
