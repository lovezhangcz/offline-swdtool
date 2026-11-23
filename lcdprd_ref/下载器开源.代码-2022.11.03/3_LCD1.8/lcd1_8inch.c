/**
 * @Brief 1.8inch显示屏驱动程序
 *        记住一定要一步一个脚印，不可以追求过快的速度，务必要合理
 */

#include "lcd1_8inch.h" 
#include "lcdfont.h"   // 用于字库的显示和操作
#include "lcdCFG.h" 
#include "STM32_USART.h"


OS_MUTEX LCD_ShowsrtingUseRight_Mutex;  // LCD使用权，防止被中断使用，出现不明原因错误！


//void delayLittleForHid( void )
//{
//	for(vu32 i = 0;i<0xf;i++);
//}

void LCD_WriteCommand( u8 command )
{
	if( isHIDVersion == 1 )
	{
		HID_LCD_DC = 0;  // 命令
		HID_LCD_WR = 0;  // 写命令先保持，用于上升沿做准备。
		HID_LCD_CS = 0;  // 实际测试，不用CS一直保持为0也可以，但是为了稳定！
		GPIOC->ODR.highPort_RW = command;
		HID_LCD_WR = 1; // 上升沿
		HID_LCD_CS = 1;
	}
	else
	{
		LCD_DataOrCommand = 0;  // 类型为命令
		LCD_CS = 0;  // 片选信号
		{ // 
			SPISendAData( SPI2,command );
		}
		LCD_CS = 1;  // 片选信号
	}
}

void LCD_WriteData( u8 data )
{
	if( isHIDVersion == 1 )
	{
		HID_LCD_DC = 1;  // 数据
		HID_LCD_WR = 0;  // 写命令先保持，用于上升沿做准备。
		HID_LCD_CS = 0;  // 实际测试，不用CS一直保持为0也可以，但是为了稳定！
		GPIOC->ODR.highPort_RW = data;
		HID_LCD_WR = 1; // 上升沿
		HID_LCD_CS = 1;
	}
	else
	{
		LCD_DataOrCommand = 1;  // 类型为数据
		LCD_CS = 0;  // 片选信号
		{ // 
			SPISendAData( SPI2,data );
		}
		LCD_CS = 1;  // 片选信号
	}
}
/**
 *  \B 这是一个写全屏幕的程序，设置方框地址
 *
 */

void LCD_DispalyWindows( void )
{
	LCD_WriteCommand(0x2A);  // 设置列地址
	{
		LCD_WriteData(0x00);   // 列起始地址
		LCD_WriteData(0x00);   
	}
	{
		LCD_WriteData(0x00);   // 列结束地址，0x7F正好是宽度
		LCD_WriteData(0x7F);
	}

	LCD_WriteCommand(0x2B);  // 设置行地址
	{
		LCD_WriteData(0x00);
		LCD_WriteData(0x00);
	}
	{
		LCD_WriteData(0x00);
		LCD_WriteData(0x9f);
	}
	LCD_WriteCommand(0x2C);  // 这个时候表示可以写参数了！然后后面就是跟着写参数。
}

/**
 *  @B 这是一个屏幕着色的程序
 *
 */
void LCD_DispalyColor( u16 RGBValue )
{
	LCD_DispalyWindows( );
	for(u8 i=Height;i>0;i--)
	{
		for(u8 j=Width; j>0;j--)
		{
			LCD_WriteData(RGBValue>>8);
			LCD_WriteData(RGBValue);
		}
	}
}

/**
 *  @B 这应该是一个显示屏的测试，可能是用于RGB的测试
 *  \B 实际中这是一个显示RGB的代码！
 */
void LCD_DisplayRGB( void )
{
// 有点感觉下面的程序不理想
	for (u8 i=53;i>0;i--)
	{
		for (u8 j=Width;j>0;j--)
		{
			LCD_WriteData(     RED>>8 );
			LCD_WriteData( (u8)RED );
		}
	}
  for (u8 i=53;i>0;i--)
	{
		for (u8 j=Width;j>0;j--)
		{
			LCD_WriteData(     GREEN>>8 );
			LCD_WriteData( (u8)GREEN );
		}
	}
  for (u8 k=54;k>0;k--)
	{
		for (u8 j=Width;j>0;j--)
		{
			LCD_WriteData(     BLUE>>8 );
			LCD_WriteData( (u8)BLUE );
		}
	}
}

/**
 *  @B 顾名思义，这是一个显示全屏图片的程序
 *
 */
