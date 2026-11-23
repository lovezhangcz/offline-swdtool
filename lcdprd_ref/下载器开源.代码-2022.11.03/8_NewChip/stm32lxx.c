/**
* @brief  由于日后添加芯片会比较多，所以新添加的芯片，本文件主要用于STM32Lxx系列芯片的支持
* @Author 
* @Data   2018.08.20
* \Attention 
*/

#include "stm32lxx.h"

vu32 forCheckWriteProtectL1[4] = {0,0,0,0};
/**
 *  @B 32L1系列的读写保护配置操作
 *      1：程序下载开始前的清除所有保护，所以两个都是不保护，就是表示清除读保护
				2：程序下载结束后的加保护操作，所以一旦有一个使能，也就表示前期已经没有了保护。
				3：发现：操作选项字节，可以直接更新，而不需要擦除操作。
 */   
void stm32L1xxProtection( u8 readProtectionEnable,u8 writeProtectionEnable )
{
		u32 readWord0 = 0;
		if( ( readProtectionEnable == DISABLE ) && ( writeProtectionEnable == DISABLE ) )
		{ // 发现保护，清除保护
			{
				writeAP( AP_CSW,0x23000002);
				
				writeAP( AP_TAR, (u32)&L1_FLASH->FLASH_PEKEYR );
				writeAP( AP_DRW, L1_UNLOCKCODE_PEKEY1 );
				
				writeAP( AP_TAR, (u32)&L1_FLASH->FLASH_PEKEYR );
				writeAP( AP_DRW, L1_UNLOCKCODE_PEKEY2 );
				
				writeAP( AP_TAR, (u32)&L1_FLASH->FLASH_OPTKEY );
				writeAP( AP_DRW, L1_UNLOCKCODE_OPTKER1 );
				
				writeAP( AP_TAR, (u32)&L1_FLASH->FLASH_OPTKEY );
				writeAP( AP_DRW, L1_UNLOCKCODE_OPTKER2 );
			}
			{  // 清除错误，防止错误受到影响
				writeAP(AP_TAR, (u32)&L1_FLASH->FLASH_SR);
				writeAP( AP_DRW, 0x3F00 );                                            // 写EARSE与PROG
			}
			{
				writeAP( AP_TAR, (u32)&L1_OPTION->RDP_SPRMOD );
				writeAP( AP_DRW, 0xFF5500AA );  // 无保护
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP11 );
				writeAP( AP_DRW, 0xFFFF0000 );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP12 );
				writeAP( AP_DRW, 0xFFFF0000 );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP21 );
				writeAP( AP_DRW, 0xFFFF0000 );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP22 );
				writeAP( AP_DRW, 0xFFFF0000 );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP31 );
				writeAP( AP_DRW, 0xFFFF0000 );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP32 );
				writeAP( AP_DRW, 0xFFFF0000 );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP41 );
				writeAP( AP_DRW, 0xFFFF0000 );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP42 );
				writeAP( AP_DRW, 0xFFFF0000 );
			}
			
		}
		else
		{ // 没有保护，加保护,只允许增加一级的读保护
			{
				{  // 发现不能两次解锁PECR寄存器，所以和上面的全面解锁不一样。
					writeAP( AP_CSW,0x23000002);
					
					writeAP( AP_TAR, (u32)&L1_FLASH->FLASH_OPTKEY );
					writeAP( AP_DRW, L1_UNLOCKCODE_OPTKER1 );
					
					writeAP( AP_TAR, (u32)&L1_FLASH->FLASH_OPTKEY );
					writeAP( AP_DRW, L1_UNLOCKCODE_OPTKER2 );
				}
				{  // 清除错误，防止错误受到影响
					writeAP(AP_TAR, (u32)&L1_FLASH->FLASH_SR);
					writeAP( AP_DRW, 0x3F00 );  // 写EARSE与PROG
				}
				
			}
			if( readProtectionEnable == ENABLE )
			{
				writeAP( AP_TAR, (u32)&L1_OPTION->RDP_SPRMOD );
				writeAP( AP_DRW, 0xFFFF0000 );  // 发现使用FF4400BB，会出现：增加读保护会出问题。
			}
			if( writeProtectionEnable == ENABLE )
			{
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP11 );
				writeAP( AP_DRW, 0x0000FFFF );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP12 );
				writeAP( AP_DRW, 0x0000FFFF );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP21 );
				writeAP( AP_DRW, 0x0000FFFF );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP22 );
				writeAP( AP_DRW, 0x0000FFFF );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP31 );
				writeAP( AP_DRW, 0x0000FFFF );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP32 );
				writeAP( AP_DRW, 0x0000FFFF );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP41 );
				writeAP( AP_DRW, 0x0000FFFF );
				
				writeAP( AP_TAR, (u32)&L1_OPTION->WRP42 );
				writeAP( AP_DRW, 0x0000FFFF );
			}
		}

		writeAP( AP_CSW,0x00000002);   // 读
		do
		{
			writeAP(AP_TAR, (u32)&L1_FLASH->FLASH_SR);
			readAP(AP_DRW,&readWord0);
			readDP(DP_RDBUFF,&readWord0);
		}while(readWord0&0x1); // 当为1的时候说明没有操作完成！
		{
			writeAP( AP_CSW,0x23000002);
			writeAP(AP_TAR, (u32)&L1_FLASH->FLASH_PECR);
			writeAP( AP_DRW, 0x200|(1<<18) );
		}
}


