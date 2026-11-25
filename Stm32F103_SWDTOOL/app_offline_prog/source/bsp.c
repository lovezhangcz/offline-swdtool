/**
  * @file  bsp.c
  * @brief LED 初始化、毫秒时基(SysTick) 与简易延时 (寄存器级, 不依赖 SPL)
  */
#include "board.h"
#include "bsp.h"

static volatile uint32_t s_ms = 0;

/* 将某个 GPIO 引脚配置为指定 CNF/MODE (pin: 0..15) */
void BSP_PinConfig(GPIO_TypeDef *port, uint32_t pin, uint32_t cnf_mode)
{
    volatile uint32_t *reg;
    uint32_t shift;
    uint32_t tmp;

    if (pin < 8u) { reg = &port->CRL; shift = pin * 4u; }
    else          { reg = &port->CRH; shift = (pin - 8u) * 4u; }
    tmp  = *reg;
    tmp &= ~(0xFu << shift);
    tmp |=  (cnf_mode & 0xFu) << shift;
    *reg = tmp;
}

void BSP_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN |
                    RCC_APB2ENR_AFIOEN;

#if BOARD_DISABLE_JTAG
    /* 新主板: SWDIO=PB4(NJTRST)、USB 上拉=PA15(JTDI) 与 JTAG 复用。
     * SWJ_CFG=010: 关闭 JTAG-DP, 保留 SW-DP (PA13/PA14 仍可烧录本板)。 */
    AFIO->MAPR = (AFIO->MAPR & ~(7u << 24)) | (2u << 24);
#endif

    /* LED: PB12 推挽输出 2MHz (MODE=10 CNF=00 -> 0x2), 初始熄灭 */
    BSP_PinConfig(LED_GPIO, LED_PIN, 0x2u);
    LED_OFF();

    /* SysTick 1ms */
    SysTick_Config(SYSCLK_HZ / 1000u);
}

void SysTick_Handler(void)
{
    s_ms++;
}

uint32_t BSP_Millis(void)
{
    return s_ms;
}

void BSP_DelayMs(uint32_t ms)
{
    uint32_t t0 = s_ms;
    while ((s_ms - t0) < ms) { }
}

/* ---------------- 独立看门狗 ----------------
   目的: 程序卡死(例如等 FPEC/等 SWD 应答的死循环)时自动复位, 不用拔电。
   LSI 约 40kHz, PR=100(64 分频) -> 1.6ms 每拍, RLR=2000 -> 约 3.2s 超时。
   注意: IWDG 一旦启动无法关闭, 任何系统复位都会重新关闭它,
        所以只在应用里启动(不启动 boot 那边)。 */
void WDT_Init(void)
{
    IWDG->KR  = 0xCCCCu;        /* 启动 */
    IWDG->KR  = 0x5555u;        /* 允许写 PR/RLR */
    IWDG->PR  = 4u;             /* 64 分频 */
    IWDG->RLR = 2000u;          /* 3.2s */
    while (IWDG->SR) { }        /* 等更新生效 */
    IWDG->KR  = 0xAAAAu;        /* 喂一次 */
}

void WDT_Feed(void)
{
    IWDG->KR = 0xAAAAu;
}
