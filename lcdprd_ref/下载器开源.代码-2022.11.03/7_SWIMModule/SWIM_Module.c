/**
* @brief  用IO模拟SWIM，离线下载器的一个收入点：STM8系列，所以这个是一定要加上去的。会是日后的一个收入点！
* @Author 
* @Data   2018.03.07  - 2018.10.20：直接移植别人的代码，但是别人的代码用的库，所以移植起来来特别小心。
* \Attention SWIM和SWD不尽相同，研究仍然是需要一段时间的，但是也务必把这个事情做好。
							SWIM是单线双向的调试接口。操作越来可能会有一点麻烦。但是到最终，也只是时间问题。当然：要记住一点：不可操之过急！
*/

#include "SWIM_Module.h"


u32 ReadCount = 1024;  // 这里用作一个测试！测试下载一个256字节的数据！！
u8 fileDataForStm8[1024];
u8 forPreventCrash = 0;  // 有一个地址，可能会一直循环下去，所以设置这个位，用于重启相关的操作。
u8 keyTmp[2];  // 发现局部变量是调试模式下，似乎值有问题。
union u32Tou8
{
 	vu32 ulongx;
	vu16 u16x[2];
	vu8  u8x[4];
};

static uint8_t SWIM_Inited;  // 用来表明是否初始化成功的
static u16 SWIM_PULSE_0;
static u16 SWIM_PULSE_1;
static u16 SWIM_PULSE_Threshold;
static u16 SWIM_DMA_IN_Buffer[12];
static u16 SWIM_DMA_OUT_Buffer[12];
static uint16_t SWIM_clock_div = 0;
static uint8_t ReadBuff[256];
static uint8_t WriteBuff[256];
static uint8_t cmdrtv;


enum{ lowBlockSize64 = 64,HighBolckSize128 = 128 } STM8_blockSize = lowBlockSize64;
enum{ AHF_S_Series = 0 ,ATL_L_Series = 1 }STM8_regType = AHF_S_Series; 
enum{ 
			opSeries_0 = 0,
			opSeries_1 = 1,
			opSeries_2 = 2,
			opSeries_3 = 3,
			opSeries_4 = 4,
			opSeries_5 = 5,
			opSeries_6 = 6,
			opSeries_7 = 7,
			opSeries_8 = 8,
			opSeries_9 = 9,
		} STM8_optionByteType;  // 通过这个表，来建立选项字节！

MCU_INFO *PMcu = &STM8_StoreIfo[0];
		// 关于一个指针的错误:如果上面定义的为:MCU_INFO *PMcu; 或者:MCU_INFO *PMcu =0;  则会出现HardFaultError,因为定义的指针,需要为指针变量分配内存空间,否则会直接错!!
		
//u8 op_stm8sL[11]  = {0x00,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,};
//u8 op_stm8sMH[17] = {0x00,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,};


static u8 stm8WriteFlashAbyte( u32 address,u8 type,u8 data );
static u8 stm8WriteRegAbyte( u32 address,u8 data );

// 下面是人杰增加，因为使用的实在是太多了。
// 这个函数是清除所有的DMA配置！

static void DMA_DeInit_CRJ( DMA_Channel_TypeDef* DMAy_Channelx )
{
	DMAy_Channelx->CCR.ChannelEnable_RW = DISABLE;
	DMAy_Channelx->CCR.All = 0;
	DMAy_Channelx->CNDTR.All = 0;
	DMAy_Channelx->CPAR.All = 0;
	DMAy_Channelx->CMAR.All = 0;
	
	
	if (DMAy_Channelx == DMA1_Channel1)
  {
		DMA1->IFCR.GlobalInterruptClear1_RW = ENABLE;
		DMA1->IFCR.HalfTransferClear1_RW = ENABLE;
		DMA1->IFCR.TransferCompleteClear1_RW = ENABLE;
		DMA1->IFCR.TransferErrorClear1_RW = ENABLE;
  }
  else if (DMAy_Channelx == DMA1_Channel2)
  {
		DMA1->IFCR.GlobalInterruptClear2_RW = ENABLE;
		DMA1->IFCR.HalfTransferClear2_RW = ENABLE;
		DMA1->IFCR.TransferCompleteClear2_RW = ENABLE;
		DMA1->IFCR.TransferErrorClear2_RW = ENABLE;
  }
  else if (DMAy_Channelx == DMA1_Channel3)
  {
		DMA1->IFCR.GlobalInterruptClear3_RW = ENABLE;
		DMA1->IFCR.HalfTransferClear3_RW = ENABLE;
		DMA1->IFCR.TransferCompleteClear3_RW = ENABLE;
		DMA1->IFCR.TransferErrorClear3_RW = ENABLE;
  }
  else if (DMAy_Channelx == DMA1_Channel4)
  {
		DMA1->IFCR.GlobalInterruptClear4_RW = ENABLE;
		DMA1->IFCR.HalfTransferClear4_RW = ENABLE;
		DMA1->IFCR.TransferCompleteClear4_RW = ENABLE;
		DMA1->IFCR.TransferErrorClear4_RW = ENABLE;
  }
  else if (DMAy_Channelx == DMA1_Channel5)
  {
		DMA1->IFCR.GlobalInterruptClear5_RW = ENABLE;
		DMA1->IFCR.HalfTransferClear5_RW = ENABLE;
		DMA1->IFCR.TransferCompleteClear5_RW = ENABLE;
		DMA1->IFCR.TransferErrorClear5_RW = ENABLE;
  }
  else if (DMAy_Channelx == DMA1_Channel6)
  {
		DMA1->IFCR.GlobalInterruptClear6_RW = ENABLE;
		DMA1->IFCR.HalfTransferClear6_RW = ENABLE;
		DMA1->IFCR.TransferCompleteClear6_RW = ENABLE;
		DMA1->IFCR.TransferErrorClear6_RW = ENABLE;
  }
  else if (DMAy_Channelx == DMA1_Channel7)
  {
		DMA1->IFCR.GlobalInterruptClear7_RW = ENABLE;
		DMA1->IFCR.HalfTransferClear7_RW = ENABLE;
		DMA1->IFCR.TransferCompleteClear7_RW = ENABLE;
		DMA1->IFCR.TransferErrorClear7_RW = ENABLE;
  }
  else if (DMAy_Channelx == DMA2_Channel1)
  {
		DMA2->IFCR.GlobalInterruptClear1_RW = ENABLE;
		DMA2->IFCR.HalfTransferClear1_RW = ENABLE;
		DMA2->IFCR.TransferCompleteClear1_RW = ENABLE;
		DMA2->IFCR.TransferErrorClear1_RW = ENABLE;
  }
  else if (DMAy_Channelx == DMA2_Channel2)
  {
		DMA2->IFCR.GlobalInterruptClear2_RW = ENABLE;
		DMA2->IFCR.HalfTransferClear2_RW = ENABLE;
		DMA2->IFCR.TransferCompleteClear2_RW = ENABLE;
		DMA2->IFCR.TransferErrorClear2_RW = ENABLE;
  }
  else if (DMAy_Channelx == DMA2_Channel3)
  {
		DMA2->IFCR.GlobalInterruptClear3_RW = ENABLE;
		DMA2->IFCR.HalfTransferClear3_RW = ENABLE;
		DMA2->IFCR.TransferCompleteClear3_RW = ENABLE;
		DMA2->IFCR.TransferErrorClear3_RW = ENABLE;
  }
  else if (DMAy_Channelx == DMA2_Channel4)
  {
		DMA2->IFCR.GlobalInterruptClear4_RW = ENABLE;
		DMA2->IFCR.HalfTransferClear4_RW = ENABLE;
		DMA2->IFCR.TransferCompleteClear4_RW = ENABLE;
		DMA2->IFCR.TransferErrorClear4_RW = ENABLE;
  }
  else
  { 
    if (DMAy_Channelx == DMA2_Channel5)
    {
			DMA2->IFCR.GlobalInterruptClear5_RW = ENABLE;
			DMA2->IFCR.HalfTransferClear5_RW = ENABLE;
			DMA2->IFCR.TransferCompleteClear5_RW = ENABLE;
			DMA2->IFCR.TransferErrorClear5_RW = ENABLE;
    }
  }
}

// 已验证2遍！
static void SYNCSWPWM_IN_TIMER_RISE_DMA_INIT( u32 numbers ,u32 address )
{
	SYNCSWPWM_IN_TIMER_RISE_DMA->CCR.All &= ~1;
	SYNCSWPWM_IN_TIMER_RISE_DMA->CNDTR.All = (numbers);
	SYNCSWPWM_IN_TIMER_RISE_DMA->CMAR.All = (uint32_t)(address);
	SYNCSWPWM_IN_TIMER_RISE_DMA->CCR.All |= 1;
}


static void SYNCSWPWM_IN_TIMER_RISE_DMA_WAIT( u32 dly )
{
	while((!(DMA1->ISR.All & ((uint32_t)0x02000000))) && --dly);  // 通道传输完成标志！ - DMAL-7
	DMA1->IFCR.All = ((uint32_t)0x02000000);
}

static void SYNCSWPWM_OUT_TIMER_DMA_INIT( u32 numbers ,u32 address )
{
	SYNCSWPWM_OUT_TIMER->EGR.All = ((uint16_t)0x0001);
	SYNCSWPWM_OUT_TIMER_DMA->CCR.All &= ~1;
	SYNCSWPWM_OUT_TIMER_DMA->CNDTR.All = (numbers);
	SYNCSWPWM_OUT_TIMER_DMA->CMAR.All = (uint32_t)(address);
	SYNCSWPWM_OUT_TIMER_DMA->CCR.All |= 1;
}


static u8 SYNCSWPWM_OUT_TIMER_DMA_WAIT()	
{
	u32 dly = 0xFFFF;   // 这个是自己加的，实际中可能需要小心处理！！主要就是为了防止死机！
	
	while((!(DMA1->ISR.All & ((uint32_t)0x00000020))) && --dly);  // DMA1-2  -- 注意：这个有时候会死机在这里，要特别小心！！
	DMA1->IFCR.All = ((uint32_t)0x00000020);
	if( dly == 0 )
	{
		return 1;
	}
	return 0;
}
	

// 这个是GPIO的引脚配置函数，而且是初始函数的引脚配置！
void SWIM_GPIO_Init(void)
{
	if( HARDWARE_VERSION == MINI_VERSION )
	{
	}
	else
	{
		{ // 初始化SWIM复位引脚PB4
			RCC->APB2ENR.IOPBClockEnable_RW = ENABLE;
			GPIOB->CRL.InOutMode4_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
			GPIOB->CRL.PinConfig4_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
			GPIOB->ODR.OutputData4_RW = 1;
		}
	}
	// 下面几个都是一样的。
		{  // PA8 - SWIM-OUT引脚
			RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
			GPIOA->CRH.InOutMode8_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
			GPIOA->CRH.PinConfig8_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_OPEN_DRAIN;
			GPIOA->ODR.OutputData8_RW = 1;
		}
		{ // 浮空输入 - PA0-PA1
			RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
			GPIOA->CRL.InOutMode0_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOA->CRL.PinConfig0_RW = GPIO_CR_PINCONFG_IN_FLOATING;
			
			GPIOA->CRL.InOutMode1_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOA->CRL.PinConfig1_RW = GPIO_CR_PINCONFG_IN_FLOATING;
		}
		{
			GPIOA->ODR.OutputData0_RW = 1;  // 浮空输入下这个是没有作用的。
			GPIOA->ODR.OutputData1_RW = 1;	// 浮空输入下这个是没有作用的。
		}
	
}



// 因为是使用PWM的，不知道下面几个函数的具体功能！

void SYNCSWPWM_PORT_OD_INIT(void)
{
	RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
	GPIOA->ODR.OutputData8_RW = 1;
	GPIOA->CRH.InOutMode8_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
	GPIOA->CRH.PinConfig8_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_OPEN_DRAIN;
}

// 根据发现，这个实际上没有被使用到！ -  当然参考代码的中也没有指明是P几口。
//void SYNCSWPWM_PORT_PP_INIT(void)
//{
//	GPIOA->ODR.OutputData8_RW = 1;
//	GPIOA->CRH.InOutMode8_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
//	GPIOA->CRH.PinConfig8_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
//}

/**
	* \Attention 个人觉得，可以开漏模式下，最好把输出引脚设置为1。但是为了尊重原来版本，暂时不改动！！
	*/
void SYNCSWPWM_PORT_ODPP_FINI(void)
{
//	GPIOA->ODR.OutputData8_RW = 1;
	GPIOA->CRH.InOutMode8_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
	GPIOA->CRH.PinConfig8_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_OPEN_DRAIN;
}


