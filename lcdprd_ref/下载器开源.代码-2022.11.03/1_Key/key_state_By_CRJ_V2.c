/**
 * @brief 按键处理函数，日后务必要方便移植。主要针对的是：定时检测按键程序。
 * @Way 本程序主要针对去抖动。对于硬件防抖，只需要处理边沿就可以了。
 * @Attention 按键是属性，所以一定要减少关联硬件！！
							2018.06.21  - by CRJ 
 */
 
#include "stm32f10x.h"
#include "stdio.h"
#include "string.h"
#include "key_state_By_CRJ_V2.h"


/**
 * @brief 按键处理函数2.0：主要为：防止多击元事件长按事件受到之前状态的影响。
 * @Way 本程序主要针对去抖动。对于硬件防抖，只需要处理边沿就可以了。
 * @Attention 按键是属性，所以一定要减少关联硬件！！
							参数说明：effectAtPressOrLoosen：	表示按键按下起作用，还是松开起作用。
																								不同的操作习惯。
																								对于长按：两者都要可以一直起作用。也可以通过判断只进入单次识别作用。
												effectInstantOrWaitOpreationConfirm:表示立刻起作用，还是等待操作确认再起作用
																								比如不仅仅存在按下操作，对于仅仅按下一次按键，需要等待确认没有下一个操作。这样的一个缺点是：状态机之前的一个状态，响应存在时差。
																								优点是：可以多种不冲突的操作。（作为对比：Windows的双击操作之前存在：单击事件。）
												doubluePressThresholdValue：表示连击的时间间隔计数
																								作为多击事件中两次的时间间隔
												longPressThresholdValue：表示长按的时间间隔计数，显然：长按的时间：必须要比双击的间隔大。
												maxDoubleHitNumbers：最大连击数，表示能使能的最大连击数据。
												警告：必须保证，如果存在长按和多击事件，长按的时间大于双击的时间。
 */
void keyInitV2( keyPropertyV2*       									key,
								_effectAtPressOrLoosen 								effectAtPressOrLoosen,
								_effectInstantOrWaitOpreationConfirm  effectInstantOrWaitOpreationConfirm,  // 立刻使能，或者确认操作使能：想必立刻使能大部分应用在简单的按键操作上。
								_maxDoubleHitNumbers 									maxDoubleHitNumbers,      // 这里也可以屏蔽多击功能
								u16 																	doubleHitThresholdValue,  // 当值为0表示屏蔽多击功能。
							  u16 																	longPressThresholdValue ) // 当值为0时表示屏蔽长按功能
{
	memset( key,0,sizeof(keyPropertyV2)) ;
	key->Status = keyOriginalStatusV2; 
	key->effectAtPressOrLoosen = effectAtPressOrLoosen;
	key->effectInstantOrWaitOpreationConfirm = effectInstantOrWaitOpreationConfirm;
	{  // 下面的语句处理为：连击没有时间 等价 最大连击禁止  连击有时间 等价 存在最大连击数。
			//									主要为了防止出现：连击有时间，但是最大连击没有禁止，造成判断的难度。
			//									经过这样处理后：判断没有双击就可以用其中任何一个进行判断了。
		if( ( maxDoubleHitNumbers == maxDoubleHitIsDisableDoubleHit ) || ( doubleHitThresholdValue == 0 ) )
		{
			key->maxDoubleHitNumbers = maxDoubleHitIsDisableDoubleHit;
			key->doubleHitThresholdValue = 0;
		}
		else
		{
			key->maxDoubleHitNumbers = maxDoubleHitNumbers;
			key->doubleHitThresholdValue = doubleHitThresholdValue;
		}
	}
	key->longPressThresholdValue = longPressThresholdValue;  // 
}



/**
 * @brief 以下函数采用了最简单的操作方法。因为思路简单：所以不容易错。容易让人明白。
					就是4个分支：按下还是松开起作用。立刻还是确认起作用。
					警告：关于立刻起作用：本程序暂时停止开发，仅仅保留：原因是：关于存在多击事件的操作过程中，前面的多击事件，会产生影响！
								而且，对于确认起作用的，在最后的多击（也可以是单击）和长按也会立刻响应！
 */