/**
 *  @B L1系列的下载函数，特别注意：STM32L1系列清除Flash值为：0x00，而不是0xFF
 *  	 
 */
u32 DownloadFlashOfSTM32L1xx( _FileInformation _userfile )
{
	u32 returnNumber = 0;
	u8 isHaveProtecet = FAIL;  // FAIL:无保护,TRUE:存在保护
	{ // 检查保护区
		OS_ERR err;
		u32 apId = 0;
		writeDP( DP_SELECT, 0x00 );  // 访问寄存器
		writeAP( AP_CSW,0x2 );   // 长度为32bit，同时地址自动增加
		writeAP( AP_TAR,(u32)&L1_FLASH->FLASH_OBR);   // 写目标地址
		readAP( AP_DRW,&apId);  // 第一次数据，不要！
		readDP( DP_RDBUFF, &apId );
		
		switch( apId&0xFF )
		{
			case 0xAA:  // 无保护
				{  // 检查写保护
					writeAP( AP_TAR,(u32)&L1_FLASH->FLASH_WRPR1);
					readAP( AP_DRW,&apId);
					readDP( DP_RDBUFF, &apId );
					forCheckWriteProtectL1[0] = apId;
					
					writeAP( AP_TAR,(u32)&L1_FLASH->FLASH_WRPR2);
					readAP( AP_DRW,&apId); 
					readDP( DP_RDBUFF, &apId );
					forCheckWriteProtectL1[1] = apId;
					
					writeAP( AP_TAR,(u32)&L1_FLASH->FLASH_WRPR3);
					readAP( AP_DRW,&apId);
					readDP( DP_RDBUFF, &apId );
					forCheckWriteProtectL1[2] = apId;
							
					writeAP( AP_TAR,(u32)&L1_FLASH->FLASH_WRPR4);
					readAP( AP_DRW,&apId);
					readDP( DP_RDBUFF, &apId );
					forCheckWriteProtectL1[3] = apId;
					
					if( ( forCheckWriteProtectL1[0]!= 0 ) // 为什么只用了一个？当然，这样已到达128KB
//						|| 
//							( forCheckWriteProtect[1]!= 0 ) || 
//							( forCheckWriteProtect[2]!= 0 ) || 
//							( forCheckWriteProtect[3]!= 0 )   
					)
					{
						if( L_ENGLISH_VER != 0x55AA )
						{
							LCD_ShowString(0,12*10,12,RED, "      发现写保护      ");
						}
						else
						{
							LCD_ShowString(0,12*10,12,RED, "  have write protect  ");
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
			stm32L1xxProtection(DISABLE,DISABLE);
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
//		if( _userfile.ChipEraseWay == 00 )  // 全擦除
//		{
//			
//		}
//		else
//		{  // 擦除对应
//			
//		}
		// 发现STM32L1的似乎没有相对应的Flash 全部擦除选项，所以这里暂时使用对应扇区操作的方式
		{  // 只能是按照页：256字节进行擦除
			u32 startAddressTmp0 = 0x08000000; // 起始页所在地址
			u32 endAddressTmp1 = 0x08000000;   // 结束页所在地址
			u32 startRollingAddressTmp3 = 0x08000000;
			u32 endRollingAddressTmp4 = 0x08000000;
			for( u32 tmp0 = 0;tmp0<2048;tmp0++ ) // 2048个页最多，需要找到开始与结束的页码
				{
					if( _userfile.ProgramStartAddress < ( 0x08000000 + 256 * tmp0 ) )
					{
						startAddressTmp0 = 0x08000000 + 256 * (tmp0 - 1);
						break;
					}
				}
			
			for( u32 tmp6 = 0;tmp6<2048;tmp6++ ) // 
				{
					if( (_userfile.ProgramStartAddress + _userfile.ProgramSize) < ( 0x08000000 + 256 * tmp6 ) )
					{
						endAddressTmp1 = 0x08000000 + 256 * (tmp6 - 1);
						break;
					}
				}
				
				for( u32 tmp7 = 0;tmp7<2048;tmp7++ ) // 
					{
						if( _userfile.RollingCodeStartAddress < ( 0x08000000 + 256 * tmp7 ) )
						{
							startRollingAddressTmp3 = 0x08000000 + 256 * (tmp7 - 1);
							break;
						}
					}
				for( u32 tmp8 = 0;tmp8<2048;tmp8++ ) // 
					{
						if( (_userfile.RollingCodeStartAddress + 32 ) < ( 0x08000000 + 256 * tmp8 ) )  // 这个的SIZE只有32
						{
							endRollingAddressTmp4 = 0x08000000 + 256 * (tmp8 - 1);
							break;
						}
					}
					
					{  // 清除Flash操作
						writeAP( AP_CSW,0x23000002);
	
						writeAP(AP_TAR, (u32)&L1_FLASH->FLASH_PEKEYR);
						writeAP(AP_DRW, L1_UNLOCKCODE_PEKEY1);
						
						writeAP(AP_TAR, (u32)&L1_FLASH->FLASH_PEKEYR);
						writeAP(AP_DRW, L1_UNLOCKCODE_PEKEY2);
						
						writeAP(AP_TAR, (u32)&L1_FLASH->FLASH_PRGKEY);
						writeAP(AP_DRW, L1_UNLOCKCODE_PRGKEY1);
						
						writeAP(AP_TAR, (u32)&L1_FLASH->FLASH_PRGKEY);
						writeAP(AP_DRW, L1_UNLOCKCODE_PRGKEY2);
						
						writeAP( AP_CSW,0x00000002);   // 读
						u32 readWord0 = 0;
						do
						{
							writeAP(AP_TAR, (u32)&L1_FLASH->FLASH_SR);
							readAP(AP_DRW,&readWord0);
							readDP(DP_RDBUFF,&readWord0);
						}while(readWord0&0x1); // 当为1的时候说明没有操作完成！
						
						
						writeAP( AP_CSW,0x23000002);   // 一定要先提前写上这个，不仅仅表示是32位，同时表明访问的方向是写访问！否则会报错！！
						writeAP( AP_TAR, (u32)&L1_FLASH->FLASH_PECR );
						writeAP( AP_DRW, 0x208 );  // 写EARSE与PROG
						for( u32 addressTmp9 = startAddressTmp0; addressTmp9<= endAddressTmp1; addressTmp9 += 256 )  // 清除程序区
						{
							writeAP( AP_CSW,0x23000002); 

							writeAP( AP_TAR, addressTmp9 );
							writeAP( AP_DRW, 0x00 );
							
							writeAP( AP_CSW,0x00000002);   // 读
							u32 readWord1 = 0;
							do
							{
								writeAP(AP_TAR, (u32)&L1_FLASH->FLASH_SR);
								readAP(AP_DRW,&readWord1);
								readDP(DP_RDBUFF,&readWord1);
							}while(readWord1&0x1); // 当为1的时候说明没有操作完成！
						}
						{ // 恢复现场
							writeAP( AP_CSW,0x23000002);  
							writeAP( AP_TAR, (u32)&L1_FLASH->FLASH_PECR );
							writeAP( AP_DRW, 0x408 );
						}
						
						
						if( _userfile.RollingCodeFunction == 0x55 ) // 使能滚码操作，只有当滚码操作使能的时候，再操作，防止访问了错误的地址！
						{
							writeAP( AP_CSW,0x23000002);   // 一定要先提前写上这个，不仅仅表示是32位，同时表明访问的方向是写访问！否则会报错！！
							writeAP( AP_TAR, (u32)&L1_FLASH->FLASH_PECR );
							writeAP( AP_DRW, 0x208 );  // 写EARSE与PROG
							for( u32 addressTmp10 = startRollingAddressTmp3; addressTmp10<= endRollingAddressTmp4; addressTmp10 += 256 )  // 清除滚码区
							{
								writeAP( AP_CSW,0x23000002); 

								writeAP( AP_TAR, addressTmp10 );
								writeAP( AP_DRW, 0x00 );
								
								writeAP( AP_CSW,0x00000002);   // 读
								u32 readWord1 = 0;
								do
								{
									writeAP(AP_TAR, (u32)&FLASH->SR.All);
									readAP(AP_DRW,&readWord1);
									readDP(DP_RDBUFF,&readWord1);
								}while(readWord1&0x1); // 当为1的时候说明没有操作完成！
							}
							{ // 恢复现场
								writeAP( AP_CSW,0x23000002);  
								writeAP( AP_TAR, (u32)&L1_FLASH->FLASH_PECR );
								writeAP( AP_DRW, 0x408 );
							}
						}
						
					}
		}
	}
	
	{  // 写Flash，包括滚码区域
		{ // 写程序区
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
						for( u32 tmp12 = 0;tmp12 <0x1000; tmp12 += 4 ) // 写入4K
						{
							#define SWD_AUTO_ADDRESS_ADD_VALUE 0x400   // 特别警告：STM32F0系列的地址自动增加大小为：1024个，而不是F1和F4的4096个！
							if( ((Address + tmp12) %SWD_AUTO_ADDRESS_ADD_VALUE) == 0 ) // 说明到了4k临界点，需要重新写地址
							{
								writeAP( AP_TAR, Address + tmp12 );  // 地址！
							}
							writeAP(AP_DRW, + ( W25Q4KBuf.databuf4K[tmp12+3] << 24) + ( W25Q4KBuf.databuf4K[tmp12+2] << 16 ) + ( W25Q4KBuf.databuf4K[tmp12+1] << 8) + ( W25Q4KBuf.databuf4K[tmp12] ) ); 
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
		
				{ // 写滚码区域，注意：由于上面使用的是：32字节的连续操作，不知道会不会对这个滚码有影响。
					u32 programTimesTmp15 = 0;  // 实际已经编程次数
					u32 valueTmp16 = 0; 				// 实际写入的滚码值
					if( _userfile.RollingCodeFunction == 0x55 ) // 表示需要操作滚码区
					{
						
						if( _userfile.DownloaderNumbers[255] != 0xFF )
						{
							programTimesTmp15 = _userfile.AlreadyProgrammedCarrayBit*(256+1) + 256;
						}
						else
						{
							for( u16 Tmp16 = 0;Tmp16<256;Tmp16++ )
							{
								if( _userfile.DownloaderNumbers[Tmp16] == 0xFF )
								{
									programTimesTmp15 = _userfile.AlreadyProgrammedCarrayBit*(256+1) + Tmp16;  // 实际编程次数
									break;
								}
							}
						}
						valueTmp16 = _userfile.RollingCodeStartValue + ( _userfile.RollingCodeStepValue* programTimesTmp15 );
						writeAP( AP_TAR, _userfile.RollingCodeStartAddress );  // 地址！
						{
							if( (_userfile.RollingCodeStartAddress)%4 != 0 ) // 表示不是4的倍数，则取的16位在高位
							{
								writeAP(AP_DRW, + ( ((valueTmp16>>24)&0xFF) << 24) + ( ((valueTmp16>>16)&0xFF) << 16 ) ); 
							}
							else
							{
								writeAP(AP_DRW, + ( ((valueTmp16>>8)&0xFF) << 8) + ( ((valueTmp16>>0)&0xFF) ) ); 
							}
						}
						{
							writeAP( AP_TAR, _userfile.RollingCodeStartAddress+2 );  // 地址！
							{
								if( (_userfile.RollingCodeStartAddress+2)%4 != 0 ) // 表示不是4的倍数，则取的16位在高位
								{
									writeAP(AP_DRW, + ( ((valueTmp16>>24)&0xFF) << 24) + ( ((valueTmp16>>16)&0xFF) << 16 ) ); 
								}
								else
								{
									writeAP(AP_DRW, + ( ((valueTmp16>>8)&0xFF) << 8) + ( ((valueTmp16>>0)&0xFF) ) ); 
								}
							}
						}
					
						{ // 检验滚码区
							u32 apId = 0;
							writeDP( DP_SELECT, 0x00 );  // 访问寄存器
							writeAP( AP_CSW,0x2 | 0x10);   // 长度为32bit，同时地址自动增加
							writeAP( AP_TAR,_userfile.RollingCodeStartAddress);   // 写目标地址
							readAP( AP_DRW,&apId);  // 第一次数据，不要！
							readDP( DP_RDBUFF, &apId );
							if( apId != valueTmp16 )
							{
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*11,12,RED, "    滚码校验失败     ");
								}
								else
								{
									LCD_ShowString(0,12*11,12,RED, "Roll code check failed");
								}
								returnNumber = 0x00;  // 检测错误
								return returnNumber;
							}
						}
					}
				}
									
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
									if( ( addressTmp16 %0x1000 ) == 0 )
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
			{ // 操作保护区！(其实这里应该加一个两个都不使能的操作！)
				switch( _userfile.OptionBytesFunction )
				{
					case 0x55: // 使能选项字节，日后功能开放！
						break;
					case 0xAA: // 使能读保护
						stm32L1xxProtection( ENABLE,DISABLE );
						break;
					case 0x11: // 使能写保护
						stm32L1xxProtection( DISABLE,ENABLE );
						break;
					case 0x1A: // 使能读写保护
						stm32L1xxProtection( ENABLE,ENABLE );
						break;
				}
			}
			{
				resetAndHaltTarget( );
				runTarget( );
			}
	}
	return returnNumber;
}















