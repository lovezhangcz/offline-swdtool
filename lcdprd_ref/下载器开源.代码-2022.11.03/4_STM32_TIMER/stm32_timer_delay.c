/**
* @brief  主要的操作是：STM32定时器，为是延时函数来制作的
* @Author CRJ QQ:43058655  WeChat：TC4300
* @Data   2017.12.12
*/

#include "stm32_timer_delay.h"

/**
 * \b 软件延时函数，注意在时钟72MHZ下。
 * \b 注意，这个时间不够准确。精度：1%。
 */
void TimeDelayOfSoftAtMs( u32 xms )
{
	// 对于下面的运算每M时间的计数值为77个。
	SystemCoreClockUpdate();  // 为了程序的稳定性，更新下时钟
	vu16 countNum = 77*(SystemCoreClock / 1000000); 
	for( volatile u32 temp1=0; temp1<xms ; temp1++ )
	{
		for( volatile u32 temp2 = 0; temp2<= countNum;temp2++ );  // 大约1MS
	}
}


/**
 *  使用Timer6生产定时，注意最大与最小时间
 *  一个特别重要的警告：定时器的分频值如果通过寄存器改变，则生效的时间是改变产生一次更新事件，也就是滞后一个更新！
 */
void timerHardDelyaInit( void )
{
	RCC->APB1ENR.TIM7ClockEnable_RW = ENABLE;
	TIM7->CR1.OnePulseMode_RW = ENABLE;  // 使能单次模式和不使能单次模式的区别是：在发生更新事件的时候CNT继续不继续运行！！！
																					// 而且单次模式下，会停止计数。
	// 所以，也就是：不使能单次模式，会影响下次的时间
	// TIM7-7发生更新事件后：CNT的值为0。
	System_Delay_us(10);
}

/**
 *  下面的函数比较专用，如果改了条件！
    APB1的时钟，当前配置下为72MHZ的时钟！
		T = (PSC+1)*(ARLOD+1) / F
		注意：最大值为：65556us，也就是大约65ms！最小值为2！
 */

void System_Delay_us( u16 xus )
{
	if( xus >=2 )
	{
		TIM7->PSC.PrescaleValue_RW = 71;   // 单位为us
		TIM7->ARR.AutoReloadValue_RW = xus-1;  // 
		TIM7->CNT.CurrentCountValue_RW = 0;  // 从0开始
		TIM7->CR1.CountEnable_RW = ENABLE;
		while(TIM7->SR.UpdateInterruptFlag_RCW0 == 0);
		TIM7->SR.UpdateInterruptFlag_RCW0 = 0;
	}
}

/**
 * @brief 之所以用循环的方式，是因为更新PSC需要一个循环，也就是通过切换PSC的值，于是改了ARR就会导致按照上一个的PSC来做，就出现了错误！！
 */
void System_Delay_ms( u32 xms )
{
	for( u32 i = 0;i<xms;i++ )
	{
		System_Delay_us(1000);
	}
}