void keyDetectV2( keyPropertyV2* key,_Bool keyCurrentValue )
{
	if( key->effectAtPressOrLoosen == effectAtPress )
	{
		if( key->effectInstantOrWaitOpreationConfirm == effectInstant )
		{// 按下起作用，立刻起作用。
		 // 暂时保留立刻起作用的设定
		 // 这里暂时只针对单击事件和长按事件起作用 -- 因为离线下载器暂时需要用到这个功能。
			if( keyCurrentValue == KEY_STATUS_UP )
			{
				switch( key->Status )
					{
						default:
							key->Status = keyUpV2;
							break;
					}
			}
			else
			{
				switch( key->Status )
					{
						case keyOriginalStatusV2:
							key->Status = keyOriginalStatusV2;
							break;
						case keyUpV2:
							key->Status = keyFirstPress;
							if( key->longPressThresholdValue != 0 )
							{
								key->longPressCount = key->longPressThresholdValue;
							}
							break;
						case keyFirstPress:
							if( key->longPressThresholdValue != 0  )
							{
								key->Status = keyFirstPressDoneAndWaitLongPress;
								key->longPressCount--;
							}
							else
							{
								key->Status = keyOriginalStatusV2;
							}
						case keyFirstPressDoneAndWaitLongPress:
							if( key->longPressCount != 0 )
							{
								key->longPressCount--;
							}
							else
							{
								key->Status = keyLongPressV2;
							}
							break;
						case keyLongPressV2:
							key->Status = keyLongPressV2;
							break;
						default:
							break;
					}
			}
			
		}
		else
		{ // 按下起作用，等待确认起作用。
			if( keyCurrentValue == KEY_STATUS_UP )
			{ // 按键值，当前松开。
					switch( key->Status )
					{
						case keyOriginalStatusV2:
							key->Status = keyUpV2;
							break;
						case keyUpV2:
							key->Status = keyUpV2;
							break;
						case keyLongPressV2:
							key->Status = keyUpV2;
							break;
						case keyFirstPressButNotCofirmedAndStayDown:  // 这个状态，经过这个事件，显然不会生成长按事件。
							if( key->longPressThresholdValue == 0 )  // 不存在长按事件，则必然存在多击事件。
							{
								if( key->doubleHitCount != 0 )
								{
									key->doubleHitCount --;
									key->Status = keyFirstPressButNotCofirmedAndStayUp;  // 这个时候，还不能确定单击还是多击事件的判断。
								}
								else
								{
									key->Status = keyFirstPress;  // 单击事件。
								}
							}
							else  // 存在长按事件
							{
								if( key->doubleHitThresholdValue == 0 )  // 不存在多击事件
								{
									key->Status = keyFirstPress;
								}
								else  // 存在多击事件
								{
									if( key->doubleHitCount != 0 )
									{
										key->doubleHitCount --;
										key->Status = keyFirstPressButNotCofirmedAndStayUp;  // 这个时候，还不能确定单击还是多击事件的判断。
									}
									else
									{
										key->Status = keyFirstPress;  // 单击事件。
									}
								}
							}
							break;
						case keyFirstPressButNotCofirmedAndStayUp:  // 这种时候，还没有确定，显然就是存在多击事件，至少是双击事件。
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount --;
							}
							else
							{
								key->Status = keyFirstPress;  // 单击事件。
							}
							break;
						case keyFirstPress:
							key->Status = keyUpV2;  // 恢复按键状态
							break;

						
// 双击事件后的模板。因为此时去掉长按之后，无其它类型。
						case keyDoublePressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
								key->Status = keyDoublePressButNotCofirmedAndStayUp;
							}
							else
							{
								key->Status = keyDoublePressed;  // 可以确定为双击事件。
							}
							break;
						case keyDoublePressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keyDoublePressed;
							}
							break;
						case keyDoublePressed:
							key->Status = keyUpV2;
							break;

						
						
// 三击事件之后，几乎是类似的了。所以类型是一样。				
						case keyThirdPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
								key->Status = keyThirdPressButNotCofirmedAndStayUp;
							}
							else
							{
								key->Status = keyThirdPress;  // 可以确定为双击事件。
							}
							break;
						case keyThirdPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keyThirdPress;
							}
							break;
						case keyThirdPress:
							key->Status = keyUpV2;
							break;
	
						
					
						case keyForthPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
								key->Status = keyForthPressButNotCofirmedAndStayUp;
							}
							else
							{
								key->Status = keyForthPressed;  // 可以确定为双击事件。
							}
							break;
						case keyForthPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keyForthPressed;
							}
							break;
						case keyForthPressed:
							key->Status = keyUpV2;
							break;
	
						
						
						case keyFivthPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
								key->Status = keyFivthPressButNotCofirmedAndStayUp;
							}
							else
							{
								key->Status = keyFivthPressed;  // 可以确定为双击事件。
							}
							break;
						case keyFivthPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keyFivthPressed;
							}
							break;
						case keyFivthPressed:
							key->Status = keyUpV2;
							break;
	
						
						
						case keySixthPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
								key->Status = keySixthPressButNotCofirmedAndStayUp;
							}
							else
							{
								key->Status = keySixthPressed;  // 可以确定为双击事件。
							}
							break;
						case keySixthPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keySixthPressed;
							}
							break;
						case keySixthPressed:
							key->Status = keyUpV2;
							break;
						
						
	
						case keySeventhPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
								key->Status = keySeventhPressButNotCofirmedAndStayUp;
							}
							else
							{
								key->Status = keySeventhPressed;  // 可以确定为双击事件。
							}
							break;
						case keySeventhPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keySeventhPressed;
							}
							break;
						case keySeventhPressed:
							key->Status = keyUpV2;
							break;
	
						
						case keyEighthPressButNotCofirmedAndStayDown:  // 这种情况下不应该出现的状态
							break;
						case keyEighthPressButNotCofirmedAndStayUp:  // 这种情况下不应该出现的状态
							break;
						case keyEighthPressed:
							key->Status = keyUpV2;
							break;
						default:
							break;
