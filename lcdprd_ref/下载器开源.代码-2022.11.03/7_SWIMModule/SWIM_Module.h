/**
 *@brief 这是日后的一个大工程，一定要建立的非常优秀
 *
 */

#include "stm32f10x.h"
#include "stm32_timer_delay.h"
#include "STM8_Ifo.h"
#include "memoryManage.h" 
#include "memoryManage.h" 
#include "lcd1_8inch.h" 
#include "w25qxx.h" 


#ifndef __SWIM_MODULE_H
#define __SWIM_MODULE_H

#define USE_PROGRESS	0
/** 
  * @brief  FLASH Status  
  */

typedef enum
{ 
  FLASH_BUSY = 1,
  FLASH_ERROR_PG,
  FLASH_ERROR_WRP,
  FLASH_COMPLETE,
  FLASH_TIMEOUT
}FLASH_Status;


#define SWIM_MAX_DLY					0xfffff  //
#define SWIM_MAX_RESEND_CNT		20

#define SWIM_CMD_BITLEN					3
#define SWIM_CMD_SRST					0x00	 //复位
#define SWIM_CMD_ROTF					0x01	 //SWIM 飞速读
#define SWIM_CMD_WOTF					0x02	 //SWIM 飞速写

#define SWIM_SYNC_CYCLES				128

//#define SYNCSWPWM_OUT_TIMER_MHZ			  72               // 要求主时钟为72MHZ
#define SYNCSWPWM_OUT_TIMER				    TIM1
#define SYNCSWPWM_OUT_TIMER_DMA			  DMA1_Channel2
#define SYNCSWPWM_IN_TIMER				    TIM2
#define SYNCSWPWM_IN_TIMER_RISE_DMA		DMA1_Channel7		//ch2
#define SYNCSWPWM_IN_TIMER_FALL_DMA		DMA1_Channel5		//ch1


#if HARDWARE_VERSION == MINI_VERSION
	#define SWIM_RST_HIGH()	do{GPIOB->ODR.OutputData6_RW = 1;GPIOB->ODR.OutputData1_RW = 1;}while(0);			 // mini版本实际上是共用两个引脚的。所以要注意！两个引脚的控制。
#else
	#define SWIM_RST_HIGH()	(GPIOB->ODR.OutputData4_RW = 1)			 
	#define SWIM_RST_LOW() 	(GPIOB->ODR.OutputData4_RW = 0)
#endif
#define SWIM_HIGH() (GPIOA->BSRR.SetOutputHigh8_W = ENABLE)			 // PA8 - 开漏模式，这个可能就是嵌位高电平！
#define SWIM_LOW() 	(GPIOA->ODR.OutputData8_RW = 0)      // PA8 
#define SWIM_IN1()	(GPIOA->IDR.InputData0_R)            // PA0
#define SWIM_IN2()	(GPIOA->IDR.InputData1_R)            // PA1

#define SET_U16_MSBFIRST(p, v)		\
	do{\
		*((uint8_t *)(p) + 0) = (((uint16_t)(v)) >> 8) & 0xFF;\
		*((uint8_t *)(p) + 1) = (((uint16_t)(v)) >> 0) & 0xFF;\
	} while (0)
#define SET_U24_MSBFIRST(p, v)		\
	do{\
		*((uint8_t *)(p) + 0) = (((uint32_t)(v)) >> 16) & 0xFF;\
		*((uint8_t *)(p) + 1) = (((uint32_t)(v)) >> 8) & 0xFF;\
		*((uint8_t *)(p) + 2) = (((uint32_t)(v)) >> 0) & 0xFF;\
	} while (0)
#define SET_U32_MSBFIRST(p, v)		\
	do{\
		*((uint8_t *)(p) + 0) = (((uint32_t)(v)) >> 24) & 0xFF;\
		*((uint8_t *)(p) + 1) = (((uint32_t)(v)) >> 16) & 0xFF;\
		*((uint8_t *)(p) + 2) = (((uint32_t)(v)) >> 8) & 0xFF;\
		*((uint8_t *)(p) + 3) = (((uint32_t)(v)) >> 0) & 0xFF;\
	} while (0)