void LCD_DispalyImage( u8 *pic )
{
	u16 p=0;
 	LCD_DispalyWindows();
	for (u16 i=Width*Height*2;i>0;i--)
	{
		LCD_WriteData(pic[p++]);
	}  
}



/**
 *  @B 定义起始位置，同时限制大小的图片的显示！
 *
 */
void LCD_DispalyImage_Size( u8 *pic ,u16 startX,u16 startY,u16 wideSize,u16 heightSize )
{
	u16 p=0;
	{
	LCD_WriteCommand(0x2A);  // 设置列地址
		{
			LCD_WriteData(0x00);   // 列起始地址
			LCD_WriteData(startX);   
		}
		{
			LCD_WriteData(0x00);   // 列结束地址，0x7F正好是宽度
			LCD_WriteData(startX+wideSize-1);
		}

		LCD_WriteCommand(0x2B);  // 设置行地址
		{
			LCD_WriteData(0x00);
			LCD_WriteData(startY);
		}
		{
			LCD_WriteData(0x00);
			LCD_WriteData(startY+heightSize-1);
		}
		LCD_WriteCommand(0x2C);  // 这个时候表示可以写参数了！然后后面就是跟着写参数。
	}
	for (u16 i=wideSize*heightSize*2;i>0;i--)
	{
		LCD_WriteData(pic[p++]);
	}  
}





/**
 * \B 显示屏使用了ST7735显示IC芯片，初始化实际上就是控制ST7735这个芯片
 * 因为日后都需要知道这个，所以务必要了解每个指令的具体含义，这样日后可以快速开发芯片
 * 
 */

