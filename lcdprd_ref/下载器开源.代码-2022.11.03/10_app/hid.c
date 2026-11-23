/**
	* \B 如何处理离线下载器的HID，所有的接收都放到Com4缓冲区中，当作串口处理。
	* \B 所有的发送，也都是通过串口发送的。所以数据内容都集中到串口进行发送。也就会变得简单了。最主要的是要试错！因为单次发送都是64字节的，所以每次发送记得要64字节对齐！
	此外，由于发送函数，为非阻塞型，所以考虑每次发送完，等待1ms再进行下次发送，但是这样会存在：界面刷新卡界面的问题。但是先这样做，后期再进行维护！
	当前和重点，是可以利用USB-HID恢复之前的开发，并且达到兼容的地步！
	*
**/


#include "teeny_usb.h"
#include "string.h"
#include "includes.h" // 为了嵌入μC/OS
#include "STM32_USART.h"

extern OS_MUTEX USART2_SendUseRight_Mutex;  // 串口2发送使用权
extern OS_SEM   USART2_HadReceiveData_Sem;  // 串口2接收到数据



void HID_Task( void *p_arg )
{
}