static void SWIM_Init()
{
	if (!SWIM_Inited)
	{
		SWIM_Inited = 1;
		{  // 下面这个比较重要了，一旦出错，非常严重。
			{
				RCC->AHBENR.DMA1ClockEnable_RW = ENABLE;
				RCC->APB2ENR.TIM1ClockEnable_RW = ENABLE;
			}
			
			{
				DMA_DeInit_CRJ(DMA1_Channel2);
			}
			{
				
			}
			{ // DMA的初始化
				DMA1_Channel2->CCR.All = 0x00001590;  // 控制命令
				DMA1_Channel2->CNDTR.All = 0;
				DMA1_Channel2->CPAR.All = (u32)&TIM1->CCR1;
				DMA1_Channel2->CMAR.All = 0;
			}
			{
				DMA1_Channel2->CCR.ChannelEnable_RW = ENABLE;  // 使能操作！
			}
			
			{  // OUT_TIM:TMI1
				
				TIM1->CR1.All = 0;
				TIM1->ARR.AutoReloadValue_RW = 0;
				TIM1->PSC.PrescaleValue_RW = 0;
				TIM1->RCR.All = 0;
				TIM1->EGR.All = 1;
			}
			{
				TIM1->CCER.All &= ~0x01;
				TIM1->CCER.All = 0;
				TIM1->CR2.All = 0;
				TIM1->Output_CCMR1.All = 0x60;
				TIM1->CCR1.CaptureCompareValue_RW = 0;
				TIM1->CCER.All = 0x03;
			}
			{
				TIM1->Output_CCMR1.All = 0x68; // 需要注意的是：在其中的0x60是后来读出来的一个数据。因为无法实际测试，所以要比较小心！
				TIM1->CR1.AutoReloadPreloadEnable_RW = ENABLE;
			}
			{
				TIM1->DIER.CaptureCompareDMARequestEnable1_RW = 1;
				TIM1->CR1.CountEnable_RW = 1;
				TIM1->BDTR.MainOutputEnable_RW = 1;
			}
		}
		SYNCSWPWM_PORT_OD_INIT();		//端口切换到第二功能输出口
	}
}


/**
	* @brief  SWIM操作的结束，恢复配置现场！！
	*/
static void SWIM_Fini()
{
	SYNCSWPWM_PORT_ODPP_FINI();
//	RCC->AHBENR.DMA1ClockEnable_RW = DISABLE;
//	RCC->AHBENR.DMA1ClockEnable_RW = ENABLE;
	{ // 也是只用到了一次
		{ // 复位TIM1
			RCC->APB2RSTR.TIM1Reset_RW = ENABLE;
			RCC->APB2RSTR.TIM1Reset_RW = DISABLE;
		}
		{ 
			RCC->APB2ENR.TIM1ClockEnable_RW = DISABLE;
		}
		{
			DMA_DeInit_CRJ(SYNCSWPWM_OUT_TIMER_DMA);
		}
		{
			RCC->AHBENR.DMA1ClockEnable_RW = DISABLE;
		}
	}
	{ 
		{
			RCC->APB1RSTR.TIM2Reset_RW = ENABLE;
			RCC->APB1RSTR.TIM2Reset_RW = DISABLE;
		}
		{
			RCC->APB1ENR.TIM2ClockEnable_RW = DISABLE;
		}
		{
			DMA_DeInit_CRJ(SYNCSWPWM_IN_TIMER_RISE_DMA);
		}
		{
			DMA_DeInit_CRJ(SYNCSWPWM_IN_TIMER_FALL_DMA);
		}
		{
			RCC->AHBENR.DMA1ClockEnable_RW = DISABLE;
		}
	}
	{
		
	}
	SWIM_Inited = 0;
}


/**
	* @brief  已测试过两遍！
	*/
static void SWIM_EnableClockInput(void)
{
	SWIM_clock_div = 0;
	{ // 又是一个最复杂的
		RCC->AHBENR.DMA1ClockEnable_RW = ENABLE;
		RCC->APB1ENR.TIM2ClockEnable_RW = ENABLE;
	}
	{
		DMA_DeInit_CRJ(SYNCSWPWM_IN_TIMER_RISE_DMA);
	}
	{
		SYNCSWPWM_IN_TIMER_RISE_DMA->CCR.All = 0x2580;
		SYNCSWPWM_IN_TIMER_RISE_DMA->CNDTR.All = 0;
		SYNCSWPWM_IN_TIMER_RISE_DMA->CPAR.All = (uint32_t)&(SYNCSWPWM_IN_TIMER->CCR2);
		SYNCSWPWM_IN_TIMER_RISE_DMA->CMAR.All = 0;
	}
	{
		SYNCSWPWM_IN_TIMER_RISE_DMA->CCR.ChannelEnable_RW = ENABLE;
	}
	{
		DMA_DeInit_CRJ(SYNCSWPWM_IN_TIMER_FALL_DMA);
	}
	{
		SYNCSWPWM_IN_TIMER_FALL_DMA->CCR.All = 0x2580;
		SYNCSWPWM_IN_TIMER_FALL_DMA->CNDTR.All = 0;
		SYNCSWPWM_IN_TIMER_FALL_DMA->CPAR.All = (uint32_t)&(SYNCSWPWM_IN_TIMER->CCR1);
		SYNCSWPWM_IN_TIMER_FALL_DMA->CMAR.All = 0;
	}
	{
		SYNCSWPWM_IN_TIMER_FALL_DMA->CCR.ChannelEnable_RW = ENABLE;
	}
	{ // TIM_PWMIConfig(SYNCSWPWM_IN_TIMER, &TIM_ICInitStructure);
		SYNCSWPWM_IN_TIMER->CCER.All &= (uint16_t)~((uint16_t)0x10);
		SYNCSWPWM_IN_TIMER->Input_CCMR1.All = 0x0200;
		SYNCSWPWM_IN_TIMER->CCER.All = 0x0010;
		
		SYNCSWPWM_IN_TIMER->Input_CCMR1.All &= (uint16_t)~((uint16_t)0x0C00);
		
		SYNCSWPWM_IN_TIMER->CCER.All &= (uint16_t)~((uint16_t)0x01);
		SYNCSWPWM_IN_TIMER->Input_CCMR1.All = 0x0201;
		SYNCSWPWM_IN_TIMER->CCER.All = 0x0013;
		
		SYNCSWPWM_IN_TIMER->Input_CCMR1.All &= (uint16_t)~((uint16_t)0x0C);
		
	}
	{
		SYNCSWPWM_IN_TIMER->SMCR.All = 0x50;
	}
	{
		SYNCSWPWM_IN_TIMER->SMCR.All &= (uint16_t)~((uint16_t)0x07);
		SYNCSWPWM_IN_TIMER->SMCR.All |= 0x04;
	}
	{
		SYNCSWPWM_IN_TIMER->SMCR.All &= (uint16_t)~((uint16_t)0x80);
		SYNCSWPWM_IN_TIMER->SMCR.All |= 0x80;
	}
	{
		SYNCSWPWM_IN_TIMER->DIER.All |= 0x400; 
	}
	{
		SYNCSWPWM_IN_TIMER->DIER.All |= 0x200; 
	}
	{
		SYNCSWPWM_IN_TIMER->PSC.PrescaleValue_RW = 0;
		SYNCSWPWM_IN_TIMER->EGR.All = 0x01;
	}
	{
		SYNCSWPWM_IN_TIMER->CR1.CountEnable_RW = ENABLE;
	}
}


/**
	* \Attention 返回值不为0，则表示出错！！需要重新下载！！
	*/
