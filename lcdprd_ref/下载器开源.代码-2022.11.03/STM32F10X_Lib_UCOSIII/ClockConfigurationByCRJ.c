/**************************************************************************//**
 * @file     ClockConfigurationByCRJ.c
 * @brief    STM32初始化时钟配置工具：只适合STM32小容量中容量大容量的时钟配置,不适合互联型配置
 *       参考STM32中文参考手册P56的时钟树或STM32英文参考手册P93的ClockTree
 *          
 * @version  V1.00
 * @date     2. July 2016
 *
 * @Author  CRJ  QQ：43058655  WeChat：TC4300
 *
 * @par
 ******************************************************************************/

#include "stm32f10x.h"
#include "ClockConfigurationByCRJ.h"
#include <stdint.h>
#include "Encrypt_Code.H"

/*
//-------- <<< Use Configuration Wizard in Context Menu >>> ------------------
*/

// <e> Flash Accelerator Configuration (Author:CRJ QQ：43058655 WeChat：TC4300)
//   <i> 特别注意：一定要配置正确的Flash访问等待周期，否则程序将会跑飞(可能的表现为：调试中调试窗口显示 FFFFFFFF DCD 0xFFFFFFFF)
///  <o1.0..2> FLASHTIM: Flash Access Time
//               <0=> 0 Wait State ( 0MHZ 小于 SYSCLK 小于等于 24MHZ)
//               <1=> 1 Wait State (24MHZ 小于 SYSCLK 小于等于 48MHZ)
//               <2=> 2 Wait State (48MHZ 小于 SYSCLK 小于等于 72MHZ)
//
// </e>
//

#define FLASH_SETUP   1
#define FLASH_ACR_VAL 0x00000002