void LCDInitOfST7735CtlIC( void )
{
	if( isHIDVersion == 0 )
	{
		{ // 作为一个项目，这个初始化IO引脚的不应该存在这里，因为这应该是一个通用的不应该更改的模板
			// 初始化引脚
			RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
			GPIOA->CRH.InOutMode9_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
			GPIOA->CRH.PinConfig9_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
			GPIOA->CRH.InOutMode10_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
			GPIOA->CRH.PinConfig10_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
			
			RCC->APB2ENR.IOPBClockEnable_RW = ENABLE;
			GPIOB->CRH.InOutMode12_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
			GPIOB->CRH.PinConfig12_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
			
			LCD_RST = 1;
			LCD_MOSI = 1;
			LCD_SCK = 1;
			LCD_CS = 1;
			
			SPIInit(SPI2,SPI_MasterMode,SPI_BaudRatePrescaler_2,SPIDataLengthToEight,PolarityType10,DisableIrq);  // 模式可能会改变
			
		}
		
		LCD_RST = 1;
		TimeDelayOfSoftAtMs(300);
		LCD_RST = 0; // 低电平复位
		TimeDelayOfSoftAtMs(300);
		LCD_RST = 1;
		TimeDelayOfSoftAtMs(300);  // 延时以稳定
	}
	else
	{  // hid版本
		RCC->APB2ENR.IOPCClockEnable_RW = ENABLE;
		GPIOC->CRH.All = 0x33333333;
		GPIOC->ODR.highPort_RW = 0xFF;  // LCD数据引脚
		
		RCC->APB2ENR.IOPBClockEnable_RW = ENABLE;
		GPIOB->CRL.InOutMode5_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
		GPIOB->CRL.PinConfig5_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
		GPIOB->ODR.OutputData5_RW = 1;
		
		GPIOB->CRL.InOutMode6_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
		GPIOB->CRL.PinConfig6_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
		GPIOB->ODR.OutputData6_RW = 1;
		
		GPIOB->CRL.InOutMode7_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
		GPIOB->CRL.PinConfig7_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
		GPIOB->ODR.OutputData7_RW = 1;
		
		GPIOB->CRL.InOutMode3_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
		GPIOB->CRL.PinConfig3_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
		GPIOB->ODR.OutputData3_RW = 1;
		
		RCC->APB2ENR.IOPDClockEnable_RW = ENABLE;
		GPIOD->CRL.InOutMode2_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
		GPIOD->CRL.PinConfig2_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
		GPIOD->ODR.OutputData2_RW = 1;
		
		HID_LCD_RST = 1;
		TimeDelayOfSoftAtMs(300);
		HID_LCD_RST = 0; // 低电平复位
		TimeDelayOfSoftAtMs(300);
		HID_LCD_RST = 1;
		TimeDelayOfSoftAtMs(300);  // 延时以稳定
		
		HID_LCD_RD = 1; // 保持不读状态！(之前测试为0也是不读状态也是可以的！)
		TimeDelayOfSoftAtMs(5);
	}
	
	//--------------------------------End ST7735R Reset Sequence --------------------------------------// 
	//--------------------------------End ST7735S Reset Sequence --------------------------------------// 
	LCD_WriteCommand(0x11); //Sleep out 
			TimeDelayOfSoftAtMs(500);              //Delay 120ms 
	//------------------------------------ST7735S Frame Rate-----------------------------------------// 
	LCD_WriteCommand(0xB1); // FRAME RATE 控制
			TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x05);    // 下面三个值在GM[1,0]=11下手册建议，GM[1,0]=11，表示显示屏参数：128x160
			TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x3C); 
			TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x3C);
		TimeDelayOfSoftAtMs(5);	
	LCD_WriteCommand(0xB2); 
			TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x05);     // 下面三个值在GM[1,0]=11下手册建议
			TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x3C); 
			TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x3C); 
			TimeDelayOfSoftAtMs(5);
	LCD_WriteCommand(0xB3); 
			TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x05); 
			TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x3C); 
			TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x3C); 
			TimeDelayOfSoftAtMs(5);
	
	LCD_WriteData(0x05);    // 下面六值在GM[1,0]=11下手册建议
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x3C); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x3C); 
	TimeDelayOfSoftAtMs(5);
	//------------------------------------End ST7735S Frame Rate---------------------------------// 
	LCD_WriteCommand(0xB4); //Dot inversion 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x03); 
	TimeDelayOfSoftAtMs(5);
	//------------------------------------ST7735S Power Sequence---------------------------------// 
	LCD_WriteCommand(0xC0); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x28); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x08); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x04); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteCommand(0xC1); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0XC0); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteCommand(0xC2); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x0D); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x00); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteCommand(0xC3); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x8D); 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x2A); TimeDelayOfSoftAtMs(5);
	LCD_WriteCommand(0xC4); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x8D); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0xEE); TimeDelayOfSoftAtMs(5);
	//---------------------------------End ST7735S Power Sequence-------------------------------------// 
	LCD_WriteCommand(0xC5); //VCOM 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x1A);    //1a - C
	TimeDelayOfSoftAtMs(5);
	LCD_WriteCommand(0x36); //MX, MY, RGB mode 
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0xC0); 
	TimeDelayOfSoftAtMs(5);
	//------------------------------------ST7735S Gamma Sequence---------------------------------// 
	LCD_WriteCommand(0xE0); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x04); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x22); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x07); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x0A); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x2E); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x30); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x25); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x2A); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x28); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x26); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x2E); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x3A); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x00); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x01); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x03); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x13); TimeDelayOfSoftAtMs(5);
	LCD_WriteCommand(0xE1); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x04); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x16); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x06); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x0D); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x2D); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x26); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x23); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x27); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x27); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x25); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x2D); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x3B); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x00); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x01); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x04); TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x13); TimeDelayOfSoftAtMs(5);
	//------------------------------------End ST7735S Gamma Sequence-----------------------------// 
	LCD_WriteCommand(0x3A); //65k mode ,注意：对于一般的显示屏，其实这个65K(16位)色，就显得比较多余，所以应该使用256（8位），这样可以节约图片1/2的存储容量，可以增加两倍速度。如果是2位的黑白图，则速度再增加8倍
	TimeDelayOfSoftAtMs(5);
	LCD_WriteData(0x05);    // 表示一个像素用16位数据 -C
	TimeDelayOfSoftAtMs(5);
	// 之前这一个数据是：0x05，但是手册建议为0x55
	LCD_WriteCommand(0x29); //Display on,显示开启命令，是不是可以通过关和开显示，来达到看起来没有刷的方式！
	TimeDelayOfSoftAtMs(5);
	LCD_WriteCommand(0x29); //Display on,显示开启命令，是不是可以通过关和开显示，来达到看起来没有刷的方式！
	TimeDelayOfSoftAtMs(5);
}

/**
 *  @B 这是一个显示英文字符串的函数
 *
 */

