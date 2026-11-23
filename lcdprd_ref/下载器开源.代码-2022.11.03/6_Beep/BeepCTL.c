/**
 * @brief 蜂鸣器控制，用PWM输出控制蜂鸣器的声音，当前的要求，显然是响几声和频率。然后自己控制！
 * 显然，操作系统是可以达到目的，但是本次为了简单，使用内部一个定时器和一个PWM定时器，减少和操作系统的关系！
 * 应该的操作方式是：定时器倒计时定时
 */
 
#include "BeepCTL.h"


/**
 * @brief 
 */

u8 BeepTimerConut = 0;  // 默认声音次数
u8 BeepEnable = 0;  // 

void BeepInit( void )
{
	
	{ // 定时器使用基本定时器6！根据分析，使用的是：72000000MHZ的时钟输入，要求设置时基为：1ms（每100个计数器）
		{
			RCC->APB1RSTR.TIM6Reset_RW = ENABLE;
			RCC->APB1RSTR.TIM6Reset_RW = DISABLE;     // 复位
			RCC->APB1ENR.TIM6ClockEnable_RW = ENABLE; // 使能
			NVIC_EnableIRQ(TIM6_IRQn);
			TIM6->PSC.PrescaleValue_RW = 719; // 则输入时钟为7200000/(719+1) = 1Mhz。计数100个为1ms时间
			{
				// 一定在这里先初始化一次,用来解决第一次给定时器上电，然后使能定时器就会产生SR中更新事件的问题！
				{
					TIM6->CR1.UpdateRequestSource_RW = 1; 
					TIM6->EGR.UpdateGeneration_W = ENABLE;
				}
				TIM6->CR1.AutoReloadPreloadEnable_RW = DISABLE;  // 一定要是DISABLE,因为写ARR事件是在后面,之前的算是初始值
				TIM6->CR1.OnePulseMode_RW = DISABLE;
				TIM6->CR1.UpdateRequestSource_RW = 0;  // 计数器溢出，设置UG位都可产生更新事件
				TIM6->CR1.UpdateDisable_RW = 0;        // 事件更新:溢出+设置UG位
				TIM6->DIER.UpdateInterruptEnable_RW = ENABLE;
			}
		}
		if( isHIDVersion == 0 )  // 只存在非LED版本与LED版本
		{
			{ // 通用定时器，36M时钟，
				{ // BEEP引脚(PB0)初始化，使用PWM信号，控制蜂鸣器的运行！
					RCC->APB2ENR.IOPBClockEnable_RW = ENABLE;
					GPIOB->CRL.InOutMode0_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;           // 
					GPIOB->CRL.PinConfig0_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;   // 先设置为普通IO。
					GPIOB->ODR.OutputData0_RW = 0;   // 内部下拉，否则引脚MIC通电，电流太大（50-60MA）
				}
				RCC->APB1RSTR.TIM3Reset_RW = ENABLE;
				RCC->APB1RSTR.TIM3Reset_RW = DISABLE;     // 复位
				RCC->APB1ENR.TIM3ClockEnable_RW = ENABLE; // 使能
				{// 下面是为了解决只要使用定时器就进入中断的方法!先确定PSC,再CR1中的URS为1,然后就是EGR中UG更新,
					TIM3->PSC.PrescaleValue_RW = 719;  // 1MHZ的时钟输入
					TIM3->CR1.UpdateRequestSource_RW = 1;  // 设置UG位不产生更新事件
					TIM3->EGR.UpdateGeneration_W = ENABLE; // (一定要注意这个语句的位置)一定在这里先初始化一次,用来解决第一次给定时器上电，然后使能定时器就会产生SR中更新事件的问题！
					{// 公用部分
						// 频率表：越大频率越小，最后使用
						TIM3->ARR.AutoReloadValue_RW = 55; // 频率大约2700HZ
						TIM3->CR1.Direction_RW = 0; // 向上计数模式(默认值)
						TIM3->CR1.Direction_RW = 0; //向上计数
						TIM3->CR2.All = 0; // 默认值
						TIM3->SMCR.All = 0; // 默认值
						TIM3->DIER.All = 0; // 默认值
					}
					{ // 通道3
						TIM3->Output_CCMR2.OutputCompareMode3_RW = _0b0111;
						TIM3->Output_CCMR2.OutputComparePreloadEnable3_RW = ENABLE;
						TIM3->CCER.CaptureCompareOutputPolarity3_RW = 0; // 高电平有效(默认值)
						TIM3->CCER.CaptureCompareOutputEnable3_RW = ENABLE; // 使能输出
						TIM3->CCR3.CaptureCompareValue_RW = TIM3->ARR.AutoReloadValue_RW/2;  // 占空比50%
					}
				}
			}
		}
		else
		{ // PWM只能使用定时器3！
			{ // 通用定时器，36M时钟，
				{ // BEEP引脚(PB0)初始化，使用PWM信号，控制蜂鸣器的运行！
					RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
					GPIOA->CRL.InOutMode2_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;           // 
					GPIOA->CRL.PinConfig2_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;   // 先设置为普通IO。
					GPIOA->ODR.OutputData2_RW = 0;   // 内部下拉，否则引脚MIC通电，电流太大（50-60MA）
				}
				RCC->APB1RSTR.TIM5Reset_RW = ENABLE;
				RCC->APB1RSTR.TIM5Reset_RW = DISABLE;     // 复位
				RCC->APB1ENR.TIM5ClockEnable_RW = ENABLE; // 使能
				{// 下面是为了解决只要使用定时器就进入中断的方法!先确定PSC,再CR1中的URS为1,然后就是EGR中UG更新,
					TIM5->PSC.PrescaleValue_RW = 36-1;  // 1MHZ的时钟输入
					TIM5->CR1.UpdateRequestSource_RW = 1;  // 设置UG位不产生更新事件
					TIM5->EGR.UpdateGeneration_W = ENABLE; // (一定要注意这个语句的位置)一定在这里先初始化一次,用来解决第一次给定时器上电，然后使能定时器就会产生SR中更新事件的问题！
					{// 公用部分
						// 频率表：越大频率越小，最后使用
						TIM5->ARR.AutoReloadValue_RW = 450; // 发现使用500的时候，其蜂鸣器响度比较大！
						TIM5->CR1.Direction_RW = 0; // 向上计数模式(默认值)
						TIM5->CR1.Direction_RW = 0; //向上计数
						TIM5->CR2.All = 0; // 默认值
						TIM5->SMCR.All = 0; // 默认值
						TIM5->DIER.All = 0; // 默认值
					}
					{ // 通道3
						TIM5->Output_CCMR2.OutputCompareMode3_RW = _0b0111;  // 巧了，都是3
						TIM5->Output_CCMR2.OutputComparePreloadEnable3_RW = ENABLE;
						TIM5->CCER.CaptureCompareOutputPolarity3_RW = 0; // 高电平有效(默认值)
						TIM5->CCER.CaptureCompareOutputEnable3_RW = ENABLE; // 使能输出
						TIM5->CCR3.CaptureCompareValue_RW = TIM5->ARR.AutoReloadValue_RW/2;  // 占空比50%
					}
					{  // HID的LCD版本，也使用了TIM5，所以TIM5需要共用！
						{  // 测试LCD的灯光，日后可以用这个来调光！！减少功耗！
							 // 
							if( HARDWARE_VERSION == 1 ) 
							{ // LCD版本，使用PWM，注意LCD的老版本的PA3不使用
								RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
								GPIOA->CRL.InOutMode3_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
								GPIOA->CRL.PinConfig3_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
								GPIOA->ODR.OutputData3_RW = 1;
								{ // 使能定时器，暂时设定使用：10KHZ
									TIM5->Output_CCMR2.OutputComparePreloadEnable4_RW = 1;
									TIM5->Output_CCMR2.OutputCompareMode4_RW = _0b0110; // 向上计数时CNT<CCR为有效电平
									TIM5->CCER.CaptureCompareOutputEnable4_RW = 1;
									if( L_LIGNT_LV > 9 )  // 防止首次升级-黑屏
									{
										TIM5->CCR4.CaptureCompareValue_RW = (TIM5->ARR.AutoReloadValue_RW/10)*(9+1); // 
									}
									else
									{
										TIM5->CCR4.CaptureCompareValue_RW = (TIM5->ARR.AutoReloadValue_RW/10)*(L_LIGNT_LV+1); // 
									}
								}
							}
						}
					}
					TIM5->CR1.CountEnable_RW = 1;
				}
			}
		}
	}
	
}
/**
 * @brief，一个是次数，一个是时间间隔，
 */