// <e0> Clock Configuration (作者：程人杰 QQ：43058655 WeChat：TC4300)
//    <i> 当使能时，用本函数初始化时配置时钟。若不使能，请自己时钟配置
//    <i> 特别注意1：时钟上存在逻辑关系，不正确的逻辑的关系会使程序卡死(比如：关闭了HSE,但PLL却使用了HSE作为时钟源)
//    <i> 特别注意2：请先配置正确的Flash访问选项，否则程序会跑飞(可能的表现为：调试中调试窗口显示 FFFFFFFF DCD 0xFFFFFFFF)
//    <h> 时钟使能选项
//    		<o1.0> HSI Enable   (系统复位默认开启)
//          <i> 这个时钟不是想关就关的，程序中设置最后一个关闭时钟
//          <i> 当时钟被直接或间接的用作系统时钟时，关闭无效！
//        <o1.18> HSE Bypass (Must set before HSE enable)
//          <i> 字面意思：绕过HSE
//   			<o1.16> HSE Enable
//          <i> 当时钟被直接或间接的用作系统时钟时，关闭无效！
//    		<o7.0> LSI Enable
//    		<o6.0> LSE Enable
//          <i> 想要开外部LSE，需要开启PWR，然后开启PWR_CR下的DPB！
//          <i> 外部若不存在时钟，程序将会卡死
//    </h>
//
//				<o2.0..1> Clock Source For SysCLK  (SYSCLK最大72MHZ)
//					<0=> HSI
//					<1=> HSE
//					<2=> PLL
//
//
//    <e1.24> PLL Configuration 
//        <i> 选择此项表示启用PLL，不选用表示停用PLL
//        <i> 当PLL被用作系统时钟时，关闭无效！
//				<o2.16> PLL Source 
//					<0=> HSIDiv2
//					<1=> PLLXTPRE 选择的时钟
//				<o2.17> PLLXTPRE
//					<0=> HSE
//					<1=> HSEDiv2
//				<o2.18..21> PLLMultiplicationFactor(PLLMUL) 
//          <i>  注意:16倍频用的是PLLMUL = 14，等于15也可以，但是宏定义的__CORE_CLK的计算会受影响
//          <i>  提示：倍频16的设置PLL = 14/15 都是一样效果
//					<0=> 2 倍频
//					<1=> 3 倍频
//					<2=> 4 倍频
//					<3=> 5 倍频
//					<4=> 6 倍频
//					<5=> 7 倍频
//					<6=> 8 倍频
//					<7=> 9 倍频
//					<8=> 10倍频
//					<9=> 11倍频
//					<10=> 12倍频
//					<11=> 13倍频
//					<12=> 14倍频
//					<13=> 15倍频
//					<14=> 16倍频 (PLLMUL = 14)
//          <15=> 16倍频 (PLLMUL = 15)
//      </e>
//
//     <e6.15> RTC Enable (注意：RTC时钟，暂时已关闭，配置无效)
//     <o6.8..9> RTC Clock Source
//					<1=> LSE
//					<2=> LSI
//					<3=> HSEDiv128
//     </e>
//
//     <e2.26> Main clock output enable
//        <i> 配置时钟输出，一定要注意逻辑关系，因为程序无法检查是否时钟已输出
//        <i> 比如：配置时钟输出为0x5：表示HSI时钟输出，但是HSI却没打开。
//        <i> MCO寄存器本身不能检查时钟输出的有效性
//     		<o2.24..25> Main Clock Output
//					<0=> SYSCLK
//					<1=> HSI
//					<2=> HSE
//					<3=> PLLClockDiv2
//      </e>
//
//     <h> Clock Prescale
//				<o2.4..7> AHB  Prescaler
//					<0=>  1
//					<8=>  2
//					<9=>  4
//					<10=> 8
//					<11=> 16
//					<12=> 64
//					<13=> 128
//					<14=> 256
//					<15=> 512
//
//				<o2.8..10> APB1 Prescaler (最大36MHZ)
//          <i> APB1/2的时钟都来源于AHB时钟
//					<0=> 1
//					<4=> 2
//					<5=> 4
//					<6=> 8
//					<7=> 16
//
//				<o2.11..13> APB2 Prescaler (最大72MHZ)
//          <i> APB1/2的时钟都来源于AHB时钟
//					<0=> 1
//					<4=> 2
//					<5=> 4
//					<6=> 8
//					<7=> 16
//
//				<o2.22> USB Prescaler  (USB时钟必须是48MHZ)
//          <i> USB的时钟都来源于PLL时钟
//					<0=> PLLCLKDiv1.5 (PLLCLK must 72MHZ)
//					<1=> PLLCLK (PLLCLK must 48MHZ)
//
//				<o2.14..15> ADC Prescaler (ADC的时钟都来源于APB2时钟)
//          <i> ADC的时钟都来源于APB2时钟,不得超过14MHZ
//					<0=> 2
//					<1=> 4
//					<2=> 6
//					<3=> 8
//     </h>
//
//
//		 <h> AHB Clock Enable
//         <o3.10> SDIO Enable
//         <o3.8> FSMC Enable
//         <o3.6> CRC Enable
//         <o3.4> FLITF Enable  (默认使能)
//         <i> 睡眠模式时闪存接口电路0(关闭)/1(开启)
//         <o3.2> SRAM Enable  (默认使能)
//         <i> 睡眠模式时SRAM时钟0(关闭)/1(开启)
//         <o3.1> DMA2 Enable
//         <o3.0> DMA1 Enable
//     </h>
//
//		 <h> APB1 Clock Enable 最大36MHZ
//         <i> 最大36MHZ
//         <o5.29> DAC Enable
//         <o5.28> PWR Enable
//         <o5.27> BKP Enable
//         <o5.25> CAN Enable
//         <o5.23> USB Enable
//         <o5.22> I2C2 Enable
//         <o5.21> I2C1 Enable
//         <o5.20> UART5 Enable
//         <o5.19> UART4 Enable
//         <o5.18> USART3 Enable
//         <o5.17> USART2 Enable
//         <o5.15> SPI3 Enable
//         <o5.14> SPI2 Enable
//         <o5.11> WWDG Enable
//         <o5.5> TIM7 Enable
//         <o5.4> TIM6 Enable
//         <o5.3> TIM5 Enable
//         <o5.2> TIM4 Enable
//         <o5.1> TIM3 Enable
//         <o5.0> TIM2 Enable
//     </h>
//
//		 <h> APB2 Clock Enable 最大72MHZ
//         <i> 最大72MHZ
//         <o4.15> ADC3 Enable
//         <o4.14> USART1 Enable
//         <o4.13> TIM8 Enable
//         <o4.12> SPI1 Enable
//         <o4.11> TIM1 Enable
//         <o4.10> ADC2 Enable
//         <o4.9> ADC1 Enable
//         <o4.8> IOPG Enable
//         <o4.7> IOPF Enable
//         <o4.6> IOPE Enable
//         <o4.5> IOPD Enable
//         <o4.4> IOPC Enable
//         <o4.3> IOPB Enable
//         <o4.2> IOPA Enable
//         <o4.0> AFIO Enable
//     </h>
//     
// </e>
//

