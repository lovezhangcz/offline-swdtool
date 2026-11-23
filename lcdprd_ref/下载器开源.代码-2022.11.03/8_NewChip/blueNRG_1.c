/**
* @brief  支持BLUENRG-132芯片
* @Author 
* @Data   2018.09.07
* \Attention 主要是针对客户的定制需要！！但是这个芯片还好，只有部分，且功能比较简单！！
*/

#include "blueNRG_1.h"

u8 becauseAutoAdd = 0;
void MassErase(void)
{
	u32 readWord1 = 0;              
	
	writeAP( AP_CSW,0x23000002); 
	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->COMMAND); 
	writeAP( AP_DRW, 0x22 ); 
	
	writeAP( AP_CSW,0x00000002); 
	do 
	{  
		writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQRAW); 
		readAP(AP_DRW,&readWord1);
		readDP(DP_RDBUFF,&readWord1);
	}while((readWord1&0x3) != 0x03);

	writeAP( AP_CSW,0x23000002);
	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQRAW);
	writeAP( AP_DRW, 0x00 ); 
	
//	writeAP( AP_CSW,0x00000002);
//	do
//	{
//		writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//		readAP(AP_DRW,&readWord1);
//		readDP(DP_RDBUFF,&readWord1);
//	}while((readWord1&0x01) != 0x01);
//	
//	writeAP( AP_CSW,0x23000002); 
//	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//	writeAP( AP_DRW, readWord1 );  
}
/**
 *  @B 用于连续的写4个值，注意：没有用到的值，必须为0xFFFFFFFF。
 */   
void blueNRG_1_BurstWrite( u32 startAddress, u32 data0,u32 data1 ,u32 data2,u32 data3 )
{
	u32 readWord1 = 0;
	u32 timeOver = 0;
	writeAP( AP_CSW,0x23000002);   //32位写访问
	
	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->DATA[0]);
	writeAP( AP_DRW, data0 );

	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->DATA[1]);
	writeAP( AP_DRW, data1 ); 

	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->DATA[2]);
	writeAP( AP_DRW, data2 ); 
	
	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->DATA[3]);
	writeAP( AP_DRW, data3 ); 
	
	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->ADDRESS);
	writeAP( AP_DRW, startAddress );
		
	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->COMMAND);
	writeAP( AP_DRW, 0xCC );
	
	writeAP( AP_CSW,0x00000002);   // 读
	
	writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->ADDRESS);
	readAP(AP_DRW,&readWord1);
	readDP(DP_RDBUFF,&readWord1);
		
	
	do
	{
		writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQRAW);
		readAP(AP_DRW,&readWord1);
		readDP(DP_RDBUFF,&readWord1);
		timeOver++;
	}while( (readWord1&0x3) != 0x03 &&  (timeOver <0x8) ); // 当为1的时候说明操作完成
	
	writeAP( AP_CSW,0x23000002 );   //32位写访问
	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQRAW );
	writeAP( AP_DRW, 0x0 );  // 写1清除，这里通过之前读出来值进行操作。
	
//	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->COMMAND );
//	writeAP( AP_DRW, 0x00 );  // 写1清除，这里通过之前读出来值进行操作。
	
//	writeAP( AP_CSW,0x00000002);   // 读
//	do
//	{
//		writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//		readAP(AP_DRW,&readWord1);
//		readDP(DP_RDBUFF,&readWord1);
//	}while((readWord1&0x01) != 0x01); // 当为1的时候说明操作完成
//	
//	writeAP( AP_CSW,0x23000002);   //32位写访问
//	writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//	writeAP( AP_DRW, readWord1 );  
}


/**
 *  @B 只有一个读保护，没有写保护
 */   