void LCD_ShowChar(u8 startX, u8 startY,u8 size,u16 fontColor, vu16 asciiVector)
{
	{
		LCD_WriteCommand(0x2A);  // 设置列地址
		{
			LCD_WriteData(0x00);   // 列起始地址
			LCD_WriteData(startX); // 总共就几个地址，用u16真是浪费！！
		}
		{
			LCD_WriteData(0x00);   // 列结束地址，0x7F正好是宽度
			LCD_WriteData(startX+size/2 -1);  // x的宽度是SIZE的一半
		}

		LCD_WriteCommand(0x2B);  // 设置行地址
		{
			LCD_WriteData(0x00);
			LCD_WriteData(startY);
		}
		{
			LCD_WriteData(0x00);
			LCD_WriteData(startY+size-1);
		}
		LCD_WriteCommand(0x2C);  // 这个时候表示可以写参数了！然后后面就是跟着写参数。
	}
	
	asciiVector = asciiVector-' ';//得到偏移后的值
	if( asciiVector>94 )  // 防止意外字符
	{
		return;
	}
	
	switch(size)  // 不同大小ASCII编码值，不一样！暂时只有3种大小！
	{
		case 12:
			for (u16 typeface_row=0;typeface_row<12;typeface_row++)
			{
				for( u16 typeface_column=0;typeface_column<6;typeface_column++)
				{
					if( ((ascii_12[asciiVector][typeface_row] >> ( 7-typeface_column)) &0x01) == 0x01 )
					{
						LCD_WriteData(     fontColor>>8 );
						LCD_WriteData( (u8)fontColor    );
					}
					else
					{
						LCD_WriteData(     WHITE>>8 );
						LCD_WriteData( (u8)WHITE    );
					}
				}
			}
			break;
		case 16:
			for (u16 typeface_row=0;typeface_row<16;typeface_row++)
			{
				for( u16 typeface_column=0;typeface_column<8;typeface_column++)
				{
					if( ((ascii_16[asciiVector][typeface_row] >> ( 7-typeface_column)) &0x01) == 0x01 )
					{
						LCD_WriteData(     fontColor>>8 );
						LCD_WriteData( (u8)fontColor    );
					}
					else
					{
						LCD_WriteData(     WHITE>>8 );
						LCD_WriteData( (u8)WHITE    );
					}
				}
			}
			break;
		case 24:
			for (u16 typeface_row=0;typeface_row<24;typeface_row++)
			{
				for( u16 typeface_column=0;typeface_column<12;typeface_column++)
				{
					if( (((u16)(((ascii_24[asciiVector][typeface_row*2])<<8)+(ascii_24[asciiVector][typeface_row*2+1]) )
								>> ( 15-typeface_column)) &0x01) == 0x01 )
					{
						LCD_WriteData(     fontColor>>8 );
						LCD_WriteData( (u8)fontColor    );
					}
					else
					{
						LCD_WriteData(     WHITE>>8 );
						LCD_WriteData( (u8)WHITE    );
					}
				}
			}
			break;
		default:
			break;
	}
}


/**
 *  @B 这是一个显示汉字的函数，但是汉字字库实在是太大，所以平时更新程序的时候，需要注意一点为好。
 *
 */
void LCD_Show_hz(u8 startX,u8 startY,u8 size, u32 fontColor,const char *p)
{
	{  // 设置其方框
		LCD_WriteCommand(0x2A);  // 设置列地址
		{
			LCD_WriteData(0x00);   // 列起始地址
			LCD_WriteData(startX); // 总共就几个地址，用u16真是浪费！！
		}
		{
			LCD_WriteData(0x00);   // 列结束地址，0x7F正好是宽度
			LCD_WriteData(startX+size -1);  // x的宽度是SIZE的一半
		}

		LCD_WriteCommand(0x2B);  // 设置行地址
		{
			LCD_WriteData(0x00);
			LCD_WriteData(startY);
		}
		{
			LCD_WriteData(0x00);
			LCD_WriteData(startY+size-1);
		}
		LCD_WriteCommand(0x2C);  // 这个时候表示可以写参数了！然后后面就是跟着写参数。
	}
	
	{
		u16 offsetAddr = 0;
		offsetAddr  = (*p++ - 0xA1)*(16*6-2);   // 暂时不优化公式，公式已验证！
		offsetAddr += *p - 0xA1;
		
		if( offsetAddr>8177 ) // 防止意外字符
		{
			return;
		}
		
		switch(size)  // 汉字暂时只设定12x12的大小，其中最关键就是对应的公式
		{
			case 12:
				for (u16 typeface_row=0;typeface_row<12;typeface_row++)  // 共有12行
				{
					for( u16 typeface_column=0;typeface_column<12;typeface_column++)  // 要操作12次，汉字比英文大2倍
					{
						if( (((u16)(((GB2312_12[offsetAddr][typeface_row*2])<<8)+(GB2312_12[offsetAddr][typeface_row*2+1]) )
								>> ( 15-typeface_column)) &0x01) == 0x01 )
						{
							LCD_WriteData(     fontColor>>8 );
							LCD_WriteData( (u8)fontColor    );
						}
						else
						{
							LCD_WriteData(     WHITE>>8 );
							LCD_WriteData( (u8)WHITE    );
						}
					}
				}
				break;
			case 16:
				break;
			case 24:
				break;
			default:
				break;
		}
	}
}


