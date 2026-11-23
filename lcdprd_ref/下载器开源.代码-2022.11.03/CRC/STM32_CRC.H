/**
 * @brief CRC-16/MODBUS    x16+x15+x2+1 Ëã·¨
 * @author CRJ WeChat:TC4300 QQ:43058655
 * @data 2017-11-15
 */
 




#ifndef __STM32_CRC_H
#define __STM32_CRC_H

#include "stm32f10x.h"




extern u16 CRC16_OfCalculate( u8 *Databuf,u32 Length );
extern u16 CRC16_OfCheckTable( u8 *Databuf,u32 Length );

#endif


