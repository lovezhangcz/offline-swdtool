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

/* ============================================================
 *  LED 状态指示 (在 1ms SysTick 时基里推进)
 *  设计要点: 闪烁节奏放在中断里, 主循环被 TargetProg_Run 阻塞
 *  (10~20s) 时灯也不会冻住, 操作者始终能判断烧录器在干什么。
 *  每个状态对应一段 {亮/灭, 持续ms} 序列, 到末尾循环。
 * ============================================================ */
typedef struct { uint8_t on; uint16_t ms; } led_step_t;

static const led_step_t s_pat_no_image[] = { {1,70}, {0,1930} };              /* 空闲: 2s 一短闪 */
static const led_step_t s_pat_armed[]    = { {1,90}, {0,310}  };              /* 探测中: ~2.5Hz 中闪 */
static const led_step_t s_pat_prog[]     = { {1,30}, {0,30}   };              /* 烧录中: ~16Hz 快闪 (勿拔) */
static const led_step_t s_pat_done[]     = { {1,1000} };                       /* 成功: 常亮 */
static const led_step_t s_pat_fail[]     = { {1,70},{0,70},{1,70},{0,70},
                                             {1,70},{0,990} };                 /* 失败: 三短闪循环 */
static const led_step_t s_pat_seen[]     = { {1,70},{0,70},{1,70},{0,590} };  /* 已识别目标: 双闪 */

static const led_step_t *s_led_pat    = 0;
static uint8_t           s_led_len    = 0;
static uint8_t           s_led_idx    = 0;
static uint16_t          s_led_remain = 0;

static void led_apply(const led_step_t *p)
{
    if (p->on) LED_ON(); else LED_OFF();
}

void LED_SetState(uint8_t state)
{
    const led_step_t *p = s_pat_no_image;
    uint8_t n = 2;
    switch (state) {
        case 1u: p = s_pat_armed;    n = 2; break;
        case 2u: p = s_pat_prog;     n = 2; break;
        case 3u: p = s_pat_done;     n = 1; break;
        case 4u: p = s_pat_fail;     n = 6; break;
        case 5u: p = s_pat_seen;     n = 4; break;
        case 0u:
        default: p = s_pat_no_image; n = 2; break;
    }
    __disable_irq();
    s_led_pat    = p;
    s_led_len    = n;
    s_led_idx    = 0;
    s_led_remain = p[0].ms;
    __enable_irq();
    led_apply(p);
}

static void LED_Tick(void)   /* 由 SysTick_Handler 每 1ms 调用一次 */
{
    const led_step_t *p;
    if (!s_led_pat || !s_led_len) return;
    if (s_led_remain == 0u) return;
    if (--s_led_remain != 0u) return;
    if (++s_led_idx >= s_led_len) s_led_idx = 0u;   /* 循环; done 仅 1 步 -> 保持常亮 */
    p = &s_led_pat[s_led_idx];
    s_led_remain = p->ms;
    led_apply(p);
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
    LED_Tick();                 /* LED 闪烁节奏 (1ms), 主循环被烧录阻塞也照闪 */
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