// 三击事件之后，几乎是类似的了。所以类型是一样。
					}
			}
			else
			{ // 按键值，当前按下。
				switch( key->Status )
					{
						case keyOriginalStatusV2:
							key->Status = keyOriginalStatusV2;
							break;
						case keyUpV2:
							if( key->doubleHitThresholdValue == 0 )  // 不存在多击事件。
							{
								if( key->longPressThresholdValue == 0 ) // 不存在长按事件
								{
									key->Status = keyFirstPress;  // 确定为：单击事件。
								}
								else // 存在长按事件
								{
									key->longPressCount = key->longPressThresholdValue; // 进行开始累计计数。
									key->Status = keyFirstPressButNotCofirmedAndStayDown;
								}
							}
							else // 存在连击事件，至少为双击事件。
							{
								if( key->longPressThresholdValue == 0 )  // 不存在长按事件
								{
									key->doubleHitCount = key->doubleHitThresholdValue;
									key->Status = keyFirstPressButNotCofirmedAndStayDown;
								}
								else  // 存在长按事件
								{
									key->doubleHitCount = key->doubleHitThresholdValue;
									key->longPressCount = key->longPressThresholdValue; // 进行开始累计计数。
									key->Status = keyFirstPressButNotCofirmedAndStayDown;
								}
							}
							break;
						
						case keyFirstPressButNotCofirmedAndStayDown:  // 能进入此，至少存在长按和连击中的其中一种。
							if( key->longPressThresholdValue == 0 )  // 不存在长按事件，则必存在多击事件。
							{
									if( key->doubleHitCount != 0 )
									{
										key->doubleHitCount --;
									}
									else
									{
										key->Status = keyFirstPress;  // 单击事件。
									}
							}
							else  // 存在长按事件
							{
								if( key->doubleHitThresholdValue == 0 )  // 不存在多击事件
								{
									if( key->longPressCount != 0 )
									{
										key->longPressCount --;
									}
									else
									{
										key->Status = keyLongPressV2;  // 变成长按状态。
									}
								}
								else  // 存在多击事件，也就是两个事件都存在，但是注意：长按的时间，是大于连击事件的。
								{
									if( key->doubleHitCount != 0 )
									{
										key->doubleHitCount --;  // 双击的计数仍然需要判断。
									}
									if( key->longPressCount != 0 )
									{
										key->longPressCount --;
									}
									else
									{
										key->Status = keyLongPressV2;  // 变成长按状态。
									}
								}
								
							}
							break;
						case keyFirstPressButNotCofirmedAndStayUp:  // 显然存在多击事件，才会有这个对话框。
							if( key->doubleHitCount != 0 )
							{
								if( key->maxDoubleHitNumbers == maxDoubleHitIs2 ) // 最大双击数为：2
								{
									key->Status = keyDoublePressed;
								}
								else
								{
									key->doubleHitCount = key->doubleHitThresholdValue;  // 重新计数
									key->Status = keyDoublePressButNotCofirmedAndStayDown;
								}
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keyDoublePressed;
							}
							break;
						case keyFirstPress:  // 按键已经确定为按键按下
							key->Status = keyOriginalStatusV2;  // 恢复到原始状态
							break;
						case keyLongPressV2:  // 考虑到长按是需要某种操作
							key->Status = keyLongPressV2;  // 状态一直保存，如果应用程序中不想一直保存，则在应用程序中更改状态。
							break;

						
// 双击事件后的模板。因为此时去掉长按之后，无其它类型。
						case keyDoublePressButNotCofirmedAndStayDown:  // 没有确定状态，则存在三击或者以上状态。
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keyDoublePressed;  // 确认为双击事件
							}
							break;
						case keyDoublePressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								if( key->maxDoubleHitNumbers == maxDoubleHitIs3 ) // 最大双击数为：3
								{
									key->Status = keyThirdPress;
								}
								else
								{
									key->doubleHitCount = key->doubleHitThresholdValue;  // 重新计数
									key->Status = keyThirdPressButNotCofirmedAndStayDown;
								}
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keyThirdPress;
							}
							break;
						case keyDoublePressed:
							key->Status = keyOriginalStatusV2;
							break;


						
