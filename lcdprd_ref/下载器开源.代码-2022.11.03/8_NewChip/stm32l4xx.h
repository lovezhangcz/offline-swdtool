/**
* @brief  
* @Author 
* @Data   2019.03.18
* \Attention 手册中的Cat.表示Category：分类
*/

#ifndef __STM32L4XX_H
#define __STM32L4XX_H


#include "stm32f10x.h"
#include "memoryManage.h" 
#include "SWD_Module.h"

#define L4_UNLOCKCODE_PRGKEY1 ( 0x45670123 ) // 解锁操作Flash
#define L4_UNLOCKCODE_PRGKEY2 ( 0xCDEF89AB )
#define L4_UNLOCKCODE_OPTKER1 ( 0x08192A3B ) // 选项字节操作
#define L4_UNLOCKCODE_OPTKER2 ( 0x4C5D6E7F )  

typedef struct
{
  __IO u32 ACR;  
  __IO u32 FLASH_PDKEYR; 
  __IO u32 FLASH_KEYR;                
  __IO u32 FLASH_OPTKEY;  
	__IO u32 FLASH_SR;
	__IO u32 FLASH_CR;
	__IO u32 FLASH_ECCR;   
	__IO u32 res0;  // 这里有个默认值
	__IO u32 FLASH_OPTR;  
	__IO u32 FLASH_PCROP1SR;
	__IO u32 FLASH_PCROP1ER;
	__IO u32 FLASH_WRP1AR;
	__IO u32 FLASH_WRP1BR;
}L4_FLASH_TypeDef;

#define L4_FLASH               ((L4_FLASH_TypeDef *) ((u32)0x40022000))


typedef struct
{ // 注意，原码与补码共64位。不像其它的是占用32位。
  __IO u32 OPT_RDP;    // RDP与其它一些,默认值：0xFFEFF8AA
  __IO u32 ComOPT_RDP; 
  __IO u32 PCROP1_STRT;   // 0xFFFFFFFF
  __IO u32 ComPCROP1_STRT;       
  __IO u32 PCROP1_END;    // 0xFFFF0000
  __IO u32 ComPCROP1_END;  	
	
	__IO u32 WRP1A;  // 0xFF00FFFF
	__IO u32 Com_WRP1A;
	__IO u32 WRP1B;  // 0xFF00FFFF
	__IO u32 Com_WRP1B;
}L4_FLASH_OPT_TypeDef;

#define L4_OPTION               ((L4_FLASH_OPT_TypeDef *) ((u32)0x1FFF7800))  // 用于确定地址


u32 DownloadFlashOfSTM32L4xx( _FileInformation _userfile );


#endif




