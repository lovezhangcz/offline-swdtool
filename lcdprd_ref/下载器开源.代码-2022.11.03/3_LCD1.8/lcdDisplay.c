/**
 * @Brief LCD显示程序，用于实际显示的界面，显然这个是状态机和程序。是要根据之前的状态来操作的
 * @Attention 显然之前并没有实际操作过显示界面程序，这次的操作也算是入门级别的操作。
 * 关于显示：上电默认的显示为文件信息。也就是
 */


#include "lcdDisplay.h" 

#define HW2 GPIOC->IDR.InputData15_R
#define HW1 GPIOC->IDR.InputData14_R
#define HW0 GPIOC->IDR.InputData13_R



u8 randomCode[8] = {0};   // 随机产生，主要的目的是远程文件！当然，在非远程文件可以正常运行后加上它会比较好！

//u8 PageNumberIndex = 11;  // 用于左右移动，9：引脚描述，10：状态信息，11：文件正文(默认！)12-22-：文件正文！(0-8暂时没有分配，用于日后的信息！备用！)
//u8 FileNumberIndexInPage = 0;   // 用于上下移动文件，通过和上面有配合，判断文件！（每一页，最多存储10个文件。）
//																// 0：表示当前无信息。1-10，表示文件编号！
//u8 FileOperationIndex = 0;      // 用于表示操作文件，显然在不同的状态下其按键性质不一样。
//																// 暂时只用于：0，下载程序。1：智能下载程序！
//u8 menuDepth = 0;   // 用于区分菜单深度，目前设计的只有二级菜单，日后其实还是要上emWin的，因为设计这个确实比较麻烦！
										// 0：一级。1：二级。
_MenuManager MenuManager = { 9,0,0,0,0 };  // 初始化

__align(8) char Value[60] = {0}; ;  // 定义显示电压的字符串
__align(8) char DisPlayBuffer[90] = {0}; 
/**
 *  @B 这个函数的操作界面为显示状态信息。
 *
 */