// 三击事件之后，几乎是类似的（和双击事件开始是一样的。）了。所以类型是一样。
						case keyThirdPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keyThirdPress;  // 确认为双击事件
							}
							break;
						case keyThirdPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								if( key->maxDoubleHitNumbers == maxDoubleHitIs4 ) // 最大双击数为：4
								{
									key->Status = keyForthPressed;
								}
								else
								{
									key->doubleHitCount = key->doubleHitThresholdValue;  // 重新计数
									key->Status = keyForthPressButNotCofirmedAndStayDown;
								}
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keyForthPressed;
							}
							break;
						case keyThirdPress:
							key->Status = keyOriginalStatusV2;
							break;
						
	
						case keyForthPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keyForthPressed;  // 确认为双击事件
							}
							break;
						case keyForthPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								if( key->maxDoubleHitNumbers == maxDoubleHitIs5 ) // 最大双击数为：3
								{
									key->Status = keyFivthPressed;
								}
								else
								{
									key->doubleHitCount = key->doubleHitThresholdValue;  // 重新计数
									key->Status = keyFivthPressButNotCofirmedAndStayDown;
								}
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keyFivthPressed;
							}
							break;
						case keyForthPressed:
							key->Status = keyOriginalStatusV2;
							break;
	
						
						
						case keyFivthPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keyFivthPressed;  // 确认为双击事件
							}
							break;
						case keyFivthPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								if( key->maxDoubleHitNumbers == maxDoubleHitIs6 ) // 最大双击数为：3
								{
									key->Status = keySixthPressed;
								}
								else
								{
									key->doubleHitCount = key->doubleHitThresholdValue;  // 重新计数
									key->Status = keySixthPressButNotCofirmedAndStayDown;
								}
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keySixthPressed;
							}
							break;
						case keyFivthPressed:
							key->Status = keyOriginalStatusV2;
							break;
	
						
						
						case keySixthPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keySixthPressed;  // 确认为双击事件
							}
							break;
						case keySixthPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )
							{
								if( key->maxDoubleHitNumbers == maxDoubleHitIs7 ) 
								{
									key->Status = keySeventhPressed;
								}
								else
								{
									key->doubleHitCount = key->doubleHitThresholdValue;  // 重新计数
									key->Status = keySeventhPressButNotCofirmedAndStayDown;
								}
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keySeventhPressed;
							}
							break;
						case keySixthPressed:
							key->Status = keyOriginalStatusV2;
							break;
	
						
						
						case keySeventhPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							else  // 正好在这个时候计数到0。
							{
								key->Status = keySeventhPressed;
							}
							break;
						case keySeventhPressButNotCofirmedAndStayUp:  
								key->Status = keyEighthPressed;  // 显然，达到最大的八击事件是最后一个状态。
							break;
						case keySeventhPressed:
							key->Status = keyOriginalStatusV2;
							break;
						
						
						case keyEighthPressButNotCofirmedAndStayDown:  // 这个状态，不应该存在。
							break;
						case keyEighthPressButNotCofirmedAndStayUp:  // 这个也不应该存在
							break;
						case keyEighthPressed:
							key->Status = keyOriginalStatusV2;
							break;
						default:
							break;
