#if !defined(BUILD_BOOT)
/**
  * @file  swd_host.c
  * @brief SWD 主机位带驱动 (ARM ADIv5)
  *
  *   时序策略 (移植自 blackmagic swdptap.c):
  *     - 每个位原语以下降沿结束; 时钟低电平期间就摆好输出数据
  *     - 输入在上升沿到来之前采样 (前一下降沿后目标已驱动稳定)
  *     - SWDIO 方向带缓存: 方向没变就不重新配置引脚 (减少毛刺窗口)
  *
  *   事务结构 (LSB 先传):
  *     请求(8b host) -> Trn(1clk) -> ACK(3b target)
  *     读: -> 数据(32b+parity target) -> Trn -> host 接管
  *     写: -> Trn -> 数据(32b+parity host)
  *     结束后保持 SWDIO 低 (idle)
  */
#include "swd_host.h"
#include "board.h"
#include "bsp.h"

/* ------------------------------------------------ 引脚操作 --------------- */
/* SWDIO 可能拆成两个脚: OUT = 发送(输出), IN = 采样(输入)
 * SWCLK 当前驱动脚可在运行时切换 (A 板 PA3 默认 / PB13 备用) */
static GPIO_TypeDef *s_clk_port = SWCLK_GPIO;
static uint16_t      s_clk_pin  = (uint16_t)SWCLK_PIN;
/* SWDIO 发送/采样脚也支持运行时对调 (便于现场排查接反) */
static GPIO_TypeDef *s_do_port = SWDIO_OUT_GPIO;
static uint16_t      s_do_pin  = (uint16_t)SWDIO_OUT_PIN;
static GPIO_TypeDef *s_di_port = SWDIO_IN_GPIO;
static uint16_t      s_di_pin  = (uint16_t)SWDIO_IN_PIN;

#define SWCLK_H()   (s_clk_port->BSRR = (1u << s_clk_pin))
#define SWCLK_L()   (s_clk_port->BRR  = (1u << s_clk_pin))
#define SWDIO_H()   (s_do_port->BSRR = (1u << s_do_pin))
#define SWDIO_L()   (s_do_port->BRR  = (1u << s_do_pin))
#define SWDIO_RD()  ((s_di_port->IDR >> s_di_pin) & 1u)

void SWD_SetClockPin(GPIO_TypeDef *port, uint16_t pin)
{
    if (port == s_clk_port && pin == s_clk_pin) return;
    BSP_PinConfig(s_clk_port, s_clk_pin, 0x8u);      /* 旧脚释放为上拉输入 */
    s_clk_port = port;
    s_clk_pin  = pin;
    BSP_PinConfig(s_clk_port, s_clk_pin, 0x3u);      /* 新脚推挽输出 */
    SWCLK_L();
}

void SWD_SetDioPins(GPIO_TypeDef *out_port, uint16_t out_pin,
                    GPIO_TypeDef *in_port, uint16_t in_pin);

/* SWDIO 方向缓存: 0=输入(释放总线) 1=输出; 0xFF=未知, 强制重新配置 */
#define DIR_IN     0u
#define DIR_OUT    1u
#define DIR_UNK    0xFFu
static uint8_t s_dir = DIR_UNK;

/* MODE=11(50MHz) CNF=00 推挽 -> 0x3 ; CNF=10 上下拉输入 MODE=00 -> 0x8
 * CNF=01 浮空输入 -> 0x4 (释放输出脚, 避免与目标争抢总线)              */
