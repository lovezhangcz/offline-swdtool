/**
  * @file  main.c
  * @brief GD32F103C8T6 最简测试工程 - PC13 LED 1Hz 闪烁
  *
  *   硬件: PC13 接 LED (低电平点亮, 常见 BluePill 板)
  *   时钟: 内部 8MHz RC, 不超频
  */

#include "stm32f10x.h"

/* 软件延时 (约 ms 级, 8MHz 内部时钟) */
static void delay_ms(uint32_t ms)
{
    /* 简单循环延时, 大约 8MHz 下 1ms ~ 1000 循环 */
    for (uint32_t i = 0; i < ms; i++) {
        for (uint32_t j = 0; j < 800; j++) {
            __NOP();
        }
    }
}

int main(void)
{
    /* 使能 GPIOC 时钟 (APB2) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* PC13 配置: 通用推挽输出, 2MHz
     * CNF13[1:0] = 00 (推挽输出), MODE13[1:0] = 10 (2MHz)
     * GPIOC->CRH bit 20-23 对应 PC13 */
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_1;   /* MODE13 = 10 (2MHz) */
                                    /* CNF13 = 00 (推挽输出) */

    for (;;) {
        /* PC13 输出低电平 -> LED 亮 */
        GPIOC->BRR = GPIO_BRR_BR13;
        delay_ms(5000);

        /* PC13 输出高电平 -> LED 灭 */
        GPIOC->BSRR = GPIO_BSRR_BS13;
        delay_ms(5000);
    }
}
