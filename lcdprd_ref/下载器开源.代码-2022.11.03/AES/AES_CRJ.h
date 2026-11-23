/**
 * @B AES 的加密与解密暂时还不是利用的很好，还需要日后不断的熟悉
 */


#ifndef __AES_CRJ_H
#define __AES_CRJ_H
#include "stm32f10x.h"

extern unsigned char AES_Key_Table[32];     // 密钥
extern void aesEncInit(void);               // 加密初始化函数
extern void aesDecInit(void);               // 解密初始化函数
extern void AES_Enc(unsigned char *buffer); // 加密函数
extern void AES_Dec(unsigned char *buffer); // 解密函数
	
#endif

