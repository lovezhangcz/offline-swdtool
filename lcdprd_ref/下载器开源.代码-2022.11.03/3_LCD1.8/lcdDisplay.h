/**
 * LCD16028（1.8寸）是离线下载器的标准显示屏，可以高大上的显示和操作。
 * 
 */

#ifndef __LCDDISPALY_H
#define __LCDDISPALY_H			    

#include "stm32f10x.h"
#include "stm32_spi.h"
#include "stdio.h"
#include "string.h"
#include "Encrypt_Code.H"
#include "memoryManage.h" 
#include "w25qxx.h" 
#include "lcd1_8inch.h" 
#include "lcdfont.h" 



typedef struct
{
	u32 PageNumberIndex; 			 	// 用于左右移动，8：引脚描述，9：状态信息，10：文件正文(默认！)10-21：文件正文！(0-8暂时没有分配，用于日后的信息！备用！)
	
	u32 FileOperationIndex;     // 用于表示操作文件，显然在不同的状态下其按键性质不一样。
															// 暂时只用于：0，下载程序。1：智能下载程序！
	u32 menuDepth;  						// 用于区分菜单深度，目前设计的只有二级菜单，日后其实还是要上emWin的，因为设计这个确实比较麻烦！
															// 0：一级。1：二级。
	u32 FileNumberIndexInPage;  // 用于上下移动文件，通过和上面有配合，判断文件！（每一页，最多存储10个文件。）
															// 0：表示当前无信息。1-10，表示文件编号！
	u32 lastDisplayWindows;    // 为了解决切换闪烁问题，引入：lastDisplay：值为：1（引脚显示界面），2（信息），4（下载信息界面）10-21（文件正文界面，和PageNumberIndex同值！）上电初始化值为：0.表示一定要切换和配置过文件后。
}_MenuManager;

extern _MenuManager MenuManager;
extern u8 randomCode[8];

//extern u8 PageNumberIndex;  // 用于左右移动，9：引脚描述，10：状态信息，11：文件正文(默认！)12-22-：文件正文！(0-8暂时没有分配，用于日后的信息！备用！)
//extern u8 FileNumberIndexInPage;   // 用于上下移动文件，通过和上面有配合，判断文件！（每一页，最多存储10个文件。）
//																// 0：表示当前无信息。1-10，表示文件编号！
//extern u8 FileOperation;
void displayStatus( void );
void displayFileList( u16 pageIndex );
void displayFileIndex( u8 moveStep );
void freashDisplayInformation( _MenuManager managerInput );
void freashDisplayProgramTimes( _FileInformation userFile );

#endif


