void displayStatus( )
{
	LCD_DispalyColor(WHITE);  // 清屏操作
	LCD_DispalyImage_Size( (u8 *)gImage_DownLoader,6,0,24,24);
	if( L_ENGLISH_VER != 0X55AA )
	{
		LCD_ShowString( 32,6,12,RED,"离线下载器");
	}
	else
	{
		LCD_ShowString( 32,6,12,RED," OFD-A1");
	}
	{
		{ // 更新并上传状态信息
			W25QFlashProgramFileInformation( );  // 执行一次文件信息检查
		}
	}
	{ // 
		if( L_ENGLISH_VER != 0X55AA )
		{
			strcpy(DisPlayBuffer," Boot 版本号:");
		}
		else
		{
			strcpy(DisPlayBuffer," Bootloader:");
		}
//		sprintf( Value,"%d.%d",L_BOOTLOADER_VERSION/10,L_BOOTLOADER_VERSION%10);
		sprintf( Value,"%d.%d",2,1);
		strcat( DisPlayBuffer,Value);
		LCD_ShowString(0,12*2,12,BLACK,DisPlayBuffer);
	}
	{
		if( L_ENGLISH_VER != 0X55AA )
		{
			strcpy(DisPlayBuffer," 软件版本号:");
		}
		else
		{
			strcpy(DisPlayBuffer," Application:");
		}
		if( L_APP_VERSION%2 == 0 ) // beta版本
		{
			sprintf( Value,"V%d.%d.%d_b",(L_APP_VERSION>>12)&0xF,(L_APP_VERSION>>8)&0xF,(L_APP_VERSION>>4)&0xF);
		}
		else // releas版本
		{
			sprintf( Value,"V%d.%d.%d_r",(L_APP_VERSION>>12)&0xF,(L_APP_VERSION>>8)&0xF,(L_APP_VERSION>>4)&0xF);
		}
		
		strcat( DisPlayBuffer,Value);
		LCD_ShowString(0,12*3,12,BLACK,DisPlayBuffer);
	}
	{
		u8 HardVesion = 0;
		{	// 硬件版本状态：从:2.0往上发布，注意STM32F1手册中表明：上拉电阻40K，所以控制版本的电阻建议：1K以下任意值，因为需要用到22R，所以使用22R电阻。
			// HW2 HW1 HW0 ：V
			//  1   1   1    2
			//  1   1   0    3   // 硬件版本注意：当前硬件版本已经从V3.0开始。但是前面10PCS嘉立创的芯片，因为使用了10K电阻，所以有可能显示版本会跳到2.0版本上！
			//  1   0   1    4
			//  1   0   0    5
			//  0   1   1    6
			//  0   1   0    7
			//  0   0   1    8
			//  0   0   0    9
			if(			 ( HW2 == 1 )&&( HW1 == 1 )&&( HW0 == 1 ) )
			{
				HardVesion = 20;
			}
			else if( ( HW2 == 1 )&&( HW1 == 1 )&&( HW0 == 0 ) )
			{
				HardVesion = 30;
			}
			else if( ( HW2 == 1 )&&( HW1 == 0 )&&( HW0 == 1 ) )
			{
				HardVesion = 40;
			}
			else if( ( HW2 == 1 )&&( HW1 == 0 )&&( HW0 == 0 ) )
			{
				HardVesion = 50;
			}
			else if( ( HW2 == 0 )&&( HW1 == 1 )&&( HW0 == 1 ) )
			{
				HardVesion = 60;
			}
			else if( ( HW2 == 0 )&&( HW1 == 1 )&&( HW0 == 0 ) )
			{
				HardVesion = 70;
			}
			else if( ( HW2 == 0 )&&( HW1 == 0 )&&( HW0 == 1) )
			{
				HardVesion = 80;
			}
			else 
			{
				HardVesion = 90;
			}
		}
		if( L_ENGLISH_VER != 0X55AA )
		{
			strcpy(DisPlayBuffer," 硬件版本号:");  // 硬件版本号，通过外部引脚的设置来改变的，日后进行更改！
		}
		else
		{
			strcpy(DisPlayBuffer," Hardware:"); 
		}
		sprintf( Value,"%d.%d",HardVesion/10,HardVesion%10);
		strcat( DisPlayBuffer,Value);
		LCD_ShowString(0,12*4,12,BLACK,DisPlayBuffer);
	}
	{
		if( L_ENGLISH_VER != 0X55AA )
		{
			strcpy(DisPlayBuffer," 剩余存储大小：           ");
		}
		else
		{
			strcpy(DisPlayBuffer," Free Space:  ");
		}
		sprintf( Value,"%d",W25QFlashInformation.RemainingSize);
		strcat( DisPlayBuffer,Value);
		if( L_ENGLISH_VER != 0X55AA )
		{
			strcat( DisPlayBuffer," 字节");
		}
		else
		{
			strcat( DisPlayBuffer," Bytes");
		}
		LCD_ShowString(0,12*5,12,BLACK,DisPlayBuffer);
	}
	{
		{ // 考虑到删除的方式，只是删除文件信息，所以暂时不显示，连续的最大文件信息。只在上位机中进行显示。
			// 这里用于显示：本芯片是否读保护，以及是否关闭了SWD。这里，只用于显示。
			// 此外：一定要禁止写保护。要不不能升级！
			if(  FLASH->OBR.ReadProtection_R == 1  )
			{
				if( L_ENGLISH_VER != 0X55AA )
				{
					LCD_ShowString(0,12*7,12,RED," 读保护中！    ");
				}
				else
				{
					LCD_ShowString(0,12*7,12,RED," Read protection!");
				}
			}
			else
			{
				if( L_ENGLISH_VER != 0X55AA )
				{
					LCD_ShowString(0,12*7,12,RED," 读保护错误！   ");
				}
				else
				{
					LCD_ShowString(0,12*7,12,RED," Read protect error");
				}
			}
//			if( FLASH->WRPR.WriteProtect_R == 0xFFFFFFFF )
//			{
//				if( L_ENGLISH_VER != 0X55AA )
//				{
//					LCD_ShowString(0,12*8,12,RED," 正确，无写保护 ");
//				}
//				else
//				{
//					LCD_ShowString(0,12*8,12,RED,"No Write Protection");
//				}
//			}
//			else
//			{
//				if( L_ENGLISH_VER != 0X55AA )
//				{
//					LCD_ShowString(0,12*8,12,RED," 错误，写保护中!  ");
//				}
//				else
//				{
//					LCD_ShowString(0,12*8,12,RED," Write protected! ");
//				}
//			}
		}
	}
	{
		if( L_ENGLISH_VER != 0X55AA )
		{
			strcpy(DisPlayBuffer," 文件数量:");
		}
		else
		{
			strcpy(DisPlayBuffer," Total Files: ");
		}
		sprintf( Value,"%d",W25QFlashInformation.FlieNumbers);
		strcat( DisPlayBuffer,Value);
		LCD_ShowString(0,12*8,12,BLACK,DisPlayBuffer);
	}
	{  // 亮度显示
		if( isHIDVersion == 1 )
		{
			if( L_ENGLISH_VER != 0X55AA )
			{
				strcpy(DisPlayBuffer,"     亮度:");
			}
			else
			{
				strcpy(DisPlayBuffer,"  Brightness: ");
			}
			u8 beishu = (L_LIGNT_LV+1)*10;  // 正常情况下应该是：1-10，还有一个是大于10
			if( beishu > 100 ) { beishu = 100; }
			sprintf( Value,"%02d%% ",beishu);  // 
			strcat( DisPlayBuffer,Value);
			LCD_ShowString(0,12*9,12,BLUE,DisPlayBuffer);
		}
	}
	{  // 随机码关闭
		{ // 改为蜂鸣器
			if( L_BEEP_ENABLE == 0x55AA )
			{ // 关闭蜂鸣器
				LCD_ShowString_Language(6,12*10,12,BLUE,"  蜂鸣器已关闭","    Buzzer off");					
			}
			else
			{ // 开启蜂鸣器
				LCD_ShowString_Language(6,12*10,12,BLUE,"  蜂鸣器已开启","    Buzzer on ");
			}
		}
	}
	{
		if( ( L_STM32SWDFUN == 0X5A ) && ( L_STM8SWINFUM == 0x5A ) )
		{
			strcpy(DisPlayBuffer,"   SWD+SWIN Enable   ");
		}
		else if( ( L_STM32SWDFUN == 0X5A ) && ( L_STM8SWINFUM != 0x5A ) )
		{
			strcpy(DisPlayBuffer,"     SWD Enable      ");
		}
		else if( ( L_STM32SWDFUN != 0X5A ) && ( L_STM8SWINFUM == 0x5A ) )
		{
			strcpy(DisPlayBuffer,"     SWIN Enable     ");
		}
		else if( ( L_STM32SWDFUN != 0X5A ) && ( L_STM8SWINFUM != 0x5A ) )
		{
			if( L_ENGLISH_VER != 0X55AA )
			{
				strcpy(DisPlayBuffer," 语言(language):Eng");
			}
			else
			{
				strcpy(DisPlayBuffer," 语言(language):中文");
			}
		}
		LCD_ShowString(0,12*11,12,BLUE,DisPlayBuffer); 
	}
	
	{  // mini的文件，需要增加剩余下载次数的显示。
		
	}
}