// 三击事件之后，类型是一样的。程序模块都是一样的了。
					}
			}
		}
	}
	else  // 按键松开有效
				// 关于按键松开有效，针对是：单击和多击事件，对于长按，仍然是：按下有效果。当然，也可以禁止长按的配置！
				// 对于一种反悔机制：就是按键松开有效，但是长按之后就“撤消了”，其实是：长按抵消了单击事件。（当前，针对于按键按下道理也是一样的）
	{
		if( key->effectInstantOrWaitOpreationConfirm == effectInstant )
		{// 松开起作用，立刻起作用。
		 // 暂时保留立刻起作用的设定。
		}
		else
		{// 松开起作用，等待确认起作用。
			if( keyCurrentValue == KEY_STATUS_UP )
			{ // 按键值，当前松开。
					switch( key->Status )
					{
						case keyOriginalStatusV2:
							key->Status = keyUpV2;
							break;
						case keyUpV2:
							key->Status = keyUpV2;
							break;
						case keyLongPressV2:
							key->Status = keyUpV2;
							break;
						case keyFirstPressButNotCofirmedAndStayDown:  // 这个时候，还没有确认：长按事件，单击和多击事件都有可能。
																													// 长按事件下在这片没有确定的前提下：不能进入长按。所以只判断单击和多击事件。
							if( key->doubleHitThresholdValue != 0 ) // 表示存在多击事件
							{
								if( key->doubleHitCount != 0 )  // 
								{
									key->doubleHitCount--;
									key->Status = keyFirstPressButNotCofirmedAndStayUp;
								}
								else
								{
									key->Status = keyFirstPress;  // 单击事件
								}
							}
							else  // 不存在多击事件，只存在单击事件，显然对应：按键松开有效果的，这个时候为单击事件。
							{
								key->Status = keyFirstPress;  // 单击事件
							}
							break;
						case keyFirstPressButNotCofirmedAndStayUp:  // 这种时候，还没有确定，显然就是存在多击事件，至少是双击事件。
							if( key->doubleHitCount != 0 )  // 
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keyFirstPress;  // 单击事件
							}
							break;
						case keyFirstPress:
							key->Status = keyUpV2;
							break;

						
// 双击事件后的模板。因为此时去掉长按之后，无其它类型。
						case keyDoublePressButNotCofirmedAndStayDown:
							if( key->maxDoubleHitNumbers == maxDoubleHitIs2 )
							{
								key->Status = keyDoublePressed;
							}
							else
							{
								if( key->doubleHitCount != 0 )  // 
								{
									key->doubleHitCount--;
									key->Status = keyDoublePressButNotCofirmedAndStayUp;
								}
								else
								{
									key->Status = keyDoublePressed;
								}
							}
							break;
						case keyDoublePressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )  // 
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keyDoublePressed; 
							}
							break;
						case keyDoublePressed:
							key->Status = keyUpV2;
							break;

						
						
// 三击事件之后，几乎是类似的了。所以类型是一样。				
						case keyThirdPressButNotCofirmedAndStayDown:
							if( key->maxDoubleHitNumbers == maxDoubleHitIs3 )
							{
								key->Status = keyThirdPress;
							}
							else
							{
								if( key->doubleHitCount != 0 )  // 
								{
									key->doubleHitCount--;
									key->Status = keyThirdPressButNotCofirmedAndStayUp;
								}
								else
								{
									key->Status = keyThirdPress;
								}
							}
							break;
						case keyThirdPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )  // 
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keyThirdPress; 
							}
							break;
						case keyThirdPress:
							key->Status = keyUpV2;
							break;
	
						
					
						case keyForthPressButNotCofirmedAndStayDown:
							if( key->maxDoubleHitNumbers == maxDoubleHitIs4 )
							{
								key->Status = keyForthPressed;
							}
							else
							{
								if( key->doubleHitCount != 0 )  // 
								{
									key->doubleHitCount--;
									key->Status = keyForthPressButNotCofirmedAndStayUp;
								}
								else
								{
									key->Status = keyForthPressed;
								}
							}
							break;
						case keyForthPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )  // 
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keyForthPressed; 
							}
							break;
						case keyForthPressed:
							key->Status = keyUpV2;
							break;
	
						
						
						case keyFivthPressButNotCofirmedAndStayDown:
							if( key->maxDoubleHitNumbers == maxDoubleHitIs5 )
							{
								key->Status = keyFivthPressed;
							}
							else
							{
								if( key->doubleHitCount != 0 )  // 
								{
									key->doubleHitCount--;
									key->Status = keyFivthPressButNotCofirmedAndStayUp;
								}
								else
								{
									key->Status = keyFivthPressed;
								}
							}
							break;
						case keyFivthPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )  // 
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keyFivthPressed; 
							}
							break;
						case keyFivthPressed:
							key->Status = keyUpV2;
							break;
	
						
						
						case keySixthPressButNotCofirmedAndStayDown:
							if( key->maxDoubleHitNumbers == maxDoubleHitIs6 )
							{
								key->Status = keySixthPressed;
							}
							else
							{
								if( key->doubleHitCount != 0 )  // 
								{
									key->doubleHitCount--;
									key->Status = keySixthPressButNotCofirmedAndStayUp;
								}
								else
								{
									key->Status = keySixthPressed;
								}
							}
							break;
						case keySixthPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )  // 
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keySixthPressed; 
							}
							break;
						case keySixthPressed:
							key->Status = keyUpV2;
							break;
						
						
	
						case keySeventhPressButNotCofirmedAndStayDown:
							if( key->maxDoubleHitNumbers == maxDoubleHitIs7 )
							{
								key->Status = keySeventhPressed;
							}
							else
							{
								if( key->doubleHitCount != 0 )  // 
								{
									key->doubleHitCount--;
									key->Status = keySeventhPressButNotCofirmedAndStayUp;
								}
								else
								{
									key->Status = keySeventhPressed;
								}
							}
							break;
						case keySeventhPressButNotCofirmedAndStayUp:
							if( key->doubleHitCount != 0 )  // 
							{
								key->doubleHitCount--;
							}
							else
							{
								key->Status = keySeventhPressed; 
							}
							break;
						case keySeventhPressed:
							key->Status = keyUpV2;
							break;
	
						
						case keyEighthPressButNotCofirmedAndStayDown:  // 这种情况下，显然就是在等待按键抬起。
							key->Status = keyEighthPressed;
							break;
						case keyEighthPressButNotCofirmedAndStayUp:  // 这种情况下不应该出现的状态，因为最大就是八击状态。
							break;
						case keyEighthPressed:
							key->Status = keyUpV2;
							break;
						default:
							break;
