/**
 * @brief 用于连接裸机和UCOS的衔接
 * @data 11 August 2016
 * @Author CRJ  WeChat：TC4300 QQ：43058655
 */
 
 
#include "ForUCOSByCRJ.H"
#include "ClockConfigurationByCRJ.h"
#include "includes.h"					//ucos 使用	 

/**
 * @brief  特别注意：SystemTick的时钟源是：AHB时钟/8.所以下面表示系统的节拍就是:OSCfg_TickRate_Hz
 */
void SysTick_Init()
{
	SystemCoreClockUpdate();
	SysTick->CTRL|=SysTick_CTRL_TICKINT_Msk;   	//开启SYSTICK中断
	SysTick->LOAD = SystemCoreClock/(8*OSCfg_TickRate_Hz);
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk;   	//开启SYSTICK    
}								    

void SysTick_Handler(void)
{	
	if(OSRunning==1)						//OS开始跑了,才执行正常的调度处理
	{
		OSIntEnter();							//进入中断，
		OSTimeTick();       			//调用ucos的时钟服务程序               
		OSIntExit();       	 			//触发任务切换软中断
	}
}

/**
 * @brief 系统中断分组设置化,THUMB指令不支持汇编内联,采用如下方法实现执行汇编指令WFI 
 */

void WFI_SET(void)
{
	__ASM volatile("wfi");		  
}
//关闭所有中断
void INTX_DISABLE(void)
{		  
	__ASM volatile("cpsid i");
}
//开启所有中断
void INTX_ENABLE(void)
{
	__ASM volatile("cpsie i");		  
}
//设置栈顶地址
//addr:栈顶地址
__asm void MSR_MSP(u32 addr) 
{
    MSR MSP, r0 			//set Main Stack value
    BX r14
}
