/**
 * @brief 用于处理所有的机械按键，使用需要为定时扫描按键程序。程序本身不涉及按键处理，只涉及状态。
 */



#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

typedef enum
{
	keyOriginalStatus = 0,  // 解决按键一直被按下的问题
	keyUp = 1,
	keyPress = 2,
	keyPressButNotReachLongPress = 3,
	keyLongPress = 4,
	keyDoublePress = 5,
	doublePressKeyNotUp = 6,
}keyStatus;
	
typedef struct{ 
	keyStatus Status;
	u16 longPressCount;  // 从按键按下到认为长按的计数！
	u16 doublePressCount; // 用于确定双击的时间间隔！
	u16 longPressThresholdValue; // 用于定义长按的判断时间。为0时禁止长按功能。
	u16 doublePressThresholdValue; // 用于定义双击的判断时间。为0时禁止双击功能。
}keyProperty;

void keyInit( keyProperty* key,u16 doubluePressThresholdValue, u16 longPressThresholdValue );
void keyDetect( keyProperty* key,_Bool keyCurrentValue );

#endif