void LCD_ShowString_Language(  u8 startX, u8 startY, u8 size, u32 fontColor, const char *pC,const char *pE)
{
	if( L_ENGLISH_VER != 0X55AA )
	{
		LCD_ShowString(  startX,  startY,  size,  fontColor, pC );
	}
	else
	{
		LCD_ShowString(  startX,  startY,  size,  fontColor, pE );
	}	
}

/**
 *  @B 这是一个针对性的显示LCD显示程序，用于显示汉字和英文字符
 *  @pra size :程序只有三种Size，注意汉字字库可能会比较大！汉字可能只会选择一种字库，因为可能会比较大！（可能会是16*16）
 *  // 这个函数用的太频繁！
 */
void LCD_ShowString( u8 startX, u8 startY, u8 size, u32 fontColor, const char *p )
{ 
	{ // LCD显示的部分也要同时串口输出。
		ComPrintString(UART4,p);
		ComPrintString(UART4,"\r\n");
	}
	
	if( startX == 255 )
	{
		return;
	}
	
	if( HARDWARE_VERSION == 1 )  // 表示带有LCD显示。
	{
		while(*p!='\0')	// 如果没有结束
		{       
			if( *p>0x80 )//如果是中文，暂时屏蔽，因为每次下载文件实在是太大了。
			{
				if( (*p=='\n') || ( startX> ( Width-size ) ) )  // 换段和换行
				{
					startY=startY+size;   //没有行间距
					startX=0;             //没有边距
				}
				LCD_Show_hz(startX, startY,size,fontColor, p);  // 这里直接把指针地址传入进去了
				startX+=size;
				p+=2;  // 汉字的指针要加2！
			}
			else	   //如果是英文，英文只占用一半
			{
				if( *p=='\n' )   // 换段和换行
				{
					startY=startY+size;   //没有行间距
					startX -= size/2;             //没有边距
				}
				else if ( startX> ( Width - (size/2)/2 ) )
				{
					startY=startY+size;   //没有行间距
					startX = 0; 
				}
				else if( ( *p >= 0x20 ) && ( *p <= 0x7e ) ) // 之所以这样写，是因为可能出现：其它的含有其它含义的ASCII，如果被LCD_ShowChar识别，则会出现硬件错误，不可挽回！
				{
					LCD_ShowChar(startX, startY,size,fontColor, *p);
					startX+=size/2;
				}
				else 
					startX+=size/2;
				p++;
			}
		} 
	}
	else
	{  // 这个时候用LED闪烁，用于指示程序正在运行！
		LED_MINI_1 = ~LED_MINI_1;
	}
}

/**
 *  @B 这是显示状态信息~
 *
 */
void LCDShowZMDZPinDescription( void )
{
	LCD_DispalyColor(WHITE);  // 清屏操作
	
	LCD_DispalyImage_Size((u8*) gImage_C_80x40_ROHS,2,0,80,40);
//	LCD_DispalyImage_Size((u8*) gImage_C_80x40_USB,24,42,80,40);
	LCD_DispalyImage_Size((u8*) gImage_C_XH254_60x34,2,42,60,34);
	LCD_DispalyImage_Size((u8*) gImage_C_XH254_60x34,66,42,60,34);
	LCD_DispalyImage_Size((u8*) gImage_C_40x40_FC,86,0,40,40);
	
	LCD_ShowString(8,              80,12,BLACK," \nG\nN\nD");
	LCD_ShowString(8+6+8,          80,12,RED,"S\nW\nD\nI\nO");
	LCD_ShowString(8+6+8+6+8,      80,12,RED,"S\nW\nC\nL\nK");
	LCD_ShowString(8+6+8+6+8+6+8,  80,12,BLACK,"V\nr\ne\nf\n|");

	LCD_ShowString(60,              80,12,BLACK," \n \n \n \n_");
	
	LCD_ShowString(64+8,              80,12,BLACK," \n5\nV\n \n_");
	LCD_ShowString(64+8+6+8,          80,12,BLACK,"V\nr\ne\nf\n|");
	LCD_ShowString(64+8+6+8+6+8,      80,12,BLACK,"3\n.\n3\nV");
	LCD_ShowString(64+8+6+8+6+8+6+8,  80,12,RED,"R\nE\nS\nE\nT");
	
}