/** 
* @CRJ Clock Definition
*/
#define CLOCK_SETUP       1            // 0 决定是否启用本程序来配置时钟 
#define RCC_CR_VAL        0x01010001   // 1
#define RCC_CFGR_VAL      0x011F440E   // 2 
#define RCC_AHBENR_VAL    0x00000017   // 3
#define RCC_APB2ENR_VAL   0x00000001   // 4
#define RCC_APB1ENR_VAL   0x08800011   // 5
#define RCC_BDCR_VAL      0x00000300   // 6
#define RCC_CSR_VAL       0x00000000   // 7

/*
//-------- <<< end of configuration section >>> ------------------------------
*/

/*----------------------------------------------------------------------------
  Define clocks  RTC时钟，LSE,LSI时钟的值，本程序中暂时没有配置
 *----------------------------------------------------------------------------*/
#define XTAL        (16000000UL)        /* Oscillator frequency               */
#define OSC_CLK     (     XTAL)        /* Main oscillator frequency          */
// #define LSE_CLK     (  32768UL)        /* LSE oscillator frequency           */
// #define LSI_CLK     (  40000UL)        /* LSI oscillator frequency           */
#define IRC_OSC     (8000000UL)        /* Internal RC oscillator frequency   */

/* Determine core clock frequency according to settings */
#if ( CLOCK_SETUP == 1)
	#if ( (RCC_CFGR_VAL&0x03) == 0)  // HSI
		#define __CORE_CLK  IRC_OSC
	#elif ( (RCC_CFGR_VAL&0x03) == 1) // HSE
		#define __CORE_CLK  OSC_CLK
	#elif ( (RCC_CFGR_VAL&0x03) == 2) // PLL
					#if ( (RCC_CFGR_VAL&(1<<16)) == (0<<16) )  // HSI/2 作为PLL源
						#if ( ((RCC_CFGR_VAL>>18)&0xF) == 15  ) 
							#define __CORE_CLK ( IRC_OSC/2 * 16 )
						#else
							#define __CORE_CLK ( IRC_OSC/2 * (((RCC_CFGR_VAL>>18)&0xF)+2) )  // 
						#endif
					#elif ( (RCC_CFGR_VAL&(1<<16)) == (1<<16) ) // HSE 或 HSE/2 作为PLL源
						#if ( (RCC_CFGR_VAL&(1<<17)) == (0<<17) )  // HSE
							#if ( ((RCC_CFGR_VAL>>18)&0xF) == 15  ) 
								#define __CORE_CLK ( OSC_CLK * 16 )
							#else
								#define __CORE_CLK ( OSC_CLK * (((RCC_CFGR_VAL>>18)&0xF)+2) ) 
							#endif
						#else  // HSE/2
							#if ( ((RCC_CFGR_VAL>>18)&0xF) == 15  ) 
								#define __CORE_CLK ( OSC_CLK/2 * 16 )
							#else
								#define __CORE_CLK ( OSC_CLK/2 * (((RCC_CFGR_VAL>>18)&0xF)+2) ) 
							#endif
						#endif
					#endif
	#else
		#waring "请选择正确的SYS时钟源"
	#endif
	
	#if ( __CORE_CLK > 72000000UL )
		#error "SYSCLK 超出范围,SYSCLK最大72MHZ,请检查后重新配置"
	#endif
	
	#if ( ((RCC_CFGR_VAL>>4)&0xF) <= 0x7)  // AHB时钟相等
		#define __AHB_CLK  ( __CORE_CLK )
	#elif ( ((RCC_CFGR_VAL>>4)&0xF) = 0x8)   // 0b1000
		#define __AHB_CLK  ( __CORE_CLK/2 ) 
	#elif ( ((RCC_CFGR_VAL>>4)&0xF) = 0x9)   // 0b1001
		#define __AHB_CLK  ( __CORE_CLK/4 ) 
	#elif ( ((RCC_CFGR_VAL>>4)&0xF) = 0xA)   // 0b1010
		#define __AHB_CLK  ( __CORE_CLK/8 )
	#elif ( ((RCC_CFGR_VAL>>4)&0xF) = 0xB)   // 0b1011
		#define __AHB_CLK  ( __CORE_CLK/16 )
	#elif ( ((RCC_CFGR_VAL>>4)&0xF) = 0xC)   // 0b1100
		#define __AHB_CLK  ( __CORE_CLK/64 )
	#elif ( ((RCC_CFGR_VAL>>4)&0xF) = 0xD)   // 0b1101
		#define __AHB_CLK  ( __CORE_CLK/128 ) 
	#elif ( ((RCC_CFGR_VAL>>4)&0xF) = 0xE)   // 0b1110
		#define __AHB_CLK  ( __CORE_CLK/256 ) 
	#elif ( ((RCC_CFGR_VAL>>4)&0xF) = 0xF)   // 0b1111
		#define __AHB_CLK  ( __CORE_CLK/512 )
	#endif
	
	#if ( ((RCC_CFGR_VAL>>8)&0x7) <= 0x3)   // 
		#define __APB1_CLK  ( __AHB_CLK )
	#elif ( ((RCC_CFGR_VAL>>8)&0x7) == 0x4) 
		#define __APB1_CLK  ( __AHB_CLK/2 )
	#elif ( ((RCC_CFGR_VAL>>8)&0x7) == 0x5) 
		#define __APB1_CLK  ( __AHB_CLK/4 )
	#elif ( ((RCC_CFGR_VAL>>8)&0x7) == 0x6) 
		#define __APB1_CLK  ( __AHB_CLK/8 )
	#elif ( ((RCC_CFGR_VAL>>8)&0x7) == 0x7) 
		#define __APB1_CLK  ( __AHB_CLK/16 )
	#endif
	
	#if ( __APB1_CLK > 36000000UL )
		#error "APB1 超出范围 APB1 最大时钟36MHZ,请检查后重新配置"
	#endif
		
	#if ( ((RCC_CFGR_VAL>>11)&0x7) <= 0x3)   // 
		#define __APB2_CLK  ( __AHB_CLK )
	#elif ( ((RCC_CFGR_VAL>>11)&0x7) == 0x4) 
		#define __APB2_CLK  ( __AHB_CLK/2 )
	#elif ( ((RCC_CFGR_VAL>>11)&0x7) == 0x5) 
		#define __APB2_CLK  ( __AHB_CLK/4 )
	#elif ( ((RCC_CFGR_VAL>>11)&0x7) == 0x6) 
		#define __APB2_CLK  ( __AHB_CLK/8 )
	#elif ( ((RCC_CFGR_VAL>>11)&0x7) == 0x7) 
		#define __APB2_CLK  ( __AHB_CLK/16 )
	#endif
	
	#if ( __APB2_CLK > 72000000UL )
		#error "APB2 超出范围,APB2最大时钟72MHZ,请检查后重新配置"
	#endif
	