/**
 *  @B 这个函数用于显示文件列表！
 *     pageIndex：1-12（因为最多只有：120个文件！）
 *
 */
void displayFileList( u16 pageIndex )  // 可以改变传入的page值。注意状态！
{
	char DisPlayBuffer[50];   // 
	if( (pageIndex >= 1) && (pageIndex <= 12) )
	{
		LCD_DispalyColor(WHITE);  // 清屏操作
		
		if( L_ENGLISH_VER != 0X55AA )
		{
			LCD_ShowString(0,0,12,BLACK, "     文件列表      ");// 128/12 = 21.3,也就是清除需要21个空格！
		}
		else
		{
			LCD_ShowString(0,0,12,BLACK, "     File list     ");// 128/12 = 21.3,也就是清除需要21个空格！
		}
			{
				for( u8 i = (pageIndex-1)*10;i<=(pageIndex-1)*10+9;i++ )  // 只需要找前10个文件就可以了！
				{
					W25QXX_Read( filex.Buf,W25QXX_FlieDensityAddress( i/8,(i%8)*2  ,0 ),sizeof(filex.Buf));
					if( filex.Status == 0x55 )  // 表示这是一个正常的文件。这个时候需要显示这个文件！
					{
						memset(DisPlayBuffer,0,sizeof(DisPlayBuffer));
						DisPlayBuffer[0] = ' ';  // 第一个为空白！用于">"的显示
						for( u8 j=0;j<19;j++ )
						{
							DisPlayBuffer[j+1] = filex.FileName[j];
						}
						LCD_ShowString(0,12*(i%10+2),12,BLACK,DisPlayBuffer);  // 文件显示的行数：
					}
					else
					{
						if( i == (pageIndex-1)*10 )  // 表示第一个就没有文件！
						{
//							if( L_ENGLISH_VER != 0X55AA )
//							{
//								LCD_ShowString(0,12*2,12,BLACK,"       没有文件      " );
//							}
//							else
//							{
//								LCD_ShowString(0,12*2,12,BLACK,"       No File      " );
//							}
//							break;
						}
					}
				}
			}
	}
}

/**
 *  @B 这个函数用于指示当前所在的文件指示图标">"
 *
 */
//void displayFileIndex( u8 moveStep )  // 0:只显示不移动，1：向上移动，文件编号减。2：向下移动，文件编号加！
//{
//	if( (MenuManager.PageNumberIndex >=11) && (MenuManager.PageNumberIndex <= 22) )   // 只有是文件信息的时候才能操作！
//	{
//		switch( moveStep )  // 操作文件标号！
//		{
//			case 0:
//				break;
//			case 1:
//				if( MenuManager.FileNumberIndexInPage >= 2 )  // 1的话，不变，2的以上标号才减。
//					MenuManager.FileNumberIndexInPage = MenuManager.FileNumberIndexInPage - 1;
//				break;
//			case 2:
//				if( ( MenuManager.FileNumberIndexInPage + 1 <= 10 )  // 不超过10.
//						&&( ((MenuManager.FileNumberIndexInPage + 1)+((MenuManager.PageNumberIndex-10)*10 - 10)) <= W25QFlashInformation.FlieNumbers ) ) // 不能超过文件数！
//					MenuManager.FileNumberIndexInPage = MenuManager.FileNumberIndexInPage + 1;
//				break;
//			default:
//				break;
//		}
//		{ // 显示文件标号！
//			LCD_ShowString(0,12*(MenuManager.FileNumberIndexInPage+1),12,">");
//		}
//	}
//	else
//	{
//		MenuManager.FileNumberIndexInPage = 0;
//	}
//}

/**
 *  @B 更新编程次数显示信息！
 *
 */
