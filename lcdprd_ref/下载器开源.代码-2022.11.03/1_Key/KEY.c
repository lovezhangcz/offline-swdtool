/**
 * @brief 按键处理函数，日后务必要方便移植。主要针对的是：定时检测按键程序。
 * @Way 本程序主要针对去抖动。对于硬件防抖，只需要处理边沿就可以了。
 * @Attention 按键是属性，所以一定要减少关联硬件！！
 */
 
#include "KEY.h"
#include "stdio.h"
#include "string.h"


/**
 * @brief 按键属性初始化，建议20ms的扫描时间。双击时间建议设置为：500ms，长按为1000ms
 */

void keyInit( keyProperty* key,u16 doubluePressThresholdValue, u16 longPressThresholdValue )
{
	memset(key,0,sizeof(keyProperty));
	key->doublePressThresholdValue = doubluePressThresholdValue;
	key->longPressThresholdValue = longPressThresholdValue;
}
/**
 * @brief 下面程序应用于循环扫描10ms-20ms和任务或者定时器中
 * 输入按键引脚的值，然后处理按键的状态。尽量少关连实际的硬件。
 */
void keyDetect( keyProperty* key,_Bool keyCurrentValue )
{
	if( keyCurrentValue == 1 ) // 表示按键当前松开，实际中需要根据情况来定。
	{
		key->Status = keyUp;
		if( key->doublePressCount != 0)  // 双击用的是递减！
		{
			key->doublePressCount--;
		}
	}
	else if( keyCurrentValue == 0 ) // 表示按键当前按下
	{
		switch( key->Status )
			{
				case keyOriginalStatus: // 这种状态也可以作为上电初始化的一种选择！
					key->Status = keyOriginalStatus;  // 仍然不变
					break;
				case keyUp: // 只有两种可能：要么进入keyDown状态，要么进入doublePress，显然是根据时间来判断的。
					if( key->doublePressCount == 0 )  //时间已超时
					{
						key->Status = keyPress;
						key->doublePressCount = key->doublePressThresholdValue;  // 这个值会在日后递减，用于计时。参考Winddows的时间间隔默认为500ms
					}
					else
					{
						key->Status = keyDoublePress;
						key->doublePressCount = 0;  // 下次Press事件，不会从doublePress再次回到Double
					}
					break;
				case keyPress:
					key->Status = keyPressButNotReachLongPress;
					key->longPressCount = 0;
					if( key->doublePressCount != 0 )
					{
						key->doublePressCount--;
					}
					break;
				case keyPressButNotReachLongPress:
					if( key->doublePressCount != 0 )
					{
						key->doublePressCount--;
					}
					if( key->longPressThresholdValue == 0) // 此时在这个状态循环，不进入长按状态。
					{
						break;
					}
					else
					{
						key->longPressCount++;
						if( key->longPressCount >= key->longPressThresholdValue )
						{
							key->Status = keyLongPress;
							key->longPressCount = 0;
						}
						break;
					}
				case keyLongPress:
					key->doublePressCount = 0;  // 进入了长按，就不允许进入双击！
					key->Status = keyLongPress;  // 不变
					break;
				case keyDoublePress:
					key->Status = doublePressKeyNotUp;
					key->doublePressCount = 0; // 防止进入两次双击
					break;
				case doublePressKeyNotUp:
					key->Status = doublePressKeyNotUp; // 不变
				default: 
					break;
			}
	}
	else // 不应该出现这样的值
	{
		printf("Error:Key value unexpected! Line:%d,Fun:%s\n",__LINE__,__func__);
	}
	
	
}



