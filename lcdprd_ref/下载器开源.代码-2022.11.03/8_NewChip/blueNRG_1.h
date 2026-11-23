/**
* @brief  
* @Author 
* @Data   2018.08.20
* \Attention 手册中的Cat.表示Category：分类
*/


#ifndef __BLUENRG_1_H
#define __BLUENRG_1_H


#include "stm32f10x.h"
#include "memoryManage.h" 
#include "SWD_Module.h"

typedef union
{
	u8 IndexLen8[2048];
	u16 IndexLen16[1024];
	u32 IndexLen32[512];
}_BLUE_NRG_1_Flash_Density_TypeDef;

// 最多80页，0-79的值
#define BLUE_NRG_1_FlashPage(x)    ((_BLUE_NRG_1_Flash_Density_TypeDef *) (0x10040000+x*2048))

typedef struct
{
  __IO u32 COMMAND;   // 0X00
  __IO u32 CONFIG;    // 0X00    
  __IO u32 IRQSTAT;   // 0X00
  __IO u32 IRQMASK;   // 0X3F
	
  __IO u32 IRQRAW;    // 0X00    
  __IO u32 SIZE;      // 0X9FFF
	__IO u32 ADDRESS;   // 0X00
		__IO u32 UnlockM;   
	
		__IO u32 UnlockL;   
		__IO u32 LFSRVAL;  
		__IO u32 TIMETRIM1;
		__IO u32 TIMETRIM2; 
	
	__IO u32 RES[4];    // 
	__IO u32 DATA[4];   // 0XFFFFFFFF
}BLUENRG_1_FLASH_TypeDef;

#define BLUENRG_1_FLASH               ((BLUENRG_1_FLASH_TypeDef *) ((u32)0x40100000))



#define BLUE_NRG_1_DO_MASS_ERASE    \
	do                                \
	{                                 \
		u32 readWord1 = 0;              \
		\
		writeAP( AP_CSW,0x23000002); \
		writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->COMMAND); \
		writeAP( AP_DRW, 0x22 ); \
		\
		writeAP( AP_CSW,0x00000002); \
		do \
		{  \
			writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT); \
			readAP(AP_DRW,&readWord1);\
			readDP(DP_RDBUFF,&readWord1);\
		}while(readWord1&0x2);\
	\
		writeAP( AP_CSW,0x23000002);\
		writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);\
		writeAP( AP_DRW, readWord1 ); \
		\
		\
		writeAP( AP_CSW,0x00000002);\
		do\
		{\
			writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);\
			readAP(AP_DRW,&readWord1);\
			readDP(DP_RDBUFF,&readWord1);\
		}while(readWord1&0x01);\
		\
		writeAP( AP_CSW,0x23000002); \
		writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);\
		writeAP( AP_DRW, readWord1 );  \
	}while(0); \

extern u32 DownloadFlashOfblueNRG_1( _FileInformation _userfile );

#endif



