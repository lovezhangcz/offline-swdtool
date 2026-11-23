/**
* @brief STM32 USART函数,通过查询方式发送,中断方式接收,注意引脚配置需要自己配置
* @Author QQ：43058655 WeChat：TC4300
* @Data 6 July 2016
* @Attention :1:自适应时钟算法，如果在程序中改过时钟，建议在初始化之前启用SystemCoreClockUpdate更新时钟
*             2:引脚配置说明,TX引脚建议为复用推挽输出,RX引脚为浮空输入
*             3:在配置引脚时,引脚会引发串口事件
*/

#include "stm32f10x.h"
#include "STM32_USART.h"
#include "string.h"


OS_MUTEX USART2_SendUseRight_Mutex;  // 串口2发送使用权
OS_SEM   USART2_HadReceiveData_Sem;  // 串口2接收到数据

ComBuf Com4;   // 本函数使用了，串口4
/**
* @brief 纯串口两线发送接收，其中USARTCR2_3都不用设置，保持为默认值为0。
*/

//  下面为官方移植的波特率BRR算法！具体的机理不明
u32 BaudrateCalculate( u32 sourceClockRate, u32 baudRate)
{
	u32 integerdivider = 0;
	u32 tmpreg = 0;
	u32 fractionaldivider = 0;
	integerdivider = ((25 * sourceClockRate) / (4 * (baudRate)));
	tmpreg = (integerdivider / 100) << 4;
  fractionaldivider = integerdivider - (100 * (tmpreg >> 4));
	tmpreg |= ((((fractionaldivider * 16) + 50) / 100)) & ((u8)0x0F);
	return tmpreg;
}


/**
*  @B 2018-1-15 10:05:52，做了如下更改，使用DMA接收方式，因为串口接收存在丢数据的情况，而且怀疑是加上操作系统后进入中断频率太高的问题，所以使用DMA方式！
 *
 */
u8 ComInit( USART_TypeDef* comPort,u32 baudRate,IRQEnableType irqEnable )
{
	u32 APBClock = 0;
	{
		if (comPort == USART2)
			{
				APBClock = APB1PCLK;
				RCC->APB1RSTR.USART2Reset_RW = 1; 
				RCC->APB1RSTR.USART2Reset_RW = 0; // 复位一次
				RCC->APB1ENR.USART2ClockEnable_RW = ENABLE;
//				memset(&Com2,0,sizeof(ComBuf));
				if(irqEnable == EnableIrq)
					NVIC_EnableIRQ(USART2_IRQn);
				else
					NVIC_DisableIRQ(USART2_IRQn);
			}
			else if( comPort == UART4 )
			{
				APBClock = APB1PCLK;
				RCC->APB1RSTR.UART4Reset_RW = 1;
				RCC->APB1RSTR.UART4Reset_RW = 0;
				RCC->APB1ENR.UART4ClockEnable_RW = ENABLE;
				memset(&Com4,0,sizeof(ComBuf));
				if(irqEnable == EnableIrq)
					NVIC_EnableIRQ(UART4_IRQn);
				else
					NVIC_DisableIRQ(UART4_IRQn);
			}
			else
			{
				return FALSE;
			}
		}
	SystemCoreClockUpdate();
	comPort->CR3.DMATransmitterEnable_RW = DISABLE;
	comPort->CR1.DataLenth_RW = 0; // 1个起始位8个数据位
  comPort->SR.TransmissionComplete_RCW0 = 0;  // 清除TC位
	comPort->CR1.RecvDataRegNotEmptyInterruptEnable_RW = ENABLE;  // 接收中断
	comPort->CR1.TransmitterEnable_RW = ENABLE; // 使能接收
	comPort->CR1.ReceiverEnable_RW = ENABLE;
	comPort->BRR.All = BaudrateCalculate( APBClock,baudRate);
	comPort->CR1.USARTEnable_RW = ENABLE;

	return TRUE;
}

void ComSendByte( USART_TypeDef* comPort,u8 nSendbyte ) 
{
	while( comPort->SR.TransmitDataRegisterEmpty_R == FALSE );  // 当有不为空时卡住
	comPort->DR.Data_RW = nSendbyte;
}

void ComPrintString( USART_TypeDef* comPort,char const *puts )  // 发送字符串,不用静态变量GCC编译器会报错。
{
	for(;*puts !=0;puts++)
		{
			while( comPort->SR.TransmitDataRegisterEmpty_R == FALSE );  // 当有不为空时卡住
						comPort->DR.Data_RW = *puts;
		}
}

void ComSendBuf( USART_TypeDef* ComPort,u8 *lpBuf,u8 nLen )
{
	u8 i = 0;
	while(nLen)
	{
	//	while( ComPort->SR.TransmitDataRegisterEmpty_R == FALSE ); 
		while( (ComPort->SR.All & (1<<7)) == 0);
		ComPort->DR.Data_RW = *(lpBuf+i);
		nLen--;
		i++;
	}
}

/**
 * @brief UART4 中断函数
 */

void UART4_IRQHandler()
{
	OS_ERR err;
	
	OSIntEnter();   // 使用μC/OS系统
	while( UART4->SR.ReadDataRegisterNotEmpty_RCW0 == TRUE )
	{
		Com4.Buf[Com4.pWrite++] = UART4->DR.Data_RW;
		Com4.pWrite %= COMBUF_MAXBUFF_SIZE;
	}
	OSSemPost ( (OS_SEM  *)&USART2_HadReceiveData_Sem,
							(OS_OPT   )OS_OPT_POST_1,
							(OS_ERR  *)&err );
	OSIntExit();
}
void DMA2_Channel3_IRQHandler(void)
{
}
