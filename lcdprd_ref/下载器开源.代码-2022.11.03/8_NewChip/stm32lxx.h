/**
* @brief  
* @Author 
* @Data   2018.08.20
* \Attention 手册中的Cat.表示Category：分类
*/


#ifndef __STM32LXX_H
#define __STM32LXX_H


#include "stm32f10x.h"
#include "memoryManage.h" 
#include "SWD_Module.h"



#define L1_UNLOCKCODE_PEKEY1  ( 0x89ABCDEF ) // 解锁EEPROM和PECR寄存器的操作功能 
#define L1_UNLOCKCODE_PEKEY2  ( 0x02030405 )
#define L1_UNLOCKCODE_PRGKEY1 ( 0x8C9DAEBF ) // 解锁操作Flash
#define L1_UNLOCKCODE_PRGKEY2 ( 0x13141516 )
#define L1_UNLOCKCODE_OPTKER1 ( 0xFBEAD9C8 ) // 选项字节操作
#define L1_UNLOCKCODE_OPTKER2 ( 0x24252627 )  


typedef struct
{
  __IO u32 ACR;  
  __IO u32 FLASH_PECR;                
  __IO u32 FLASH_PDKEYR;   
  __IO u32 FLASH_PEKEYR;    // 这个和EEPROM相关，不需要使能             
  __IO u32 FLASH_PRGKEY;               
  __IO u32 FLASH_OPTKEY;  
	__IO u32 FLASH_SR;
	__IO u32 FLASH_OBR;
	__IO u32 FLASH_WRPR1;
	__IO u32 FLASH_WRPR2;
	__IO u32 FLASH_WRPR3;
	__IO u32 FLASH_WRPR4;
}L1_FLASH_TypeDef;

#define L1_FLASH               ((L1_FLASH_TypeDef *) ((u32)0x40023C00))  // 用于确定地址


typedef struct
{
  __IO u32 RDP_SPRMOD;  
  __IO u32 USER;                
  __IO u32 WRP11;   
  __IO u32 WRP12;                
  __IO u32 WRP21;               
  __IO u32 WRP22;  
  __IO u32 WRP31;
	__IO u32 WRP32;
	__IO u32 WRP41;
	__IO u32 WRP42;
}L1_FLASH_OPT_TypeDef;

#define L1_OPTION               ((L1_FLASH_OPT_TypeDef *) ((u32)0x1FF80000))  // 用于确定地址



u32 DownloadFlashOfSTM32L1xx( _FileInformation _userfile );










#endif




