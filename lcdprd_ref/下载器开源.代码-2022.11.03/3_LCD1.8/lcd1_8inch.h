/**
 * LCD16028（1.8寸）是离线下载器的标准显示屏，可以高大上的显示和操作。
 * 
 */

#ifndef __LCD1_8INCH_H
#define __LCD1_8INCH_H			    

#include "stm32f10x.h"
#include "stm32_timer_delay.h" // 用于软件模拟后的延时操作
#include "stm32_spi.h"
#include "includes.h" // 为了嵌入μC/OS

#define Height 160     // 0xA0 显示屏高度
#define Width  128     // 0x80 显示屏宽度
#define RED    0xf800  // RGB的值
#define GREEN  0x07e0
#define BLUE   0x001f
#define YELLOW 0xffe0
#define WHITE  0xffff // 全1为白
#define BLACK  0x0000 // 全0为黑
#define PURPLE 0xf81f



#define LCD_RST            GPIOA->ODR.OutputData10_RW   // 低电平复位
#define LCD_DataOrCommand  GPIOA->ODR.OutputData9_RW   // 0:命令 1：数据
#define LCD_MOSI           GPIOB->ODR.OutputData15_RW  // 给的例程中，只有数据写入显示屏
#define LCD_SCK            GPIOB->ODR.OutputData13_RW  // 时钟引脚
#define LCD_CS             GPIOB->ODR.OutputData12_RW  // 片选引脚

//#define LCD_DELAY          TimeDelayOfSoftAtMs( 1 )    // 用于模拟下的时间延时

#define LCD_DELAY          for( vu16 i = 0;i<0x0;i++ )    // 用于模拟下的时间延时





#define HID_LCD_RD  GPIOB->ODR.OutputData7_RW
#define HID_LCD_WR  GPIOB->ODR.OutputData6_RW
#define HID_LCD_DC  GPIOB->ODR.OutputData5_RW
#define HID_LCD_RST GPIOB->ODR.OutputData4_RW
#define HID_LCD_CS  GPIOD->ODR.OutputData2_RW




extern OS_MUTEX LCD_ShowsrtingUseRight_Mutex;  // 串口2发送使用权


void LCD_DispalyWindows( void );


void LCD_DispalyColor( u16 RGBValue );
void LCD_DisplayRGB( void );
void LCD_DispalyImage( u8 *pic );
void LCD_DispalyImage_Size( u8 *pic ,u16 startX,u16 startY,u16 wideSize,u16 heightSize );
void LCDInitOfST7735CtlIC( void );
void LCD_ShowString( u8 startX, u8 startY, u8 size, u32 fontColor, const char *p );

void test_ShowHZ( void );
void LCDShowZMDZPinDescription( void );

void LCD_ShowString_Language(  u8 startX, u8 startY, u8 size, u32 fontColor, const char *pC,const char *pE);

#endif
















