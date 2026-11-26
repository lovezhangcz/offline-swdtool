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

/* ---- LED 状态指示 (SysTick 中断驱动, 主循环被烧录阻塞时也持续闪烁) ----
 * 状态码与 main.c 的 ST_* 保持一致:
 *   0 = 无镜像      (2s 一短闪)
 *   1 = 待命/探测中 (~2.5Hz 中闪)
 *   2 = 烧录中      (~16Hz 快闪, 提醒操作者勿拔)
 *   3 = 成功        (常亮)
 *   4 = 失败        (三短闪循环)
 *   5 = 已识别目标  (双闪, 稳定电源中, 勿拔) */
void     LED_SetState(uint8_t state);

#endif
