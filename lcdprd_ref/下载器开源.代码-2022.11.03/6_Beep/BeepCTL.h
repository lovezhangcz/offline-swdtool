/**
 * @brief 用于处理所有的机械按键，使用需要为定时扫描按键程序。程序本身不涉及按键处理，只涉及状态。
 */



#ifndef __BEEP_CTL_H
#define __BEEP_CTL_H

#include "stm32f10x.h"

void BeepInit( void );
void BeepCtrol( u8 outTimers,u16 timeInterval_ms );

#endif


