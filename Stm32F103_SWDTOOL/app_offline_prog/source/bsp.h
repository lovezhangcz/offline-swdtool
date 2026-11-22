#ifndef __BSP_H
#define __BSP_H

#include <stdint.h>
#include "stm32f10x.h"

void     BSP_Init(void);
void     BSP_PinConfig(GPIO_TypeDef *port, uint32_t pin, uint32_t cnf_mode);
uint32_t BSP_Millis(void);
void     BSP_DelayMs(uint32_t ms);

#endif
