#if !defined(BUILD_BOOT)
/**
  * @file  flash_algo.c
  * @brief Keil FLM 烧录算法的目标侧执行器
  *
  *   原理 (与 DAPLink/pyOCD 相同):
  *     1. 算法 blob 经 AHB-AP 写入目标 RAM (0x20000000 起)
  *     2. 经 DCRSR/DCRDR 写 R0-R3(参数), R9=RW 段基址, SP, LR=BKPT 地址,
  *        PC=函数入口, xPSR.T=1
  *     3. 解除停机让目标执行; 函数返回到 LR 处的 BKPT 自动再次停机
  *     4. 轮询 DHCSR.S_HALT, 读回 R0 作为返回值 (0=成功)
  */
#include "flash_algo.h"
#include "swd_host.h"
#include "bsp.h"
#include "board.h"

#if USB_HAS_CDC
  #include "proto.h"
  #define ALGO_POLL()   Proto_Poll()
#else
  #define ALGO_POLL()   ((void)0)
#endif

/* ---- Cortex-M 内核调试寄存器 ---- */
#define DHCSR        0xE000EDF0u
#define DCRSR        0xE000EDF4u
#define DCRDR        0xE000EDF8u
#define DHCSR_KEY    0xA05F0000u
#define S_REGRDY     (1u << 16)
#define S_HALT       (1u << 17)

#define REGWR_TIMEOUT   200u    /* 写内核寄存器等待 S_REGRDY 次数 */

static uint8_t wait_regrdy(void)
{
    uint32_t v, to = REGWR_TIMEOUT;
    while (to--) {
        if (SWD_Mem_Read32(DHCSR, &v) != SWD_OK) return FA_ERR_SWD;
        if (v & S_REGRDY) return FA_OK;
    }
    return FA_ERR_TIMEOUT;
}

static uint8_t core_reg_write(uint8_t reg, uint32_t val)
{
    uint8_t r;
    if (SWD_Mem_Write32(DCRDR, val) != SWD_OK) return FA_ERR_SWD;
    if (SWD_Mem_Write32(DCRSR, 0x00010000u | reg) != SWD_OK) return FA_ERR_SWD;
    r = wait_regrdy();
    return r;
}

static uint8_t core_reg_read(uint8_t reg, uint32_t *val)
{
    uint8_t r;
    if (SWD_Mem_Write32(DCRSR, reg) != SWD_OK) return FA_ERR_SWD;
    r = wait_regrdy();
    if (r != FA_OK) return r;
    if (SWD_Mem_Read32(DCRDR, val) != SWD_OK) return FA_ERR_SWD;
    return FA_OK;
}

uint8_t FlashAlgo_Load(const flash_algo_t *algo)
{
    /* blob 大小恒为 4 的倍数 (生成脚本保证) */
    return (SWD_Mem_WriteBlock(algo->algo_start, algo->algo_blob,
                               algo->algo_size / 4u) == SWD_OK)
           ? FA_OK : FA_ERR_SWD;
}

uint8_t FlashAlgo_Call(const flash_algo_t *algo, uint32_t entry,
                       uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
                       uint32_t timeout_ms, uint32_t *ret)
{
    uint32_t v, t0;
    uint8_t r;

    /* 参数与执行上下文 */
    if ((r = core_reg_write(0,  a1)) != FA_OK) return r;
    if ((r = core_reg_write(1,  a2)) != FA_OK) return r;
    if ((r = core_reg_write(2,  a3)) != FA_OK) return r;
    if ((r = core_reg_write(3,  a4)) != FA_OK) return r;
    if ((r = core_reg_write(9,  algo->syscall.static_base))   != FA_OK) return r;
    if ((r = core_reg_write(13, algo->syscall.stack_pointer)) != FA_OK) return r;
    if ((r = core_reg_write(14, algo->syscall.breakpoint))    != FA_OK) return r;
    if ((r = core_reg_write(15, entry))                       != FA_OK) return r;
    if ((r = core_reg_write(16, 0x01000000u))                 != FA_OK) return r; /* xPSR.T */

    /* 放行执行: C_DEBUGEN=1, C_HALT=0 */
    if (SWD_Mem_Write32(DHCSR, DHCSR_KEY | 0x1u) != SWD_OK) return FA_ERR_SWD;

    /* 等待函数返回到 BKPT 而停机 */
    t0 = BSP_Millis();
    for (;;) {
        ALGO_POLL();                 /* 长操作中保持 USB 响应 */
        if (SWD_Mem_Read32(DHCSR, &v) != SWD_OK) return FA_ERR_SWD;
        if (v & S_HALT) break;
        if ((BSP_Millis() - t0) > timeout_ms) {
            /* 超时: 把当前 PC 带出 (ret), 便于判断算法卡在哪/是否进了 HardFault */
            uint32_t pc = 0;
            (void)core_reg_read(15, &pc);
            if (ret) *ret = pc;
            (void)SWD_Mem_Write32(DHCSR, DHCSR_KEY | 0x3u);  /* 强制停机 */
            return FA_ERR_TIMEOUT;
        }
    }

    /* R0 = 算法返回值 (0=成功) */
    if ((r = core_reg_read(0, &v)) != FA_OK) return r;
    if (ret) *ret = v;
    return (v == 0) ? FA_OK : FA_ERR_RETVAL;
}
#endif
