/**
* @brief  STM32 SPI 务必要根据SPI的特性要操作
* @Author CRJ QQ:43058655  WeChat：TC4300
* @Data   2017.12.11
*/

/**
 * @brief STM32系列总共有3个SPI接口,用一个函数写完三个SPI
 * \B     关于SPI，如果想写好，最终还是需要用来设计调试工具工程才是最OK的
 */
#include "stm32_spi.h"
#include "stm32f10x.h"

u8 DummySPI; // 全双工模式下,需要读出数据才能开始下个数据,不需要的时候就用这个来取值
vu32 SPIClock = 0;  // 这样定义全局变量，是为了防止被优化掉
SPIBuf SPIBuf1, SPIBuf2, SPIBuf3;

/**
 * @brief SPI速度=fAPB/(2^(SpeedSet+1)) (2/4/8/16/32/64/128/256) 
 *        APB1(SPI2/3)最大36MHZ  APB2(SPI1)最大72MHZ  从机不受分频控制最大fAPB/2 
 *       不包含引脚配置
 * @Attention SPI3引脚部分和JTAG(不和SW)引脚共用,配置时要注意
 *          此外注意NSS引脚的管理
 *          主从模式下引脚功能配置都不一样，这个要特别注意
 */
 
 
 /**
  * @brief 主模式下NSS为输出,MOSI为输出,MISO为输入,SCK为输出
	*        从模式下NSS为输入,MOSI为输入,MISO为输出,SCK为输入
	* \b     返回值为SPI设置的频率值
  */

