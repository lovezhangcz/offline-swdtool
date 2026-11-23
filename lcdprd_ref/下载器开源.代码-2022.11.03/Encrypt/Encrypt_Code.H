/**
 * @brief 加密程序
 */

#ifndef __ENCRYPT_CODE_H
#define __ENCRYPT_CODE_H


// 下面几个宏定义是定义了UserData在Flash中的地址
// 下面几个宏定义是定义了UserData在Flash中的地址
#define L_BOOTLOADER_VERSION              HighDensity_FlashPage(5)->IndexLen16[0]
#define L_BOOTLOADER_SIZE_H               HighDensity_FlashPage(5)->IndexLen16[1]
#define L_BOOTLOADER_SIZE_L               HighDensity_FlashPage(5)->IndexLen16[2]
#define L_BOOTLOADER_CRC_VALUE            HighDensity_FlashPage(5)->IndexLen16[3]
#define L_UNIQUE_ID0                      HighDensity_FlashPage(5)->IndexLen16[4]
#define L_UNIQUE_ID1                      HighDensity_FlashPage(5)->IndexLen16[5]
#define L_UNIQUE_ID2                      HighDensity_FlashPage(5)->IndexLen16[6]
#define L_UNIQUE_ID3                      HighDensity_FlashPage(5)->IndexLen16[7]
#define L_UNIQUE_ID4                      HighDensity_FlashPage(5)->IndexLen16[8]
#define L_UNIQUE_ID5                      HighDensity_FlashPage(5)->IndexLen16[9]
#define L_UNIQUE_ID_SUPPLEMENT_FOR_AES0   HighDensity_FlashPage(5)->IndexLen16[10]
#define L_UNIQUE_ID_SUPPLEMENT_FOR_AES1   HighDensity_FlashPage(5)->IndexLen16[11]

#define L_APP_VERSION                     HighDensity_FlashPage(6)->IndexLen16[0]
#define L_APP_SIZE_H                      HighDensity_FlashPage(6)->IndexLen16[1]  // 这个需要32位大小，16位只能60KB左右大小
#define L_APP_SIZE_L                      HighDensity_FlashPage(6)->IndexLen16[2]
#define L_APP_CRC_VALUE                   HighDensity_FlashPage(6)->IndexLen16[3]
#define L_UPDATA_FLAG                     HighDensity_FlashPage(7)->IndexLen16[0]

#define L_STM32SWDFUN                     HighDensity_FlashPage(7)->IndexLen16[1]   // 0x5A表示开启，其它值表示关闭
#define L_STM8SWINFUM                     HighDensity_FlashPage(7)->IndexLen16[2]   // 0x5A表示开启，其它值表示关闭






//void CheckUniqueIDInCaseOfCrack( void );

//void CPUIDCRCCkeck( void );

void MaskSWDPin( unsigned char TRUEOrFLASE );
#endif



