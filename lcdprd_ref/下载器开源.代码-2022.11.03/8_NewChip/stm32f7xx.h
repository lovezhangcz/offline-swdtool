/**
* @brief  
* @Author 
* @Data   2019.07.16
* \Attention 手册中的Cat.表示Category：分类
*/


#ifndef __STM32F7xx_H
#define __STM32F7xx_H


#include "stm32f10x.h"
#include "memoryManage.h" 
#include "SWD_Module.h"


#define STM32F7_MASS_KEY1  0x45670123
#define STM32F7_MASS_KEY2  0xCDEF89AB
#define STM32F7_OPT_KEY1   0x08192A3B
#define STM32F7_OPT_KEY2   0x4C5D6E7F

typedef union
{
	u8 IndexLen8[2048];
	u16 IndexLen16[1024];
	u32 IndexLen32[512];
}stm32f7xx_Flash_Density_TypeDef;

// 最多80页，0-79的值
#define stm32f7xx_FlashPage(x)    ((stm32f7xx_Flash_Density_TypeDef *) (0x10040000+x*2048))

typedef struct
{
  __IO u32 FLASH_ACR;   // 0x00
  __IO u32 FLASH_KEY;   
  __IO u32 FLASH_OPTKEY; 
  __IO u32 FLASH_SR;   // 0x00
	
  __IO u32 FLASH_CR;      // 0x8000 0000    
  __IO u32 FLASH_OPTCR;   // 0xC0FF AAFD
	__IO u32 FLASH_OPTCR1;  // 0x0040 0080
	__IO u32 FLASH_OPTCR2;  // 0x8000 00FF
}stm32f7xx_FLASH_TypeDef;

#define stm32f7xx_FLASH               ((stm32f7xx_FLASH_TypeDef *) ((u32)0x40023C00))  // F7手册已查询！



extern u32 DownloadFlashOf_stm32f7xx( _FileInformation _userfile );

#endif



