









/**
* @brief  用IO模拟SWIM，由于别人的代码不懂，且难以实现，所以使用IO模拟的方法重新制作！！
* @Author 程人杰
* @Data   2018年10月26日
* \Attention 2018年10月30日：最终放弃，因为速度太快，实在不可以操作！
*/

//#include "SWIM_Module_Imitate.h"


///**
//	* @brief 模拟SWIN协议 PB4:SWIM_IO  PA8:SWIM_RST
//	* @Data 
//	* \Attention 由于使用模拟的方式，且SWD协议中，SWCLK一直为输出，所以SWCLK控制的这个引脚就作为SWIM_RST(PA8)，而另外一个引脚就作为SWIM_IO(PB4)引脚！
//								于是和原理图中就不太对应，这个要注意！
//								但是如果这样的话，使用的就是内部弱上拉，这个要注意！
//	*/
//void SWIM_Init( void )
//{
//	// 注意：两个引脚SWIM_RST与SWIM_IO实际上都已经被初始化过了！甚至包括其它引脚！
//}



///**
//	* @brief 由于需要精确的延时，所以本次使用Tim7做为一个时间基准！
//	*/

//void SWIM_TimerInit( void )
//{
//	RCC->APB1ENR.TIM7ClockEnable_RW = ENABLE;
//	TIM7->CR1.OnePulseMode_RW = ENABLE;
//}


///**
//	* @brief 提供一个0.125÷9的时间基准！用倍数的关系来操作，因为这样的时间是最快的了。最大910us左右的时间！
//	*/

//void SWIM_TimerBasicDivide9( u16 x_0_0138us )
//{
//	if( x_0_0138us >=2 )
//	{
//		TIM7->PSC.PrescaleValue_RW = 0;   // 单位为us
//		TIM7->ARR.AutoReloadValue_RW = x_0_0138us-1;  // 
//		TIM7->CNT.CurrentCountValue_RW = 0;  // 从0开始
//		TIM7->CR1.CountEnable_RW = ENABLE;
//		while(TIM7->SR.UpdateInterruptFlag_RCW0 == 0);
//		TIM7->SR.UpdateInterruptFlag_RCW0 = 0;
//	}
//}

///**
//	* @brief 这是一个单个的时间基准，而且这个时间是经过调教的时间！不是理论的计算时间！用来达到非常准确的时间！
//	* \Attention 这个将用于低速的模式单个时间基准测试！
//	*/
//void SWIM_TimerBasicTime( u32 numbers )
//{
//	SWIM_TimerBasicDivide9( 9*numbers );
//}


///**
//	* @brief 认为在每次发送前都是高电平，发送后也要置为高电平！只允许存在0、1情况！
//	*/

//void SWIM_SendBit( u8 bitNum )
//{
//	if( bitNum == 0)
//	{
//		SWIM_IO_PIN_OUT = 0;
//		for( u32 i=0;i<20;i++ )
//		{
//			__nop();
//		}
//		SWIM_IO_PIN_OUT = 1;
//		for( u32 i=0;i<5;i++ )
//		{
//			__nop();
//		}
//	}
//	else
//	{
//		SWIM_IO_PIN_OUT = 0;
//		__nop();__nop();
//		__nop();__nop();
//		__nop();__nop();
//		__nop();__nop();
//		SWIM_IO_PIN_OUT = 1;
//		for( u32 i=0;i<20;i++ )
//		{
//			__nop();
//		}
//	}
//}

///**
//	* @brief SWIM_IO引脚，始终使用开漏输出，用自带的几十K作为上拉（日后需要改版本，增加上拉！）
//	* \Attention 
//	*/

//u32 SWIM_GetACK( void )
//{
//	u32 i = 0;
//	while( (SWIM_IO_PIN_IN == 1)&&(i>0) )
//	{
//		i--;
//	}
////	if( i )
//}

///**
//	* @brief SWIM三个命令，SRST(0)，ROTF(1),WOTF(2)命令
//	* @Data 
//	* \Attention 
//	*/


//u32 SWIM_Command( u8 command,u8* data,u32 lenth )
//{
//	
//}

///**
// *  @B 操作Flash的终极程序
// *  @Attention 当返回值为0x55AA的时候，认为操作已经成功，此时需要更新程序的下载次数！(滚码是根据已下载次数来的。)
//                         0x1122的时候，表示操作失败，具体日后需要不需要定义出错码，看日后V2.0版本的升级！
// */

//u32 DownloadFlash_SWIM( _FileInformation userfile )
//{
//	{ // 设置引脚配置 (最终发现，引脚配置的值与SWD默认值是一样的，但是还是要初始化一下，防止万一！)
//		// PB4：RST，推挽输出为1
//		GPIOB->CRL.InOutMode4_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
//		GPIOB->CRL.PinConfig4_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
//		SWIM_RST_PIN = 1;
//		
//		// PA8：开漏输出
//		GPIOA->CRH.InOutMode8_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
//		GPIOA->CRH.PinConfig8_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_OPEN_DRAIN;
//		GPIOA->ODR.OutputData4_RW = 1;
//	}
//	
//	{ // 恢复引脚配置给SWD使用！
//		// 不需要恢复，和SWD初始化是一样的！
//	}
//	
//	
//}


