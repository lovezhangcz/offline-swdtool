#if defined(BUILD_BOOT)
/**
  * @file  boot_main.c
  * @brief USB CDC Bootloader 主程序
  *
  *   LED (PB12 低电平亮):
  *     等待主机 / 可升级 : 5Hz 快闪
  *     擦除/写入中      : 常亮
  *     无有效应用 / 连续跳转失败达到上限 : 1Hz 慢闪 (常驻 boot)
  */
#include "board.h"
#include "bsp.h"
#include "usb_cdc.h"
#include "boot.h"

/* 断开 D+ 上拉, 让主机看到一次"拔插", 保证跳转后重新枚举 */
static void usb_disconnect(void)
{
#ifdef USB_PULLUP_PORT
  #if USB_PULLUP_ACTIVE
    USB_PULLUP_PORT->BRR  = (1u << USB_PULLUP_PIN);
  #else
    USB_PULLUP_PORT->BSRR = (1u << USB_PULLUP_PIN);
  #endif
#endif
}

/* 连续"跳转到应用却没等到应用确认"的次数, 存备份寄存器 (复位不丢) */
static uint32_t s_try;

uint8_t Boot_TakeForceFlag(void)
{
    uint8_t forced;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;

    forced = (BKP_REG_FORCE == BOOT_FORCE_MAGIC) ? 1u : 0u;
    if (forced) BKP_REG_FORCE = 0;         /* 只生效一次 */

    s_try = BKP_REG_BOOTTRY;
    return forced;
}

uint8_t Boot_TooManyRetries(void)
{
    return (s_try >= BOOT_TRY_MAX) ? 1u : 0u;
}

void Boot_ArmRetry(void)
{
    if (s_try < 0xFFFFu) s_try++;
    BKP_REG_BOOTTRY = (uint16_t)s_try;
}

void Boot_ClearRetry(void)
{
    s_try = 0;
    BKP_REG_BOOTTRY = 0;
}

void Boot_JumpToApp(void)
{
    uint32_t sp;
    uint32_t pc;
    uint8_t i;

    /* 双保险: 跳转函数自己再校验一次, 即使调用方漏判也绝不跳进空片/无效应用 */
    if (!Boot_AppValid()) return;

    sp = *(volatile uint32_t *)APP_BASE;
    pc = *(volatile uint32_t *)(APP_BASE + 4u);

    /* 栈顶必须落在 SRAM(20KB, 0x20000000..0x20005000), 复位向量必须落在应用区 */
    if ((sp & 0x2FFE0000u) != 0x20000000u || sp > 0x20005000u) return;
    if (pc < APP_BASE || pc >= APP_BASE + APP_SIZE) return;
    /* 向量表里的复位向量带 Thumb 位 (LSB=1), 必须校验"是"奇数地址;
       之前写成 if (pc & 1u) return 会把所有正常应用都拒掉, 永远跳不过去 */
    if ((pc & 1u) == 0u) return;

    usb_disconnect();
    BSP_DelayMs(120);

    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    for (i = 0; i < 8u; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }
    SCB->VTOR = APP_BASE;
    __set_MSP(sp);
    __enable_irq();                 /* 应用继承 boot 关掉的中断, 必须放开 */
    ((void (*)(void))pc)();
}

int main(void)
{
    uint8_t app_ok, forced, stay;
    uint32_t t0;

    BSP_Init();
    USB_CDC_Init();
    Boot_Init();

    forced = Boot_TakeForceFlag();
    app_ok = Boot_AppValid();

    if (!app_ok) Boot_ClearRetry();         /* 无有效应用: 计数无意义 */
    Boot_SendBanner(app_ok, (uint8_t)s_try);
    t0 = BSP_Millis();

    for (;;) {
        Boot_Poll();
        USB_CDC_Poll();

        stay = (!app_ok || Boot_TooManyRetries()) ? 1u : 0u;

        if (Boot_Busy()) {
            LED_ON();
        } else if (stay) {
            if ((BSP_Millis() % 1000u) < 500u) LED_ON(); else LED_OFF();
        } else {
            if ((BSP_Millis() % 200u) < 100u) LED_ON(); else LED_OFF();
        }

        if (!app_ok) continue;                  /* 无应用: 常驻 boot */
        if (Boot_TooManyRetries()) {            /* 连续跳转失败达到上限: 驻留 boot 供升级/排查 */
            Boot_ClearRetry();                  /* 本次上电不再跳, 下次上电重新尝试 */
            app_ok = 0;
            continue;
        }
        if (Boot_HostSeen()) continue;          /* 主机已联系: 常驻 boot */

        if ((BSP_Millis() - t0) >
            (forced ? BOOT_FORCED_WAIT_MS :
                       ((s_try > 0u) ? BOOT_RETRY_WAIT_MS : BOOT_WAIT_MS))) {
            Boot_ArmRetry();                    /* 先记账: 应用若跑不起来, 下次复位计数+1 */
            Boot_JumpToApp();
            /* 只有跳转前检查失败才会返回: 退避后重试, 不空转 */
            BSP_DelayMs(1000);
            t0 = BSP_Millis();
        }
    }
}
#endif
