///**
// *@brief 
// *
// */

//#include "stm32f10x.h"
//#include "stm32_timer_delay.h"
//#include "STM8_Ifo.h"
//#include "memoryManage.h" 




//#ifndef __SWIM_MODULE_IMITATE_H
//#define __SWIM_MODULE_IMITATE_H


//#define COMMAND_SRST  0x00  // 系统复位命令！CSR-BIT5为1才有效！
//#define COMMAND_ROTF  0x01
//#define COMMAND_WOTF  0x02


//#define SWIM_CSR_ADDRESS     0x7F80  // bit5：访问DM模块！手册建议开始的时候写0x0A0.也就是Bit5和Bit7（禁止内部Reset！）
//#define CLK_SWIMCCR_ADDRESS  0x50CD  // 保持默认值就好！


//#define SWIM_RST_PIN								GPIOB->ODR.OutputData4_RW  // PB4引脚，作为输出引脚
//#define SWIM_IO_PIN_OUT							GPIOA->ODR.OutputData8_RW
//#define SWIM_IO_PIN_IN							GPIOA->IDR.InputData8_R

//																			
//#define SWD_TRANSFER_TIME_TO_STEP  3  // 用于SWD每一步操作的重试次数，至于总的每次下载的次数。是由软件设定的，不应该被宏定义

//																			
//																			
//																			
//																			
//																			
//																			
//																			
//																			
//																			
//																			
//																			
//void SWIM_Init( void );
//void SWIM_TimerInit( void );
//void SWIM_TimerBasicDivide9( u16 x_0_025us );
//void SWIM_TimerBasicTime( u32 numbers );


//void SWIM_SendBit( u8 bitNum );
//#endif





