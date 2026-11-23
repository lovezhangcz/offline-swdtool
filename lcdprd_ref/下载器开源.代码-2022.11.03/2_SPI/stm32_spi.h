/**
 *@brief 一定要建立一个非常优秀的库
 *
 */


#ifndef __STM32_SPI_H
#define __STM32_SPI_H
#include "stm32f10x.h"

typedef enum 
{ // 模式只有两种，用枚举型变量
	SPI_SlaveMode = 0,
	SPI_MasterMode = 1,
}SPIModeType;

typedef enum
{ // 频率的分频值，也只有这几种
	SPI_BaudRatePrescaler_2    =  0,
	SPI_BaudRatePrescaler_4    =  1,
	SPI_BaudRatePrescaler_8    =  2,
	SPI_BaudRatePrescaler_16   =  3,
	SPI_BaudRatePrescaler_32   =  4,
	SPI_BaudRatePrescaler_64   =  5,
	SPI_BaudRatePrescaler_128  =  6,
	SPI_BaudRatePrescaler_256  =  7,
}SPI_BaudRatePrescalerType;

typedef enum 
{ // 注意：先发送MSB还是LSB可能是会和数据长度的关
	SPIDataLengthToEight = 0,
	SPIDataLengthToSixteen = 1,
}SPIDataLengthType;

typedef enum 
{ // 注意：先发送MSB还是LSB可能是会和数据长度的关
	PolarityType00 = 0,
	PolarityType01 = 1,
	PolarityType10 = 2,
	PolarityType11 = 3,
}SPIPolarityType;

#define SPIBUF_MAXBUFF_SIZE 128

typedef struct
{
	u8 Buf[SPIBUF_MAXBUFF_SIZE];
	u8 pRead;
	u8 pWrite;
}SPIBuf;

extern vu32 SPIClock;
extern SPIBuf SPIBuf1, SPIBuf2, SPIBuf1;
u32 SPIInit(	SPI_TypeDef* 							SPIPort,
							SPIModeType 							MasterOrSlave,
							SPI_BaudRatePrescalerType SpeedSet,
							SPIDataLengthType         DataLength,
							SPIPolarityType           PolarityType,
							IRQEnableType 						irqEnable );
u8 SPISendAData( SPI_TypeDef* SPIPort,u8 SPIData );
void SPISendBuf( SPI_TypeDef* SPIPort,u16* SPIDataBuf,u16 bufLength );
void SPIPrintString( SPI_TypeDef* SPIPort,char *puts );


#endif