static void swdio_out(void)
{
    if (s_dir == DIR_OUT) return;        /* 方向没变, 不动引脚配置 */
    BSP_PinConfig(s_do_port, s_do_pin, 0x3u);
    s_dir = DIR_OUT;
}
static void swdio_in(void)
{
    if (s_dir == DIR_IN) return;
#if SWDIO_SHARED
    /* 同脚: 配成上拉输入 */
    BSP_PinConfig(s_do_port, s_do_pin, 0x8u);
    SWDIO_H();
#else
    /* 双引脚: 发送脚释放为浮空输入, 采样脚上拉输入 (ODR=1 确保是上拉) */
    BSP_PinConfig(s_do_port, s_do_pin, 0x4u);
    BSP_PinConfig(s_di_port, s_di_pin, 0x8u);
    s_di_port->BSRR = (1u << s_di_pin);
#endif
    s_dir = DIR_IN;
}

void SWD_SetDioPins(GPIO_TypeDef *out_port, uint16_t out_pin,
                    GPIO_TypeDef *in_port, uint16_t in_pin)
{
    BSP_PinConfig(s_do_port, s_do_pin, 0x4u);        /* 旧发送脚释放 */
    BSP_PinConfig(s_di_port, s_di_pin, 0x4u);        /* 旧采样脚释放 */
    s_do_port = out_port; s_do_pin = out_pin;
    s_di_port = in_port;  s_di_pin = in_pin;
    s_dir = DIR_UNK;                                 /* 强制下次重配方向 */
    swdio_in();
}

static uint32_t s_swd_dly = SWD_DELAY_LOOP;
uint8_t g_swd_dbg_ack = 0xFFu;   /* 诊断用: 最近一次 ACK 原始值 */

void SWD_SetSpeed(uint32_t loop)
{
    s_swd_dly = loop ? loop : 1u;
}

static void swd_delay(void)
{
    volatile uint32_t i = s_swd_dly;
    while (i--) { __NOP(); }
}

#if DBG_TOOLS
/* 引脚自检: 判断线束是否真的接到目标 (目标复位态 SWDIO/PA13 内部上拉、
 * SWCLK/PA14 内部下拉)。
 *   idle   : 采样脚带我方上拉时的电平 (只说明我方网络)
 *   io_rel : PA4 拉低后全部浮空, 读回 SWDIO; 目标上拉会把它拉回 1
 *   io_hold: 同上, 但等 100ms 再读 —— 只有存在直流上拉才会保持 1
 *   ck_rel : PA3 拉高后全部浮空, 读回 SWCLK; 目标下拉会把它拉回 0
 *   ck_hold: 同上, 等 100ms 再读 —— 只有存在直流下拉才会保持 0
 * 典型"目标已接好且上电": io_rel=1 io_hold=1 ck_rel=0 ck_hold=0 */
void SWD_PinDiag(uint8_t *idle, uint8_t *io_rel, uint8_t *io_hold,
                 uint8_t *ck_rel, uint8_t *ck_hold)
{
    s_dir = DIR_UNK;
    swdio_in();
    BSP_DelayMs(1);
    *idle = (uint8_t)SWDIO_RD();

    /* SWDIO: 强驱动低, 然后彻底释放 (浮空、无上拉), 看外部是否拉回高 */
    BSP_PinConfig(s_do_port, s_do_pin, 0x3u);
    SWDIO_L();
    BSP_DelayMs(1);
    BSP_PinConfig(s_do_port, s_do_pin, 0x4u);
    BSP_PinConfig(s_di_port, s_di_pin, 0x4u);             /* 采样脚也去掉上拉 */
    BSP_DelayMs(1);
    *io_rel = (uint8_t)SWDIO_RD();
    BSP_DelayMs(100);
    *io_hold = (uint8_t)SWDIO_RD();

    /* SWCLK: 强驱动高, 然后释放, 看外部下拉是否拉回低 */
    BSP_PinConfig(s_clk_port, s_clk_pin, 0x3u);
    SWCLK_H();
    BSP_DelayMs(1);
    BSP_PinConfig(s_clk_port, s_clk_pin, 0x4u);
    BSP_DelayMs(1);
    *ck_rel = (uint8_t)((s_clk_port->IDR >> s_clk_pin) & 1u);
    BSP_DelayMs(100);
    *ck_hold = (uint8_t)((s_clk_port->IDR >> s_clk_pin) & 1u);
#if SWD_HAS_CLK_ALT
    BSP_PinConfig(SWD_CLK_ALT_GPIO, SWD_CLK_ALT_PIN, 0x4u);
#endif

    s_dir = DIR_UNK;
    swdio_in();
}
#endif /* DBG_TOOLS */