#endif

/**
*@Brief 宏定义时钟，若程序中更改过时钟，请使用SystemCoreClockUpdate更新下面几个时钟
*/
u32 SystemCoreClock = __CORE_CLK;
u32 AHBPCLK  = __AHB_CLK;
u32 APB1PCLK = __APB1_CLK;
u32 APB2PCLK = __APB2_CLK;

/**
* @Brief     根据寄存器值，来更新SYSCLK AHBCLK APB1 APB2 CLK
* @Important 若时钟更新显示：SYSCLK/APB1/APB2的时钟不在范围，会返回0。否则返回1
*/
u8 SystemCoreClockUpdate ( void )
{
	switch ( RCC->CFGR.SystemClockSwithchStatus_R )
	{
		case 0: SystemCoreClock = IRC_OSC;
			break;
		case 1: SystemCoreClock = OSC_CLK;
			break;
		case 2:   // 时钟来自于PLL
		{
			if ( RCC->CFGR.PLLSourceClock_RW == 0 )  // 来自 HSI/2
			{
				if ( RCC->CFGR.PLLMultiplicationFactor_RW == 15 )
					SystemCoreClock = IRC_OSC/2 * 16;
				else
					SystemCoreClock = IRC_OSC/2 * (RCC->CFGR.PLLMultiplicationFactor_RW + 2);
			}
			else  // 来自 HSE 或 HSE/2
			{
				if ( RCC->CFGR.HSEDivForPLLEntry_RW == 0 )  // HSE
				{
					if ( RCC->CFGR.PLLMultiplicationFactor_RW == 15 )
						SystemCoreClock = OSC_CLK * 16;
					else 
						SystemCoreClock = OSC_CLK * (RCC->CFGR.PLLMultiplicationFactor_RW + 2);
				}
				else // HSE/2
				{
					if ( RCC->CFGR.PLLMultiplicationFactor_RW == 15 )
						SystemCoreClock = OSC_CLK/2 * 16;
					else 
						SystemCoreClock = OSC_CLK/2 * (RCC->CFGR.PLLMultiplicationFactor_RW + 2);
				}
			}
		}
		break;
		default :break;
 }
	
 switch ( RCC->CFGR.AHBPrescale_RW )   // AHB
 {
	 case 0:case 1:case 2:case 3:case 4:case 5:case 6:case 7:
			AHBPCLK = SystemCoreClock;
			break;
	 case 8: AHBPCLK = SystemCoreClock/2;
			break;
	 case 9: AHBPCLK = SystemCoreClock/4;
			break;
	 case 10:AHBPCLK = SystemCoreClock/8;
			break;
	 case 11:AHBPCLK = SystemCoreClock/16;
			break;
	 case 12:AHBPCLK = SystemCoreClock/64;
			break;
	 case 13:AHBPCLK = SystemCoreClock/128;
			break;
	 case 14:AHBPCLK = SystemCoreClock/256;
			break;
	 case 15:AHBPCLK = SystemCoreClock/512;
			break;
	 default:break;
 }
 
 switch ( RCC->CFGR.APB1Prescale_RW)  // APB1
 {
	 case 4: APB1PCLK = AHBPCLK/2;
		 break;
	 case 5: APB1PCLK = AHBPCLK/4;
		 break;
	 case 6: APB1PCLK = AHBPCLK/8;
		 break;
	 case 7: APB1PCLK = AHBPCLK/16;
		 break;
	 default:break;
 }
 
 switch ( RCC->CFGR.APB2Prescale_RW)  //APB2
 {
	 case 4: APB2PCLK = AHBPCLK/2;
		 break;
	 case 5: APB2PCLK = AHBPCLK/4;
		 break;
	 case 6: APB2PCLK = AHBPCLK/8;
		 break;
	 case 7: APB2PCLK = AHBPCLK/16;
		 break;
	 default:break;
 }
 
 if ( (SystemCoreClock > 72000000UL)||(AHBPCLK>72000000)||(APB1PCLK>36000000UL)||(APB2PCLK>72000000) )
	 return FALSE;
// if ( (SystemCoreClock = 0UL)||(AHBPCLK=0)||(APB1PCLK=0)||(APB2PCLK=0) )
//	 return FALSE;
 else
	 return TRUE;
 
}