#define SET_U16_LSBFIRST(p, v)		\
	do{\
		*((uint8_t *)(p) + 0) = (((uint16_t)(v)) >> 0) & 0xFF;\
		*((uint8_t *)(p) + 1) = (((uint16_t)(v)) >> 8) & 0xFF;\
	} while (0)
#define SET_U24_LSBFIRST(p, v)		\
	do{\
		*((uint8_t *)(p) + 0) = (((uint32_t)(v)) >> 0) & 0xFF;\
		*((uint8_t *)(p) + 1) = (((uint32_t)(v)) >> 8) & 0xFF;\
		*((uint8_t *)(p) + 2) = (((uint32_t)(v)) >> 16) & 0xFF;\
	} while (0)
#define SET_U32_LSBFIRST(p, v)		\
	do{\
		*((uint8_t *)(p) + 0) = (((uint32_t)(v)) >> 0) & 0xFF;\
		*((uint8_t *)(p) + 1) = (((uint32_t)(v)) >> 8) & 0xFF;\
		*((uint8_t *)(p) + 2) = (((uint32_t)(v)) >> 16) & 0xFF;\
		*((uint8_t *)(p) + 3) = (((uint32_t)(v)) >> 24) & 0xFF;\
	} while (0)
	
#define SET_LE_U16(p, v)			SET_U16_LSBFIRST(p, v)
#define SET_LE_U24(p, v)			SET_U24_LSBFIRST(p, v)
#define SET_LE_U32(p, v)			SET_U32_LSBFIRST(p, v)
#define SET_BE_U16(p, v)			SET_U16_MSBFIRST(p, v)
#define SET_BE_U24(p, v)			SET_U24_MSBFIRST(p, v)
#define SET_BE_U32(p, v)			SET_U32_MSBFIRST(p, v)
	
	
	
	
#define SWIM_CSR					0x00007F80
#define DM_CSR2						0x00007F99
#define STM8_FLASH_KEY		0x00005062
#define STM8_FLASH_CR2		0x0000505B
#define STM8_FLASH_IAPCSR	0x0000505f
#define STM8_FLASH_EOF		0x04

	
	
// 下面表示的是寄存器的地址！
#define STM8_AF_S_FLASH_CR1   0x505A
#define STM8_AF_S_FLASH_CR2   0x505B
#define STM8_AF_S_FLASH_NCR2  0x505C
#define STM8_AF_S_FLASH_FPR   0x505D
#define STM8_AF_S_FLASH_NFPR  0x505E
#define STM8_AF_S_FLASH_IAPSR 0x505F
#define STM8_AF_S_FLASH_PUKR  0x5062
#define STM8_AF_S_FLASH_DUKR  0x5064


#define STM8_AL_L_FLASH_CR1   0x5050
#define STM8_AL_L_FLASH_CR2   0x5051
#define STM8_AL_L_FLASH_PUKR  0x5052
#define STM8_AL_L_FLASH_DUKR  0x5053
#define STM8_AL_L_FLASH_IAPSR 0x5054

#define STM8_MASS_KEY_FLASH_1 0x56
#define STM8_MASS_KEY_FLASH_2 0xAE

#define STM8_MASS_KEY_EEPROM_1 0xAE
#define STM8_MASS_KEY_EEPROM_2 0x56

#define ERROR_TAKE 	{ if( cmdrtv > 0) {return 7;} }   // 用于简化显示！出错则进行返回操作！


extern void SWIM_GPIO_Init(void);
extern void STMProTest( void );
	
extern u32 DownloadFlashOfSTM8( _FileInformation _userfile  );  // 实际下载的函数体
	
#endif