/* 时钟基本拍: SWCLK 空闲高电平, 每个时钟是"低脉冲"(拉低->拉高),
 * 与成功参考实现一致; 目标在上升沿采样, 低相位期间数据保持稳定 */
static void swclk_pulse(void)
{
    SWCLK_L();
    swd_delay();
    SWCLK_H();
    swd_delay();
}

/* NRST: 开漏输出, 低=拉住目标复位, 高=释放 (目标内部上拉) */
#ifdef SWD_USE_NRST
static void nrst_assert(void)
{
    BSP_PinConfig(SWD_NRST_GPIO, SWD_NRST_PIN, 0x7u);   /* 开漏输出 */
    SWD_NRST_GPIO->BRR = (1u << SWD_NRST_PIN);
}
#endif

void SWD_ReleaseNRST(void)
{
#ifdef SWD_USE_NRST
    SWD_NRST_GPIO->BSRR = (1u << SWD_NRST_PIN);
#endif
}

#if DBG_TOOLS
/* 时钟网络自检 (A 板参考文档: SWCLK 网络同时涉及 PA3(13脚) 与 PB13(26脚),
 * 两个脚经板上电阻并到四针口的 SWCLK)。互相驱动/采样, 可判断时钟网络是否通。
 * 返回 1 = 驱动脚拉低/拉高时另一脚能跟随。 */
void SWD_ClockDiag(uint8_t *a_drv_b, uint8_t *b_drv_a)
{
    uint8_t lo, hi;

    *a_drv_b = 0;
    *b_drv_a = 0;

#if SWD_HAS_CLK_ALT
    /* 先全部置浮空输入 */
    BSP_PinConfig(SWCLK_GPIO, SWCLK_PIN, 0x4u);
    BSP_PinConfig(SWD_CLK_ALT_GPIO, SWD_CLK_ALT_PIN, 0x4u);
    BSP_DelayMs(1);

    /* PA3 驱动 -> PB13 采样 */
    BSP_PinConfig(SWCLK_GPIO, SWCLK_PIN, 0x3u);
    SWCLK_L(); BSP_DelayMs(1); lo = (uint8_t)((SWD_CLK_ALT_GPIO->IDR >> SWD_CLK_ALT_PIN) & 1u);
    SWCLK_H(); BSP_DelayMs(1); hi = (uint8_t)((SWD_CLK_ALT_GPIO->IDR >> SWD_CLK_ALT_PIN) & 1u);
    *a_drv_b = (uint8_t)((lo == 0u && hi == 1u) ? 1u : 0u);

    /* 反过来: PB13 驱动 -> PA3 采样 */
    BSP_PinConfig(SWCLK_GPIO, SWCLK_PIN, 0x4u);
    BSP_PinConfig(SWD_CLK_ALT_GPIO, SWD_CLK_ALT_PIN, 0x3u);
    BSP_DelayMs(1);
    SWD_CLK_ALT_GPIO->BRR  = (1u << SWD_CLK_ALT_PIN); BSP_DelayMs(1);
    lo = (uint8_t)((SWCLK_GPIO->IDR >> SWCLK_PIN) & 1u);
    SWD_CLK_ALT_GPIO->BSRR = (1u << SWD_CLK_ALT_PIN); BSP_DelayMs(1);
    hi = (uint8_t)((SWCLK_GPIO->IDR >> SWCLK_PIN) & 1u);
    *b_drv_a = (uint8_t)((lo == 0u && hi == 1u) ? 1u : 0u);

    /* 恢复: 两个脚都释放 */
    BSP_PinConfig(SWD_CLK_ALT_GPIO, SWD_CLK_ALT_PIN, 0x4u);
    BSP_PinConfig(SWCLK_GPIO, SWCLK_PIN, 0x4u);
#endif
}