static uint8_t SWIM_EnterProgMode(void)
{
	uint8_t i;
	uint32_t dly;
	SYNCSWPWM_IN_TIMER_RISE_DMA_INIT(10, (u32)SWIM_DMA_IN_Buffer); 			//分配DMA存储空间
	
	SWIM_LOW();
	System_Delay_us(1000);
	for (i = 0; i < 4; i++)
	{
		SWIM_HIGH();
		System_Delay_us(500);
		SWIM_LOW();
		System_Delay_us(500);
	}
	for (i = 0; i < 4; i++)
	{
		SWIM_HIGH();
		System_Delay_us(250);
		SWIM_LOW();
		System_Delay_us(250);
	}
	SWIM_HIGH();
	dly = SWIM_MAX_DLY;
	SYNCSWPWM_IN_TIMER_RISE_DMA_WAIT(dly);
	if (!dly)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


//设置SWIM通讯时钟参数
//默认SWIM时钟为HSI/2=8MHz,周期=0.125us x 22拍 =2.75us
static uint8_t SWIM_SetClockParam(uint8_t mHz, uint8_t cnt0, uint8_t cnt1)		//cnt0和cnt1构成一个周期
{
	u32 sysCoreClock;
	{  // 
		if( isHIDVersion == 1 )
		{
			sysCoreClock = 72;
		}
		else
		{
			sysCoreClock = 64;
		}
	}
	
	uint16_t clock_div;
	
	if (SWIM_clock_div)
	{
		clock_div = SWIM_clock_div;
	}
	else
	{
		clock_div = sysCoreClock / mHz;			
		if ((sysCoreClock % mHz) >= (mHz / 2))	
		{
			clock_div++;
		}
		clock_div *= SWIM_SYNC_CYCLES;							//9x128=1152
	}

	SWIM_PULSE_0 = cnt0 * clock_div / SWIM_SYNC_CYCLES;
	if ((cnt0 * clock_div % SWIM_SYNC_CYCLES) >= SWIM_SYNC_CYCLES / 2)
	{
		SWIM_PULSE_0++;
	}
	SWIM_PULSE_1 = cnt1 * clock_div / SWIM_SYNC_CYCLES;
	if ((cnt1 * clock_div % SWIM_SYNC_CYCLES) >= SWIM_SYNC_CYCLES / 2)
	{
		SWIM_PULSE_1++;
	}
	SWIM_PULSE_Threshold = SWIM_PULSE_0 + SWIM_PULSE_1;

	{
		SYNCSWPWM_OUT_TIMER->ARR.AutoReloadValue_RW = (SWIM_PULSE_Threshold);\
		SYNCSWPWM_OUT_TIMER->EGR.All = ((uint16_t)0x0001);\
	}
	SWIM_PULSE_Threshold >>= 1;
	return 0;
}



static u8 SWIM_HW_Out(u8 cmd, u8 bitlen, u16 retry_cnt)
{
	int8_t i, p;
	u32 dly;
	u16 *ptr = &SWIM_DMA_OUT_Buffer[0];

retry:

	SYNCSWPWM_IN_TIMER_RISE_DMA_INIT(bitlen + 3, (u32)SWIM_DMA_IN_Buffer);

	*ptr++ = SWIM_PULSE_0;

	p = 0;
	for (i = bitlen - 1; i>=0; i--)
	{
		if ((cmd >> i) & 1)
		{
			*ptr++ = SWIM_PULSE_1;
			p++;
		}
		else
		{
			*ptr++ = SWIM_PULSE_0;
		}
	}
	// parity bit
	if (p & 1)
	{
		*ptr++ = SWIM_PULSE_1;
	}
	else
	{
		*ptr++ = SWIM_PULSE_0;
	}
	// wait for last waveform -- parity bit
	*ptr++ = 0;
	SYNCSWPWM_OUT_TIMER_DMA_INIT(bitlen + 3, (u32)SWIM_DMA_OUT_Buffer);
	cmdrtv = SYNCSWPWM_OUT_TIMER_DMA_WAIT();
	if( cmdrtv >0 )
	{
		return 2;
	}
	dly = SWIM_MAX_DLY;
	SYNCSWPWM_IN_TIMER_RISE_DMA_WAIT(dly);
	SYNCSWPWM_IN_TIMER_RISE_DMA_INIT(10, (u32)(SWIM_DMA_IN_Buffer + 1));

	if (!dly)
	{
		// timeout
		return 2;
	}
	else if (SWIM_DMA_IN_Buffer[bitlen + 2] > SWIM_PULSE_Threshold)		 //判断最后一个ACK应答是否为"1",小于半个周期的低电平在前
	{
		// nack	
		if (retry_cnt)
		{
			retry_cnt--;
			goto retry;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		return 0;
	}
}


static u8 SWIM_HW_In(u8* data, u8 bitlen)
{
	u8 ret = 0;
	u32 dly;

	dly = SWIM_MAX_DLY;
	SYNCSWPWM_IN_TIMER_RISE_DMA_WAIT(dly);									//先接收引导bit,目标到主机，这个位必须是1
	*data = 0;
	if (dly && (SWIM_DMA_IN_Buffer[1] < SWIM_PULSE_Threshold))				//如果=1,低电平时间小于4个脉冲
	{
		for (dly = 0; dly < 8; dly++)
		{
			if (SWIM_DMA_IN_Buffer[2 + dly] < SWIM_PULSE_Threshold)							
			{
				*data |= 1 << (7 - dly);
			}
		}
		SYNCSWPWM_IN_TIMER_RISE_DMA_INIT(11, (u32)SWIM_DMA_IN_Buffer);

		SWIM_DMA_OUT_Buffer[0] = SWIM_PULSE_1;
		SWIM_DMA_OUT_Buffer[1] = 0;
		SYNCSWPWM_OUT_TIMER_DMA_INIT(2, (u32)SWIM_DMA_OUT_Buffer);
		cmdrtv = SYNCSWPWM_OUT_TIMER_DMA_WAIT();
		if( cmdrtv >0 )
		{
			return 2;
		}
	}
	else
	{
		ret = 1;
	}
	return ret;
}

static uint8_t SWIM_SRST(void)
{
	return SWIM_HW_Out(SWIM_CMD_SRST, SWIM_CMD_BITLEN, SWIM_MAX_RESEND_CNT);
}



/**
	* @brief  快速写模式
	* @para addr:地址，会被自动拆分！ len:后面将要发送的长度  data:将要发送的数据！
	* @Data   
	* \Attention 返回值为0表示正常！
	*/

static uint8_t SWIM_WOTF(uint32_t addr, uint16_t len, uint8_t *data)
{
	uint16_t processed_len;
	uint8_t cur_len, i;
	uint32_t cur_addr, addr_tmp;
	u8 rtv2;

	if ((0 == len) || ((uint8_t*)0 == data))  // 参数错误！
	{
		return 1;
	}

	processed_len = 0;
	cur_addr = addr;
	while (processed_len < len)
	{
		if ((len - processed_len) > 255)
		{
			cur_len = 255;
		}
		else
		{
			cur_len = len - processed_len;
		}

		SET_LE_U32(&addr_tmp, cur_addr);

		if(SWIM_HW_Out(SWIM_CMD_WOTF, SWIM_CMD_BITLEN, SWIM_MAX_RESEND_CNT))
		{
			return 1;
		}
		if (SWIM_HW_Out(cur_len, 8, 0))
		{
			return 2;
		}
		rtv2=SWIM_HW_Out((addr_tmp >> 16) & 0xFF, 8, 0);	 //retry=0,出错后不重发
		if (rtv2)
		{
			return 3;
		}
		if (SWIM_HW_Out((addr_tmp >> 8) & 0xFF, 8, 0))
		{
			return 4;
		}
		if (SWIM_HW_Out((addr_tmp >> 0) & 0xFF, 8, 0))
		{
			return 5;
		}
		for (i = 0; i < cur_len; i++)
		{
			if (SWIM_HW_Out(data[processed_len + i], 8, SWIM_MAX_RESEND_CNT))
			{
				return 6;
			}
		}
		cur_addr += cur_len;
		processed_len += cur_len;
	}
	return 0;
}


/**
	* @brief  用于快递的读！返回值为0表示正常！
	*/

static uint8_t SWIM_ROTF(uint32_t addr, uint16_t len, uint8_t *data)
{
	uint16_t processed_len;
	uint8_t cur_len, i;
	uint32_t cur_addr, addr_tmp;

	if ((0 == len) || ((uint8_t*)0 == data))
	{
		return 7;
	}

	processed_len = 0;
	cur_addr = addr;
	while (processed_len < len)
	{
		if ((len - processed_len) > 255)
		{
			cur_len = 255;
		}
		else
		{
			cur_len = len - processed_len;
		}

		SET_LE_U32(&addr_tmp, cur_addr);

		if(SWIM_HW_Out(SWIM_CMD_ROTF, SWIM_CMD_BITLEN, SWIM_MAX_RESEND_CNT))
		{
			return 6;
		}
		if (SWIM_HW_Out(cur_len, 8, 0))
		{
			return 5;
		}
		if (SWIM_HW_Out((addr_tmp >> 16) & 0xFF, 8, 0))
		{
			return 4;
		}
		if (SWIM_HW_Out((addr_tmp >> 8) & 0xFF, 8, 0))
		{
			return 3;
		}
		if (SWIM_HW_Out((addr_tmp >> 0) & 0xFF, 8, 0))
		{
			return 2;
		}
		for (i = 0; i < cur_len; i++)
		{
			if (SWIM_HW_In(&data[processed_len + i], 8))
			{
				return 1;
			}
		}

		cur_addr += cur_len;
		processed_len += cur_len;
	}
	return 0;
}



// 从下面开始，好像就是可以外部调用的了！

u8 SendData[512];


/**
	* @brief  擦除Flash的操作！！
	* \Attention 清除Flash实际上就是用标准块的编程方式，用来清零！！而且是清零所有的值！
	*/
u8 Erase_Flash(void)
{
	u16 blk;
	u16 Range;
	MCU_INFO *PMcu;
//	PMcu=&STM8S_xx[StoreInfo.McuIndex];
	{ // 这里要添加一个STM8的MCU_IFO类型，用于指示操作！
		PMcu=&STM8S_xx[0];
	}
#if USE_PROGRESS
	ProgSize=PMcu->ProgramNum*1024;
#endif	
	
	Range=PMcu->FlashSize*1024/PMcu->BlockSize;
	for(blk=0;blk<Range;blk++)
	{
		WriteBuff[0]=0x01;													//0000505B,Flash_CR2 ,Bit0=1,标准块编程方式
		WriteBuff[1]=0xFE;													//0000505C,Flash_NCR2
		cmdrtv=SWIM_WOTF(STM8_FLASH_CR2,2,WriteBuff);		//每次写块数据之前都要重新配置Flash_CR2，NCR2,块写结束后被硬件自动清0
		if (cmdrtv>0)
		{
			SWIM_Fini();
			return 1;
		}
		cmdrtv=SWIM_WOTF(PMcu->ProgramAddr+(blk*PMcu->BlockSize),PMcu->BlockSize,SendData);
		if (cmdrtv>0)
		{
			SWIM_Fini();
			return 2;
		}	
		ReadBuff[0]=0;
		
#if USE_PROGRESS
		PROGBAR_SetValue(WM_GetDialogItem(MainHwin,GUI_ID_PROGBAR0),(blk*PMcu->BlockSize*100)/ProgSize);
		WM_Paint(WM_GetDialogItem(MainHwin,GUI_ID_PROGBAR0));
#endif
		
		while(!(ReadBuff[0]& STM8_FLASH_EOF))  // 一直等到END操作！
		{
			cmdrtv=SWIM_ROTF(STM8_FLASH_IAPCSR,1,ReadBuff);
			if(cmdrtv>0)
			{
				SWIM_Fini();
				return 9;
			}
		}
	}
	return 0;
}


u8 Erase_EEPROM(void)
{
	u16 blk;
	u16 Range;
	MCU_INFO *PMcu;
//	PMcu=&STM8S_xx[StoreInfo.McuIndex];
	{ // 这里要添加一个STM8的MCU_IFO类型
		PMcu=&STM8S_xx[0];
	}
#if USE_PROGRESS
	ProgSize=PMcu->EepromNum;
#endif	
	
	Range=PMcu->EepromSize/PMcu->BlockSize;
	for(blk=0;blk<1;blk++)
	{
		WriteBuff[0]=0x01;													//0000505B,Flash_CR2 ,Bit0=1,标准块编程方式
		WriteBuff[1]=0xFE;													//0000505C,Flash_NCR2
		cmdrtv=SWIM_WOTF(STM8_FLASH_CR2,2,WriteBuff);		//每次写块数据之前都要重新配置Flash_CR2，NCR2,块写结束后被硬件自动清0
		if (cmdrtv>0)
		{
			SWIM_Fini();
			return 1;
		}
		cmdrtv=SWIM_WOTF(PMcu->EepromAddr+(blk*64),64,SendData);
		if (cmdrtv>0)
		{
			SWIM_Fini();
			return 2;
		}	
		ReadBuff[0]=0;
		
#if USE_PROGRESS
		PROGBAR_SetValue(WM_GetDialogItem(MainHwin,GUI_ID_PROGBAR0),(blk*64*100)/ProgSize);
		WM_Paint(WM_GetDialogItem(MainHwin,GUI_ID_PROGBAR0));
#endif

		while(!(ReadBuff[0]& STM8_FLASH_EOF))
		{
			cmdrtv=SWIM_ROTF(STM8_FLASH_IAPCSR,1,ReadBuff);
			if(cmdrtv>0)
			{
				SWIM_Fini();
				return 9;
			}
		}	
	}
	return 0;
}


uint8_t Pro_Init(void)
{
 	SWIM_RST_HIGH();
	System_Delay_ms(50);
	SWIM_RST_LOW();
	System_Delay_ms(10);
	SWIM_EnableClockInput();		
	cmdrtv=SWIM_EnterProgMode();
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 1;
	}
	System_Delay_ms(10);
	SWIM_Inited=0;
	SWIM_Init();
	SWIM_SetClockParam(8,20,2);							//SWIM初始时钟为HSI/2=8MHz		
	cmdrtv=SWIM_SRST();  // 发送复位指令！
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 2;
	}
	System_Delay_ms(30);
	WriteBuff[0]=0xA0;			
	cmdrtv=SWIM_WOTF(SWIM_CSR, 1, WriteBuff);	  //SWIM_CSR,控制寄存器中写入0B1010,0000
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 3;
	}	
	System_Delay_ms(10);
	SWIM_RST_HIGH();
	System_Delay_ms(10);
	WriteBuff[0]=0xB0;			
	cmdrtv=SWIM_WOTF(SWIM_CSR, 1, WriteBuff);	  //SWIM_CSR,控制寄存器中写入0B1010,0000
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 4;
	}	
	System_Delay_ms(10);
	SWIM_SetClockParam(8,8,2);  // 之前是：8，10 ，2，但是根据手册，应该是总共10个时钟，所以改为8，8，2
	WriteBuff[0]=0x08;			
	cmdrtv=SWIM_WOTF(DM_CSR2,1,WriteBuff);	  //DM_CSR2,Bit4 STALL =1
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 5;
	}

	WriteBuff[0]=0x56;							   //MASSKEY1
	cmdrtv=SWIM_WOTF(STM8_FLASH_KEY,1,WriteBuff);	   //解锁Flash区域保护
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 6;
	}
	System_Delay_ms(1);
	WriteBuff[0]=0xAE;							  //MASSKEY2
	cmdrtv=SWIM_WOTF(STM8_FLASH_KEY,1,WriteBuff);
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 7;
	}
	cmdrtv=Erase_Flash();
	if(cmdrtv>0)
	{
		return 8;
	}																													
	return 0;  // 到这里表示成功！
}	


u8 End_Pro(void)
{	
	MCU_INFO *PMcu;
//	PMcu=&STM8S_xx[StoreInfo.McuIndex];
	{ // 这里要添加一个STM8的MCU_IFO类型
		PMcu=&STM8S_xx[0];
	}
	cmdrtv=SWIM_ROTF(PMcu->ProgramAddr,PMcu->BlockSize,ReadBuff);
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 1;
	} 
	System_Delay_ms(10);
	WriteBuff[0]=0x00;			
	cmdrtv=SWIM_WOTF(DM_CSR2,1,WriteBuff);	  //DM_CSR2,Bit4 STALL =0
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 2;
	} 
	SWIM_Fini();
	return 0;
}



u8 Pro_Bin(void)
{
	u16 i,blk=0;
	u8 *Point;
	MCU_INFO *PMcu;
//	PMcu=&STM8S_xx[StoreInfo.McuIndex];
	{ // 这里要添加一个STM8的MCU_IFO类型
		PMcu=&STM8S_xx[0];
	}
#if USE_PROGRESS
	ProgSize=PMcu->ProgramNum*1024;
#endif	
	
//	if(f_read(&FilSys,FileData,1024,&ReadCount)!=FR_OK)
//	{
//		return 1;
//	}
	ReadCount = 1024;
	while(ReadCount)
	{
		Point=fileDataForStm8;
		for(i=0;i<ReadCount/PMcu->BlockSize;i++)
		{
			WriteBuff[0]=0x01;								//0000505B,Flash_CR2 ,Bit0=1,标准块编程方式
			WriteBuff[1]=0xFE;								//0000505C,Flash_NCR2
			cmdrtv=SWIM_WOTF(STM8_FLASH_CR2,2,WriteBuff);		//每次写块数据之前都要重新配置Flash_CR2，NCR2,块写结束后被硬件自动清0
			if (cmdrtv>0)
			{
				SWIM_Fini();
				return 2;
			}
			cmdrtv=SWIM_WOTF(PMcu->ProgramAddr+(blk*PMcu->BlockSize),PMcu->BlockSize,Point);
			if (cmdrtv>0)
			{
				SWIM_Fini();
				return 3;
			}
			blk++;
			Point+=PMcu->BlockSize;
			ReadBuff[0]=0;

#if USE_PROGRESS
			PROGBAR_SetValue(WM_GetDialogItem(MainHwin,GUI_ID_PROGBAR0),(blk*PMcu->BlockSize*100)/ProgSize);
			WM_Paint(WM_GetDialogItem(MainHwin,GUI_ID_PROGBAR0));
#endif
			
			while(!(ReadBuff[0]& STM8_FLASH_EOF))
			{
				cmdrtv=SWIM_ROTF(STM8_FLASH_IAPCSR,1,ReadBuff);
				if(cmdrtv>0)
				{
					SWIM_Fini();
					return 4;
				}
			}
			
		}
		if(ReadCount%PMcu->BlockSize)
		{
			WriteBuff[0]=0x01;								//0000505B,Flash_CR2 ,Bit0=1,标准块编程方式
			WriteBuff[1]=0xFE;								//0000505C,Flash_NCR2
			cmdrtv=SWIM_WOTF(STM8_FLASH_CR2,2,WriteBuff);		//每次写块数据之前都要重新配置Flash_CR2，NCR2,块写结束后被硬件自动清0
			if (cmdrtv>0)
			{
				SWIM_Fini();
				return 5;
			}
			cmdrtv=SWIM_WOTF(PMcu->ProgramAddr+(blk*PMcu->BlockSize),ReadCount%PMcu->BlockSize,Point);
			if (cmdrtv>0)
			{
				SWIM_Fini();
				return 6;
			}
			cmdrtv=SWIM_WOTF(PMcu->ProgramAddr+(blk*PMcu->BlockSize)+ReadCount%PMcu->BlockSize,PMcu->BlockSize-ReadCount%PMcu->BlockSize,SendData);
			blk++;
			if (cmdrtv>0)
			{
				SWIM_Fini();
				return 7;
			}
			ReadBuff[0]=0;
			
#if USE_PROGRESS
			PROGBAR_SetValue(WM_GetDialogItem(MainHwin,GUI_ID_PROGBAR0),100);
			WM_Paint(WM_GetDialogItem(MainHwin,GUI_ID_PROGBAR0));
#endif
			
			while(!(ReadBuff[0]& STM8_FLASH_EOF))
			{
				cmdrtv=SWIM_ROTF(STM8_FLASH_IAPCSR,1,ReadBuff);
				if(cmdrtv>0)
				{
					SWIM_Fini();
					return 8;
				}
			}
		}
//		if(ReadCount>=1024)
//		{
//			if(f_lseek(&FilSys,FilSys.fptr)!=FR_OK)
//			{
//				return 9;
//			}
//			if(f_read(&FilSys,FileData,1024,&ReadCount)!=FR_OK)
//			{
//				return 10;
//			}
//		}
//		else
//		{
			ReadCount=0;
//		}
	}
	return 0;
}