u32 SPIInit(	SPI_TypeDef* 							SPIPort,
							SPIModeType 							MasterOrSlave,
							SPI_BaudRatePrescalerType SpeedSet,
							SPIDataLengthType         DataLength,
							SPIPolarityType           PolarityType,
							IRQEnableType 						irqEnable  )
{
	SystemCoreClockUpdate();  // 再次更新下时钟，虽然时钟几乎不变，但是初始化的时候，也不占用资源
	if( SPIPort == SPI1 )
	{
		RCC->APB2RSTR.SPI1Reset_RW = ENABLE;  // 复位一次
		RCC->APB2RSTR.SPI1Reset_RW = DISABLE;
		RCC->APB2ENR.SPI1ClockEnable_RW = ENABLE; // 使能时钟
		{ // 引脚配置
			if ( MasterOrSlave == SPI_MasterMode )
			{
				RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
				GPIOA->CRL.InOutMode4_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;  // NSS
				GPIOA->CRL.PinConfig4_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL; // NSS作为GPIO引脚
				GPIOA->ODR.OutputData4_RW = 1; // 输出为1
				GPIOA->CRL.InOutMode5_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;  // SCK
				GPIOA->CRL.PinConfig5_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
				GPIOA->CRL.InOutMode6_RW = GPIO_CR_INOUNTMODE_INPUT;  // MISO
				GPIOA->CRL.PinConfig6_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
				GPIOA->ODR.OutputData6_RW = 1; // 使能上拉
				GPIOA->CRL.InOutMode7_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;  // MOSI
				GPIOA->CRL.PinConfig7_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
				
				SPIPort->CR1.SoftwareSlaveManagement_RW = ENABLE;  // 启用内部引脚管理
				SPIPort->CR1.InternalSlaveSelect_RW = 1;  // 始终高电平
				SPIPort->CR2.SSOutputEnable_RW = DISABLE; // 禁止主模式SS输出
				
				SPIPort->CR1.MasterModeEnable_RW = ENABLE; // 主模式
			}
			else 
			{
				RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
				GPIOA->CRL.InOutMode4_RW = GPIO_CR_INOUNTMODE_INPUT;  // NSS
				GPIOA->CRL.PinConfig4_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN; // NSS作为复用引脚
				GPIOA->ODR.OutputData4_RW = 1; // 使能上拉
				GPIOA->CRL.InOutMode5_RW = GPIO_CR_INOUNTMODE_INPUT;  // SCK
				GPIOA->CRL.PinConfig5_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;  //
				GPIOA->ODR.OutputData5_RW = 1; // 使能上拉
				GPIOA->CRL.InOutMode6_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;  // MISO
				GPIOA->CRL.PinConfig6_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
				GPIOA->CRL.InOutMode7_RW = GPIO_CR_INOUNTMODE_INPUT;  // MOSI
				GPIOA->CRL.PinConfig7_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
				GPIOA->ODR.OutputData7_RW = 1; // 使能上拉
				
				SPIPort->CR1.SoftwareSlaveManagement_RW = DISABLE;  // 由硬件管理NSS电平
				
				SPIPort->CR1.MasterModeEnable_RW = DISABLE; // 从模式
			}
			if( irqEnable == EnableIrq)
			{
				NVIC_EnableIRQ( SPI1_IRQn );
			}
			else
			{
				NVIC_DisableIRQ( SPI1_IRQn );
			}
		}
	}
	else if( SPIPort == SPI2 )
	{
		RCC->APB1RSTR.SPI2Reset_RW = ENABLE;
		RCC->APB1RSTR.SPI2Reset_RW = DISABLE;
		RCC->APB1ENR.SPI2ClockEnable_RW = ENABLE;
		
		{ // 引脚配置
			if ( MasterOrSlave == SPI_MasterMode )
			{
				RCC->APB2ENR.IOPBClockEnable_RW = ENABLE;
//				GPIOB->CRH.InOutMode12_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;  // NSS
//				GPIOB->CRH.PinConfig12_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL; // NSS作为GPIO引脚
//				GPIOB->ODR.OutputData12_RW = 1; // 输出为1
				GPIOB->CRH.InOutMode13_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;  // SCK
				GPIOB->CRH.PinConfig13_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
				GPIOB->CRH.InOutMode14_RW = GPIO_CR_INOUNTMODE_INPUT;  // MISO
				GPIOB->CRH.PinConfig14_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
				GPIOB->ODR.OutputData14_RW = 1; // 使能上拉
				GPIOB->CRH.InOutMode15_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;  // MOSI
				GPIOB->CRH.PinConfig15_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
				
				SPIPort->CR1.SoftwareSlaveManagement_RW = ENABLE;  // 启用内部引脚管理
				SPIPort->CR1.InternalSlaveSelect_RW = 1;  // 始终高电平
				SPIPort->CR2.SSOutputEnable_RW = DISABLE; // 禁止主模式SS输出
				
				SPIPort->CR1.MasterModeEnable_RW = ENABLE; // 主模式
			}
			else 
			{
				RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
				GPIOB->CRH.InOutMode12_RW = GPIO_CR_INOUNTMODE_INPUT;  // NSS
				GPIOB->CRH.PinConfig12_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN; // NSS作为复用引脚
				GPIOB->ODR.OutputData12_RW = 1; // 使能上拉
				GPIOB->CRH.InOutMode13_RW = GPIO_CR_INOUNTMODE_INPUT;  // SCK
				GPIOB->CRH.PinConfig13_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;  //
				GPIOB->ODR.OutputData13_RW = 1; // 使能上拉
				GPIOB->CRH.InOutMode14_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;  // MISO
				GPIOB->CRH.PinConfig14_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
				GPIOB->CRH.InOutMode14_RW = GPIO_CR_INOUNTMODE_INPUT;  // MOSI
				GPIOB->CRH.PinConfig14_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
				GPIOB->ODR.OutputData14_RW = 1; // 使能上拉
				
				SPIPort->CR1.SoftwareSlaveManagement_RW = DISABLE;  // 由硬件管理NSS电平
				
				SPIPort->CR1.MasterModeEnable_RW = DISABLE; // 从模式
			}
			
			if( irqEnable == EnableIrq)
			{
				NVIC_EnableIRQ( SPI2_IRQn );
			}
			else
			{
				NVIC_DisableIRQ( SPI2_IRQn );
			}
		}
	}
//	else if( SPIPort == SPI3 )
//	{
//		RCC->APB1RSTR.SPI3Reset_RW = ENABLE;
//		RCC->APB1RSTR.SPI3Reset_RW = DISABLE;
//		RCC->APB1ENR.SPI3ClockEnable_RW = ENABLE;
//		if( irqEnable == EnableIrq)
//		{
//			NVIC_EnableIRQ( SPI3_IRQn );
//		}
//		else
//		{
//			NVIC_DisableIRQ( SPI3_IRQn );
//		}
//	}
	else
	{
		return FALSE;
	}
	{
		if( irqEnable == EnableIrq )  // 显然从模式下需要使用接收中断
		{
			SPIPort->CR2.ErrorInterruptEnable_RW = ENABLE;
			SPIPort->CR2.RxBufferNotEmptyInterruptEnable_RW = ENABLE;
		}
		else
		{
			SPIPort->CR2.ErrorInterruptEnable_RW = DISABLE;
			SPIPort->CR2.RxBufferNotEmptyInterruptEnable_RW = DISABLE;
		}
	}
		
	SPIPort->CR1.RecvOnly_RW = DISABLE;
	SPIPort->CR1.BidirectionDataModeEnable_RW = DISABLE;  // 和上面组合成全双工模式
	SPIPort->CR1.DataLength_RW = DataLength; // 0:8位  1:16位
	{
		switch(PolarityType)
		{
			case PolarityType00:
				SPIPort->CR1.ClockPolarity_RW = 0; 
				SPIPort->CR1.ClockPhase_RW = 0; // 模式00
				break;
			case PolarityType01:
				SPIPort->CR1.ClockPolarity_RW = 0; 
				SPIPort->CR1.ClockPhase_RW = 1; // 模式00
				break;
			case PolarityType10:
				SPIPort->CR1.ClockPolarity_RW = 1; 
				SPIPort->CR1.ClockPhase_RW = 0; // 模式00
				break;
			case PolarityType11:
				SPIPort->CR1.ClockPolarity_RW = 1; 
				SPIPort->CR1.ClockPhase_RW = 1; // 模式00
				break;
		}
		SPIPort->CR1.ClockPolarity_RW = 0; 
		SPIPort->CR1.ClockPhase_RW = 0; // 模式00
	}
	SPIPort->CR1.BaudratePrescale_RW = SpeedSet;  // 最大为7
	SPIPort->CR1.FrameFormatLSBSendFirstEnable_RW = DISABLE; //MSB先发送,一般都是MSB！
	SPIPort->CR1.SPIEnable_RW = ENABLE;
	
	{// 计算SPI时钟的程序，注意这个代码可能会被优化掉，但是也是无关紧要的
		if( SPIPort == SPI1 ) // APB2
		{
			SPIClock = APB2PCLK;
			SPIClock = ( SPIClock >> (SpeedSet+1) );
		}
		else if( (SPIPort == SPI2) || (SPIPort == SPI3) )  // APB1
		{
			SPIClock = APB1PCLK;
			SPIClock = ( SPIClock >> (SpeedSet+1) );
		}
		return SPIClock;
	}
}