/* SWDIO 自回环: 验证发送脚(PA4)与采样脚(PA7)是否真并在一起 (A 板双引脚接法)。
 * 目标空白/复位时 PA13 仅内部上拉(高阻), 不会强驱, 适合做此测试。
 * 返回 bit0 = 发送脚拉低时采样脚读到的电平, bit1 = 拉高时读到的电平。
 *   跟随正常 = bit0=0, bit1=1 (返回 2); 0b11 (=3, 采样脚恒高/未并) ; 0b00 (=0, 恒低)。 */
uint8_t SWD_SelfEcho(void)
{
    uint8_t lo, hi;
    BSP_PinConfig(s_di_port, s_di_pin, 0x8u);      /* 采样脚上拉输入 */
    BSP_PinConfig(s_do_port, s_do_pin, 0x3u);      /* 发送脚推挽输出 */
    SWDIO_L(); BSP_DelayMs(2);
    lo = (uint8_t)SWDIO_RD();
    SWDIO_H(); BSP_DelayMs(2);
    hi = (uint8_t)SWDIO_RD();
    s_dir = DIR_UNK;
    swdio_in();
    return (uint8_t)((hi << 1) | lo);
}
#endif /* DBG_TOOLS */

#if DBG_TOOLS
/* 示波器时钟测试: 持续在 SWCLK(PA3) 输出方波, SWDIO(PA4) 输出更低频方波,
 * 便于用示波器确认四针口上哪根是时钟、哪根是数据。g_clk_test 由命令控制。
 * 每次只输出 ms 毫秒就返回, 让主循环还能轮询 USB (可随时发停止命令)。 */
volatile uint8_t g_clk_test;

void SWD_ClkTestBurst(uint32_t ms)
{
    uint32_t t0 = BSP_Millis();
    uint32_t n = 0;

    BSP_PinConfig(s_clk_port, s_clk_pin, 0x3u);   /* SWCLK 推挽输出 */
    BSP_PinConfig(s_do_port,  s_do_pin,  0x3u);   /* SWDIO 推挽输出 */
    SWDIO_L();

    while ((BSP_Millis() - t0) < ms) {
        uint8_t i;
        for (i = 0; i < 64u; i++) {
            volatile uint32_t d = 40;
            SWCLK_H();
            while (d--) { __NOP(); }
            d = 40;
            SWCLK_L();
            while (d--) { __NOP(); }
            if ((n++ & 63u) == 0u) {              /* 每 64 个 SWCLK 翻转一次 SWDIO */
                if (n & 64u) SWDIO_H(); else SWDIO_L();
            }
        }
    }
    SWCLK_L();
    s_dir = DIR_UNK;
    swdio_in();                                    /* SWDIO 释放回输入 */
}
#endif /* DBG_TOOLS */

void SWD_GPIO_Init(void)
{
    s_dir = DIR_UNK;                        /* 引脚状态未知, 强制重配 */
    /* SWCLK 推挽输出, 空闲高 (对齐成功参考实现) */
    BSP_PinConfig(s_clk_port, s_clk_pin, 0x3u);
    SWCLK_H();
    swdio_in();
#ifdef SWD_USE_NRST
    nrst_assert();                      /* 连接全程拉住目标复位 */
#endif
}

void SWD_GPIO_Release(void)
{
    SWD_ReleaseNRST();
    s_dir = DIR_UNK;
    BSP_PinConfig(s_clk_port, s_clk_pin, 0x8u);   /* 上拉输入 */
    swdio_in();
}