void STMProTest( void )
{
	for( u32 i=0;i<1024;i++)
	fileDataForStm8[i] = i*1; 
	Pro_Init();
//	{
////		return 1;
//	}
	Pro_Bin();
//		return 4;
	End_Pro();
//		return 5;
}

/**
	* @brief  初始化的操作！用于最终的函数！
	* @Author 
	* @Data   
	* \Attention 
	*/
uint8_t Pro_Init_CRJ(void)
{
 	SWIM_RST_HIGH();
	System_Delay_ms(50);
	SWIM_RST_LOW();
	System_Delay_ms(10);
	SWIM_EnableClockInput();		
	cmdrtv=SWIM_EnterProgMode();  // 虽然可能存在毛刺，但是不影响后续的操作！
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 1;
	}
//	{
//		SWIM_RST_HIGH();
//		System_Delay_ms(50);
//	}
	System_Delay_ms(10);
	SWIM_Inited=0;
	SWIM_Init();
	SWIM_SetClockParam(8,20,2);							//SWIM初始时钟为HSI/2=8MHz	
  // 实测：发现，下面这个语句在低电平的时候，容易出错误，所以改为SWIM高电平。但是务必要测试！！	
	cmdrtv=SWIM_SRST();  // 发送复位指令！复位SWIM相关的寄存器！
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 2;
	}
	System_Delay_ms(30);
	WriteBuff[0]=0xA0;			
	cmdrtv=SWIM_WOTF(SWIM_CSR, 1, WriteBuff);	  //SWIM_CSR,控制寄存器中写入0B1010,0000，屏蔽内部复位，使能DM模块
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 3;
	}	
	System_Delay_ms(10);
	SWIM_RST_HIGH();
	System_Delay_ms(10);
	WriteBuff[0]=0xB0;			
	cmdrtv=SWIM_WOTF(SWIM_CSR, 1, WriteBuff);	  //SWIM_CSR,控制寄存器中写入0B1011,0000，bit5：使加高速模式
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 4;
	}	
	System_Delay_ms(10);
	SWIM_SetClockParam(8,8,2);  // 之前是：8，10 ，2，但是根据手册，应该是总共10个时钟，所以改为8，8，2
	WriteBuff[0]=0x08;			
	cmdrtv=SWIM_WOTF(DM_CSR2,1,WriteBuff);	  //DM_CSR2,Bit4 STALL =1，使CPU进入停止模式，防止程序影响！（手册中这个是只读的，也就是没有影响？）
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 5;
	}
	
//	SWIM_RST_HIGH();
	return 0;  // 到这里表示成功！
}	


/**
	* @brief  最终的实际下载函数！
	* @Author 
	* @Data  2019.07.16 本次程序进行了重新的改写，工程量巨大。但是如果解决了，就非常好！
	* \Attention 下载应该检查几点：1：AL/L系列和AF/AH/S系列的，寄存器有区别！2：BlockSize的区别。3：选项字节地址和配置。
    \warning 本次的STM8需要是最后一次的STM8的更新，要达到一定程序的程序稳定性。每个地方都是仔细进行检查！此版本务必保持稳定。也就是需要仔细检查。
		\warning 要不要使用单字节下载：主要是下载时间现在不确定会差多少，但是单字节下载，肯定比较稳定，不容易出问题！
	*/
