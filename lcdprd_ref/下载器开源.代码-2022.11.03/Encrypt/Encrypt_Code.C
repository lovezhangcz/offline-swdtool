/**
 * @brief 加密程序
 */
/**
 * @brief 一个函数陷阱：为了防止直接跳出程序，需要在程序中使用和主功能相关的算法，如果跳出，主函数也使用不了！
 * @Attention 
 */
#include "Encrypt_Code.H"
#include "STM32_CRC.H"

	volatile unsigned int CpuID[3] = {0,0,0};  // 使用全局变量，实际读出的CPUID值！
	volatile u32 CPUID[3] = {0,0,0}; // 写的CPUID值！
	
	volatile u8 CheckIDOK = FALSE;
	volatile u8 CRCCheckOK = FALSE;
	
void CheckUniqueIDInCaseOfCrack( void )
{
	volatile unsigned short int MemorySize = 0;

// 下面的几个地址定义用于计算得出！！！防止直接破解定位地址！
	volatile unsigned int MemorySizeAddress = 0;
	volatile unsigned int IDAddress0 = 0;
	volatile unsigned int IDAddress1 = 0;
	volatile unsigned int IDAddress2 = 0;
	
	volatile unsigned int baseCal0 = 0x1FF;
	volatile unsigned int baseCal1 = 0xFF7;
	volatile unsigned int baseCal2 = 0xE0;
	
	
	
	MemorySize = *(volatile unsigned short int*)(0x1FFFF7E0);  // 闪存容量值，明文防止破解！
	
	MemorySize = *(volatile unsigned short int*)( (baseCal0<<(5*4)) + (baseCal1<<(2*4)) + baseCal2 );  // 闪存容量值
	
	volatile unsigned int baseIDCal = 0x1FFFF7E;
	
	CpuID[0]=*(volatile unsigned int*)(0x1FFFF7E8);  // 不能省略，用一个明文值，但是实际对比的值，不是这个！
	CpuID[1]=*(volatile unsigned int*)(0x1FFFF7EC);
	CpuID[2]=*(volatile unsigned int*)(0x1FFFF7F0);
	
	CpuID[0]=*(volatile unsigned int*)( (baseIDCal<<4) + 0x08);
	CpuID[1]=*(volatile unsigned int*)( (baseIDCal<<4) + 0x0C);
	CpuID[2]=*(volatile unsigned int*)( (baseIDCal<<4) + 0x10);
	
	if( MemorySize != 0x40 )
	{
//		while(1);
		if( RCC->APB1ENR.USART2ClockEnable_RW == ENABLE ) // 防止硬件中断
		{
			if( USART2->BRR.All != 0xff ) 
				USART2->BRR.All = 0xff; // 这个时候波特率：141176
		}
		return;
	}
	
	
// 更改方式：改下面几个数字的数值
// 除此之外，其ID值也需要通过计算得出。可以通过写一个上位机的算法，和下位机对应。防止被破解。
// 直接写明文非常不好，可以采用一种不知名的CRC算法！或者直接找到96位也UID也不行。还需要对这个UID值，进行验证！
	
  CPUID[0] = 0x066CFF51;
	CPUID[1] = 0x54487851;
	CPUID[2] = 0x87025431;
	if(  ( CpuID[0] != CPUID[0] ) 
		|| ( CpuID[1] != CPUID[1] )
		|| ( CpuID[2] != CPUID[2] ) )
	{
//		while(1);
		if( RCC->APB1ENR.USART2ClockEnable_RW == ENABLE ) // 防止硬件中断
		{
			if( USART2->BRR.All != 0xAA ) 
				USART2->BRR.All = 0xAA; // 这个时候波特率：211764
		}
		return;
	}
	
	{ // 和主函数相关的功能。如果这个函数被跳出，则串口波特率不正确！
		// 如果程序没有跳出，则串口波特率可正常！
		// 注意串口初始化函数中串口波特率已经被
		if( RCC->APB1ENR.USART2ClockEnable_RW == ENABLE ) // 防止硬件中断
		{
			if( USART2->BRR.All != 0x4e )
				USART2->BRR.All = 0x4e;
		}
		CheckIDOK = TRUE;
	}
}

/** 下面为CRC校验算法
 * @attention 这个CRC是被人为改变的，防止太容易被发现！
 * 注意：CRC一定要先计算出来，而不是程序内部计算的。
 * 为什么再写一个函数，是因为：防止上个函数，被人为的加return函数，直接跨越！
 * 这个函数用于软陷阱：就是程序看似可正常运行，不卡住。
 **/

void CPUIDCRCCkeck( void )
{
	{
		volatile u16 CRCResult = 0;
		u8 IDDisassemble[12] = {  (u8)( CpuID[0]>>24 ),(u8)( CpuID[0]>>16 ),(u8)( CpuID[0]>>8 ),(u8)CpuID[0],
															(u8)( CpuID[1]>>24 ),(u8)( CpuID[1]>>16 ),(u8)( CpuID[1]>>8 ),(u8)CpuID[1],
															(u8)( CpuID[2]>>24 ),(u8)( CpuID[2]>>16 ),(u8)( CpuID[2]>>8 ),(u8)CpuID[2],		};
		if( CRCResult != 0xB13A )
		{
			if( RCC->APB1ENR.USART2ClockEnable_RW == ENABLE ) // 防止硬件中断
			{
				if( USART2->BRR.All != 0xee ) 
					USART2->BRR.All = 0xee; // 这个时候波特率：151260
			}
		return;
		}
	}
	{ // 和主函数相关的功能。如果这个函数被跳出，则串口引脚不配置。
		// 通过CRC后，才能配置串口引脚。
		if( RCC->APB2ENR.IOPAClockEnable_RW == ENABLE )
		{
			GPIOA->CRL.InOutMode2_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
			GPIOA->CRL.PinConfig2_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
			GPIOA->CRL.InOutMode3_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOA->CRL.PinConfig3_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
			GPIOA->ODR.OutputData3_RW = 1;
		}
		{
			CRCCheckOK = TRUE;
		}
	}
}

/**
 * @brief 加密方式3：通过屏蔽SWD功能的引脚！
 * @Attention 
 */

void MaskSWDPin( u8 TRUEOrFLASE )
{
	if( TRUEOrFLASE == TRUE )  // 这个时候屏蔽SWD引脚！  PA13 / PA14
	{
		AFIO->MAPR.SerialWireJTAGConfiguration_W = _0b0100;
	}
	else  // 这个时候恢复SWD引脚
	{
		AFIO->MAPR.SerialWireJTAGConfiguration_W = 0;
	}
}
