

#ifndef __CLOCKCONFIGURATIONBYCRJ_H__
#define __CLOCKCONFIGURATIONBYCRJ_H__

#include "stm32f10x.h"

extern u32 SystemCoreClock,AHBPCLK,APB1PCLK,APB2PCLK;
void SystemInit ( void );
// 时钟配置在正常范围，返回1，否则返回0。建议程序应该加入防错处理：
// ： 一定卡住，提示错误
// ： 利用可显示信息发送诸如：Error 905,Clock not in range!
u8 SystemCoreClockUpdate ( void );  


#endif