u32 DownloadFlashOfSTM8( _FileInformation _userfile  )
{
	// 根据分析，其重要系统参数：1：Block大小，因为使用的是Block的方式（只有64和128）。2：寄存器和解锁不一样：A系列和L系列存在差别。
	// 3选项字节。这个也要分多种类。
	{  // 通过选项型号进行区分，进行一次表项基本检查！
		switch(_userfile.ChipTypeSuffix)
            {   // 注意下面这个表与C#上面是一致的，如果更改了C#代码，这个仍然需要进行更改！
                // AF 与 AH系列
                // ( 4800 - 480E 00 00FF 00FF 00FF...  4810-4818 00 00 00 00 00 00 00 00 C7 487E-487F 00FF )
								// 下面这些型号都认为是：128Block型号
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:

                case 24:
                case 25:
                case 26:
                case 27:
                case 28:
                case 29:
                case 30:
                case 31:
                case 32:
                case 33:
                case 34:
                case 35:
                case 36:
                case 37:
                case 38:
                case 39:
                case 40:
                case 41:
                case 42:
                case 43:
                case 44:
                case 45:
                case 46:
												{
													STM8_optionByteType = opSeries_0;
													STM8_blockSize = HighBolckSize128;
													STM8_regType = AHF_S_Series;
												}
                    break;

								// 这些都是8KB_FLASH，虽然手册表明8A系列只有128K的Block，但是手册比较老，且查看对应手册其Option区，根本扩大不了到128.
								// 最好的方式就是购买芯片进行测试
                case 20: // 00 00 00 00 00 00 (4800-480A 00 00FF 00FF 00FF 00FF 00FF)
                case 21:
                case 22:
                case 23:
												{
													STM8_optionByteType = opSeries_1;
													STM8_blockSize = lowBlockSize64;
													STM8_regType = AHF_S_Series;
												}
									
                    break;


								// 认为在7的位置必须写上0。
								// L系列的出现
                case 47: // AA 00 00 00 00 00 00  (0 2 8 9 A B C )
                case 48:
                case 49:
                case 50:
                case 51:
                case 52:

                case 55:
                case 56:
                case 57:
                case 58:

                
                case 71:
                case 72:

                case 74:
                case 75:
                case 76:
                case 77:
												{
													STM8_optionByteType = opSeries_2;
													STM8_blockSize = HighBolckSize128;
													STM8_regType = ATL_L_Series;
												}
                    break;
								case 64:  // 虽然是32KB，但是却是：64Byte，所以务必每个芯片都要进行检查！！
												{
													STM8_optionByteType = opSeries_2;
													STM8_blockSize = lowBlockSize64;
													STM8_regType = ATL_L_Series;
												}
													break;

                case 53: // AA 00 00 00 00 01 00 00 ( 4800 AA 2/7/8/9/B/C:00 A:01 ) (7：PSIZE ，其它少一位的都是由于这个位必须为0而已！)
                case 54:
                case 59:
                case 60:
                
                
                case 65:
                case 69:
                case 70:

                case 73:
                
                case 78:
                case 79:
                case 80:
                case 81:
                case 82:
                case 83:
												{
													STM8_optionByteType = opSeries_3;
													STM8_blockSize = HighBolckSize128;
													STM8_regType = ATL_L_Series;
												}
                    break;
								case 62: // 手册中：8.3.5明确表明为：64K大小
								case 63:
												{
													STM8_optionByteType = opSeries_3;
													STM8_blockSize = lowBlockSize64;
													STM8_regType = ATL_L_Series;
												}
									break;

								// Option的Block的大小128，暂时认为其Block大小为128
								// STM8L001J3
                case 61: // 8KB 00 00 00 00 ( 0 2 3 8 其中1必须写0)  
												{
													STM8_optionByteType = opSeries_4;
													STM8_blockSize = HighBolckSize128;
													STM8_regType = ATL_L_Series;
												}
                    break;

								// L101x系列的为64Bolck
								// 但是对比其具体的手册，发现其Block的大小可能是128Byte，所以这个芯片应该进行测试！！
                case 66: // 00 00 00 ( 4800 4802 4808 )( 手册说明其4807必须写00，但是应该是：4801地址，因为顺序上是4801。且STM8TL型号也表明是4801 )
                case 67:
												{
													STM8_optionByteType = opSeries_5;
													STM8_blockSize = lowBlockSize64;
													STM8_regType = ATL_L_Series;
												}
                    break;
                case 68: // 00 00 00 00 无EEPROM (多了一个DataSize 4803)
												{
													STM8_optionByteType = opSeries_6;
													STM8_blockSize = lowBlockSize64;
													STM8_regType = ATL_L_Series;
												}
                    break;


								// 下面就进入了STM8S系列的范围！
								// 8S为常用系列型号，则说明这个需要更具体的测试！
								// 下面的都是：8KB以下的小容量
                case 84: // 00 00 00 00 00 00 (4800-480A 00 00FF 00FF 00FF 00FF 00FF)(同上)
                case 85:
                case 86:
                case 90:
                case 91:
                case 92:
                case 119:
                case 120:
												{
													STM8_optionByteType = opSeries_7;
													STM8_blockSize = lowBlockSize64;
													STM8_regType = AHF_S_Series;
												}
                    break;


                case 87: // 00 00 00 00 00 00 00 00 00 ( 4800-480E 00 00FF 00FF 00FF 00FF 00FF 00FF 00FF 487E-487F 00FF )
                case 88:
                case 89:

                case 93:
                case 94:
                case 95:
                case 96:
                case 97:
                case 98:
                case 99:

                case 100:
                case 101:
                case 102:
                case 103:
                case 104:
                case 105:
                case 106:
                case 107:
                case 108:
                case 109:

                case 110:
                case 111:
                case 112:
                case 113:
                case 114:
                case 115:
                case 116:
                case 117:
                case 118:
												{
													STM8_optionByteType = opSeries_8;
													STM8_blockSize = HighBolckSize128;
													STM8_regType = AHF_S_Series;
												}
                    break;

                // TL 系列，需要进行检查
								// 其大小为16KB大小
                case 121: // AA 00 00 00 00 
                case 122:
												{
													STM8_optionByteType = opSeries_9;
													STM8_blockSize = HighBolckSize128;
													STM8_regType = ATL_L_Series;
												}
                    break;

                default:
                    break;
            }
						
						
						switch(_userfile.ChipTypeSuffix)
            { 
							// 警告：由于采用的是Block的方式，但是综合手册中却没有特别详细的具体的表明芯片的Blocksize，
							// 所以下面利用每个的查表，用来确定Block的大小。
							// 综合起来：只有64与128的大小。
							// 注意：这个表中只列出，能通过手册查出来的型号。需要注意的是：比如STM8AF516是查不到具体手册的。
							case 61:
							case 62:
							case 63:
							case 64:
							case 66:
							case 67:
							case 68:
							case 69:
							case 70:
//								STM8_blockSize = lowBlockSize64;
//								break;
							case 65:
								STM8_blockSize = HighBolckSize128;
								break;
            }
	}
	
	
	u32 returnNumber = 0;

	
	u8 for8SH_test = 1;
	restart:  // 实验中发现：L051在解除写保护后，写Flash会出现错误，但是第二次重写，却是可以的，所以解除保护后，重新开始！
						// 当然，如果没有发现具体的原因，这样的方式是不推荐的！！
	
	
	SWIM_Fini();  // 每次开始的时候，结束一次，防止出现意外！
	SWIM_GPIO_Init();  // 初始化引脚部分！这个一定要在：SWIM_Fini()之后，防止出问题！！
	
	if(_userfile.ChipTypePrefix != 0x60) // 表示STM8系列！
	{
		if( L_ENGLISH_VER != 0x55AA )
		{
				LCD_ShowString(0,12*10,12, RED,  " 请选择正确的STM8型号");
		}
		else
		{
			LCD_ShowString(0,12*10,12, RED,  " ERR:Check STM8 model!");
		}
		return 0;
	}
	else
	{
//		PMcu = &STM8_StoreIfo[_userfile.ChipTypeSuffix];  // 器件选型！
		{  // 为了改变不大，这里做一个改动，只改变其ADDR和EEPROMADDR，因为只有这两个可能存在不一样！
			PMcu->BlockSize = STM8_blockSize;
			if( STM8_regType == ATL_L_Series )
			{
				PMcu->EepromAddr = 0x1000;
			}
			else
			{
				PMcu->EepromAddr = 0x4000;
			}
			
			PMcu->EepromSize = 0;  // 这个日后再进行改进！
			PMcu->FlashSize  = 0;  // 这个日后再进行改进！
			PMcu->ProgramAddr = 0x8000;
			PMcu->OptionAddr  = 0x4800;
		}
	}
	cmdrtv = Pro_Init_CRJ();
	if( cmdrtv > 0 )
	{
		if( L_ENGLISH_VER != 0x55AA )
		{
			LCD_ShowString(0,12*10,12, RED,  "    SWIM进入失败    ");
			LCD_ShowString(0,12*11,12, RED,  "   请检查硬件连接   ");
			ComSendByte(UART4,cmdrtv);
			ComSendByte(UART4,0xFF);
			ComSendByte(UART4,0xFF);
		}
		else
		{
			LCD_ShowString(0,12*10,12, RED,  " SWIM Enter failure ");
			LCD_ShowString(0,12*11,12, RED,  " please Check Hardware");
			ComSendByte(UART4,cmdrtv);
			ComSendByte(UART4,0xFF);
			ComSendByte(UART4,0xFF);
		}
		return 1;
	}
	
	{ // 检查保护区！主要是检查读写保护区。如果有这些，则清除这两个字节。
		enum { inProtected = 0,unProtected = 1 } STM8ProtectStatus = unProtected;
		u8 protectBuf[2];  // 选项字节的前两个
		cmdrtv=SWIM_ROTF(PMcu->OptionAddr,20,ReadBuff);
		if( cmdrtv > 0 )
		{
			return 1;
		}
		{
			switch( STM8_regType )  // 注意：1：保护内容不同 2：ATL_L_Series 不需要写反码
			{
				case ATL_L_Series:
					{ 
						switch( STM8_optionByteType )
						{ // 001J3 与 101x 系列，其无保护：00
							case opSeries_4: // STM8L001J3
							case opSeries_5: // L101
							case opSeries_6: // L101
								{
									if( ( ReadBuff[0] != 00 ) || ( ReadBuff[2] != 00 ) )  // 这个比对的是0/2
									{
										STM8ProtectStatus = inProtected;
										protectBuf[0] = 00;
										protectBuf[1] = 00;
									}
									else
									{
										STM8ProtectStatus = unProtected;
									}
								}
								break;
							// 其他系列的无保护：AA
							default:
								{
									if( ( ReadBuff[0] != 0xAA ) || ( ReadBuff[2] != 00 ) )
									{
										STM8ProtectStatus = inProtected;
										protectBuf[0] = 0xAA;
										protectBuf[1] = 00;
									}
									else
									{
										STM8ProtectStatus = unProtected;
									}
								}
								break;
						}
						{ // 解保护设置
							if( STM8ProtectStatus == inProtected )
							{
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*10,12, RED,  " 发现保护,解除中....");
								}
								else
								{
									LCD_ShowString(0,12*10,12, RED,  " Protection Cleaning.");
								}
											cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_PUKR, STM8_MASS_KEY_FLASH_1 );
											ERROR_TAKE;
											cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_PUKR, STM8_MASS_KEY_FLASH_2 );
											ERROR_TAKE;
											cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_DUKR, STM8_MASS_KEY_EEPROM_1 );
											ERROR_TAKE;
											cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_DUKR, STM8_MASS_KEY_EEPROM_2 );
											ERROR_TAKE;
											cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_CR2, 0X80 );// 使能编写OPT位！
											ERROR_TAKE;
											{ // 编程选项字节，这个时候说系统需要重启！
												for( u8 i = 0;i<2;i++)  // 实际测试中发现，如果只用一次的话，不能解除读保护，但是可以下载。也就是不改变OPT的值，但是可以进行读操作。
																								// 当且写入两次的时候，可以改变OPT的值！！
																								// 注意：但是当写读保护的时候，却是一次性就可以的！
																								// 最终查询手册，原来Flash编程手册中已说明了！！而且只针对于ROP！
												{
													cmdrtv = stm8WriteFlashAbyte(PMcu->OptionAddr,ATL_L_Series,protectBuf[0]);
													ERROR_TAKE;
												}
												{ // 清除写保护！
													cmdrtv = stm8WriteFlashAbyte(PMcu->OptionAddr+2,ATL_L_Series,protectBuf[1]);
													ERROR_TAKE;
												}
												{  // 恢复默认值
													cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_IAPSR, 0x00 );
													ERROR_TAKE;
													cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_CR2, 0x00 );
													ERROR_TAKE;
												}
												if( L_ENGLISH_VER != 0x55AA )
												{
													LCD_ShowString(0,12*11,12, RED,  "   保护解除中完毕!  ");
												}
												else
												{
													LCD_ShowString(0,12*11,12, RED,  " Protection aborted!");
												}
												goto restart;
											}
							}
						}
					}
					break;
				case AHF_S_Series:
					{
						if( ( ReadBuff[0] != 00 ) || ( ReadBuff[1] != 00 ) )  // 这个比对的是：0/1。
						{
							STM8ProtectStatus = inProtected;
						}
						else
						{
							if( ReadBuff[1] ==  (( ~ReadBuff[2] )&0xFF) )
							{
								STM8ProtectStatus = unProtected;
							}
							else
							{
								STM8ProtectStatus = inProtected; // 选项字节不是彼此的补码！不通过！也需要重新配置！
							}
						}
						{ // 这里直接处理读写保护
							if( STM8ProtectStatus == inProtected )
							{
											cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_PUKR, STM8_MASS_KEY_FLASH_1 );
											ERROR_TAKE;
											cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_PUKR, STM8_MASS_KEY_FLASH_2 );
											ERROR_TAKE;
											cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_DUKR, STM8_MASS_KEY_EEPROM_1 );
											ERROR_TAKE;
											cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_DUKR, STM8_MASS_KEY_EEPROM_2 );
											ERROR_TAKE;
											cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_CR2, 0X80 );// 使能编写OPT位！
											ERROR_TAKE;
											{
												cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_CR2+1, (u8)~0x80 );// 使能编写OPT位！
												ERROR_TAKE;
											}
											{ // 编程选项字节！
												{ // 清除读保护
													cmdrtv = stm8WriteFlashAbyte(PMcu->OptionAddr,AHF_S_Series,0x00);
													ERROR_TAKE;
												}
												{ // 清除写保护！
													{
														cmdrtv = stm8WriteFlashAbyte(PMcu->OptionAddr+1,AHF_S_Series,0x00);
														ERROR_TAKE;
														cmdrtv = stm8WriteFlashAbyte(PMcu->OptionAddr+2,AHF_S_Series,(u8)~0x00);
														ERROR_TAKE;
													}
													
												}
												{  // 恢复默认值
													cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_IAPSR, 0x00 );
													ERROR_TAKE;
													cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_CR2, 0x00 );  // 禁止OPT位！
													ERROR_TAKE;
													{
														cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_CR2+1, (u8)~0x00 );
														ERROR_TAKE;
													}
												}
												if( L_ENGLISH_VER != 0x55AA )
												{
													LCD_ShowString(0,12*11,12, RED,  "   保护解除中完毕!  ");
												}
												else
												{
													LCD_ShowString(0,12*11,12, RED,  " Protection aborted!");
												}
												goto restart;
											}
							}
						}
					}
					break;
				default:
					break;
			}
			
		}
	}

	{ // 清除Flash
		// STM8编程不需要清除Flash，直接用标准编程模式就可以了！
		// 关于全部擦除，为了使用方便，日后是一定要加上去的。
	}
	{ // 写Flash，同时要写滚码操作区！
		// 这里因为要使用块编程模式，所以必须计算出块头地址和块尾巴地址！然后一次性写入！
		{ // 写程序区，这个是算法的重中之重了。
			// 这个的更改要非常认真的更改，不小心就会出错！！
					{ // 解锁MASS，不管怎么样，把Flash和EEprom都给解锁了。后面的操作就不用解锁了！
						{ // 解Flash
							WriteBuff[0] = STM8_MASS_KEY_FLASH_1;
							if( STM8_regType == ATL_L_Series )  // AL和L系列的！
							{						
								cmdrtv=SWIM_WOTF(STM8_AL_L_FLASH_PUKR,1,WriteBuff);	 
							}
							else
							{
								cmdrtv=SWIM_WOTF(STM8_AF_S_FLASH_PUKR,1,WriteBuff);	
							}	
							if (cmdrtv>0)
							{
								SWIM_Fini();
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
								}
								else
								{
									LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
								}
								return 6;
							}
							System_Delay_ms(1);
							WriteBuff[0]=STM8_MASS_KEY_FLASH_2;	
							if( STM8_regType == ATL_L_Series )  // AL和L系列的！
							{						
								cmdrtv=SWIM_WOTF(STM8_AL_L_FLASH_PUKR,1,WriteBuff);	 
							}
							else
							{
								cmdrtv=SWIM_WOTF(STM8_AF_S_FLASH_PUKR,1,WriteBuff);	
							}	
							if (cmdrtv>0)
							{
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
								}
								else
								{
									LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
								}
								SWIM_Fini();
								return 7;
							}
						}
						{ // 解EEPROM
							WriteBuff[0]=STM8_MASS_KEY_EEPROM_1;
							if( STM8_regType == ATL_L_Series )  // AL和L系列的！
							{						
								cmdrtv=SWIM_WOTF(STM8_AL_L_FLASH_DUKR,1,WriteBuff);	 
							}
							else
							{
								cmdrtv=SWIM_WOTF(STM8_AF_S_FLASH_DUKR,1,WriteBuff);	
							}	
							if (cmdrtv>0)
							{
								SWIM_Fini();
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
								}
								else
								{
									LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
								}
								return 6;
							}
							System_Delay_ms(1);
							WriteBuff[0]=STM8_MASS_KEY_EEPROM_2;	
							if( STM8_regType == ATL_L_Series )  // AL和L系列的！
							{						
								cmdrtv=SWIM_WOTF(STM8_AL_L_FLASH_DUKR,1,WriteBuff);	 
							}
							else
							{
								cmdrtv=SWIM_WOTF(STM8_AF_S_FLASH_DUKR,1,WriteBuff);	
							}	
							if (cmdrtv>0)
							{
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
								}
								else
								{
									LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
								}
								SWIM_Fini();
								return 7;
							}
						}
					}
//					{ // 最新的采用方式：使用单个字节写的方式，因为需要确定不同的Block，可能会存在问题
							// 但是经过最新的测试，字节下载时间真的是太慢了，完全不能接受！！
//						u32 needReadHowManyW25Q = 0;
//						u32 writeFlashCount = 0; 
//						needReadHowManyW25Q = ( _userfile.ProgramSize / 0x1000 ) + ( ( _userfile.ProgramSize % 0x1000 ) > 0  ? 1 : 0 );
//						for( u32 pW25Q = 0; pW25Q < needReadHowManyW25Q; pW25Q++ )
//						{
//							if( ( pW25Q + 1 ) == needReadHowManyW25Q )
//							{ // 说明是最后一块
//								W25QXX_Read( W25Q4KBuf.databuf4K , _userfile.ProgramSaveAtW25QAddress + ( pW25Q*0x1000 ), ( ( _userfile.ProgramSize % 0x1000 ) == 0 ) ? 0x1000: ( ( _userfile.ProgramSize % 0x1000 ) ) ); 
//							}
//							else
//							{ // 可以直接读4K
//								W25QXX_Read( W25Q4KBuf.databuf4K , _userfile.ProgramSaveAtW25QAddress + ( pW25Q*0x1000 ), 0x1000); 
//							}
//							
//							for( u32 pCount = 0;pCount<0x1000;pCount++ )
//							{
//								stm8WriteFlashAbyte( _userfile.ProgramStartAddress+writeFlashCount,STM8_regType,W25Q4KBuf.databuf4K[pCount] ); 
//								writeFlashCount++;
//								if( writeFlashCount >= _userfile.ProgramSize )
//								{
//									LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>>>   100%");
//									break;
//								}
//								{ // 进度条
//									if( ( writeFlashCount % 100 ) == 0 )
//									{ // 每一定时期进行一次判断，而不是每次
//															float tmp14 = (float)((float)((float)writeFlashCount)/(float)_userfile.ProgramSize);

