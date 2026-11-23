/**
 * @brief 
 * @data 9 August 2016
 */
 
 
 
#ifndef __STM32_FLASH_H
#define __STM32_FLASH_H

#include "stm32f10x.h"


extern void Flash_Program( u16* offsetAddr,u16 data);
extern void Flash_ErasePage( u32 Address );
extern void Flash_EraseAllPage( void );

extern u8 Flash_Write( _Flash_High_Density_TypeDef * Address,u16 offset,u16*pBuffer,u16 NumToWrite );
extern void FLASH_ReadOutProtection(u8 EnableOrDisable);

#endif