void freashDisplayProgramTimes( _FileInformation userFile )
{
	char DisPlayBuffer[50] = {0}; 
	char Value1[30] = {0}; ;  // 定义显示电压的字符串
	filex.AlreadyProgrammedCarrayBit = 	( filex.DownloaderNumbers[0]<<24 )
																		+ ( filex.DownloaderNumbers[1]<<16 )
																		+	( filex.DownloaderNumbers[2]<<8  )
																		+ ( filex.DownloaderNumbers[3]<<0  );  // 由于文件结构变动，现在使用这一种方法
	{ // 编程次数
		u32 timesTmp0 = 0;
		if( filex.ProgrammeTimes == 0xFFFFFFFF ) // 不限制次数
		{
			timesTmp0 = 0xFFFFFFFF;
		}
		else
		{
			if( filex.DownloaderNumbers[255] == 0x55 ) // 表示指示区已经满
			{
				if( filex.ProgrammeTimes <= filex.AlreadyProgrammedCarrayBit*(251+1) + 251 )  // 表明已经不能在下载了
				{
					timesTmp0 = 0;
				}
				else
				{
					timesTmp0 = filex.ProgrammeTimes - (filex.AlreadyProgrammedCarrayBit*(251+1) + 251);
				}
			}
			else
				{ // 先找出操作的指示区次数！
					for( u32 tmp0 = 5;tmp0<256;tmp0 ++)
					{
						if( filex.DownloaderNumbers[tmp0] == 0xFF )
						{
							if( filex.ProgrammeTimes < filex.AlreadyProgrammedCarrayBit*(256+1) + tmp0 -5 + 1 )
							{
								timesTmp0 = 0;
							}
							else
							{
								timesTmp0 = filex.ProgrammeTimes - (filex.AlreadyProgrammedCarrayBit*(251+1) + tmp0-5);
							}
							break;
						}
					}
				}
			}
		memset(Value1,0,sizeof(Value1));
		memset(DisPlayBuffer,0,sizeof(DisPlayBuffer));
			if( L_ENGLISH_VER != 0X55AA )
			{
				strcpy(DisPlayBuffer," 次     ");
			}
			else
			{
				strcpy(DisPlayBuffer," times  ");
			}
		sprintf( Value1,"%d",timesTmp0 );
		strcat( Value1,DisPlayBuffer);
			{
		memset(DisPlayBuffer,0,sizeof(DisPlayBuffer));
			if( L_ENGLISH_VER != 0X55AA )
			{
				strcpy(DisPlayBuffer,"剩余: ");
			}
			else
			{
				strcpy(DisPlayBuffer,"Remain: ");
			}
		strcat( DisPlayBuffer,Value1);
			}
			if( timesTmp0 != 0xFFFFFFFF )
			{
				LCD_ShowString( 6*2,12*3,12,BLUE,DisPlayBuffer );
			}
			else
			{
				if( L_ENGLISH_VER != 0X55AA )
				{
					LCD_ShowString( 6*2,12*3,12,BLUE,"不限下载次数" );
				}
				else
				{
					LCD_ShowString( 6*2,12*3,12,BLUE,"Unlimited downloads" );
				}
			}
			{  // 
			}
	}
}

/**
 *  @B 更新显示信息，用状态来更新显示！
 *  @B 程序中，同时也对文件进行了检查，可以一定程序上指示文件属性的错误！
 *
 */