//																	if( (tmp14 > 0.00) && ( tmp14 <= 0.1) )
//																	{
//																		LCD_ShowString(0,12*10,12,BLACK, "   >             10%");
//																	}
//																	else if( (tmp14 > 0.1) && (tmp14 <=0.2 ) )
//																	{
//																		LCD_ShowString(0,12*10,12, BLACK,"   >>            20%");
//																	}
//																	else if( (tmp14 > 0.2) && (tmp14 <=0.3) )
//																	{
//																		LCD_ShowString(0,12*10,12, BLACK,"   >>>           30%");
//																	}
//																	else if( (tmp14 > 0.3) && (tmp14 <=0.4) )
//																	{
//																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>          40%");
//																	}
//																	else if( (tmp14 > 0.4) && (tmp14 <=0.5) )
//																	{
//																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>         50%");
//																	}
//																	else if( (tmp14 > 0.5) && (tmp14 <=0.6) )
//																	{
//																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>        60%");
//																	}
//																	else if( (tmp14 > 0.6) && (tmp14 <=0.7) )
//																	{
//																		LCD_ShowString(0,12*10,12, BLACK,"   >>>>>>>       70%");
//																	}
//																	else if( (tmp14 > 0.7) && (tmp14 <=0.8) )
//																	{
//																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>      80%");
//																	}
//																	else if( (tmp14 > 0.8) && (tmp14 <=0.9) )
//																	{
//																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>>     90%");
//																	}
//									}
//								}
//							}
//						}
//					}
					{  // 这个是写1区的程序，这个是一定要有的。
						 // 这个程序理论上有优化的空间。
						if( _userfile.ProgramStartAddress != 0xFFFFFFFF )
						{
												u32 needHowManyBlock = 0;  // 计算出需要多少块！
												u32 StartBlockNum = 0;  // 从0开始！
												u32 EndBlockNum = 0;
												{ // 起始地址不在！
													for( u32 i=0;i<0xFFFF;i++ )  // 使能查询法！
													{
														if(  ( ( i*PMcu->BlockSize ) <= (_userfile.ProgramStartAddress-0x8000) )  
															 &&( ( (i+1)*PMcu->BlockSize ) > (_userfile.ProgramStartAddress-0x8000) ) )
														{
															StartBlockNum = i;
														}
														
														if(  ( ( i*PMcu->BlockSize ) <= (_userfile.ProgramStartAddress + _userfile.ProgramSize - 0x8000) )  
															 &&( ( (i+1)*PMcu->BlockSize ) > (_userfile.ProgramStartAddress + _userfile.ProgramSize - 0x8000) ) )
														{
															EndBlockNum = i;
															break;
														}
													}
													needHowManyBlock = EndBlockNum -StartBlockNum + 1;  // 得出多少块，每次写1块操作！
												}
												

												
												u32 needReadHowManyDataStart=0;  // 因为按照块的方式，所以可能首尾可能不是一个整的。所以这个是用来决定读多少数据的。
												u32 needReadHowManyDataEnd=0;
												for( u32 j = 0;j<needHowManyBlock;j++ )  // 每块都单独编程，按照块的编程方式！
												{
													u8 tmpBuf[2];
													if( j==0 )  // 第一块！如何拼接数据？
													{
														needReadHowManyDataStart = PMcu->BlockSize - (_userfile.ProgramStartAddress - PMcu->ProgramAddr)%PMcu->BlockSize;
														
														
														W25QXX_Read( W25Q4KBuf.databuf4K , _userfile.ProgramSaveAtW25QAddress, needReadHowManyDataStart);
														
														memset(WriteBuff,0,sizeof(WriteBuff));
														for( u32 k=0;k<needReadHowManyDataStart;k++ )
														{
															WriteBuff[PMcu->BlockSize - needReadHowManyDataStart + k] = W25Q4KBuf.databuf4K[k];
														}
													}
													else if( j+1 == needHowManyBlock )  // 最后一块！如何拼接数据？
													{
													//	needReadHowManyDataEnd = (_userfile.ProgramSize - PMcu->ProgramAddr)%PMcu->BlockSize + 1;
														{ // 下面这个是为了防止越界，尤其是正好达到Flash的大小的标准下，具体机理可能存在问题，但是由于没有时间，所以无法系统的解决问题！
															// 警告：其中计算需要多少块出了问题（在正好在块的结尾时候，会多计算一块！），但是当前情况下程序，都是根据问题来的，所以这是临时解决方法。
															needReadHowManyDataEnd = (_userfile.ProgramSize - PMcu->ProgramAddr)%PMcu->BlockSize;
															if( needReadHowManyDataEnd == 0 )
															{ // 此时认为越界，需要停止。
																goto crossBorad1;
															}
														}
														W25QXX_Read( W25Q4KBuf.databuf4K , _userfile.ProgramSaveAtW25QAddress + needReadHowManyDataStart + (j-1)*PMcu->BlockSize, needReadHowManyDataEnd);  // 里面有个J-2，也就表示，必须>=2块，这个一般容易做到！
														memset(WriteBuff,0,sizeof(WriteBuff));
														for( u32 m=0;m<needReadHowManyDataEnd;m++ )
														{
															WriteBuff[m] = W25Q4KBuf.databuf4K[m];
														}
													}
													else // 过程中的拼接数据！
													{
														W25QXX_Read( W25Q4KBuf.databuf4K , _userfile.ProgramSaveAtW25QAddress + needReadHowManyDataStart + (j-1)*PMcu->BlockSize, PMcu->BlockSize);  // 里面有个J-2，也就表示，必须>=2块，这个一般容易做到！
														
														for( u32 n=0;n<PMcu->BlockSize;n++ )
														{
															WriteBuff[n] = W25Q4KBuf.databuf4K[n];
														}
													}
													
													{ // 通用的块编程操作！
														if( STM8_regType == ATL_L_Series )  // AL和L系列的！
														{	
															tmpBuf[0]=0x01;								//0000505B,Flash_CR2 ,Bit0=1,标准块编程方式
															cmdrtv=SWIM_WOTF(STM8_AL_L_FLASH_CR2,1,tmpBuf);		//每次写块数据之前都要重新配置Flash_CR2，NCR2,块写结束后被硬件自动清0
															if (cmdrtv>0)
															{
																SWIM_Fini();
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																}
																else
																{
																	LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																}
																return 5;
															}
															cmdrtv=SWIM_WOTF( PMcu->ProgramAddr+PMcu->BlockSize*( StartBlockNum+j),PMcu->BlockSize,WriteBuff );
															if (cmdrtv>0)
															{
																SWIM_Fini();
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																}
																else
																{
																	LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																}
																return 6;
															}
															ReadBuff[0] = 0;
															while(!(ReadBuff[0]& STM8_FLASH_EOF))
															{
																cmdrtv=SWIM_ROTF(STM8_AL_L_FLASH_IAPSR,1,ReadBuff);
																if(cmdrtv>0)
																{
																	SWIM_Fini();
																	if( L_ENGLISH_VER != 0x55AA )
																	{
																		LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																	}
																	else
																	{
																		LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																	}
																	return 8;
																}
															}
														}
														else // S-AF系列！
														{
															tmpBuf[0]=0x01;								//0000505B,Flash_CR2 ,Bit0=1,标准块编程方式
															tmpBuf[1]=0xFE;								//0000505C,Flash_NCR2
															cmdrtv=SWIM_WOTF(STM8_AF_S_FLASH_CR2,2,tmpBuf);		//每次写块数据之前都要重新配置Flash_CR2，NCR2,块写结束后被硬件自动清0
															if (cmdrtv>0)
															{
																SWIM_Fini();
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																}
																else
																{
																	LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																}
																return 5;
															}
															cmdrtv=SWIM_WOTF( PMcu->ProgramAddr+PMcu->BlockSize*( StartBlockNum+j),PMcu->BlockSize,WriteBuff);
															if (cmdrtv>0)
															{
																SWIM_Fini();
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																}
																else
																{
																	LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																}
																return 6;
															}
															ReadBuff[0] = 0;
															while(!(ReadBuff[0]& STM8_FLASH_EOF))
															{
																cmdrtv=SWIM_ROTF(STM8_AF_S_FLASH_IAPSR,1,ReadBuff);
																if(cmdrtv>0)
																{
																	SWIM_Fini();
																	if( L_ENGLISH_VER != 0x55AA )
																	{
																		LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																	}
																	else
																	{
																		LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																	}
																	return 8;
																}
															}
														}
													}
													{  // 用于显示进度！
														crossBorad1:
														if( (j +1) == needHowManyBlock )
														{
															LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>>>   100%");  // 下载成功100%
														}
														else
														{
															float tmp13 = (float)((float)((float)j)/(float)needHowManyBlock);
															for( float tmp14 = 0.0;tmp14 <= (float)1.0; tmp14 += 0.1 )
															{
																if( tmp13 <= tmp14 ) // 找到第一次开始小于等于的值！
																{
																	if( (tmp14 > 0.00) && ( tmp14 <= 0.1) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >             10%");
																		break;
																	}
																	else if( (tmp14 > 0.1) && (tmp14 <=0.2 ) )
																	{
																		LCD_ShowString(0,12*10,12, BLACK,"   >>            20%");
																		break;
																	}
																	else if( (tmp14 > 0.2) && (tmp14 <=0.3) )
																	{
																		LCD_ShowString(0,12*10,12, BLACK,"   >>>           30%");
																		break;
																	}
																	else if( (tmp14 > 0.3) && (tmp14 <=0.4) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>          40%");
																		break;
																	}
																	else if( (tmp14 > 0.4) && (tmp14 <=0.5) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>         50%");
																		break;
																	}
																	else if( (tmp14 > 0.5) && (tmp14 <=0.6) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>        60%");
																		break;
																	}
																	else if( (tmp14 > 0.6) && (tmp14 <=0.7) )
																	{
																		LCD_ShowString(0,12*10,12, BLACK,"   >>>>>>>       70%");
																		break;
																	}
																	else if( (tmp14 > 0.7) && (tmp14 <=0.8) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>      80%");
																		break;
																	}
																	else if( (tmp14 > 0.8) && (tmp14 <=0.9) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>>     90%");
																		break;
																	}
																}
															}
													}
												}
											}
												
							{ // 校验程序区，暂时只使用和校验！
								// 一个一个读出来！每次最多读64个，作为尝试！
								u32 addressPoint = 0;  // 最大值是：程序的大小！
								u16 sumValue = 0;
								while( addressPoint <= ( _userfile.ProgramSize-1) ) // 只要他小，就可以操作！
								{
									if( addressPoint+64 <= _userfile.ProgramSize )  // 认为可以读整数！
									{
										cmdrtv = SWIM_ROTF(_userfile.ProgramStartAddress+addressPoint,64,ReadBuff);
										sumValue += GetSumOf16Bit(ReadBuff,64);
										addressPoint += 64;
									}
									else
									{
										cmdrtv = SWIM_ROTF(_userfile.ProgramStartAddress+addressPoint, _userfile.ProgramSize-1 - addressPoint + 1 ,ReadBuff);
										sumValue += GetSumOf16Bit(ReadBuff,_userfile.ProgramSize-1 - addressPoint + 1);
										addressPoint += _userfile.ProgramSize-1 - addressPoint + 1;
									}
								}
								if( sumValue == _userfile.SumValueOfProgram )
								{
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString(0,12*11,12, BLACK,"   FLASH 校验成功   ");
									}
										else
									{
										LCD_ShowString(0,12*11,12, BLACK," FLASH Check success ");
									}
									returnNumber = 0x55AA;
								}
								else
								{
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString(0,12*11,12,BLACK, "   FLASH 校验失败    ");
									}
										else
									{
										LCD_ShowString(0,12*11,12, BLACK," FLASH Check failure ");
									}
									return 8;
								}
							}
						}
					}
						if( _userfile.STM8_CFG == 0x5A )  // 表示和程序2区的操作！
						{  // 下面是写2区的程序
												u32 needHowManyBlock = 0;  // 计算出需要多少块！
												u32 StartBlockNum = 0;  // 从0开始！
												u32 EndBlockNum = 0;
												{ // 起始地址不在！
													for( u32 i=0;i<0xFFFF;i++ )  // 使能查询法！
													{
														if(  ( ( i*PMcu->BlockSize ) <= (_userfile.ProgramStartAddress_STM8 - PMcu->EepromAddr ) )  
															 &&( ( (i+1)*PMcu->BlockSize ) > (_userfile.ProgramStartAddress_STM8 - PMcu->EepromAddr ) ) )
														{
															StartBlockNum = i;
														}
														
														if(  ( ( i*PMcu->BlockSize ) <= (_userfile.ProgramStartAddress_STM8 + _userfile.ProgramSize_STM8 - PMcu->EepromAddr ) )  
															 &&( ( (i+1)*PMcu->BlockSize ) > (_userfile.ProgramStartAddress_STM8 + _userfile.ProgramSize_STM8 - PMcu->EepromAddr ) ) )
														{
															EndBlockNum = i;
															break;
														}
													}
													needHowManyBlock = EndBlockNum -StartBlockNum + 1;  // 得出多少块，每次写1块操作！
												}
												

												
												u32 needReadHowManyDataStart=0;  // 因为按照块的方式，所以可能首尾可能不是一个整的。所以这个是用来决定读多少数据的。
												u32 needReadHowManyDataEnd=0;
												for( u32 j = 0;j<needHowManyBlock;j++ )  // 每块都单独编程，按照块的编程方式！
												{
													u8 tmpBuf[2];
													if( j==0 )  // 第一块！如何拼接数据？
													{
														needReadHowManyDataStart = PMcu->BlockSize - (_userfile.ProgramStartAddress_STM8 - PMcu->EepromAddr)%PMcu->BlockSize;
														
														
														W25QXX_Read( W25Q4KBuf.databuf4K , ( _userfile.ProgramSaveAtW25QAddress + _userfile.ProgramSize ), needReadHowManyDataStart);
														
														memset(WriteBuff,0,sizeof(WriteBuff));
														for( u32 k=0;k<needReadHowManyDataStart;k++ )
														{
															WriteBuff[PMcu->BlockSize - needReadHowManyDataStart + k] = W25Q4KBuf.databuf4K[k];
														}
													}
													else if( j+1 == needHowManyBlock )  // 最后一块！如何拼接数据？
													{
													//	needReadHowManyDataEnd = (_userfile.ProgramSize - PMcu->ProgramAddr)%PMcu->BlockSize + 1;
														{ // 下面这个是为了防止越界，尤其是正好达到Flash的大小的标准下，具体机理可能存在问题，但是由于没有时间，所以无法系统的解决问题！
															// 警告：其中计算需要多少块出了问题（在正好在块的结尾时候，会多计算一块！），但是当前情况下程序，都是根据问题来的，所以这是临时解决方法。
															needReadHowManyDataEnd = (_userfile.ProgramSize_STM8 - PMcu->EepromAddr)%PMcu->BlockSize;
															if( needReadHowManyDataEnd == 0 )
															{ // 此时认为越界，需要停止。
																goto crossBorad2;
															}
														}
														W25QXX_Read( W25Q4KBuf.databuf4K , ( _userfile.ProgramSaveAtW25QAddress + _userfile.ProgramSize ) + needReadHowManyDataStart + (j-1)*PMcu->BlockSize, needReadHowManyDataEnd);  // 里面有个J-2，也就表示，必须>=2块，这个一般容易做到！
														memset(WriteBuff,0,sizeof(WriteBuff));
														for( u32 m=0;m<needReadHowManyDataEnd;m++ )
														{
															WriteBuff[m] = W25Q4KBuf.databuf4K[m];
														}
													}
													else // 过程中的拼接数据！
													{
														W25QXX_Read( W25Q4KBuf.databuf4K , ( _userfile.ProgramSaveAtW25QAddress + _userfile.ProgramSize ) + needReadHowManyDataStart + (j-1)*PMcu->BlockSize, PMcu->BlockSize);  // 里面有个J-2，也就表示，必须>=2块，这个一般容易做到！
														
														for( u32 n=0;n<PMcu->BlockSize;n++ )
														{
															WriteBuff[n] = W25Q4KBuf.databuf4K[n];
														}
													}
													
													{ // 通用的块编程操作！
														if( STM8_regType == ATL_L_Series )  // AL和L系列的！
														{	
															tmpBuf[0]=0x01;								//0000505B,Flash_CR2 ,Bit0=1,标准块编程方式
															cmdrtv=SWIM_WOTF(STM8_AL_L_FLASH_CR2,1,tmpBuf);		//每次写块数据之前都要重新配置Flash_CR2，NCR2,块写结束后被硬件自动清0
															if (cmdrtv>0)
															{
																SWIM_Fini();
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																}
																else
																{
																	LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																}
																return 5;
															}
															cmdrtv=SWIM_WOTF( PMcu->EepromAddr+PMcu->BlockSize*( StartBlockNum+j),PMcu->BlockSize,WriteBuff );
															if (cmdrtv>0)
															{
																SWIM_Fini();
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																}
																else
																{
																	LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																}
																return 6;
															}
															ReadBuff[0] = 0;
															while(!(ReadBuff[0]& STM8_FLASH_EOF))
															{
																cmdrtv=SWIM_ROTF(STM8_AL_L_FLASH_IAPSR,1,ReadBuff);
																if(cmdrtv>0)
																{
																	SWIM_Fini();
																	if( L_ENGLISH_VER != 0x55AA )
																	{
																		LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																	}
																	else
																	{
																		LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																	}
																	return 8;
																}
															}
														}
														else // S-AF系列！
														{
															tmpBuf[0]=0x01;								//0000505B,Flash_CR2 ,Bit0=1,标准块编程方式
															tmpBuf[1]=0xFE;								//0000505C,Flash_NCR2
															cmdrtv=SWIM_WOTF(STM8_AF_S_FLASH_CR2,2,tmpBuf);		//每次写块数据之前都要重新配置Flash_CR2，NCR2,块写结束后被硬件自动清0
															if (cmdrtv>0)
															{
																SWIM_Fini();
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																}
																else
																{
																	LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																}
																return 5;
															}
															cmdrtv=SWIM_WOTF( PMcu->EepromAddr+PMcu->BlockSize*( StartBlockNum+j),PMcu->BlockSize,WriteBuff);
															if (cmdrtv>0)
															{
																SWIM_Fini();
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																}
																else
																{
																	LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																}
																return 6;
															}
															ReadBuff[0] = 0;
															while(!(ReadBuff[0]& STM8_FLASH_EOF))
															{
																cmdrtv=SWIM_ROTF(STM8_AF_S_FLASH_IAPSR,1,ReadBuff);
																if(cmdrtv>0)
																{
																	SWIM_Fini();
																	if( L_ENGLISH_VER != 0x55AA )
																	{
																		LCD_ShowString(0,12*10,12, RED,  "检查STM8型号是否正确");
																	}
																	else
																	{
																		LCD_ShowString(0,12*10,12, RED,  "ERR:Check STM8 model!");
																	}
																	return 8;
																}
															}
														}
													}
													{  // 用于显示进度！
														crossBorad2:
														if( (j +1) == needHowManyBlock )
														{
															LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>>>   100%");  // 下载成功100%
														}
														else
														{
															float tmp13 = (float)((float)((float)j)/(float)needHowManyBlock);
															for( float tmp14 = 0.0;tmp14 <= (float)1.0; tmp14 += 0.1 )
															{
																if( tmp13 <= tmp14 ) // 找到第一次开始小于等于的值！
																{
																	if( (tmp14 > 0.00) && ( tmp14 <= 0.1) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >             10%");
																		LED_MINI_1 = !LED_MINI_1;
																		break;
																	}
																	else if( (tmp14 > 0.1) && (tmp14 <=0.2 ) )
																	{
																		LCD_ShowString(0,12*10,12, BLACK,"   >>            20%");
																		LED_MINI_1 = !LED_MINI_1;
																		break;
																	}
																	else if( (tmp14 > 0.2) && (tmp14 <=0.3) )
																	{
																		LCD_ShowString(0,12*10,12, BLACK,"   >>>           30%");
																		LED_MINI_1 = !LED_MINI_1;
																		break;
																	}
																	else if( (tmp14 > 0.3) && (tmp14 <=0.4) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>          40%");
																		LED_MINI_1 = !LED_MINI_1;
																		break;
																	}
																	else if( (tmp14 > 0.4) && (tmp14 <=0.5) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>         50%");
																		LED_MINI_1 = !LED_MINI_1;
																		break;
																	}
																	else if( (tmp14 > 0.5) && (tmp14 <=0.6) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>        60%");
																		LED_MINI_1 = !LED_MINI_1;
																		break;
																	}
																	else if( (tmp14 > 0.6) && (tmp14 <=0.7) )
																	{
																		LCD_ShowString(0,12*10,12, BLACK,"   >>>>>>>       70%");
																		LED_MINI_1 = !LED_MINI_1;
																		break;
																	}
																	else if( (tmp14 > 0.7) && (tmp14 <=0.8) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>      80%");
																		LED_MINI_1 = !LED_MINI_1;
																		break;
																	}
																	else if( (tmp14 > 0.8) && (tmp14 <=0.9) )
																	{
																		LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>>     90%");
																		LED_MINI_1 = !LED_MINI_1;
																		break;
																	}
																}
															}
													}
												}
											}
												
							{ // 校验程序区，暂时只使用和校验！
								// 一个一个读出来！每次最多读64个，作为尝试！
								u32 addressPoint = 0;  // 最大值是：程序的大小！
								u16 sumValue = 0;
								while( addressPoint <= ( _userfile.ProgramSize_STM8-1) ) // 只要他小，就可以操作！
								{
									if( addressPoint+64 <= _userfile.ProgramSize_STM8 )  // 认为可以读整数！
									{
										cmdrtv = SWIM_ROTF(_userfile.ProgramStartAddress_STM8+addressPoint,64,ReadBuff);
										sumValue += GetSumOf16Bit(ReadBuff,64);
										addressPoint += 64;
									}
									else
									{
										cmdrtv = SWIM_ROTF(_userfile.ProgramStartAddress_STM8+addressPoint, _userfile.ProgramSize_STM8-1 - addressPoint + 1 ,ReadBuff);
										sumValue += GetSumOf16Bit(ReadBuff,_userfile.ProgramSize_STM8-1 - addressPoint + 1);
										addressPoint += _userfile.ProgramSize_STM8-1 - addressPoint + 1;
									}
								}
								if( sumValue == _userfile.SumValueOfProgram_STM8 )
								{
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString(0,12*11,12, BLACK,"   FLASH 校验成功   ");
									}
										else
									{
										LCD_ShowString(0,12*11,12, BLACK," FLASH Check success ");
									}
									returnNumber = 0x55AA;
								}
								else
								{
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString(0,12*11,12,BLACK, "   FLASH 校验失败    ");
									}
										else
									{
										LCD_ShowString(0,12*11,12, BLACK," FLASH Check failure ");
									}
									return 8;
								}
							}
						}
						
	{ // 写滚码，且校验滚码区！！这个直接采用标准的编程模式！写4个字节！
			u32 programTimesTmp15 = 0;  // 实际已经编程次数
			u32 valueTmp16 = 0; 				// 实际写入的滚码值
			if( _userfile.RollingCodeFunction == 0x55 ) // 表示需要操作滚码区
			{
				if( _userfile.DownloaderNumbers[255] != 0xFF )
				{
					programTimesTmp15 = _userfile.AlreadyProgrammedCarrayBit*(256+1) + 256;
				}
				else
				{
					for( u16 Tmp16 = 0;Tmp16<256;Tmp16++ )
					{
						if( _userfile.DownloaderNumbers[Tmp16] == 0xFF )
						{
							programTimesTmp15 = _userfile.AlreadyProgrammedCarrayBit*(256+1) + Tmp16;  // 实际编程次数
							break;
						}
					}
				}
				valueTmp16 = _userfile.RollingCodeStartValue + ( _userfile.RollingCodeStepValue* programTimesTmp15 );  // 这个是要写的值！
				
				{  // 实际的写入函数！
					if( _userfile.RollingCodeEndianMode == 0 )  // 小端模式
					{
						WriteBuff[0] = valueTmp16>>24;
						WriteBuff[1] = valueTmp16>>16;
						WriteBuff[2] = valueTmp16>>8;
						WriteBuff[3] = valueTmp16>>0;
					}
					else 
					{
						WriteBuff[0] = valueTmp16>>0;
						WriteBuff[1] = valueTmp16>>8;
						WriteBuff[2] = valueTmp16>>16;
						WriteBuff[3] = valueTmp16>>24;
					}
					{
						stm8WriteFlashAbyte(_userfile.RollingCodeStartAddress+0,STM8_regType,WriteBuff[0]);
						ERROR_TAKE;
						stm8WriteFlashAbyte(_userfile.RollingCodeStartAddress+1,STM8_regType,WriteBuff[1]);
						ERROR_TAKE;
						stm8WriteFlashAbyte(_userfile.RollingCodeStartAddress+2,STM8_regType,WriteBuff[2]);
						ERROR_TAKE;
						stm8WriteFlashAbyte(_userfile.RollingCodeStartAddress+3,STM8_regType,WriteBuff[3]);
						ERROR_TAKE;
					}
					
					cmdrtv = SWIM_ROTF(_userfile.RollingCodeStartAddress,4,ReadBuff);
					if (cmdrtv>0)
					{
						SWIM_Fini();
						return 6;
					}
					
					if( 	( ReadBuff[0] == WriteBuff[0] )
						&&	( ReadBuff[1] == WriteBuff[1] )
						&&	( ReadBuff[2] == WriteBuff[2] )
						&&	( ReadBuff[3] == WriteBuff[3] ) )
					{
						if( L_ENGLISH_VER != 0x55AA )
						{
							LCD_ShowString(0,12*11,12,RED, "    滚码校验成功     ");
						}
						else
						{
							LCD_ShowString(0,12*11,12,RED, "Roll code check success");
						}
						returnNumber = 0x55AA;
					}
					else
					{
						if( L_ENGLISH_VER != 0x55AA )
						{
							LCD_ShowString(0,12*11,12,RED, "    滚码校验失败     ");
						}
						else
						{
							LCD_ShowString(0,12*11,12,RED, "Roll code check failed");
						}
					}
				}
			}
	}
	{ // 操作保护区！(其实这里应该加一个两个都不使能的操作！)
		{ // 操作选项字节！注意：STM8系列的选项字节的读写保护已使用：选项字节的存储位要设定，所以只需要根据存储的选项字来操作，而不用管读写保护这里！！
			// 1：使能选项字节编程
			switch( STM8_optionByteType )
			{
				case opSeries_0:  // AF AH S系列
				case opSeries_1:
				case opSeries_7:
				case opSeries_8:
				{
						cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_PUKR, STM8_MASS_KEY_FLASH_1 );
						ERROR_TAKE;
						cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_PUKR, STM8_MASS_KEY_FLASH_2 );
						ERROR_TAKE;
						cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_DUKR, STM8_MASS_KEY_EEPROM_1 );
						ERROR_TAKE;
						cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_DUKR, STM8_MASS_KEY_EEPROM_2 );
						ERROR_TAKE;
						cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_CR2, 0X80 );// 使能编写OPT位！
						ERROR_TAKE;
						cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_CR2+1, (u8)~0x80 );
						ERROR_TAKE;
						{ // 编程选项字节！
							{ // 清除读保护
								cmdrtv = stm8WriteFlashAbyte(PMcu->OptionAddr,STM8_regType,keyTmp[0]);
								ERROR_TAKE;
							}
							{
								cmdrtv = stm8WriteFlashAbyte(PMcu->OptionAddr+1,STM8_regType,keyTmp[1]);
								ERROR_TAKE;
								cmdrtv = stm8WriteFlashAbyte(PMcu->OptionAddr+2,STM8_regType,(u8)~keyTmp[1]);
							}
						}
				}
							break;
				
				
				case opSeries_2:
				case opSeries_3:
				case opSeries_4:
				case opSeries_5:
				case opSeries_6:
				case opSeries_9:
							{
								cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_PUKR, STM8_MASS_KEY_FLASH_1 );
								ERROR_TAKE;
								cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_PUKR, STM8_MASS_KEY_FLASH_2 );
								ERROR_TAKE;
								cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_DUKR, STM8_MASS_KEY_EEPROM_1 );
								ERROR_TAKE;
								cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_DUKR, STM8_MASS_KEY_EEPROM_2 );
								ERROR_TAKE;
								cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_CR2, 0X80 );// 使能编写OPT位！
								ERROR_TAKE;
							}
							break;
			}
			
			// 2编程选项字节
			switch( STM8_optionByteType )
			{
				case opSeries_0:
				{ // 编程选项字节
					cmdrtv = stm8WriteFlashAbyte(0x4800,STM8_regType,_userfile.OptionByte[0]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4801,STM8_regType,_userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4802,STM8_regType,~_userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4803,STM8_regType,_userfile.OptionByte[2]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4804,STM8_regType,~_userfile.OptionByte[2]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4805,STM8_regType,_userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4806,STM8_regType,~_userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4807,STM8_regType,_userfile.OptionByte[4]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4808,STM8_regType,~_userfile.OptionByte[4]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4809,STM8_regType,_userfile.OptionByte[5]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480A,STM8_regType,~_userfile.OptionByte[5]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480B,STM8_regType,_userfile.OptionByte[6]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480C,STM8_regType,~_userfile.OptionByte[6]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480D,STM8_regType,_userfile.OptionByte[7]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480E,STM8_regType,~_userfile.OptionByte[7]); ERROR_TAKE;
					
					cmdrtv = stm8WriteFlashAbyte(0x4810,STM8_regType,_userfile.OptionByte[8]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4811,STM8_regType,_userfile.OptionByte[9]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4812,STM8_regType,_userfile.OptionByte[10]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4813,STM8_regType,_userfile.OptionByte[11]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4814,STM8_regType,_userfile.OptionByte[12]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4815,STM8_regType,_userfile.OptionByte[13]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4816,STM8_regType,_userfile.OptionByte[14]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4817,STM8_regType,_userfile.OptionByte[15]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4818,STM8_regType,_userfile.OptionByte[16]); ERROR_TAKE;
					
					cmdrtv = stm8WriteFlashAbyte(0x487E,STM8_regType,_userfile.OptionByte[17]);  ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x487F,STM8_regType,~_userfile.OptionByte[17]); ERROR_TAKE;
					ERROR_TAKE;
				}
					break;
				case opSeries_1:
				{
					cmdrtv = stm8WriteFlashAbyte(0x4800, STM8_regType, 	_userfile.OptionByte[0]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4801, STM8_regType, 	_userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4802, STM8_regType, ~_userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4803, STM8_regType, 	_userfile.OptionByte[2]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4804, STM8_regType, ~_userfile.OptionByte[2]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4805, STM8_regType, 	_userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4806, STM8_regType, ~_userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4807, STM8_regType, 	_userfile.OptionByte[4]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4808, STM8_regType, ~_userfile.OptionByte[4]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4809, STM8_regType, 	_userfile.OptionByte[5]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480A, STM8_regType, ~_userfile.OptionByte[5]); ERROR_TAKE;
				}
					break;
				case opSeries_2:
				{
					cmdrtv = stm8WriteFlashAbyte(0x4800, STM8_regType,  _userfile.OptionByte[0]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4802, STM8_regType,  _userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4807, STM8_regType,                     0x00); ERROR_TAKE;  // 这个没有PCODESIZE
					cmdrtv = stm8WriteFlashAbyte(0x4808, STM8_regType,  _userfile.OptionByte[2]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4809, STM8_regType,  _userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480A, STM8_regType,  _userfile.OptionByte[4]); ERROR_TAKE;  // 默认是00，有的是01
					cmdrtv = stm8WriteFlashAbyte(0x480B, STM8_regType,  _userfile.OptionByte[5]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480C, STM8_regType,  _userfile.OptionByte[6]); ERROR_TAKE;
				}
					break;
				case opSeries_3:
				{
					cmdrtv = stm8WriteFlashAbyte(0x4800, STM8_regType,  _userfile.OptionByte[0]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4802, STM8_regType,  _userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4807, STM8_regType,  _userfile.OptionByte[2]); ERROR_TAKE;  // 这个没有PCODESIZE
					cmdrtv = stm8WriteFlashAbyte(0x4808, STM8_regType,  _userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4809, STM8_regType,  _userfile.OptionByte[4]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480A, STM8_regType,  _userfile.OptionByte[5]); ERROR_TAKE;  // 默认是00，有的是01
					cmdrtv = stm8WriteFlashAbyte(0x480B, STM8_regType,  _userfile.OptionByte[6]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480C, STM8_regType,  _userfile.OptionByte[7]); ERROR_TAKE;
				}
					break;
				case opSeries_4:
				{ // STM8L001J3
					cmdrtv = stm8WriteFlashAbyte(0x4800, STM8_regType,  _userfile.OptionByte[0]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4802, STM8_regType,  _userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4803, STM8_regType,  _userfile.OptionByte[2]); ERROR_TAKE;  // 这个没有PCODESIZE
					cmdrtv = stm8WriteFlashAbyte(0x4807, STM8_regType,                     0x00); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4808, STM8_regType,  _userfile.OptionByte[3]); ERROR_TAKE;
				}
					break;
				case opSeries_5:
				{
					cmdrtv = stm8WriteFlashAbyte(0x4800, STM8_regType,  _userfile.OptionByte[0]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4802, STM8_regType,  _userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4807, STM8_regType,                     0x00); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4808, STM8_regType,  _userfile.OptionByte[2]); ERROR_TAKE;
				}
					break;
				case opSeries_6:
				{
					cmdrtv = stm8WriteFlashAbyte(0x4800, STM8_regType,  _userfile.OptionByte[0]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4802, STM8_regType,  _userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4803, STM8_regType,  _userfile.OptionByte[2]); ERROR_TAKE;  // 这个没有PCODESIZE
					cmdrtv = stm8WriteFlashAbyte(0x4807, STM8_regType,                     0x00); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4808, STM8_regType,  _userfile.OptionByte[3]); ERROR_TAKE;
				}
					break;
				case opSeries_7:
				{
					cmdrtv = stm8WriteFlashAbyte(0x4800, STM8_regType,  _userfile.OptionByte[0]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4801, STM8_regType,  _userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4802, STM8_regType, ~_userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4803, STM8_regType,  _userfile.OptionByte[2]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4804, STM8_regType, ~_userfile.OptionByte[2]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4805, STM8_regType,  _userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4806, STM8_regType, ~_userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4807, STM8_regType,  _userfile.OptionByte[4]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4808, STM8_regType, ~_userfile.OptionByte[4]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4809, STM8_regType,  _userfile.OptionByte[5]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480A, STM8_regType, ~_userfile.OptionByte[5]); ERROR_TAKE;
				}
					break;
				case opSeries_8:
				{
					cmdrtv = stm8WriteFlashAbyte(0x4800, STM8_regType,  _userfile.OptionByte[0]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4801, STM8_regType,  _userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4802, STM8_regType, ~_userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4803, STM8_regType,  _userfile.OptionByte[2]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4804, STM8_regType, ~_userfile.OptionByte[2]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4805, STM8_regType,  _userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4806, STM8_regType, ~_userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4807, STM8_regType,  _userfile.OptionByte[4]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4808, STM8_regType, ~_userfile.OptionByte[4]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4809, STM8_regType,  _userfile.OptionByte[5]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480A, STM8_regType, ~_userfile.OptionByte[5]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480B, STM8_regType,  _userfile.OptionByte[6]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480C, STM8_regType, ~_userfile.OptionByte[6]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480D, STM8_regType,  _userfile.OptionByte[7]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x480E, STM8_regType, ~_userfile.OptionByte[7]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x487E, STM8_regType,  _userfile.OptionByte[8]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x487F, STM8_regType, ~_userfile.OptionByte[8]); ERROR_TAKE;
				}
					break;
				case opSeries_9: // 这个实际上是未知的,没有找到相关手册去验证
				{
					cmdrtv = stm8WriteFlashAbyte(0x4800, STM8_regType,  _userfile.OptionByte[0]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4801, STM8_regType,                     0x00); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4802, STM8_regType,  _userfile.OptionByte[1]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4803, STM8_regType,  _userfile.OptionByte[2]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4807, STM8_regType,  _userfile.OptionByte[3]); ERROR_TAKE;
					cmdrtv = stm8WriteFlashAbyte(0x4808, STM8_regType,  _userfile.OptionByte[4]); ERROR_TAKE;
				}
					break;
				default:
					break;
				
			}

			// 恢复选项字节编程状态
			switch( STM8_optionByteType )
			{
				case opSeries_0:  // AF AH S系列
				case opSeries_1:
				case opSeries_7:
				case opSeries_8:
							{
								{  // 恢复默认值
									cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_IAPSR, 0x00 );
									ERROR_TAKE;
									cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_CR2, 0x00 );  // 禁止OPT位！
									ERROR_TAKE;
									cmdrtv = stm8WriteRegAbyte( STM8_AF_S_FLASH_CR2+1, (u8)~0x00 );
									ERROR_TAKE;
								}
							}
							break;
				
				
				case opSeries_2:
				case opSeries_3:
				case opSeries_4:
				case opSeries_5:
				case opSeries_6:
				case opSeries_9:
							{
								cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_IAPSR, 0x00 );
								ERROR_TAKE;
								cmdrtv = stm8WriteRegAbyte( STM8_AL_L_FLASH_CR2, 0x00 );
								ERROR_TAKE;
							}
							break;
			}
			
		}

			}
		}
	}
	{  // STM8复位运行！
		 // 实际中应用：下载完程序就运行，还是比较多的应用的。所以这个是需要的！
		WriteBuff[0]=0x00;			
		cmdrtv=SWIM_WOTF(DM_CSR2,1,WriteBuff);	  //DM_CSR2,Bit4 STALL =1
		{  // 把相关寄存器复位。客户反映这个会增加功耗。
			cmdrtv=SWIM_WOTF(SWIM_CSR,1,WriteBuff);
			SWIM_SRST();
			SWIM_SRST();
		}
		if (cmdrtv>0)
		{
			SWIM_Fini();
			return 5;
		}
	}
	SWIM_Fini();
	return returnNumber;
}


