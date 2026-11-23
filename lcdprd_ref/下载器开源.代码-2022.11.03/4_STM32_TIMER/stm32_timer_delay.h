/**
 *@brief 一定要建立一个非常优秀的库
 *
 */


#ifndef __STM32_TIMER_DELAY_H
#define __STM32_TIMER_DELAY_H
#include "stm32f10x.h"

void TimeDelayOfSoftAtMs( u32 xms );

extern void timerHardDelyaInit( void );
extern void System_Delay_us( u16 xus );
extern void System_Delay_ms( u32 xms );


#endif





