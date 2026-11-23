/**
 * @brief 为了简化main函数中代码过长,将部分不必过度关心的函数封装到这里
 */
 
#include "ForSampleMain.h"
#include "string.h"
 
ToUpperStatusMsg_TypeDef ToUpperStatusMsg ;   // 


u8 RunLEDFre = 2;  // 只能是2或3，当为2的时候每500ms闪烁一次，当为3的时候每1s闪烁一次！

vu32 test = 0; // 用于找出误差为0的波特率的点，用来提高串口波特率的传输速度！
vu32 actualRate = 0;
/**
 * @brief  最初的初始化外设,封装起来细节,不让外部看出来
 * @Fun    需要在最开始的时候点亮所有的LED
 */
void InitPeripheral( void )
{
	memset(&ToUpperStatusMsg,0,sizeof(ToUpperStatusMsg_TypeDef) );
	ToUpperStatusMsg.UpLoadStatusDataBuf[0] = 0xAA;
	ToUpperStatusMsg.UpLoadStatusDataBuf[1] = 0x55;
	ToUpperStatusMsg.UpLoadStatusDataBuf[29] = 0x88;
	{ // UART4 引脚配置
		RCC->APB2ENR.IOPCClockEnable_RW = ENABLE;
		GPIOC->CRH.InOutMode10_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
		GPIOC->CRH.PinConfig10_RW = GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL;
		
		GPIOC->CRH.InOutMode11_RW = GPIO_CR_INOUNTMODE_INPUT;
		GPIOC->CRH.PinConfig11_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
		GPIOC->ODR.OutputData11_RW = 1;  // RXD引脚默认上拉
	}
	
	{ // 串口初始化
		ComInit( UART4,460800,EnableIrq );  // 根据查看CH340的手册，不是CH330的手册，发现支持的高波特率为;1.5M或者2M。（发现1.8M误码特别高！）而且上电最好等待一定时间，再运行发送！
		ComPrintString(UART4,"UART4 Enable OK\n");
	}
}

/**
 * @brief u8类型的求和函数,溢出也无所谓,注意千万不要用其它类型
 */
u8 GetSum( u8* DataBuf,u16 Length )
{
	u8 Sum = 0;
	for( u8 i=0;i<Length;i++ )
	{
		Sum += DataBuf[i];
	}
	return Sum;
}

/**
 *  @B ADC初始化函数，用于检测电压
 *
 */


/**
 *  @B 利用ADC进行随机数的生成，正好本个程序用到了ADC
 *
 */