/* ------------------------------------------------ 位收发 ----------------- */
/* 输出: 时钟低电平期间摆好数据, 再给出上升沿 (下降沿后即驱动, 建立时间最大) */
static void swd_write_bits(uint32_t val, uint8_t n)
{
    while (n--) {
        if (val & 1u) SWDIO_H(); else SWDIO_L();
        swclk_pulse();
        val >>= 1;
    }
}

/* 输入: 在上升沿到来之前采样 (前一下降沿后目标已把数据驱动稳定),
   采样后立即抬钟, 保持时间要求最低 —— blackmagic swdptap 策略 */
static uint32_t swd_read_bits(uint8_t n)
{
    uint32_t v = 0, s = 0;
    while (n--) {
        SWCLK_L();                       /* 下降沿 */
        swd_delay();
        if (SWDIO_RD()) v |= (1u << s);  /* 低相位内采样(上升沿前, 目标已驱动稳定) */
        swd_delay();
        SWCLK_H();                       /* 上升沿 */
        swd_delay();
        s++;
    }
    return v;
}

/* 线复位: >=50 个时钟 SWDIO=1 (n 由调用者给, 参考实现首次 80、二次 60) */
static void swd_line_reset(uint16_t n)
{
    swdio_out();
    SWDIO_H();
    while (n--) swclk_pulse();
}

/* 空闲: SWDIO=0 打 16 个时钟后拉高 (参考实现同款) */
static void swd_idle(void)
{
    uint8_t i;
    SWDIO_L();
    for (i = 0; i < 16u; i++) swclk_pulse();
    SWDIO_H();
}

/* 构造请求字节: Start=1 Park=1 Stop=0 */
static uint8_t swd_make_req(uint8_t apndp, uint8_t rnw, uint8_t addr23)
{
    uint8_t req = (uint8_t)(0x81u | (apndp << 1) | (rnw << 2) | ((addr23 & 3u) << 3));
    uint8_t par = (uint8_t)(((req >> 1) ^ (req >> 2) ^ (req >> 3) ^ (req >> 4)) & 1u);
    return (uint8_t)(req | (par << 5));
}

/**
  * 一次 SWD 传输
  * @param req   请求字节
  * @param data  写: 待写数据; 读: 输出数据
  * @param is_wr 1=写事务
  * @return SWD_OK / SWD_ERR_WAIT / SWD_ERR_FAULT / SWD_ERR_NORESP / SWD_ERR_PARITY
  */
static uint8_t swd_transfer(uint8_t req, uint32_t *data, uint8_t is_wr)
{
    uint8_t ack;
    uint32_t val;
    uint32_t par;

    swdio_out();
    swd_write_bits(req, 8);

    /* Trn: 释放总线 1 个时钟 */
    swdio_in();
    swclk_pulse();

    /* ACK 3 位 */
    ack = (uint8_t)swd_read_bits(3);
    g_swd_dbg_ack = ack;        /* 诊断: 最后一次握手读到的 ACK 原始值 */

    if (ack == 0x7u || ack == 0x0u) {  /* 全1(上拉)/全0: 没有目标在驱动总线 */
        swclk_pulse();
        swdio_out();
        SWDIO_L();
        return SWD_ERR_NORESP;
    }

    if (ack != 0x1u) {           /* WAIT(0x2) / FAULT(0x4): 无数据阶段 */
        swclk_pulse();           /* Trn 交还主机 */
        swdio_out();
        SWDIO_L();
        swclk_pulse();
        return (ack == 0x2u) ? SWD_ERR_WAIT : SWD_ERR_FAULT;
    }

    if (is_wr) {
        /* Trn 后主机驱动数据 */
        swclk_pulse();
        swdio_out();
        val = *data;
        swd_write_bits(val, 32);
        /* 偶校验位 */
        val ^= val >> 16; val ^= val >> 8; val ^= val >> 4;
        val ^= val >> 2;  val ^= val >> 1;
        swd_write_bits(val & 1u, 1);
        swd_idle();
        return SWD_OK;
    }

    /* 读: 目标驱动 32 位 + 校验 */
    val = swd_read_bits(32);
    par = swd_read_bits(1);
    {
        uint32_t p = val;
        p ^= p >> 16; p ^= p >> 8; p ^= p >> 4; p ^= p >> 2; p ^= p >> 1;
        /* Trn 交还主机 */
        swclk_pulse();
        swdio_out();
        SWDIO_L();
        swclk_pulse();
        if ((p & 1u) != par) return SWD_ERR_PARITY;
    }
    *data = val;
    return SWD_OK;
}