void freashDisplayInformation( _MenuManager managerInput )
{
	{ // 进行纠正算法！
		if( ( managerInput.PageNumberIndex >22 ) || ( managerInput.PageNumberIndex<9 ) )
		{
			managerInput.PageNumberIndex = 8; // 引脚描述
		}
		if( managerInput.FileOperationIndex > 1 )
		{
			managerInput.FileOperationIndex = 0; // 按键下载
		}
		if( managerInput.menuDepth > 1 )
		{
			managerInput.menuDepth = 0; // 用于0级显示界面
		}
		// 关于指示，应该是要默认指示最后一条，前提是：存在一个文件
//		if( managerInput.FileNumberIndexInPage > 10 )  // 这个要进行判断了！
//		{
//			// 为什么会大于10，可以理解为往下移动到头了。
//			W25QFlashProgramFileInformation();  // 执行一次文件信息检查			
//			if( ( managerInput.PageNumberIndex >=10 ) && ( managerInput.PageNumberIndex <= 21 ) )  // 表示在文件信息之内！
//			{
//				if( W25QFlashInformation.FlieNumbers < managerInput.PageNumberIndex*10 )
//				{
//					managerInput.FileNumberIndexInPage = 0;  // 表示没有文件！
//				}
//				else if( W25QFlashInformation.FlieNumbers > ( managerInput.PageNumberIndex*10 + 9) )
//				{
//					managerInput.FileNumberIndexInPage = 10; // 这个时候只能指示最后一个文件！ 
//				}
//				else
//				{
//					managerInput.FileNumberIndexInPage = W25QFlashInformation.FlieNumbers%10 + 1;
//				}
//			}
//		}
	}
	{ // 使用状态来判断！
		switch( managerInput.menuDepth ) // 深度判断
		{
			case 0: // 第0级深度
				if( managerInput.PageNumberIndex == 8 )  // 引脚描述
				{
					LCDShowZMDZPinDescription( );
					MenuManager.lastDisplayWindows = 1;
				}
				else if( managerInput.PageNumberIndex == 9 ) // 状态信息
				{
					displayStatus( );
					MenuManager.lastDisplayWindows = 2;
				}
				else
				{
					if( MenuManager.lastDisplayWindows != MenuManager.PageNumberIndex )
					{
						displayFileList( managerInput.PageNumberIndex - 10 + 1 );  // 用于显示文件信息
						MenuManager.lastDisplayWindows = MenuManager.PageNumberIndex;
					}
					if(  W25QFlashInformation.FlieNumbers >= ( managerInput.PageNumberIndex-10)*10 )  // 如果这页面存在文件，则进行显示！
					{
						{
							// 清除LCD的指示的显示
							for( u8 tmp = 0;tmp<11;tmp++ )
							{
								LCD_ShowString(0,12*(tmp+1),12,BLUE," ");
							}
						}
						if( managerInput.FileNumberIndexInPage != 0)
							LCD_ShowString(0,12*(managerInput.FileNumberIndexInPage+1),12,BLUE,"$");
					}
				}
				break;
			case 1: // 第1级深度
				{
					if( MenuManager.lastDisplayWindows != 4 )
					{
						char DisPlayBuffer[50] = {0}; 
						char Value1[30] = {0}; ;  // 定义显示电压的字符串
						LCD_DispalyColor( WHITE ); // 清屏
	//					LCD_ShowString(0,0,12, "   File Operation   ");// 128/12 = 21.3,也就是清除需要21个空格！
						{ // 根据状态读出文件！
							W25QXX_Read( filex.Buf, 
							W25QXX_FlieDensityAddress( ((( managerInput.PageNumberIndex-10 )*10)+ managerInput.FileNumberIndexInPage - 1 )/8,(((( managerInput.PageNumberIndex-10 )*10)+ managerInput.FileNumberIndexInPage - 1 )%8)*2  ,0 ),
							sizeof(filex.Buf) );
							
							W25QXX_Read( filex.DownloaderNumbers, 
							W25QXX_FlieDensityAddress( ((( managerInput.PageNumberIndex-10 )*10)+ managerInput.FileNumberIndexInPage - 1 )/8,(((( managerInput.PageNumberIndex-10 )*10)+ managerInput.FileNumberIndexInPage - 1 )%8)*2 + 1 ,0 ),
							sizeof(filex.Buf) );
							
							{ // 进行文件检查
								{
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString(0,12*1,12,RED," 检查文件..... ");  // 指示正在进行文件检查
									}
									else
									{
										LCD_ShowString(0,12*1,12,RED," Loading File..... ");
									}
								}
								{ // 文件信息和校验
									if( filex.SumValue !=  GetSum( filex.Buf,sizeof(filex.Buf) -1 ) )
									{ // 
										if( L_ENGLISH_VER != 0x55AA )
										{
											LCD_ShowString(0,12*0,12,RED,"错误：文件检查错误");
											LCD_ShowString(0,12*1,12,RED,"请删除此文件后，再重新配置此文件");
										}
										else
										{
											LCD_ShowString(0,12*0,12,RED,"Error: file checking error,Please delete this file and reconfiguring it");
										}
											return;
										}
								}
								{ // W25Q程序存储区和校验
									u8 w25qReadValueTmp0 = 0;
									u16 sumTmp1 = 0;
									u32 startAddressTmp2 = filex.ProgramSaveAtW25QAddress;  // 起始大小
									for( u32 pTmp3 = 0;pTmp3<filex.ProgramSize;pTmp3++ )  //
									{
										W25QXX_Read( &w25qReadValueTmp0,startAddressTmp2+pTmp3,1 );
										sumTmp1 += w25qReadValueTmp0;
									}
									if( filex.SumValueOfProgram != sumTmp1 )
									{
										if( L_ENGLISH_VER != 0x55AA )
										{
											LCD_ShowString(0,12*0,12,RED,"错误：加密错误! ");
											LCD_ShowString(0,12*1,12,RED,"请重新配置此文件");
										}
										else
										{
											LCD_ShowString(0,12*0,12,RED,"Error: encryption error!Please delete this file and reconfiguring it");
										}
										return;
									}
								}
							}
							if( filex.Status == 0x55 )  // 表示这是一个正常的文件。这个时候需要显示这个文件！
							{
								memset(DisPlayBuffer,0,sizeof(DisPlayBuffer));
								DisPlayBuffer[0] = ' ';  // 为空白！这样会好看点
								DisPlayBuffer[1] = ' ';  // 为空白！这样会好看点
								for( u8 j=0;j<19;j++ )
								{
									DisPlayBuffer[j+2] = filex.FileName[j];
								}
								LCD_ShowString(0,12*0,12,BLACK,DisPlayBuffer);  //
							}
							else
							{
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*0,12,RED,"  错误：没有文件  ");
								}
								else
								{
									LCD_ShowString(0,12*0,12,RED,"  Error: no file  ");
								}
							}
						}
						LCD_ShowString(0,12*1,12,BLUE, "                    ");
						switch( filex.ChipTypePrefix )
						{
							case 0x00:
								LCD_ShowString(12,12*1,12,BLUE, "STM32F0XX");
								break;
							case 0x10:
								LCD_ShowString(12,12*1,12,BLUE, "STM32F1XX");
								break;
							case 0x20:
								LCD_ShowString(12,12*1,12,BLUE, "STM32F2XX");
								break;
							case 0x30:
								LCD_ShowString(12,12*1,12,BLUE, "STM32F3XX");
								break;
							case 0x40:
								LCD_ShowString(12,12*1,12,BLUE, "STM32F4XX");
								break;
							case 0x41:
								LCD_ShowString(12,12*1,12,BLUE, "STM32F7XX");
								break;
							case 0x50:
								LCD_ShowString(12,12*1,12,BLUE, "STM32L0XX");
								break;
							case 0x51:
								LCD_ShowString(12,12*1,12,BLUE, "STM32L1XX");
								break;
							case 0x54:
								LCD_ShowString(12,12*1,12,BLUE, "STM32L4XX");
								break;
							case 0x55:
								LCD_ShowString(12,12*1,12,BLUE, "BLUENRG-1");
								break;
							case 0x56:
								LCD_ShowString(12,12*1,12,BLUE, "BLUENRG-2");
								break;
							case 0x60:
								{ // 一共只有123种型号，这些具体型号日后有时间进行具体的更新！
									// 最终是和STVP和选项字节对应上的。所以第个型号都要对比下选项字节！！
									switch( filex.ChipTypeSuffix )
									{
										// AF型号
											case 0:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF516");  //  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 C7 00  （默认值！）
												// 32768 
											  // ( 4800 - 480E 00 00FF 00FF 00FF...  4810-4818 00 00 00 00 00 00 00 00 C7 487E-487F 00FF )
												break;
											case 1:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF517");  // 同上
												// 48K
												break;
											case 2:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF518");  // 同上
											  // 64K
												break; 
											case 3:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF518A"); // 同上
												// 64K
												break;
											case 4:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF519");  // 同上
												// 96K
												break;
											case 5:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF51A");  // 同上
												// 128K
												break;
											case 6:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF526");  // 同上
												break;
											case 7:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF528");  // 同上
												break;
											case 8:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF52A");  // 同上
												break;
											case 9:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6126");  // 同上
												// 8KB
												break;
											case 10:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6146");  // 同上
												break;
											case 11:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6148");  // 同上
												break;
											case 12:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6166");  // 同上
												break;
											case 13:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6168");  // 同上
												break;
											case 14:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6169");  // 同上
												break;
											case 15:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF617");  // 同上
												break;
											case 16:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF618");  // 同上
												break;
											case 17:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF618A");  // 同上
												break;
											case 18:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF619");  // 同上
												break;
											case 19:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF61A");  // 同上
												break;
											case 20:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6213");  // 4K 00 00 00 00 00 00 (4800-480A 00 00FF 00FF 00FF 00FF 00FF)
												break;
											case 21:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6223");  // 8K 00 00 00 00 00 00 (同上)
												break;
											case 22:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6223PxA");  // 8K 00 00 00 00 00 00 (同上)
											break;
											case 23:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6226");  //8K  00 00 00 00 00 00 (同上)
												break;
											case 24:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6226TxSSS");  // 8K  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 C7 00  （默认值！）
												break;
											case 25:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6246");  // 16KB 同上
												break;
											case 26:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6248");  // 同上
												break;
											case 27:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6226");  // 同上
												break;
											case 28:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6268");  // 同上
												break;
											case 29:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF6269");  // 同上
												break;
											case 30:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF628");  // 同上
												break;
											case 31:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AF62A");  // 同上
												break;
											
											// 下面为AH型号，应该就是AF系列相对应的没有EEPROM的型号
											case 32:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH516");  // 同上
												break;
											case 33:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH517");  // 同上
												break;
											case 34:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH518");  // 同上
												break;
											case 35:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH519");  // 同上
												break;
											case 36:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH51A");  // 同上
												break;
											case 37:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH6126");  // 同上
												break;
											case 38:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH6146");  // 同上
												break;
											case 39:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH6148");  // 同上
												break;
											case 40:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH6166");  // 同上
												break;
											
											case 41:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH6168");  // 同上
												break;
											case 42:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH6169");  // 同上
												break;
											case 43:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH617");  // 同上
												break;
											case 44:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH618");  // 同上
												break;
											case 45:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH619");  // 同上
												break;
											case 46:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AH61A");  // 同上
												break;
											
											
											// STM8AL 型号
											case 47:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3136xxSSS");  // 8K    AA 00 00 00 00 00 00
												break;
											case 48:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3138xxSSS");  // 8K   AA 00 00 00 00 00 00
												break;
											case 49:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3146"); // 16K   AA 00 00 00 00 00 00  (0 2 8 9 A B C )
												break;
											case 50:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3148"); // 16K  (同上)
												break;
											case 51:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3166"); // 32K   (同上)
												break;
											case 52:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3168"); // 32K   (同上)
												break;
											case 53:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL318x"); // 64K  AA 00 00 00 00 01 00 00 ( 好像不需要反码！这个一定要确认！ )
											break;                                         	 //     ( 4800 AA 2/7/8/9/B/C:00 A:01 ) (7：PSIZE ，其它少一位的都是由于这个位必须为0而已！)
											case 54:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL31E8x");// 64K  (同上)
												break;
											case 55:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3L46"); // 16K  AA 00 00 00 00 00 00
												break;
											case 56:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3L48");  // 16K   (同上)
												break;
											case 57: 
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3L66");  // 32K   (同上)
											break;
											case 58:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3L68");  // 32  (同上)
												break;
											case 59:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3L8x");  // 64K  AA 00 00 00 00 01 00 00
												break;                                         // ( 4800 AA 2/7/8/9/B/C:00 A:01 )
											case 60:
												LCD_ShowString(12,12*1,12,BLUE, "STM8AL3LE8x"); // 64    AA 00 00 00 00 01 00 00
												break;
											
											// STM8L 系列 
											case 61:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L001J3"); // 8KB   00 00 00 00 ( 0 2 3 8 其中1必须写0)
												break;
											case 62:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L050J3");  // 8KB   AA 00 00 00 00 01 00 00 (0 2 7 8 9 A(写01的) B C)
												break;
											case 63:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L051x3");  // 8KB   (同上)
												break;
											case 64:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L052C6");  //       AA 00 00 00 00 00 00 ( 0 2 8 9 A B C )(实际上7是默认值00)
												break;
											case 65:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L052R8");  // AA 00 00 00 00 01 00 00
												break;
											case 66:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L101F1");  // 00 00 00( 4800 4802 4808 )( 手册说明其4807必须写00，但是应该是：4801地址，因为顺序上是4801。且STM8TL型号也表明是4801 )
												break;
											case 67:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L101x2");  // 4K 选项字节：00 00 00  无EEPROM (同上)
											break;
											case 68:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L101x3");  // 8K 选项字节：00 00 00 00 无EEPROM (多了一个DataSize 4803)
												break;
											case 69:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L151x2");  // AA 00 00 00 00 01 00 00
											break;
											case 70:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L151x3");  // (同上)
												break;
											case 71:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xC4"); // 16K   AA 00 00 00 00 00 00
												break;
											case 72:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xC6"); // 32K  (同上)
												break;
											case 73:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xC8"); // 64K  AA 00 00 00 00 01 00 00
												break;
											case 74:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xG4"); // AA 00 00 00 00 00 00
												break;
											case 75:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xG6"); // (同上)
												break;
											case 76:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xK4"); // (同上)
												break;
											case 77:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xK6"); // (同上)
												break;
											case 78:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xK8");  // AA 00 00 00 00 01 00 00
												break;
											case 79:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xM8");  // (同上)
												break;
											case 80:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xR6");  // (同上)
												break;
											
											
											case 81:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L15xR8");  // (同上)
												break;
											case 82:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L162M8");  // (同上)
												break;
											case 83:
												LCD_ShowString(12,12*1,12,BLUE, "STM8L162R8");  // (同上)
												break;
											
											// 8S系列
											case 84:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S001J3"); //    00 00 00 00 00 00 
												break;
											case 85:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S003F3"); // 8K 00 00 00 00 00 00 (4800-480A 00 00FF 00FF 00FF 00FF 00FF)(同上)
												break;
											case 86:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S003K3"); // 8K 00 00 00 00 00 00 (4800-480A 00 00FF 00FF 00FF 00FF 00FF)(同上)
												break;
											case 87:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S005C6"); // 00 00 00 00 00 00 00 00 00 ( 4800-480E 00 00FF 00FF 00FF 00FF 00FF 00FF 00FF 487E-487F 00FF )
												break;
											case 88:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S005K6"); // 00 00 00 00 00 00 00 00 00 (同上)
												break;
											case 89:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S007C8"); // 同上
												break;
											case 90:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S103F2"); // 00 00 00 00 00 00 
												break;
											case 91:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S103F3"); // 同上
												break;
											case 92:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S103K3"); // 同上 
												break;
											case 93:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S105x4"); // 00 00 00 00 00 00 00 00 00
												break;
											case 94:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S105x6"); // 同上
												break;
											case 95:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207C6"); // 同上
												break;
											case 96:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207C8"); // 同上
												break;
											case 97:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207CB");  // 同上
												break;
											case 98:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207K6"); // 同上
												break;
											case 99:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207K8"); // 同上
												break;
											case 100:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207M8");// 同上
												break;
											case 101:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207MB");// 同上
												break;
											case 102:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207R6");// 同上
												break;
											case 103:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207R8");// 同上
												break;
											case 104:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207RB");// 同上
												break;
											case 105:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207S6");// 同上
												break;
											case 106:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207S8");// 同上
												break;
											case 107:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S207SB");// 同上
												break;
											case 108:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208C6");// 同上
												break;
											case 109:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208C8");// 同上
												break;
											case 110:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208CB");// 同上
												break;
											case 111:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208M8");// 同上
												break;
											case 112:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208MB");// 同上
												break;
											case 113:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208R6");// 同上
												break;
											case 114:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208R8");// 同上
												break;
											case 115:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208RB");// 同上
												break;
											case 116:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208S6");// 同上
												break;
											case 117:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208S8");// 同上
											break;
											case 118:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S208SB");// 同上
												break;
											case 119:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S903F3");  // 00 00 00 00 00 00  8K Flash(8000-) 640eep(4000-427F)  OPTION 4800-480A 默认值：00 00FF 00FF 00FF 00FF 00FF （11字节）
											break;
											case 120:
												LCD_ShowString(12,12*1,12,BLUE, "STM8S903K3");  // 00 00 00 00 00 00  8K Flash(8000-) 640eep(4000-427F)  OPTION 4800-480A 默认值：00 00FF 00FF 00FF 00FF 00FF （11字节）
												break;
											
											case 121:
												LCD_ShowString(12,12*1,12,BLUE, "STM8TL52x4"); // 16K AA 00 00 00 00 (0 2 3 7 8 ) 其中1地址必须写0
												break;
											case 122:
												LCD_ShowString(12,12*1,12,BLUE, "STM8TL3x4"); // 16K AA 00 00 00 00 (同上)
												break;
											default:
												LCD_ShowString(12,12*1,12,BLUE, "illegal STM8");
												break;
									}
								}
								break;
							default:
								break;
						}
						{
							memset(Value1,0,sizeof(Value1));
							memset(DisPlayBuffer,0,sizeof(DisPlayBuffer));
							
							if( L_ENGLISH_VER != 0x55AA )
							{
								strcpy(DisPlayBuffer," 字节");
							}
							else
							{
								strcpy(DisPlayBuffer," ByteS");
							}
							sprintf( Value1,"%d + %d",filex.ProgramSize,filex.ProgramSize_STM8 );
							strcat( Value1,DisPlayBuffer);
							LCD_ShowString( 6*2,12*2,12,BLACK,Value1 );
						}
						{ // 编程次数
							freashDisplayProgramTimes(filex);
						}
						{ // 擦除方式
							if( filex.ChipEraseWay == 0x00 )
							{
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString( 0,12*4,12,BLACK,"  全部擦除   " );
								}
								else
								{
									LCD_ShowString( 0,12*4,12,BLACK,"  All erased " );
								}
							}							
							else if( filex.ChipEraseWay == 0x01 )
							{
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString( 0,12*4,12,BLACK,"  擦除扇区 " );
								}
								else
								{
									LCD_ShowString( 0,12*4,12,BLACK,"  Erase sector " );
								}
							}
						}
						{ // 校验方式
							switch( filex.FlashCheckWay )
							{
								case 0x00:
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString( 0,12*5,12,BLUE,"  无校验   " );
									}
									else
									{
										LCD_ShowString( 0,12*5,12,BLUE,"  no parity   " );
									}
									break;
								case 0x01:
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString( 0,12*5,12,BLUE,"  逐个对比 " );
									}
									else
									{
										LCD_ShowString( 0,12*5,12,BLUE,"  By contrast " );
									}
									break;
								case 0x02:
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString( 0,12*5,12,BLUE,"  CRC 校验 " );
									}
									else
									{
										LCD_ShowString( 0,12*5,12,BLUE,"  CRC verify " );
									}
									break;
								case 0x03:
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString( 0,12*5,12,BLUE,"  MD5 校验 " );
									}
									else
									{
										LCD_ShowString( 0,12*5,12,BLUE,"  MD5 verify " );
									}
									break;
								default:
									break;
							}
						}
						{ // 保护方式
							switch( filex.OptionBytesFunction )
							{
								case 0x55: // 使能选项字节，日后功能开放！
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString( 0,12*6,12,BLACK,"  使能选项字节 " );
									}
									else
									{
										LCD_ShowString( 0,12*6,12,BLACK,"  Enable option bytes " );
									}
									break;
								case 0xAA: // 使能读保护
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString( 0,12*6,12,BLACK,"  读保护    " );
									}
									else
									{
										LCD_ShowString( 0,12*6,12,BLACK,"  read protection " );
									}
									break;
								case 0x11: // 使能写保护
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString( 0,12*6,12,BLACK,"  写保护    " );
									}
									else
									{
										LCD_ShowString( 0,12*6,12,BLACK,"  write protection " );
									}
									break;
								case 0x1A: // 使能读写保护
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString( 0,12*6,12,BLACK,"  读+写保护 " );
									}
									else
									{
										LCD_ShowString( 0,12*6,12,BLACK,"  r+w protection" );
									}
									break;
								default:
									if( L_ENGLISH_VER != 0x55AA )
									{
										LCD_ShowString( 0,12*6,12,BLACK,"  无保护    " );
									}
									else
									{
										LCD_ShowString( 0,12*6,12,BLACK,"  no protection " );
									}
									break;
							}
						}
						{	// 速度模式
							if( filex.SWDLowSpeedEnable == 0x55)
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*7,12,BLUE, "  低速模式   ");
								}
								else
								{
									LCD_ShowString(0,12*7,12,BLUE, "  Slow Mode   ");
								}
							else
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*7,12,BLUE, "  高速模式   ");
								}
								else
								{
									LCD_ShowString(0,12*7,12,BLUE, "  High Speed mode   ");
								}
						}
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*8,12,BLACK, " 单次下载   ");
									LCD_ShowString(0,12*9,12,BLACK, " 自动下载   ");
								}
								else
								{
									LCD_ShowString(0,12*8,12,BLACK, " Single program   ");
									LCD_ShowString(0,12*9,12,BLACK, " Auto program   ");
								}
					
						MenuManager.lastDisplayWindows = 4;
				}
					{  // 清除显示
						LCD_ShowString(0,12*8,12,BLUE," ");
						LCD_ShowString(0,12*9,12,BLUE," ");
					}
					if( managerInput.FileOperationIndex == 0 )
					{
						LCD_ShowString(0,12*8,12,BLUE,"$");
					}
					else if( managerInput.FileOperationIndex == 1 )
					{
						LCD_ShowString(0,12*9,12,BLUE,"$");
					}
					break;
				}
			default:
				break;
		}
	}
}