void blueNRG_1_Protection( u8 readProtectionEnable,u8 writeProtectionEnable )
{
	// 只分为两点：
	// 1：清除读保护，只有程序开始的时候会清除读保护。
	// 2：加读保护，只有程序最后会加，也就是说之前是没有读保护的
	if( readProtectionEnable == DISABLE )
	{ // 清除，读保护的方法就是：进行MASS ERASE
//		u32 readWord1 = 0;
//		
//		writeAP( AP_CSW,0x23000002);   //32位写访问
//		writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->COMMAND);
//		writeAP( AP_DRW, 0x22 );  // MASS ERASE
//		
//		writeAP( AP_CSW,0x00000002);   // 读
//		do
//		{
//			writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//			readAP(AP_DRW,&readWord1);
//			readDP(DP_RDBUFF,&readWord1);
//		}while(readWord1&0x2); // 当为1的时候说明操作完成
//		
//		writeAP( AP_CSW,0x23000002);   //32位写访问
//		writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//		writeAP( AP_DRW, readWord1 );  // 写1清除，这里通过之前读出来值进行操作。
//		
//		
//		writeAP( AP_CSW,0x00000002);   // 读
//		do
//		{
//			writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//			readAP(AP_DRW,&readWord1);
//			readDP(DP_RDBUFF,&readWord1);
//		}while(readWord1&0x01); // 当为1的时候说明操作完成
//		
//		writeAP( AP_CSW,0x23000002);   //32位写访问
//		writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//		writeAP( AP_DRW, readWord1 );  // 写1清除，这里通过之前读出来值进行操作。
		BLUE_NRG_1_DO_MASS_ERASE;
	}
	else
	{ // 加读保护的方法就是：最后两个WORD写上非全“1”的数，这里写：0x5A5A5A5A
		blueNRG_1_BurstWrite(((u32)&BLUE_NRG_1_FlashPage(79)->IndexLen32[508] )/4,0xFFFFFFFF,0xFFFFFFFF,0x5A5A5A5A,0xA5A5A5A5);
	}
	
}


/**
 *  @B 其的Flash范围为：0x1004_0000 – 0x1006_7FFF，这个和其它的STM32不一样，而且只有写保护没有读保护，这个要注意！
 */