static uint8_t swd_transfer_retry(uint8_t req, uint32_t *data, uint8_t is_wr)
{
    uint8_t i, r = SWD_ERR_WAIT;
    for (i = 0; i < SWD_RETRY; i++) {
        r = swd_transfer(req, data, is_wr);
        if (r != SWD_ERR_WAIT) break;
    }
    return r;
}

/* ------------------------------------------------ DP / AP ---------------- */
/* SELECT 缓存, 0xFFFFFFFF 表示未知 */
static uint32_t s_sel_cache = 0xFFFFFFFFu;

uint8_t SWD_DP_Read(uint8_t reg, uint32_t *val)
{
    return swd_transfer_retry(swd_make_req(0, 1, (uint8_t)((reg >> 2) & 3u)), val, 0);
}

uint8_t SWD_DP_Write(uint8_t reg, uint32_t val)
{
    return swd_transfer_retry(swd_make_req(0, 0, (uint8_t)((reg >> 2) & 3u)), &val, 1);
}

/* AP 读需要先发读请求, 再读 DP RDBUFF 取真正数据 */
static uint8_t swd_select_bank(uint8_t reg)
{
    uint32_t sel = (uint32_t)(reg & 0xF0u);      /* AP0, bank = reg[7:4] */
    if (sel != s_sel_cache) {
        uint8_t r = SWD_DP_Write(0x8u, sel);
        if (r != SWD_OK) return r;
        s_sel_cache = sel;
    }
    return SWD_OK;
}

uint8_t SWD_AP_Read(uint8_t reg, uint32_t *val)
{
    uint8_t r;
    WDT_Feed();                                  /* 所有长流程最终都落到这里, 统一喂狗 */
    r = swd_select_bank(reg);
    if (r != SWD_OK) return r;
    r = swd_transfer_retry(swd_make_req(1, 1, (uint8_t)((reg >> 2) & 3u)), val, 0);
    if (r != SWD_OK) return r;
    return SWD_DP_Read(0xCu, val);               /* RDBUFF */
}

uint8_t SWD_AP_Write(uint8_t reg, uint32_t val)
{
    uint8_t r;
    WDT_Feed();                                  /* 统一喂狗 */
    r = swd_select_bank(reg);
    if (r != SWD_OK) return r;
    return swd_transfer_retry(swd_make_req(1, 0, (uint8_t)((reg >> 2) & 3u)), &val, 1);
}

/* ------------------------------------------------ MEM-AP 访问 ------------ */
#define AP_CSW   0x00u
#define AP_TAR   0x04u
#define AP_DRW   0x0Cu

/* CSW: MasterType|Hprot 特权数据访问; SIZE: 1=16位 2=32位; bit4 地址自增 */
#define CSW_32        0x23000002u
#define CSW_16        0x23000001u
#define CSW_32_INC    0x23000012u

static uint32_t s_csw_cache = 0xFFFFFFFFu;

static uint8_t swd_csw(uint32_t csw)
{
    if (csw == s_csw_cache) return SWD_OK;
    {
        uint8_t r = SWD_AP_Write(AP_CSW, csw);
        if (r == SWD_OK) s_csw_cache = csw;
        return r;
    }
}

uint8_t SWD_Mem_Write32(uint32_t addr, uint32_t val)
{
    uint8_t r = swd_csw(CSW_32);
    if (r != SWD_OK) return r;
    r = SWD_AP_Write(AP_TAR, addr);
    if (r != SWD_OK) return r;
    return SWD_AP_Write(AP_DRW, val);
}

