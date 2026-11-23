
#ifndef __STM32_USART_H
#define __STM32_USART_H
#include "stm32f10x.h"
#include "includes.h" // 为了嵌入μC/OS

#define COMBUF_MAXBUFF_SIZE 400        // 1024个字节 
#define COMMSG_MINLEN       40          // 40帧的报文

extern OS_MUTEX USART2_SendUseRight_Mutex;  // 串口2发送使用权
extern OS_SEM   USART2_HadReceiveData_Sem;  // 串口2接收到数据

typedef struct
{
__IO	u8 Buf[COMBUF_MAXBUFF_SIZE];
__IO  u16 pRead;
__IO	u16 pWrite;
}ComBuf;

extern ComBuf Com4;
u8 ComInit( USART_TypeDef* comPort,u32 baudRate,IRQEnableType irqEnable );
void ComSendByte( USART_TypeDef* comPort,u8 nSendbyte );
void ComPrintString(USART_TypeDef* comPort,char const *puts);
void ComSendBuf( USART_TypeDef* ComPort,u8 *lpBuf,u8 nLen );
	
u8 ComDMAConfig( USART_TypeDef* comPort );
u8 ComDMATransfer( USART_TypeDef* comPort,u32 sendBufferAddress,u16 transferLength );

#endif

