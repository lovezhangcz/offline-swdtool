/**
* @brief  支持stm32f7xx芯片
* @Author 
* @Data   2019.07.16
* \Attention 暂时只支持STM32F730这个芯片型号
  \B OptionByte地址：0x1FFF 0000 - 0x1FFF 001F 共32字节！
	\B PSIZE 必须要设置成：32位的。因为到目前为止，都是设置32位的。
*/

#include "stm32f7xx.h"

u8 stm32f7xx_becauseAutoAdd = 0;
void stm32f7xx_MassErase(void)
{
}
/**
 *  @B 用于连续的写4个值，注意：没有用到的值，必须为0xFFFFFFFF。
 */   
void stm32f7xx_BurstWrite( u32 startAddress, u32 data0,u32 data1 ,u32 data2,u32 data3 )
{

}
/**
 *  @B 只有一个读保护，没有写保护
 */   
void stm32f7xx_Protection( u8 readProtectionEnable,u8 writeProtectionEnable )
{
	u32 readWord0 = 0;
	u32 writeWord = 0xC00000FD - 1; // -1 表示：不写OptionLock位
	if( readProtectionEnable == ENABLE )
	{
		writeWord += 0xFF00;
	}
	else
	{
		writeWord += 0xAA00;
	}
	 
	if( writeProtectionEnable == ENABLE )
	{
		writeWord += 0;
	}
	else
	{
		writeWord += 0xFF0000;
	}
	
	{ // 写RDP
			{
				writeAP( AP_CSW,0x23000002);
//				writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_KEY );
//				writeAP( AP_DRW, STM32F7_MASS_KEY1 );
//				writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_KEY );
//				writeAP( AP_DRW, STM32F7_MASS_KEY2 );
				writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_OPTKEY );
				writeAP( AP_DRW, STM32F7_OPT_KEY1 );
				writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_OPTKEY );
				writeAP( AP_DRW, STM32F7_OPT_KEY2 );
			}
			{
				do
				{
					writeAP(AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_SR);
					readAP(AP_DRW,&readWord0);
					readDP(DP_RDBUFF,&readWord0);
				}while(readWord0&(0x1<<16)); // 当为1的时候说明没有操作完成！
			}
			writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_OPTCR );
			writeAP( AP_DRW,  writeWord );
			
			writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_OPTCR );
			writeAP( AP_DRW,  writeWord + _0b0010);
			
			{
				do
				{
					writeAP(AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_SR);
					readAP(AP_DRW,&readWord0);
					readDP(DP_RDBUFF,&readWord0);
				}while(readWord0&(0x1<<16)); // 当为1的时候说明没有操作完成！
			}
			
			writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_OPTCR );
			writeAP( AP_DRW,  writeWord + _0b0001);  // 再Lock上
	}
}


/**
 *  @B 其的Flash范围为：0x1004_0000 – 0x1006_7FFF，这个和其它的STM32不一样，而且只有写保护没有读保护，这个要注意！
 */