/**
 * \B SPI数据寄存器是16位的，如果用8位的，则是使用低8位的数值
 * \B 警告：发现SR寄存器RXNE如果用单步调试则没有读DR寄存器就清除了，但是如果不用单步调试，从写DR寄存器开始，就会到读DR寄存器才会清除。（因为连接调试运行的时候，实际上要不断刷新，所以读了DR寄存器！）
 * \B 如果用主模式就不要用中断模式来操作了
 */

u8 SPISendAData( SPI_TypeDef* SPIPort,u8 SPIData )
{
	while( SPIPort->SR.TransmitBufferEmpty_R == FALSE ); // 当不空的时候卡住
	SPIPort->DR.Data_RW = SPIData;
	while( SPIPort->SR.RecvBufferNotEmpty_R == FALSE );
	return SPIPort->DR.Data_RW;
}

/**
 * \B SPI和特性，可能决定这发送数组和输出字符串，没有完全展示它的性能
 * \B 
 * \B 
 */

void SPISendBuf( SPI_TypeDef* SPIPort,u16* SPIDataBuf,u16 bufLength )
{
	for( u16 i=0;i<bufLength;i++)
	{
		GPIOA->ODR.OutputData4_RW = 0;
		while( SPIPort->SR.TransmitBufferEmpty_R == FALSE ); // 当不空的时候卡住
		SPIPort->DR.Data_RW = SPIDataBuf[i];
		while( SPIPort->SR.RecvBufferNotEmpty_R == FALSE );
		GPIOA->ODR.OutputData4_RW = 1;
		DummySPI = SPIPort->DR.Data_RW;
	}
}

void SPIPrintString(  SPI_TypeDef* SPIPort,char *puts )
{
	for(;*puts !=0;puts++)
	{
		GPIOA->ODR.OutputData4_RW = 0;
		while( SPIPort->SR.TransmitBufferEmpty_R == FALSE ); // 当不空的时候卡住
		SPIPort->DR.Data_RW = *puts;
		while( SPIPort->SR.RecvBufferNotEmpty_R == FALSE );
		GPIOA->ODR.OutputData4_RW = 1;
		DummySPI = SPIPort->DR.Data_RW;
	} 
}


/**
 * \B 这个中断函数，主要是用来处理接收的中断
 */

void SPI1_IRQHandler( )
{
	if( SPI1->SR.RecvBufferNotEmpty_R == TRUE )
	{
//		SPIBuf1.Buf[SPIBuf1.pWrite++] = SPI1->DR.Data_RW;
//		SPIBuf1.pWrite %= SPIBUF_MAXBUFF_SIZE;
	}
	else
	{
		
	}
}