// 三击事件之后，几乎是类似的了。所以类型是一样。
					}
			}
			else
			{ // 按键值，当前按下。
				switch( key->Status )
					{
						case keyOriginalStatusV2:
							key->Status = keyOriginalStatusV2;
							break;
						case keyUpV2:
							if( key->longPressThresholdValue != 0 )  // 表示存在长按事件
							{
								if( key->doubleHitThresholdValue != 0 ) // 表示存在多击事件
								{
									key->longPressCount = key->longPressThresholdValue;
									key->doubleHitCount = key->doubleHitThresholdValue;
									key->Status = keyFirstPressButNotCofirmedAndStayDown;
								}
								else  // 不存在多击事件
								{
									key->longPressCount = key->longPressThresholdValue;
									key->Status = keyFirstPressButNotCofirmedAndStayDown;
								}
							}
							else // 不存在长按事件
							{
								if( key->doubleHitThresholdValue != 0 ) // 表示存在多击事件
								{
									key->doubleHitCount = key->doubleHitThresholdValue;
									key->Status = keyFirstPressButNotCofirmedAndStayDown;
								}
								else  // 不存在多击事件
								{
									key->Status = keyFirstPressButNotCofirmedAndStayDown;
								}
							}
							break;
						
						case keyFirstPressButNotCofirmedAndStayDown:  // 可以是单击，多击，长按的任何一种，注意：单击，多击都是按键抬起进行的。
							if( key->longPressThresholdValue != 0 ) // 存在长按事件
							{
								if( key->longPressCount != 0 )
								{
									key->longPressCount--;
								}
								else
								{
									key->Status = keyLongPressV2;
								}
							}
							{ // 不管存在不存在长按事件，单击和多击都是按键松开执行的。
								if( key->doubleHitCount != 0 )
								{
									key->doubleHitCount--;
								}
							}
							break;
						case keyFirstPressButNotCofirmedAndStayUp: // 这个时候：还没有确认，显然，存在多击事件。
							key->doubleHitCount = key->doubleHitThresholdValue;
							key->Status = keyDoublePressButNotCofirmedAndStayDown;
							break;
						case keyFirstPress:  // 这是一个比较特殊的状态：在按键松开后，被判断为按键按下松开执行。再进行处理后：按键又被按下。
							key->Status = keyOriginalStatusV2;  // 这个时候，可以这样处理。也可以处理和：keyup状态下的按下操作也是可以了。这里为了不复杂，而且应用差异很小。
							break;
						case keyLongPressV2:  // 考虑到长按是需要某种操作，改变状态是在：应用程序中写
																	// 
							break;

						
// 双击事件后的模板。因为此时去掉长按之后，无其它类型。
						case keyDoublePressButNotCofirmedAndStayDown:  // 没有确定状态，则存在三击或者以上状态。
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							break;
						case keyDoublePressButNotCofirmedAndStayUp:
							key->doubleHitCount = key->doubleHitThresholdValue;
							key->Status = keyThirdPressButNotCofirmedAndStayDown;
							break;
						case keyDoublePressed:
							key->Status = keyOriginalStatusV2;
							break;


						
// 三击事件之后，几乎是类似的（和双击事件开始是一样的。）了。所以类型是一样。
						case keyThirdPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							break;
						case keyThirdPressButNotCofirmedAndStayUp:
							key->doubleHitCount = key->doubleHitThresholdValue;
							key->Status = keyForthPressButNotCofirmedAndStayDown;
							break;
						case keyThirdPress:
							key->Status = keyOriginalStatusV2;
							break;
						
	
						case keyForthPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							break;
						case keyForthPressButNotCofirmedAndStayUp:
							key->doubleHitCount = key->doubleHitThresholdValue;
							key->Status = keyFivthPressButNotCofirmedAndStayDown;
							break;
						case keyForthPressed:
							key->Status = keyOriginalStatusV2;
							break;
	
						
						
						case keyFivthPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							break;
						case keyFivthPressButNotCofirmedAndStayUp:
							key->doubleHitCount = key->doubleHitThresholdValue;
							key->Status = keySixthPressButNotCofirmedAndStayDown;
							break;
						case keyFivthPressed:
							key->Status = keyOriginalStatusV2;
							break;
	
						
						
						case keySixthPressButNotCofirmedAndStayDown:
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							break;
						case keySixthPressButNotCofirmedAndStayUp:
							key->doubleHitCount = key->doubleHitThresholdValue;
							key->Status = keySeventhPressButNotCofirmedAndStayDown;
							break;
						case keySixthPressed:
							key->Status = keyOriginalStatusV2;
							break;
	
						
						
						case keySeventhPressButNotCofirmedAndStayDown:  // 
							if( key->doubleHitCount != 0 )
							{
								key->doubleHitCount--;
							}
							break;
						case keySeventhPressButNotCofirmedAndStayUp:
							key->doubleHitCount = key->doubleHitThresholdValue;
							key->Status = keyEighthPressButNotCofirmedAndStayDown;
							break;
						case keySeventhPressed:
							key->Status = keyOriginalStatusV2;
							break;
						
						
						case keyEighthPressButNotCofirmedAndStayDown:  // 这个状态，只要等待八击事件了。
							break;
						case keyEighthPressButNotCofirmedAndStayUp:  // 这个不应该存在
							break;
						case keyEighthPressed:
							key->Status = keyOriginalStatusV2;
							break;
						default:
							break;
// 三击事件之后，类型是一样的。程序模块都是一样的了。
					}
			}
		}
	}
}
	