u8 isHIDVersion;  // 当为1的时候，就是HID的版本。

void SystemInit ( void )
{
	{  // 进行版本的判断。
		// 最后采用的方案是：根据之前的时钟配置来的，因为其引脚判断好像有问题！
		// HID之前的Boot配置是：使用HSE/2为主PLL的SOURCE，使用的9倍频。如果是这样，则认为是HID，否则认为是旧版本的。
		// 日后：新版本的统一使用这样的时钟配置，不要改变！旧版本，则使用：64MHZ的时钟配置！
		{  // 等待时间，进行判断
			if( (RCC->CFGR.PLLMultiplicationFactor_RW == 7 ) && ( RCC->CFGR.HSEDivForPLLEntry_RW == 1 ) )
			{
				isHIDVersion = 1;  // 表明为HID的版本，需要使用USB
			}
			else
			{
				isHIDVersion = 0;
			}
		}
		{ // 
			if( ENABLE_APP_ISSUE == 0 )  // 关闭发布版本
			{
				if( FORCE_VERSION == 0 )
				{  
					isHIDVersion = 1;  // 强制新版本
				}
				else
				{
					isHIDVersion = 0;  // 强制旧版本
				}
			}
		}
	}
	
	
	#if ( FLASH_SETUP == 1 )
		FLASH->ACR.Latency_RW = FLASH_ACR_VAL;
	#endif
	
	#if ( CLOCK_SETUP == 1 )
	{
		if( isHIDVersion == 1 )
		{  //  HID版本的按照之前的时钟来做。
			RCC->CR.All |= ((1<<18)&RCC_CR_VAL); // HSE Bypass
		
			if( (RCC_CR_VAL&(1<<16)) == (1<<16) )   // 表示开启HSE
			{
				RCC->CR.All |= (1<<16);
				while( RCC->CR.HSEReady_R == FALSE );  // 没有就绪时，卡住，等待就绪
			}
			if( (RCC_BDCR_VAL&(1<<0)) == (1<<0))   // 表示开启LSE
			{
				RCC->APB1ENR.PWRClockEnable_RW = 1;
				PWR->CR.AllowAccessRTCAndBackupRegister_RW = 1;
				RCC->BDCR.All |= (1<<0);
				while( RCC->BDCR.LSEReady_R == FALSE);
			}
			if( (RCC_CSR_VAL&(1<<0)) == (1<<0) )   // 开启LSI
			{
				RCC->CSR.All |= (1<<0);
				while( RCC->CSR.LSIReady_R == FALSE );
			}
			
			if( (RCC_CR_VAL&(1<<24)) == (1<<24) )  // PLL 使能，SYS时钟源，可能是PLL，如果不先启用PLL，则SYS时钟源状态就会卡住
			{
				RCC->CFGR.All |= ((u32)(1<<16)&(RCC_CFGR_VAL));  // PLL Source
				RCC->CFGR.All |= ((u32)(1<<17)&(RCC_CFGR_VAL));  // HSE PLL Source
				RCC->CFGR.All |= ((u32)(0xF<<18)&(RCC_CFGR_VAL)); // 倍频值
				RCC->CR.All |= (1<<24);
				while( RCC->CR.PLLReady_R == FALSE );  // PLL就绪退出
			}
			
			RCC->CFGR.All |= ((u32)(0x00000003)&RCC_CFGR_VAL); // 选择SYSCLK时钟源
			while( !(RCC->CFGR.SystemClockSwithchStatus_R ==  ((0x03)&RCC_CFGR_VAL)) );  //确认时钟源
			
	//		if( (RCC_BDCR_VAL&(1<<15)) == (1<<15) )  // RTC时钟开启
	//		{
	//			RCC->APB1ENR.PWRClockEnable_RW = 1;
	//			PWR->CR.AllowAccessRTCAndBackupRegister_RW = 1;
	//			RCC->BDCR.All |= (1<<15);
	//			RCC->BDCR.All |= ((0x03<<8)&(RCC_BDCR_VAL));
	//			while( !((RCC->BDCR.All&(0x03<<8)) == ((0x03<<8)&(RCC_BDCR_VAL))));
	//		}
			
			if( (RCC_CR_VAL&(1<<0)) == (1<<0))         // 最后再关闭HSI，因为HSI影响内容比较多，而且不是想关就关的
			{
			}
			else
			{
				RCC->CR.All &= ~(u32)0x00000001;
				while( RCC->CR.HSIReady_R == TRUE);  // 时钟就绪时，卡住，等待关闭
			}
			
			if( (RCC_CFGR_VAL&(1<<26)) == (1<<26) ) // MCO 使能
			{
				RCC->CFGR.All |= ((0x03<<24)&(RCC_CFGR_VAL));
				RCC->CFGR.All |= (1<<26);
			}
			
			RCC->CFGR.All |= ((1<<22)&RCC_CFGR_VAL);  // USB prescale
			RCC->CFGR.All |= ((0x03<<14)&RCC_CFGR_VAL);  // ADC prescale
			RCC->CFGR.All |= ((0x0F<<4)&RCC_CFGR_VAL);  // AHB prescale
			RCC->CFGR.All |= ((0x07<<11)&RCC_CFGR_VAL);  // APB2 prescale
			RCC->CFGR.All |= ((0x07<<8)&RCC_CFGR_VAL);  // APB1 prescale
			
			RCC->AHBENR.All = RCC_AHBENR_VAL;
			RCC->APB2ENR.All = RCC_APB2ENR_VAL;
			RCC->APB1ENR.All = RCC_APB1ENR_VAL;
		}
		else
		{  // 非HID版本的，使用内部的时钟，时钟重新配置！配置成64MHZ
			RCC->CR.HSIEnable_RW = 1;
			while( RCC->CR.HSIReady_R != 1 );
			
			{ // 更改时钟源，否则PLL关闭不了。之前Boot配置的就不行！
				RCC->CFGR.SystemClockSwithch_RW = 0;
				while( RCC->CFGR.SystemClockSwithchStatus_R != 0 );
			}
			RCC->CR.PLLEnable_RW = 0; // 关闭PLL。
			while( RCC->CR.PLLReady_R != 0 );  // 直到关闭！
			RCC->CFGR.SystemClockSwithch_RW = 0x2;  // PLL输出作为时钟
			RCC->CFGR.AHBPrescale_RW = 0;  // 不分频
			RCC->CFGR.APB1Prescale_RW = 4;  // 2分频，64/2 = 32MHZ
			RCC->CFGR.APB2Prescale_RW = 0;  // 不分频
			
			RCC->CFGR.PLLSourceClock_RW = 0; // 选择内部的HSI/2为时钟源。
			
			RCC->CFGR.PLLMultiplicationFactor_RW = 0xF; // 4*16 = 64M的时钟
		
			RCC->CR.PLLEnable_RW = 1; // 开启PLL。
			while( RCC->CR.PLLReady_R != 1 );  // 直到开启！
		}
	}
	#endif
}

