/**
 * @brief 用于处理所有的机械按键，使用需要为定时扫描按键程序。程序本身不涉及按键处理，只涉及状态。
 */



#ifndef __KEY_STATE_H
#define __KEY_STATE_H

#include "stm32f10x.h"

#define KEY_STATUS_UP    1   // 实际中表示按键松开的状态电平
#define KEY_STATUS_DOWN  0

typedef enum
{
	keyOriginalStatusV2 = 0,  // 解决按键一直被按下的问题
	
	keyUpV2,

	keyLongPressV2,
	
	keyFirstPressDoneAndWaitLongPress,  // 这个是专门为按下后立刻起作用添加的。之前的库中没有这个！
	
	keyFirstPressButNotCofirmedAndStayDown,
	keyFirstPressButNotCofirmedAndStayUp,
	keyFirstPress,
	
	keyDoublePressButNotCofirmedAndStayDown,
	keyDoublePressButNotCofirmedAndStayUp,
	keyDoublePressed,
	
	keyThirdPressButNotCofirmedAndStayDown,
	keyThirdPressButNotCofirmedAndStayUp,
	keyThirdPress,
	
	keyForthPressButNotCofirmedAndStayDown,
	keyForthPressButNotCofirmedAndStayUp,
	keyForthPressed,
	
	keyFivthPressButNotCofirmedAndStayDown,
	keyFivthPressButNotCofirmedAndStayUp,
	keyFivthPressed,
	
	keySixthPressButNotCofirmedAndStayDown,
	keySixthPressButNotCofirmedAndStayUp,
	keySixthPressed,
	
	keySeventhPressButNotCofirmedAndStayDown,
	keySeventhPressButNotCofirmedAndStayUp,
	keySeventhPressed,
	
	keyEighthPressButNotCofirmedAndStayDown,
	keyEighthPressButNotCofirmedAndStayUp,
	keyEighthPressed,
	
}keyStatusV2;
	
typedef enum
{
	effectAtPress = 0,
	effectAtLoosen = 1,
}_effectAtPressOrLoosen;
typedef enum
{
	effectInstant = 0,
	effectWaitOpreationConfirm = 1,
}_effectInstantOrWaitOpreationConfirm;
typedef enum
{
	maxDoubleHitIsDisableDoubleHit = 0,  // 屏蔽多击功能
	maxDoubleHitIs2 = 2,
	maxDoubleHitIs3 = 3,
	maxDoubleHitIs4 = 4,
	maxDoubleHitIs5 = 5,
	maxDoubleHitIs6 = 6,
	maxDoubleHitIs7 = 7,
	maxDoubleHitIs8 = 8,
}_maxDoubleHitNumbers;

typedef struct{ 
	keyStatusV2 Status;
	_effectAtPressOrLoosen effectAtPressOrLoosen;
	_effectInstantOrWaitOpreationConfirm effectInstantOrWaitOpreationConfirm;
	_maxDoubleHitNumbers maxDoubleHitNumbers; // 用于最大限度的增加性能。主要是在：确认按键操作下使用。使能了最大数目，则可以在最大按下时候，立刻反应。
																						// 在最大为八击操作下，显然有效的范围的是：2-8。要注意值的判断。
	u16 longPressCount; // 从按键按下到认为长按的计数！
	u16 doubleHitCount; // 用于确定连击的时间间隔！
	u16 longPressThresholdValue; // 用于定义长按的判断时间。为0时禁止长按功能。
	u16 doubleHitThresholdValue; // 用于定义双击的判断时间。为0时禁止多击功能。
}keyPropertyV2;


// 警告：effectInstantOrWaitOpreationConfirm，必须使用：effectWaitOpreationConfirm。因为effectInstant没有集成在里面。使用的时候务必要注意！
void keyInitV2( keyPropertyV2*       									key,
								_effectAtPressOrLoosen 								effectAtPressOrLoosen,
								_effectInstantOrWaitOpreationConfirm  effectInstantOrWaitOpreationConfirm,  // 立刻使能，或者确认操作使能：想必立刻使能大部分应用在简单的按键操作上。
								_maxDoubleHitNumbers 									maxDoubleHitNumbers,      // 这里也可以屏蔽多击功能
								u16 																	doubleHitThresholdValue,  // 当值为0表示屏蔽多击功能。
							  u16 																	longPressThresholdValue ); // 当值为0时表示屏蔽长按功能

void keyDetectV2( keyPropertyV2* key,_Bool keyCurrentValue );

#endif


