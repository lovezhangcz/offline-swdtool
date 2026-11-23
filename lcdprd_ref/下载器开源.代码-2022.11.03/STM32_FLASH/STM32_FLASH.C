/**
 * @brief STM32的Flash操作,实际项目中需要根据实际情况来改写函数
 * @data 9 August 2016
 * @Attention 操作Flash必需要开启HSI,否则卡住!
 * @Attention 记得读出操作，不需要HSI，可以在任何时候读出
 */
 
 
 
#include "stm32f10x.h"
#include "STM32_FLASH.H"

void Flash_Unlock( )
{
	FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
	FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2;
}

void Flash_Lock( )
{
	FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
}

/**
 * @brief 编程指定地址,注意必须确保编程处为0xFFFF，否则不能擦除，会报错！
 * @eg Flash_Program( &LowMediumDensity_FlashPage(2)->IndexLen16[0],0x1122);
 */
void Flash_Program( u16* offsetAddr,u16 data)
{
	FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
	FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2; // 解锁
	FLASH->CR.Programming_RW = ENABLE;  // 编程使能
	while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住
	*offsetAddr = data;
	while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住，等待不忙再操作。
	FLASH->CR.Programming_RW = DISABLE; // 编程禁能
	FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
}

/**
 * @brief 擦除所在的地址的页.写其中包含的地址就可以
 * @eg Flash_ErasePage( (u32)LowMediumDensity_FlashPage(i) );
 */
void Flash_ErasePage( u32 Address )
{
	FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
	FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2; // 解锁
	while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住
	FLASH->CR.PageErase_RW = ENABLE;
	FLASH->AR.Address_W = (u32)Address;
	FLASH->CR.Strat = ENABLE;
	while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住
	FLASH->CR.Strat = DISABLE;
	FLASH->CR.PageErase_RW = DISABLE;	//禁能操作，恢复现场
	FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
}

/**
 * @brief 擦除所有,可以说几乎用不到
 */
void Flash_EraseAllPage( ) 
{
	FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
	FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2; // 解锁
	
	while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住
	FLASH->CR.MassErase_RW = ENABLE;
	FLASH->CR.Strat = ENABLE;
	while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住
	FLASH->CR.MassErase_RW = DISABLE;
	
	FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
}


/**
 * @brief 写指定长度,注意不要超过界限,注意确保写时地址内容为0xFFFF
 */
u8 Flash_Write( _Flash_High_Density_TypeDef * Address,u16 offset,u16*pBuffer,u16 NumToWrite )
{
	if( (offset+NumToWrite) <= 1024 )
	{
		FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
		FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2; // 解锁
		FLASH->CR.Programming_RW = ENABLE;
		for( u16 temp0=0;temp0<NumToWrite;temp0++ )
		{
			while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住
			Address->IndexLen16[offset+temp0] = pBuffer[temp0];
		}
		while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住
		FLASH->CR.Programming_RW = DISABLE;
		FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
		return SUCCESS;
	}
	else
		return FAIL;
}

/**
 * @brief Flash的读保护功能的启用
 * \B 发现本程序下启用读保护之后，程序内容似乎被清除了，所以用软件开启和关闭读保护的方法来测试！
 * \B 注意，根据官方移植和于说明的，下面的这种操作，也会清除其它的选项字节
 * \B 关于反码，似乎只在正码位置写上之后，反码位置会自己填写上去。所以只需要操作正码位置就可以了
 * \B 注意：关于也会清除其他选项的字节说明，因为选项字节会在下次加载进去，所以在第一次的读操作，都是上上一次的。（更具体的机理不明，日后再说！）
 * \B 关于写其它选项字节，只需要写非反码位就可以了，反码位，会自己加上去！
 */

void FLASH_ReadOutProtection(u8 EnableOrDisable)
{
	FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
	FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2; // 解锁
	while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住
	FLASH->OPTKEYR.OptionByteKey_W = FLASH_KEY1;
	FLASH->OPTKEYR.OptionByteKey_W = FLASH_KEY2;  // 解密选择字节的操作
	FLASH->CR.OptionByteErase_RW = 1;  // 清除选项字节
	FLASH->CR.Strat = 1;  // 启动操作
	while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住
	FLASH->CR.OptionByteErase_RW = 0;   // 关闭清除选项字节
	FLASH->CR.OptionByteProgramming_RW  = 1; // 选项字节编程使能
	if(EnableOrDisable == ENABLE)
	{
//		OB->RDP = 0x00;
	}
	else
	{
		OB->RDP = RDP_Key;  
	}
	while( FLASH->SR.Busy_R == TRUE );  // 忙时卡住
	FLASH->CR.OptionByteProgramming_RW = 0; // 字节选项编程禁止
	FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
}