/**
	* @brief  写单个字节比较常用，且重复性比较高！
	* \Attention 前提是：必须已解锁芯片，且模式为标准编程模式。
	*/
static u8 stm8WriteFlashAbyte( u32 address,u8 type,u8 data )
{
	cmdrtv=SWIM_WOTF(address,1,&data);  // 在这个地址下写一个字节
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 7;
	}
	
	u8 i = 0;
	u32 regAddr = 0;
	if( type == ATL_L_Series )  // @包含L101x
	{
		regAddr = STM8_AL_L_FLASH_IAPSR;
	}
	else
	{
		regAddr = STM8_AF_S_FLASH_IAPSR;
	}
	
	u32 maxTime = 0xFFFFF;  // 防止超时！
	while( !(i& STM8_FLASH_EOF) )
	{
		cmdrtv=SWIM_ROTF(regAddr,1,&i);
		if(cmdrtv>0)
		{
			SWIM_Fini();
			return 8;
		}
		maxTime--;  // 解锁模式！
		if( maxTime == 0 )
		{
			return 9;
		}
	}
	return 0;
}

/**
	* @brief  写寄存器也常用！可以简化代码！
	* \Attention 可以看出，还有简化的空间，但是不是在这个时候进行简化！
	*/
static u8 stm8WriteRegAbyte( u32 address,u8 data )
{
	cmdrtv=SWIM_WOTF(address,1,&data);  // 在这个地址下写一个字节
	if (cmdrtv>0)
	{
		SWIM_Fini();
		return 7;
	}
	return 0;
}