u32 DownloadFlashOf_stm32f7xx( _FileInformation _userfile )
{
	u32 returnNumber = 0;
	u32 readWord0 = 0;
	u8 isHaveProtecet = FAIL;  // FAIL:无保护,TRUE:存在保护
	{
		
	}
	{ // 检查保护区
		OS_ERR err;
		u32 apId = 0;
		writeDP( DP_SELECT, 0x00 );  // 访问寄存器
		writeAP( AP_CSW,0x2 );   // 长度为32bit，同时地址自动增加
		writeAP( AP_TAR,(u32)&stm32f7xx_FLASH->FLASH_OPTCR);   // 写目标地址
		readAP( AP_DRW,&apId);  // 第一次数据，不要！
		readDP( DP_RDBUFF, &apId );
		
		switch( (apId>>8)&0xFF )
		{
			case 0xAA:  // 无保护
				{  // 检查写保护
					if( ( ( apId>>16 )&0xFF ) != 0xFF )
					{
						if( L_ENGLISH_VER != 0x55AA )
						{
								LCD_ShowString(0,12*10,12,RED, "      发现写保护      ");
						}
						else
						{
							LCD_ShowString(0,12*10,12,RED,   "   write Protection   ");
						}
						isHaveProtecet= TRUE;
					}
				}
				break;
			case 0xCC:  // L2保护，无法恢复的保护
				if( L_ENGLISH_VER != 0x55AA )
				{
					LCD_ShowString(0,12*10,12,RED, "         警告        ");
					LCD_ShowString(0,12*11,12,RED, "       读保护-LV2    ");
				}
					else
				{
					LCD_ShowString(0,12*10,12,RED, "       warning     ");
					LCD_ShowString(0,12*11,12,RED, "    ReadProtect-LV2  ");
				}
				return 0x00;
			default:    // L1保护
				if( L_ENGLISH_VER != 0x55AA )
				{
					LCD_ShowString(0,12*10,12,RED, "      发现读保护      ");
				}
				else
				{
					LCD_ShowString(0,12*10,12,RED, "      read protect    ");
				}
				isHaveProtecet = TRUE;
				break;
		}
		
		if( isHaveProtecet == TRUE )  // 发现保护，此时需要进行擦除操作
		{
			if( L_ENGLISH_VER != 0x55AA )
				{
					LCD_ShowString(0,12*11,12,RED, "        清除中.      ");
				}
				else
				{
					LCD_ShowString(0,12*11,12,RED, "      Cleaning...    ");
				}
			stm32f7xx_Protection( DISABLE,DISABLE );
			OSTimeDlyHMSM(0,0,0,2000,OS_OPT_TIME_HMSM_NON_STRICT,&err);  // 等待20ms用来启动传输
			if( L_ENGLISH_VER != 0x55AA )
				{
					LCD_ShowString(0,12*10,12,RED, "      保护已清除     ");
					LCD_ShowString(0,12*11,12,RED, "    请断电重新下载   ");
				}
				else
				{
					LCD_ShowString(0,12*10,12,RED, " Protect cleared,Download again without power!");
				}
			return 0x00;
		}
	}
	
	{ // 清除Flash
		if( _userfile.ChipEraseWay == 00 )  // 全擦除
		{ // NULL
		}
		else
		{ // NULL
		}
		{ // 清空所有Flash
			writeAP( AP_CSW,0x23000002);
			writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_KEY );
			writeAP( AP_DRW, STM32F7_MASS_KEY1 );
			writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_KEY );
			writeAP( AP_DRW, STM32F7_MASS_KEY2 );
			do
				{
					writeAP(AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_SR);
					readAP(AP_DRW,&readWord0);
					readDP(DP_RDBUFF,&readWord0);
				}while(readWord0&(0x1<<16)); // 当为1的时候说明没有操作完成！
				
				
				writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_CR );
				writeAP( AP_DRW, 1<<2 );
				writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_CR );
				writeAP( AP_DRW, (1<<16)+(1<<2) );
				
				do
				{
					writeAP(AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_SR);
					readAP(AP_DRW,&readWord0);
					readDP(DP_RDBUFF,&readWord0);
				}while(readWord0&(0x1<<16)); // 当为1的时候说明没有操作完成！
				
				writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_SR );
				writeAP( AP_DRW, 0x1F3 );
		}
	}
	
	{ // 写程序区
			
			writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_CR );
			writeAP( AP_DRW,  1 << 0 ); // 使能PG位，同时使能PSIZE = 10也就是 x32 Program
			writeAP( AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_CR );
			writeAP( AP_DRW,  (1 << 0) + (1 << 9) ); // 使能PG位，同时使能PSIZE = 10也就是 x32 Program
		
			writeAP(AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_CR);
								readAP(AP_DRW,&readWord0);
								readDP(DP_RDBUFF,&readWord0);
		
		
		
					u32 HowMany4K = 0;
					if( _userfile.ProgramSize % 0x1000 != 0 )
						HowMany4K = _userfile.ProgramSize/0x1000 + 1;
					else
						HowMany4K = _userfile.ProgramSize/0x1000;
					
					for( u32 tmp11 = 0;tmp11<HowMany4K; tmp11++ )  // 写Flash的操作，每次操作4K字节
					{
						u32 Address = _userfile.ProgramStartAddress + tmp11*0x1000;  // 起始操作地址
						W25QXX_Read( W25Q4KBuf.databuf4K , _userfile.ProgramSaveAtW25QAddress + tmp11*0x1000, 0x1000); // 4k大小在16进行下，就是0x1000
						
						writeAP( AP_CSW,0x23000002 | 0x10 );
						writeAP( AP_TAR, Address );  // 地址！
						// 特别注意：L4要求每次写入2个字，也就是8字节。
						for( u32 tmp12 = 0;tmp12 <0x1000; tmp12 += 4 ) // 写入4K
						{
							#define SWD_AUTO_ADDRESS_ADD_VALUE 0x400   // 特别警告：STM32F0系列的地址自动增加大小为：1024个，而不是F1和F4的4096个！
							if( ((Address + tmp12) %SWD_AUTO_ADDRESS_ADD_VALUE) == 0 ) // 说明到了4k临界点，需要重新写地址
							{
								writeAP( AP_TAR, Address + tmp12 );  // 地址！
							}
							writeAP(AP_DRW, + ( W25Q4KBuf.databuf4K[tmp12+3] << 24) + ( W25Q4KBuf.databuf4K[tmp12+2] << 16 ) + ( W25Q4KBuf.databuf4K[tmp12+1] << 8) + ( W25Q4KBuf.databuf4K[tmp12] ) ); 

// 发现一个问题：就是不能在这里检查Flash_SR寄存器，而应该是一直写下去，否则会直接报错！！
//							writeAP(AP_DRW, + ( W25Q4KBuf.databuf4K[tmp12+7] << 24) + ( W25Q4KBuf.databuf4K[tmp12+6] << 16 ) + ( W25Q4KBuf.databuf4K[tmp12+5] << 8) + ( W25Q4KBuf.databuf4K[tmp12+4] ) ); 
//							do
//							{
//								writeAP(AP_TAR, (u32)&stm32f7xx_FLASH->FLASH_SR);
//								readAP(AP_DRW,&readWord0);
//								readDP(DP_RDBUFF,&readWord0);
//							}while(readWord0&(0x1<<16)); // 当为1的时候说明没有操作完成！
						
						}
						{ // 程序进度区！程序进度是4K大小！进度梯度是10%为进度！
							// 这个使用emWin应该是一个简单的问题！
							float tmp13 = (float)((float)((float)tmp11+1.0)/(float)HowMany4K);
							for( float tmp14 = 0.0;tmp14 <= (float)1.0; tmp14 += 0.1 )
							{
								if( tmp13 <= tmp14 ) // 找到第一次开始小于等于的值！
								{
									if( (tmp14 > 0.00) && ( tmp14 <= 0.1) )
									{
										LCD_ShowString(0,12*10,12,BLACK, "   >             10%");
										break;
									}
									else if( (tmp14 > 0.1) && (tmp14 <=0.2 ) )
									{
										LCD_ShowString(0,12*10,12, BLACK,"   >>            20%");
										break;
									}
									else if( (tmp14 > 0.2) && (tmp14 <=0.3) )
									{
										LCD_ShowString(0,12*10,12, BLACK,"   >>>           30%");
										break;
									}
									else if( (tmp14 > 0.3) && (tmp14 <=0.4) )
									{
										LCD_ShowString(0,12*10,12,BLACK, "   >>>>          40%");
										break;
									}
									else if( (tmp14 > 0.4) && (tmp14 <=0.5) )
									{
										LCD_ShowString(0,12*10,12,BLACK, "   >>>>>         50%");
										break;
									}
									else if( (tmp14 > 0.5) && (tmp14 <=0.6) )
									{
										LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>        60%");
										break;
									}
									else if( (tmp14 > 0.6) && (tmp14 <=0.7) )
									{
										LCD_ShowString(0,12*10,12, BLACK,"   >>>>>>>       70%");
										break;
									}
									else if( (tmp14 > 0.7) && (tmp14 <=0.8) )
									{
										LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>      80%");
										break;
									}
									else if( (tmp14 > 0.8) && (tmp14 <=0.9) )
									{
										LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>>     90%");
										break;
									}
								}
							}
							if( tmp11 + 1 >= HowMany4K )
							{
								LCD_ShowString(0,12*10,12,BLACK, "   >>>>>>>>>>   100%");
							}
						}
					}
	}
	{ // 校验程序区
			{ // 校验程序区，暂时只使用和校验！
					switch( _userfile.FlashCheckWay )
					{
						case 0x00: // 不检验
							if( L_ENGLISH_VER != 0x55AA )
							{
								LCD_ShowString(0,12*11,12,RED, "     不校验FLASH     ");
							}
							else
							{
								LCD_ShowString(0,12*11,12,RED, "   not check FLASH   ");
							}
							returnNumber = 0x55AA;
							break;
						case 0x01: // 逐个检验
							break;
						case 0x02: // CRC校验（和校验） -- 日后应该只会用这种校验方式，因为其它的也没有必要性了！
							{ // 这样的校验方式有这样的要求：起始地址4字节对齐，程序大小4字节倍数！这个日后务必要保证，如果保证不了，可以从软件上解决这个问题！
								u16 sumTmp15 = 0;  // 最后的和校验程序
								writeDP( DP_SELECT, 0x00 );  // 访问寄存器
								writeAP( AP_CSW,0x2 | 0x10);   // 长度为32bit，同时地址自动增加
								u32 addressTmp16 = _userfile.ProgramStartAddress;
								writeAP( AP_TAR,addressTmp16);   // 写目标地址
								
								u32 apId = 0;
								readAP( AP_DRW,&apId);  // 第一次数据，不要！
								
								for( ;addressTmp16 < ( _userfile.ProgramStartAddress + _userfile.ProgramSize );addressTmp16 += 4 )
								{
									if( ( addressTmp16 %0x400 ) == 0 )  // 注意：不同芯片系列增长的不同！！
									{
										writeAP( AP_TAR,addressTmp16);   // 写目标地址
										readAP( AP_DRW,&apId); 
									}
									readAP( AP_DRW,&apId);  // 这次数据是上次的。但是也是需要的。
									sumTmp15 += GetSumOf16Bit((u8 *)&apId,4);
								}
								if( sumTmp15 == _userfile.SumValueOfProgram )
								{
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString(0,12*11,12, BLACK,"   FLASH 校验成功   ");
									}
										else
									{
										LCD_ShowString(0,12*11,12, BLACK," FLASH Check success ");
									}
									returnNumber = 0x55AA;
								}
								else
								{
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString(0,12*11,12,BLACK, "   FLASH 校验失败    ");
									}
										else
									{
										LCD_ShowString(0,12*11,12, BLACK," FLASH Check failure ");
									}
									returnNumber = 0x1122;
								}
							}
							break;
						case 0x03: // MD5值检验
							break;
						default:
							break;
					}
				}
	}
	
	{ // 操作保护区！(其实这里应该加一个两个都不使能的操作！)
		
		switch( _userfile.OptionBytesFunction )
		{
			case 0x55: // 使能选项字节，日后功能开放！
				break;
			case 0xAA: // 使能读保护
				stm32f7xx_Protection( ENABLE,DISABLE );
				break;
			case 0x11: // 使能写保护
				stm32f7xx_Protection( DISABLE,ENABLE );
				break;
			case 0x1A: // 使能读写保护
				stm32f7xx_Protection( ENABLE,ENABLE );
				break;
		}
	}
	{
		resetAndHaltTarget( );
		runTarget( );
	}
	return returnNumber;
}