uint8_t SWD_Mem_Write16(uint32_t addr, uint16_t val)
{
    uint8_t r = swd_csw(CSW_16);
    if (r != SWD_OK) return r;
    r = SWD_AP_Write(AP_TAR, addr);
    if (r != SWD_OK) return r;
    return SWD_AP_Write(AP_DRW, (uint32_t)val);
}

uint8_t SWD_Mem_Read32(uint32_t addr, uint32_t *val)
{
    uint8_t r = swd_csw(CSW_32);
    if (r != SWD_OK) return r;
    r = SWD_AP_Write(AP_TAR, addr);
    if (r != SWD_OK) return r;
    return SWD_AP_Read(AP_DRW, val);
}

uint8_t SWD_Mem_ReadBlock(uint32_t addr, uint32_t *buf, uint32_t nwords)
{
    uint8_t r;

    /* AHB-AP 的 DRW 自增在 1KB 边界回绕, 跨边界必须重写 TAR 分段 */
    while (nwords) {
        uint32_t n = (0x400u - (addr & 0x3FFu)) >> 2;
        uint32_t left, dummy;
        if (n > nwords) n = nwords;
        r = swd_csw(CSW_32_INC);
        if (r != SWD_OK) return r;
        r = SWD_AP_Write(AP_TAR, addr);
        if (r != SWD_OK) return r;

        /* 首次读 DRW 仅用于启动流水, 返回值丢弃; 随后每次读回上一地址数据 */
        r = swd_transfer_retry(swd_make_req(1, 1, (uint8_t)((AP_DRW >> 2) & 3u)), &dummy, 0);
        if (r != SWD_OK) return r;
        left = n;
        while (--left) {
            r = swd_transfer_retry(swd_make_req(1, 1, (uint8_t)((AP_DRW >> 2) & 3u)), buf, 0);
            if (r != SWD_OK) return r;
            buf++;
        }
        r = SWD_DP_Read(0xCu, buf);              /* 最后一个字经 RDBUFF */
        if (r != SWD_OK) return r;
        buf++;
        addr += n * 4u;
        nwords -= n;
    }
    return SWD_OK;
}

/* 块写: TAR 地址自增, 连续写 DRW (用于下载烧录算法到目标 RAM) */
uint8_t SWD_Mem_WriteBlock(uint32_t addr, const uint32_t *buf, uint32_t nwords)
{
    uint8_t r;

    /* AHB-AP 的 DRW 自增在 1KB 边界回绕, 跨边界必须重写 TAR 分段 */
    while (nwords) {
        uint32_t n = (0x400u - (addr & 0x3FFu)) >> 2;
        uint32_t left;
        if (n > nwords) n = nwords;
        r = swd_csw(CSW_32_INC);
        if (r != SWD_OK) return r;
        r = SWD_AP_Write(AP_TAR, addr);
        if (r != SWD_OK) return r;

        left = n;
        while (left--) {
            r = swd_transfer_retry(swd_make_req(1, 0, (uint8_t)((AP_DRW >> 2) & 3u)),
                                   (uint32_t *)buf, 1);
            if (r != SWD_OK) return r;
            buf++;
        }
        addr += n * 4u;
        nwords -= n;
    }
    return SWD_OK;
}

/* 原始监听诊断: 走一遍连接序列并发出 DPIDR 读请求, 随后连续采样 32 个时钟的
 * SWDIO 电平, 打包成 32 位值 (bit0 = 第一个采样点)。
 *   mode=0 上升沿前采样(与正常读一致); mode=1 上升沿后采样。
 * 用途: 判断目标到底有没有驱动 SWDIO。
 *   全 1 (0xFFFFFFFF) => 目标从未拉低, 完全没驱动总线;
 *   出现 0            => 目标有驱动, 若 ACK 仍错则是采样相位对不齐。 */
