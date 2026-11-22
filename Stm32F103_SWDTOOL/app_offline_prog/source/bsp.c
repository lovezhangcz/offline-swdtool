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
