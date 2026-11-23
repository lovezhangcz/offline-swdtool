/**
* @brief  
* @Author 
* @Data   2018.08.20
* \Attention 手册中的Cat.表示Category：分类
*/


#ifndef __STM32L0XX_H
#define __STM32L0XX_H


#include "stm32f10x.h"
#include "memoryManage.h" 
#include "SWD_Module.h"



#define L0_UNLOCKCODE_PEKEY1  ( 0x89ABCDEF ) // 解锁EEPROM和PECR寄存器的操作功能 
#define L0_UNLOCKCODE_PEKEY2  ( 0x02030405 )
#define L0_UNLOCKCODE_PRGKEY1 ( 0x8C9DAEBF ) // 解锁操作Flash
#define L0_UNLOCKCODE_PRGKEY2 ( 0x13141516 )
#define L0_UNLOCKCODE_OPTKER1 ( 0xFBEAD9C8 ) // 选项字节操作
#define L0_UNLOCKCODE_OPTKER2 ( 0x24252627 )  

#define L0_UNLOCKCODE_PDKER1 ( 0x04152637 ) // 选项字节操作
#define L0_UNLOCKCODE_PDKER2 ( 0xFAFBFCFD ) 



typedef struct
{
  __IO u32 ACR;  
  __IO u32 FLASH_PECR;      // 这个寄存器需要解锁之后才能操作     
  __IO u32 FLASH_PDKEYR;    // 这个解锁和编程Flash无关！
  __IO u32 FLASH_PEKEYR;                
  __IO u32 FLASH_PRGKEY;               
  __IO u32 FLASH_OPTKEY;  
	__IO u32 FLASH_SR;
	__IO u32 FLASH_OBR;
	__IO u32 FLASH_WRPR1;    // 32位全用
	__IO u32 FLASH_WRPR2;    // 在L053当中这个是没有的，但是手册中这个15-0位是有用的！！这个要注意！
//	__IO u32 FLASH_WRPR3;  // 这些在L1中有，但是L0中没有！
//	__IO u32 FLASH_WRPR4;
}L0_FLASH_TypeDef;

#define L0_FLASH               ((L0_FLASH_TypeDef *) ((u32)0x40022000))  // 和L1地址不一样！


typedef struct
{
  __IO u32 RDP_SPRMOD;  // 
  __IO u32 USER;    // 用户选项字节
  __IO u32 WRP11;   
  __IO u32 WRP12;   // 以下在L0X3的头文件当中都是不存在没有的。             
  __IO u32 WRP21;   // 但是手册中一直到这里，也就是有3个！           
//  __IO u32 WRP22;  
//  __IO u32 WRP31;
//	__IO u32 WRP32;
//	__IO u32 WRP41;
//	__IO u32 WRP42;
}L0_FLASH_OPT_TypeDef;

#define L0_OPTION               ((L0_FLASH_OPT_TypeDef *) ((u32)0x1FF80000))  // 用于确定地址



u32 DownloadFlashOfSTM32L0xx( _FileInformation _userfile );










#endif