u32 DownloadFlashOfblueNRG_1( _FileInformation _userfile )
{
	u32 returnNumber = 0;
	{ // 检查保护区，注意：只存在读保护
		u32 protectValue[2];  // 检查读保护的值
		OS_ERR err;
		u32 apId = 0;
		writeDP( DP_SELECT, 0x00 );  // 访问寄存器
		writeAP( AP_CSW,0x2 );   // 长度为32bit，同时地址自动增加
		writeAP( AP_TAR,(u32)&BLUE_NRG_1_FlashPage(79)->IndexLen32[510]);   // 写目标地址
		readAP( AP_DRW,&apId);  // 第一次数据，不要！
		readDP( DP_RDBUFF, &apId );
		protectValue[0] = apId;
		
		writeAP( AP_TAR,(u32)&BLUE_NRG_1_FlashPage(79)->IndexLen32[511]);   // 写目标地址
		readAP( AP_DRW,&apId);  // 第一次数据，不要！
		readDP( DP_RDBUFF, &apId );
		protectValue[1] = apId;
		
		if( (protectValue[0] != 0xFFFFFFFF) || (protectValue[1]!=0xFFFFFFFF) )
		{  // 表示存在读保护
			if( L_ENGLISH_VER != 0x55AA )
			{
				LCD_ShowString(0,12*10,12,RED, "      发现读保护      ");
				LCD_ShowString(0,12*11,12,RED, "        清除中.      ");
			}
			else
			{
				LCD_ShowString(0,12*10,12,RED, "    read protection    ");
				LCD_ShowString(0,12*11,12,RED, "        cleaning.      ");
			}
			blueNRG_1_Protection(DISABLE,DISABLE);
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
	
	{ // 擦除Flash
//		if( _userfile.ChipEraseWay == 00 )  // 全擦除
//		{
			MassErase();
//		}
//		else
//		{  // 擦除对应Flash，由于是页对应擦除，所以针对性的写函数。
//			u8 appAddressStart = 0;
//			u8 appAddressEnd = 0;
//			u8 rollAddressStart = 0;
//			u8 rollAddressEnd = 0;
//			
////			appAddressStart = ( ( _userfile.ProgramStartAddress - 0x10040000 )/2048 + ( (( _userfile.ProgramStartAddress - 0x10040000 )%2048) ? 1:0 ) ) - 1;
////			appAddressEnd = ( ( _userfile.ProgramStartAddress + _userfile.ProgramSize - 0x10040000 )/2048 + ( (( _userfile.ProgramStartAddress + _userfile.ProgramSize - 0x10040000 )%2048) ? 1:0 ) ) - 1;
//			
//			
//			appAddressStart = ( _userfile.ProgramStartAddress - 0x10040000 )/2048;
//			appAddressEnd = ( _userfile.ProgramStartAddress + _userfile.ProgramSize - 0x10040000 )/2048;
//			
//			for( u8 tmp1 = appAddressStart;tmp1<=appAddressEnd;tmp1++ )
//			{
//					u32 readWord1 = 0;
//					writeAP( AP_CSW,0x23000002);   //32位写访问
//	
//					writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->ADDRESS);
//					writeAP( AP_DRW, tmp1<<9 );  // 写地址
//				
//					writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->COMMAND);
//					writeAP( AP_DRW, 0x11 );  // 擦除扇区命令
//					
//					writeAP( AP_CSW,0x00000002);   // 读
//					do
//					{
//						writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//						readAP(AP_DRW,&readWord1);
//						readDP(DP_RDBUFF,&readWord1);
//					}while(readWord1&0x2); // 当为1的时候说明操作完成
//					
//					writeAP( AP_CSW,0x23000002 );   //32位写访问
//					writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT );
//					writeAP( AP_DRW, readWord1 );  // 写1清除，这里通过之前读出来值进行操作。
//					
//					writeAP( AP_CSW,0x00000002);   // 读
//					do
//					{
//						writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//						readAP(AP_DRW,&readWord1);
//						readDP(DP_RDBUFF,&readWord1);
//					}while(readWord1&0x01); // 当为1的时候说明操作完成
//					
//					writeAP( AP_CSW,0x23000002);   //32位写访问
//					writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
//					writeAP( AP_DRW, readWord1 );  
//			}
//			
////			if( _userfile.RollingCodeFunction == 0x55 ) // 使能滚码操作
////			{
////				rollAddressStart = ( ( _userfile.RollingCodeStartAddress - 0x10040000 )/2048 + ( (( _userfile.RollingCodeStartAddress - 0x10040000 )%2048) ? 1:0 ) ) - 1;
////				rollAddressEnd = ( ( _userfile.RollingCodeStartAddress + 4 - 0x10040000 )/2048 + ( (( _userfile.RollingCodeStartAddress + 4 - 0x10040000 )%2048) ? 1:0 ) ) - 1;
////				
////				for( u8 tmp2 = rollAddressStart;tmp2<=rollAddressEnd;tmp2++ )
////				{
////					u32 readWord1 = 0;
////					writeAP( AP_CSW,0x23000002);   //32位写访问
////	
////					writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->ADDRESS);
////					writeAP( AP_DRW, tmp2<<9 );  // 写地址
////				
////					writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->COMMAND);
////					writeAP( AP_DRW, 0x11 );  // 擦除扇区命令
////					
////					writeAP( AP_CSW,0x00000002);   // 读
////					do
////					{
////						writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
////						readAP(AP_DRW,&readWord1);
////						readDP(DP_RDBUFF,&readWord1);
////					}while(readWord1&0x2); // 当为1的时候说明操作完成
////					
////					writeAP( AP_CSW,0x23000002 );   //32位写访问
////					writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT );
////					writeAP( AP_DRW, readWord1 );  // 写1清除，这里通过之前读出来值进行操作。
////					
////					writeAP( AP_CSW,0x00000002);   // 读
////					do
////					{
////						writeAP(AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
////						readAP(AP_DRW,&readWord1);
////						readDP(DP_RDBUFF,&readWord1);
////					}while(readWord1&0x01); // 当为1的时候说明操作完成
////					
////					writeAP( AP_CSW,0x23000002);   //32位写访问
////					writeAP( AP_TAR, (u32)&BLUENRG_1_FLASH->IRQSTAT);
////					writeAP( AP_DRW, readWord1 );  
////				}
////			}
//			
//		}
		OS_ERR err;
		OSTimeDlyHMSM(0,0,0,1000,OS_OPT_TIME_HMSM_NON_STRICT,&err);
		
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
						
//						writeAP( AP_CSW,0x23000002 | 0x10 );
//						writeAP( AP_TAR, Address );  // 地址！
						for( u32 tmp12 = 0;tmp12 <0x1000; tmp12 += 16 ) // 写入4K
						{
//							#define SWD_AUTO_ADDRESS_ADD_VALUE 0x400   // 特别警告：STM32F0系列的地址自动增加大小为：1024个，而不是F1和F4的4096个！
//							if( ((Address + tmp12) %SWD_AUTO_ADDRESS_ADD_VALUE) == 0 ) // 说明到了4k临界点，需要重新写地址
//							{
//								writeAP( AP_TAR, Address + tmp12 );  // 地址！
//							}
//							writeAP(AP_DRW, + ( W25Q4KBuf.databuf4K[tmp12+3] << 24) + ( W25Q4KBuf.databuf4K[tmp12+2] << 16 ) + ( W25Q4KBuf.databuf4K[tmp12+1] << 8) + ( W25Q4KBuf.databuf4K[tmp12] ) ); 
							blueNRG_1_BurstWrite(   (( Address + tmp12 ) )/4,
																		( W25Q4KBuf.databuf4K[tmp12+3] << 24) + ( W25Q4KBuf.databuf4K[tmp12+2] << 16 ) + ( W25Q4KBuf.databuf4K[tmp12+1] << 8) + ( W25Q4KBuf.databuf4K[tmp12+0] ),
																		( W25Q4KBuf.databuf4K[tmp12+7] << 24) + ( W25Q4KBuf.databuf4K[tmp12+6] << 16 ) + ( W25Q4KBuf.databuf4K[tmp12+5] << 8) + ( W25Q4KBuf.databuf4K[tmp12+4] ),
																		( W25Q4KBuf.databuf4K[tmp12+11] << 24) + ( W25Q4KBuf.databuf4K[tmp12+10] << 16 ) + ( W25Q4KBuf.databuf4K[tmp12+9] << 8) + ( W25Q4KBuf.databuf4K[tmp12+8] ),
																		( W25Q4KBuf.databuf4K[tmp12+15] << 24) + ( W25Q4KBuf.databuf4K[tmp12+14] << 16 ) + ( W25Q4KBuf.databuf4K[tmp12+13] << 8) + ( W25Q4KBuf.databuf4K[tmp12+12] )
																		);
//						OS_ERR err;
//						OSTimeDlyHMSM(0,0,0,1,OS_OPT_TIME_HMSM_NON_STRICT,&err);
						
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
// 滚码暂时放弃！
//				{ // 写滚码区域，注意：由于上面使用的是：32字节的连续操作，不知道会不会对这个滚码有影响。
//					u32 programTimesTmp15 = 0;  // 实际已经编程次数
//					u32 valueTmp16 = 0; 				// 实际写入的滚码值
//					if( _userfile.RollingCodeFunction == 0x55 ) // 表示需要操作滚码区
//					{
//						if( _userfile.DownloaderNumbers[255] != 0xFF )
//						{
//							programTimesTmp15 = _userfile.AlreadyProgrammedCarrayBit*(256+1) + 256;
//						}
//						else
//						{
//							for( u16 Tmp16 = 0;Tmp16<256;Tmp16++ )
//							{
//								if( _userfile.DownloaderNumbers[Tmp16] == 0xFF )
//								{
//									programTimesTmp15 = _userfile.AlreadyProgrammedCarrayBit*(256+1) + Tmp16;  // 实际编程次数
//									break;
//								}
//							}
//						}
//						valueTmp16 = _userfile.RollingCodeStartValue + ( _userfile.RollingCodeStepValue* programTimesTmp15 );
//						writeAP( AP_TAR, _userfile.RollingCodeStartAddress );  // 地址！
//						{
//							if( (_userfile.RollingCodeStartAddress)%4 != 0 ) // 表示不是4的倍数，则取的16位在高位
//							{
//								writeAP(AP_DRW, + ( ((valueTmp16>>24)&0xFF) << 24) + ( ((valueTmp16>>16)&0xFF) << 16 ) ); 
//							}
//							else
//							{
//								writeAP(AP_DRW, + ( ((valueTmp16>>8)&0xFF) << 8) + ( ((valueTmp16>>0)&0xFF) ) ); 
//							}
//						}
//						{
//							writeAP( AP_TAR, _userfile.RollingCodeStartAddress+2 );  // 地址！
//							{
//								if( (_userfile.RollingCodeStartAddress+2)%4 != 0 ) // 表示不是4的倍数，则取的16位在高位
//								{
//									writeAP(AP_DRW, + ( ((valueTmp16>>24)&0xFF) << 24) + ( ((valueTmp16>>16)&0xFF) << 16 ) ); 
//								}
//								else
//								{
//									writeAP(AP_DRW, + ( ((valueTmp16>>8)&0xFF) << 8) + ( ((valueTmp16>>0)&0xFF) ) ); 
//								}
//							}
//						}
//					
//						{ // 检验滚码区
//							u32 apId = 0;
//							writeDP( DP_SELECT, 0x00 );  // 访问寄存器
//							writeAP( AP_CSW,0x2 | 0x10);   // 长度为32bit，同时地址自动增加
//							writeAP( AP_TAR,_userfile.RollingCodeStartAddress);   // 写目标地址
//							readAP( AP_DRW,&apId);  // 第一次数据，不要！
//							readDP( DP_RDBUFF, &apId );
//							if( apId != valueTmp16 )
//							{
//								LCD_ShowString(0,12*11,12,RED, "    滚码校验失败     ");
//								returnNumber = 0x00;  // 检测错误
//								return returnNumber;
//							}
//						}
//					}
//				}
									
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
									if( ( addressTmp16 %0x400 ) == 0 )
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
						blueNRG_1_Protection( ENABLE,DISABLE );
						break;
					case 0x11: // 使能写保护
						blueNRG_1_Protection( DISABLE,ENABLE );
						break;
					case 0x1A: // 使能读写保护
						blueNRG_1_Protection( ENABLE,ENABLE );
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















