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
#define SWCLK_H()   (SWD_GPIO->BSRR = (1u << SWCLK_PIN))
#define SWCLK_L()   (SWD_GPIO->BRR  = (1u << SWCLK_PIN))
#define SWDIO_H()   (SWD_GPIO->BSRR = (1u << SWDIO_PIN))
#define SWDIO_L()   (SWD_GPIO->BRR  = (1u << SWDIO_PIN))
#define SWDIO_RD()  ((SWD_GPIO->IDR >> SWDIO_PIN) & 1u)

/* SWDIO 方向缓存: 0=输入(释放总线) 1=输出; 0xFF=未知, 强制重新配置 */
#define DIR_IN     0u
#define DIR_OUT    1u
#define DIR_UNK    0xFFu
static uint8_t s_dir = DIR_UNK;

/* MODE=11(50MHz) CNF=00 推挽 -> 0x3 ; CNF=10 上下拉输入 MODE=00 -> 0x8 */
static void swdio_out(void)
{
    if (s_dir == DIR_OUT) return;        /* 方向没变, 不动引脚配置 */
    BSP_PinConfig(SWD_GPIO, SWDIO_PIN, 0x3u);
    s_dir = DIR_OUT;
}
static void swdio_in(void)
{
    if (s_dir == DIR_IN) return;
    BSP_PinConfig(SWD_GPIO, SWDIO_PIN, 0x8u);
    SWDIO_H(); /* 上拉 */
    s_dir = DIR_IN;
}

static void swd_delay(void)
{
    volatile uint32_t i = SWD_DELAY_LOOP;
    while (i--) { __NOP(); }
}

static void swclk_pulse(void)
{
    swd_delay();
    SWCLK_H();
    swd_delay();
    SWCLK_L();
}

/* NRST: 开漏输出, 低=拉住目标复位, 高=释放 (目标内部上拉) */
#ifdef SWD_USE_NRST
static void nrst_assert(void)
{
    BSP_PinConfig(SWD_GPIO, SWD_NRST_PIN, 0x7u);   /* 开漏输出 */
    SWD_GPIO->BRR = (1u << SWD_NRST_PIN);
}
#endif

void SWD_ReleaseNRST(void)
{
#ifdef SWD_USE_NRST
    SWD_GPIO->BSRR = (1u << SWD_NRST_PIN);
#endif
}

void SWD_GPIO_Init(void)
{
    s_dir = DIR_UNK;                        /* 引脚状态未知, 强制重配 */
    /* SWCLK 推挽输出, 空闲低 */
    BSP_PinConfig(SWD_GPIO, SWCLK_PIN, 0x3u);
    SWCLK_L();
    swdio_in();
#ifdef SWD_USE_NRST
    nrst_assert();                      /* 连接全程拉住目标复位 */
#endif
}

void SWD_GPIO_Release(void)
{
    SWD_ReleaseNRST();
    s_dir = DIR_UNK;
    BSP_PinConfig(SWD_GPIO, SWCLK_PIN, 0x8u);   /* 上拉输入 */
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
        swd_delay();
        if (SWDIO_RD()) v |= (1u << s); /* 上升沿前采样 */
        SWCLK_H();
        swd_delay();
        SWCLK_L();
        s++;
    }
    return v;
}

/* 线复位: >=50 个时钟 SWDIO=1 */
static void swd_line_reset(void)
{
    swdio_out();
    SWDIO_H();
    {
        uint8_t i;
        for (i = 0; i < 64; i++) swclk_pulse();
    }
}

static void swd_idle(void)
{
    SWDIO_L();
    swclk_pulse();
    swclk_pulse();
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
    uint8_t r = swd_select_bank(reg);
    if (r != SWD_OK) return r;
    r = swd_transfer_retry(swd_make_req(1, 1, (uint8_t)((reg >> 2) & 3u)), val, 0);
    if (r != SWD_OK) return r;
    return SWD_DP_Read(0xCu, val);               /* RDBUFF */
}

uint8_t SWD_AP_Write(uint8_t reg, uint32_t val)
{
    uint8_t r = swd_select_bank(reg);
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
    uint32_t dummy;

    if (nwords == 0) return SWD_OK;
    r = swd_csw(CSW_32_INC);
    if (r != SWD_OK) return r;
    r = SWD_AP_Write(AP_TAR, addr);
    if (r != SWD_OK) return r;

    /* 首次读 DRW 仅用于启动流水, 返回值丢弃; 随后每次读回上一地址数据 */
    r = swd_transfer_retry(swd_make_req(1, 1, (uint8_t)((AP_DRW >> 2) & 3u)), &dummy, 0);
    if (r != SWD_OK) return r;
    while (--nwords) {
        r = swd_transfer_retry(swd_make_req(1, 1, (uint8_t)((AP_DRW >> 2) & 3u)), buf, 0);
        if (r != SWD_OK) return r;
        buf++;
    }
    return SWD_DP_Read(0xCu, buf);              /* 最后一个字经 RDBUFF */
}

/* 块写: TAR 地址自增, 连续写 DRW (用于下载烧录算法到目标 RAM) */
uint8_t SWD_Mem_WriteBlock(uint32_t addr, const uint32_t *buf, uint32_t nwords)
{
    uint8_t r;

    if (nwords == 0) return SWD_OK;
    r = swd_csw(CSW_32_INC);
    if (r != SWD_OK) return r;
    r = SWD_AP_Write(AP_TAR, addr);
    if (r != SWD_OK) return r;

    while (nwords--) {
        r = swd_transfer_retry(swd_make_req(1, 0, (uint8_t)((AP_DRW >> 2) & 3u)),
                               (uint32_t *)buf, 1);
        if (r != SWD_OK) return r;
        buf++;
    }
    return SWD_OK;
}

/* ------------------------------------------------ 连接 ------------------- */
uint8_t SWD_Connect(uint32_t *dpidr)
{
    uint8_t  r;
    uint32_t v, to;

    s_sel_cache = 0xFFFFFFFFu;
    s_csw_cache = 0xFFFFFFFFu;
    SWD_GPIO_Init();

    /* JTAG -> SWD 切换序列 */
    swd_line_reset();
    swdio_out();
    swd_write_bits(0xE79Eu, 16);
    swd_line_reset();
    swd_idle();

    /* 先写 ABORT 清粘滞错误: 部分 GD32 的 DP 上电即带 STICKYERR,
       或前次异常会话残留错误, 不清除则所有后续事务都回 FAULT。
       ABORT 写在任何 DP 状态下都是被允许的。                       */
    (void)SWD_DP_Write(0x0u, 0x1Eu);

    /* 读 DPIDR (失败则整序列重试一次) */
    r = SWD_DP_Read(0x0u, &v);
    if (r != SWD_OK) {
        swd_line_reset();
        swdio_out();
        swd_write_bits(0xE79Eu, 16);
        swd_line_reset();
        swd_idle();
        (void)SWD_DP_Write(0x0u, 0x1Eu);
        r = SWD_DP_Read(0x0u, &v);
        if (r != SWD_OK) return r;
    }
    if ((v & 0x0FFFFFFFu) != 0x0BA01477u) {      /* Cortex-M3 DP (掩掉 Revision) */
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