void BeepCtrol( u8 outTimers,u16 timeInterval_ms )
{
	if( L_BEEP_ENABLE == 0x55AA )
	{
		return;
	}
	TIM6->CR1.CountEnable_RW = DISABLE;
	TIM6->CNT.CurrentCountValue_RW = 0;
	BeepTimerConut = outTimers*2;
	if( outTimers == 0 )
		return;
	if( timeInterval_ms > 655 )
		return;
	else
	{
		if( isHIDVersion == 0 )
		{
			GPIOB->CRL.PinConfig0_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;   // 复用输出
		}
		else
		{
			GPIOA->CRL.PinConfig2_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
		}
		TIM6->ARR.AutoReloadValue_RW = timeInterval_ms*100 - 1;
		TIM6->CR1.CountEnable_RW = ENABLE;
	}
}


void TIM6_IRQHandler()
{
	TIM6->SR.UpdateInterruptFlag_RCW0 = 0;
	if( BeepTimerConut > 0 )
	{
		if( isHIDVersion == 0 )
		{
			TIM3->CR1.CountEnable_RW = ~TIM3->CR1.CountEnable_RW;
		}
		else
		{
			if( TIM5->CCR3.CaptureCompareValue_RW > 0 )
				TIM5->CCR3.CaptureCompareValue_RW = 0;
			else
				TIM5->CCR3.CaptureCompareValue_RW = TIM5->ARR.AutoReloadValue_RW/2;
//			TIM5->CR1.CountEnable_RW = ~TIM5->CR1.CountEnable_RW;
		}
	}
	else
	{
		if( isHIDVersion == 0 )
		{
			TIM3->CR1.CountEnable_RW = DISABLE;  // 关闭PWM
		}
		else
		{
			TIM5->CCR3.CaptureCompareValue_RW = 0;
//			TIM5->CR1.CountEnable_RW = DISABLE;
		}
		TIM6->CR1.CountEnable_RW = DISABLE;  // 关闭定时器
		TIM6->CNT.CurrentCountValue_RW = 0;
		if( isHIDVersion == 0 )
		{
			GPIOB->CRL.PinConfig0_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;   // 设置为普通IO。防止电流太大！
		}
		else
		{
			GPIOA->CRL.PinConfig2_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL; 
		}
	}
	BeepTimerConut--;
}