#if DBG_TOOLS
uint32_t SWD_ListenRaw(uint8_t mode)
{
    uint32_t v = 0;
    uint8_t i;

    SWD_GPIO_Init();
    swd_line_reset(80u);
    swdio_out();
    swd_write_bits(0xE79Eu, 16);
    swd_line_reset(60u);
    swd_idle();
    swdio_out();
    swd_write_bits(swd_make_req(0, 1, 0), 8);   /* 读 DPIDR */
    swdio_in();
    swclk_pulse();                               /* 方向切换拍 */

    for (i = 0; i < 32u; i++) {
        if (mode == 0u) {                        /* 上升沿前(低相位)采样 */
            SWCLK_L(); swd_delay();
            if (SWDIO_RD()) v |= (1u << i);
            swd_delay(); SWCLK_H(); swd_delay();
        } else {                                 /* 上升沿后采样 */
            SWCLK_L(); swd_delay(); SWCLK_H(); swd_delay();
            if (SWDIO_RD()) v |= (1u << i);
        }
    }
    swdio_out();
    SWDIO_L();
    return v;
}
#endif /* DBG_TOOLS */

/* ------------------------------------------------ 连接 ------------------- */
uint8_t SWD_Connect(uint32_t *dpidr)
{
    uint8_t  r, attempt;
    uint32_t v, to;

    s_sel_cache = 0xFFFFFFFFu;
    s_csw_cache = 0xFFFFFFFFu;
    SWD_GPIO_Init();

    /* JTAG -> SWD 切换序列 (对齐成功参考实现: 80拍 / 0xE79E / 60拍 / 16拍空闲)
       目标刚复位/刚烧完时 SWD 可能尚未就绪, 整条序列最多试 8 轮, 间隔 20ms */
    for (attempt = 0; attempt < 8u; attempt++) {
        swd_line_reset(80u);
        swdio_out();
        swd_write_bits(0xE79Eu, 16);
        swd_line_reset(60u);
        swd_idle();

        r = SWD_DP_Read(0x0u, &v);
        if (r == SWD_OK && ((v & 0x0FFFFFFFu) == 0x0BA01477u ||     /* M3/M4 DP */
                            (v & 0x0FFFFFFFu) == 0x0BC11477u)) break;/* M0 DP (STM32F0) */
        if (attempt < 7u) BSP_DelayMs(20);
    }
    if (r != SWD_OK) return r;
    if ((v & 0x0FFFFFFFu) != 0x0BA01477u &&      /* 掩掉 Revision, 接受 M3/M4/M0 */
        (v & 0x0FFFFFFFu) != 0x0BC11477u) {
        *dpidr = v;
        return SWD_ERR_IDCODE;
    }
    *dpidr = v;

    /* 清粘滞错误标志 */
    r = SWD_DP_Write(0x0u, 0x1Eu);               /* ABORT: WDERRCLR|STKERRCLR|STKCMPCLR|ORUNERRCLR */
    if (r != SWD_OK) return r;

    /* 调试/系统电源域上电 */
    s_sel_cache = 0xFFFFFFFFu;
    r = SWD_DP_Write(0x8u, 0x0u);                /* SELECT = 0 */
    if (r != SWD_OK) return r;
    s_sel_cache = 0;
    r = SWD_DP_Write(0x4u, 0x50000000u);         /* CTRL/STAT: CSYSPWRUPREQ|CDBGPWRUPREQ */
    if (r != SWD_OK) return r;

    to = 100u;
    do {
        r = SWD_DP_Read(0x4u, &v);
        if (r != SWD_OK) return r;
        if ((v & 0xA0000000u) == 0xA0000000u) break;   /* 两个 ACK 均置位 */
        BSP_DelayMs(1);
    } while (--to);

    return to ? SWD_OK : SWD_ERR_POWER;
}
#endif
