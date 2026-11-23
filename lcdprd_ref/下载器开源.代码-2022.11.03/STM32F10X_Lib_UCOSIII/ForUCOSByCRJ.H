/**
 * @brief 
 * @Author WeChat：TC4300 QQ：43058655
 */

#ifndef __FORUCOSBYCRJ_H
#define __FORUCOSBYCRJ_H	
#include "stm32f10x.h"
#include "os_cfg_app.h"


void SysTick_Init( void );

void WFI_SET(void);	  	//执行WFI指令
void INTX_DISABLE(void);//关闭所有中断
void INTX_ENABLE(void);	//开启所有中断
void MSR_MSP(u32 addr);	//设置堆栈地址

#endif
