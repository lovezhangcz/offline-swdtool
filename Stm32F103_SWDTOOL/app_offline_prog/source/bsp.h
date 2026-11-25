#ifndef __BSP_H
#define __BSP_H

#include <stdint.h>
#include "stm32f10x.h"

void     BSP_Init(void);
void     BSP_PinConfig(GPIO_TypeDef *port, uint32_t pin, uint32_t cnf_mode);
uint32_t BSP_Millis(void);
void     BSP_DelayMs(uint32_t ms);

/* 独立看门狗: LSI 40kHz / 64 分频 -> 1.6ms 每拍, 重装 2000 -> 约 3.2s 超时 */
void     WDT_Init(void);
void     WDT_Feed(void);

#endif
