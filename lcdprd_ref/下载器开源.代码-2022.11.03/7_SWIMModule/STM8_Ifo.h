/**
 *@brief 这是日后的一个大工程，一定要建立的非常优秀
 *
 */

#include "stm32f10x.h"

#ifndef __STM8_IFO_H
#define __STM8_IFO_H



// 下面这个用来指示操作对应芯片的类型！！！
// STM8_StoreIfo
#define AF_L_NUM       0
#define AF_M_NUM       1
#define AF_H_NUM       2
#define AL_L_NUM       3 
#define AL_M_NUM       4
#define AL_M_PLUS_NUM  5
#define AL_H_NUM       6
#define L_L_NUM        7
#define L_M_NUM        8
#define L_M_PLUS_NUM   9
#define L_H_NUM        10
#define L_101x_NUM     11
#define S_L_NUM        12
#define S_M_NUM        13
#define S_H_NUM        14
#define TL_L_NUM       15
#define TL_M_NUM       16
#define TL_H_NUM       17


/** 
  * @brief 
  */


typedef struct
{
	int	 BlockSize;			// 每页大小
	int  EepromSize;		// eeprom的大小
	int  EepromAddr;		// eeprom的起始地址
	int  FlashSize;     // Program大小
	int  ProgramAddr;   // 起始地址！
	int  OptionAddr;		// 选项字节起始地址！
}MCU_INFO;


extern MCU_INFO STM8S_xx[];
extern MCU_INFO STM8_StoreIfo[];
extern MCU_INFO *CurStm8;


#endif





