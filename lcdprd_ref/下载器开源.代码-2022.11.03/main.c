/**
 * @brief 基于STM32F103RC（日后可能到RE）的STM32离线下载程序！
 */
/**
 * @brief 利用μC/OS建议按键扫描程序，同时测试μC/OS的软件定时器
 *        一个按键扫描任务控制两个定时器
 *        STM32F103RCT6的RAM大小：48KB，非常给力！！
 */
 
 
#include "includes.h"
#include "ForUCOSByCRJ.H"
#include "ForSampleMain.h"

#include "STM32_USART.h"
#include "stdio.h"

#include "Encrypt_Code.H"
#include "STM32_FLASH.H"
#include "AES_CRJ.h"
#include "STM32_CRC.H"

#include "w25qxx.h" 
#include "lcd1_8inch.h" 
#include "lcdfont.h"   // 用于字库的显示和操作
#include "KEY.h"
#include "BeepCTL.h"
#include "SWD_Module.h"
#include "memoryManage.h" 
#include "lcdDisplay.h" 
#include "stm32_timer_delay.h"
#include "SWIM_Module.h"
#include "SWIM_Module_Imitate.h"
#include "key_state_By_CRJ_V2.h"  // 使用V2版本的按键，因为需要进行双击长按等操作。




typedef union // 用结构体寻址
{
	unsigned char      IndexLen8[12];  	// 可正常读出，不可写入
	unsigned short int IndexLen16[6];   // 512个16位
	unsigned int       IndexLen32[3];    
}_UniqueID_TypeDef;
 
#define UniqueID  ((_UniqueID_TypeDef *) (0x1FFFF7E8)) // 通用型算法

vu16 BufferForFlashOp[1024] = {0};   // 2KB的大小

u8 maskKey = FALSE;  // 可能存在丢失文件，所以用这个指示位，来操作！
										 // 具体的方法是：在配置文件过程中屏蔽，结束配置的时候，开启！
keyPropertyV2 KeyUp,KeyDown,KeyLeft,KeyRight,KeyMedium,KeyLock;// 因为需要通过按键控制命令，所以按键定义在外面。

//UCOSIII中以下优先级用户程序不能使用，ALIENTEK
//将这些优先级分配给了UCOSIII的5个系统内部任务
//优先级0：中断服务服务管理任务 OS_IntQTask()
//优先级1：时钟节拍任务 OS_TickTask()
//优先级2：定时任务 OS_TmrTask()
//优先级OS_CFG_PRIO_MAX-2：统计任务 OS_StatTask()
//优先级OS_CFG_PRIO_MAX-1：空闲任务 OS_IdleTask()

// 重要说明，当前软件版本号已改变方式。采用类似于2.0.0_b这种形式。每个占用4位
// 比如：想要设置版本号：2.1.2_b，则二进制为：0010 0001 0010 0000 (0x2120)(其中最后一个字母：0：beta版本。1:release版本)
// 版本号：升级，其中第二个数字，表示型号：0：LCD1.8版本。1：mini版本。2：LCD12864版本。
const u16 APPVersion = 0x3150;  // 设置APP版本号，版本号用VX.Y规则，也就是至少两位数版本，日后更改版本的时候，也需要保留版本。

const u8 AES_TableForUniqueID[16] = { 1,1,2,2,3,3,4,4,8,7,6,5,4,3,2,1 };  // 96位ID值的密钥，虽然只用到一个AES密钥，但是为了程序的可读性，仍然写在这里
const u8 AES_TableSPIFlash[16] = { 0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,8,7,6,5,4,3,2,1};  // 存储在SPI-FLASH加密的数据
u8 AES_TableUnlockFunction[16] = {0x1,0x1,0x1,0x1,0x11,0x22,0x33,0x44,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; // 解锁新功能使用的密钥

void checkOrFixAPPVersion( void );
void writeUpgradeFlag( void );
void dealWithIlegal( void );
void opreta_cfg( u16 language,u16 light_lv,u16 beepEnable );


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern u8 HID_StringDescriptor3[];
#include "teeny_usb.h"
#define  TX_EP   PCD_ENDP1
#define  RX_EP   PCD_ENDP1

uint8_t buf[64];  // 这个就是实体的接收，每个的接收都在这边。
__IO uint32_t data_cnt = 0;
u8 tmpBuf[64] = {0};
tusb_device_t* dev;

// if data tx done, set rx valid again
void tusb_on_tx_done(tusb_device_t* dev, uint8_t EPn)
{
  if(EPn == TX_EP){
    tusb_set_rx_valid(dev, RX_EP);
  }
}

int tusb_on_rx_done(tusb_device_t* dev, uint8_t EPn, const void* data, uint16_t len)
{
  if(EPn == RX_EP){
    data_cnt = len;
    return len;
		tusb_set_rx_valid(dev, RX_EP);  // 接收完成就有效！
  }
  return 0;
}

void tusb_reconfig(tusb_device_t* dev)
{
  HID_TUSB_INIT(dev);
  tusb_set_recv_buffer(dev, RX_EP, buf, sizeof(buf));
  tusb_set_rx_valid(dev, RX_EP);
}

void tusb_delay_ms(uint32_t ms)
// 发现之前的延时代码存在问题！
{
	SystemCoreClockUpdate();  // 为了程序的稳定性，更新下时钟
	vu16 countNum = 100*(SystemCoreClock / 1000000); 
	for( volatile u32 temp1=0; temp1<ms ; temp1++ )
	{
		for( volatile u32 temp2 = 0; temp2<= countNum;temp2++ );  // 大约1MS
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief 各种任务的控制块
 */
#define START_TASK_PRIO		60  //任务优先级
#define START_STK_SIZE 		128 //任务堆栈大小，只是一个u32型
OS_TCB StartTaskTCB;  //任务控制块
CPU_STK START_TASK_STK[START_STK_SIZE];  //任务堆栈
void start_task(void *p_arg);  //任务函数

/**
 * @brief 定时上传和一些基础功能,设置要较低的优先级
 */
#define SystemTimerOperate_Task_PRIO  30  // 需要自己设置的最低优先级
#define SystemTimerOperate_Task_SIZE 2048
OS_TCB SystemTimerOperate_Task_TCB;
CPU_STK SystemTimerOperate_Task_STK[SystemTimerOperate_Task_SIZE];
void SystemTimerOperate_Task( void *p_arg );


/**
 * @brief 处理串口接收到命令的函数,需要有一个较高的优先级
 */
#define USARTDataProcess_Task_PRIO  13   // 关于HID的任务，如果优先级太低，则会出现文件配置容易失败！！
#define USARTDataProcess_Task_SIZE  2048
OS_TCB USARTDataProcess_Task_TCB;
CPU_STK USARTDataProcess_Task_STK[USARTDataProcess_Task_SIZE];
void  USARTDataProcess_Task( void *p_arg );

/**
 * @brief 自动下载处理任务
 */

#define AutoDownloader_Task_PRIO 21
#define AutoDownloader_Task_SIZE 1024
OS_TCB AutoDownloader_Task_TCB;
CPU_STK AutoDownloader_Task_STK[AutoDownloader_Task_SIZE];
void AutoDownloader_Task( void *p_arg );


/**
 * @brief 处理按键操作的任务
 */
#define Key_Task_PRIO 12
#define Key_Task_SIZE 2048
OS_TCB Key_Task_TCB;
CPU_STK Key_Task_STK[Key_Task_SIZE];
void Key_Task( void *p_arg );


/**
 * @brief 使用互斥信号量管理USART发送函数,防止串扰发送
 * @brief 使用信号量用来通知已接收到串口数据,通知处理函数
 * @brief 使用信号量处理CAN通过滤波器的报文发送到上位机
 * @Attention 已被嵌入到库函数中
 */


int main(void)
{
	{ // 发现LCD部分情况下白屏，怀疑是电容太大，导致电压压慢，所以加0.5S延时处理。
		for( vu32 i = 0;i<0x5FFFFF;i++ );
	}
	{  // 更新不同的序列号
		HID_StringDescriptor3[2] =  UniqueID->IndexLen8[0]%10 + 0x30;
		HID_StringDescriptor3[4] =  UniqueID->IndexLen8[1]%10 + 0x30;
		HID_StringDescriptor3[6] =  UniqueID->IndexLen8[2]%10 + 0x30;
		HID_StringDescriptor3[8] =  UniqueID->IndexLen8[3]%10 + 0x30;
		HID_StringDescriptor3[10] = UniqueID->IndexLen8[4]%10 + 0x30;
		HID_StringDescriptor3[12] = UniqueID->IndexLen8[5]%10 + 0x30;
		HID_StringDescriptor3[14] = UniqueID->IndexLen8[6]%10 + 0x30;
		HID_StringDescriptor3[16] = UniqueID->IndexLen8[7]%10 + 0x30;
		HID_StringDescriptor3[18] = UniqueID->IndexLen8[8]%10 + 0x30;
		HID_StringDescriptor3[20] = UniqueID->IndexLen8[9]%10 + 0x30;
	}
	if( ENABLE_APP_ISSUE == 1 )  // 只能当使能发布版本的时候，才启用。  
		{ // 这个一定要写在前面的。否则后面的会出问题的。
			SCB->VTOR = 0x08000000|(0x4000 & (u32)0x1FFFFF80);//设置NVIC的向量表偏移寄存器,否则不能中断！！
		}
	{ // 警告：关于Flash的读保护选项，现在由外部加入，不由自己内部进行操作。这样操作也比较方便。
	}
	{  // 因为引脚比较重要，本来可以和发布版本一起使用的，但是为了方便管理，单独用来设置！
		if( CLOSE_SWDPIN == 0 )
			MaskSWDPin( FALSE );   // 开启SWD引脚
		else
			MaskSWDPin( TRUE );    // 关闭SWD引脚
	}
 // 实际发现：isHIDVersion如果在SystemInit改变为1，但是跳到main之后，又重新变为0了。估计是因为.s文件的原因，所以这里重新以制作了下！
	{  // 进行版本的判断。
		// 最后采用的方案是：根据之前的时钟配置来的，因为其引脚判断好像有问题！
		// HID之前的Boot配置是：使用HSE/2为主PLL的SOURCE，使用的9倍频。如果是这样，则认为是HID，否则认为是旧版本的。
		{  // 等待时间，进行判断
			if( (RCC->CFGR.PLLMultiplicationFactor_RW == 7 ) && ( RCC->CFGR.HSEDivForPLLEntry_RW == 1 ) )
			{
				isHIDVersion = 1;  // 表明为HID的版本，需要使用USB
			}
			else
			{
				isHIDVersion = 0;
			}
		}
		{ // 
			if( ENABLE_APP_ISSUE == 0 )  // 关闭发布版本
			{
				if( FORCE_VERSION == 0 )
				{  
					isHIDVersion = 1;  // 强制新版本
				}
				else
				{
					isHIDVersion = 0;  // 强制旧版本
				}
			}
		}
	}
	
	OS_ERR err;  // 枚举型的错误状态变量
	CPU_SR_ALLOC( ); // 
		SysTick_Init( ); // UCOS的SystemTick初始化,强烈建议移植到BSP文件作为板级支持包,且定时频率为OSCfg_TickRate_Hz
	InitPeripheral( ); // 初始化需要用到的外设
	BeepInit();
	
	{ // 初始化LCD，同时清屏！
		LCDInitOfST7735CtlIC(   );
		LCD_DispalyColor( WHITE );
	}
	{  // 
		if( HARDWARE_VERSION == 0 ) 
		{
		}
	}
	{  // 强制进入升级模式。
		 // 在这里进入升级模式，主要是为是防止通信串口的失误，可以恢复到旧版本
		 // 由于之前的版本的BOOT，没有强制升级模式，所以本次加入
		 // 由于新版本的BOOT已有了强制升级，所以本次主要是之前的老版本和mini版本。这样就可以一定程度防止意外的发生！
		 // mini版本使用：PA15，之前老版本使用：PC2
		u8 timeCount = 5;  // 如果5S都是低电平
		if( HARDWARE_VERSION == 0 ) // mini下载版本
		{
		}
		else // LCD，老版本，PC2
		{
			RCC->APB2ENR.IOPCClockEnable_RW = ENABLE;
			GPIOC->CRL.InOutMode2_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOC->CRL.PinConfig2_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
			GPIOC->ODR.OutputData2_RW = 1;
			tusb_delay_ms(100);
			if( GPIOC->IDR.InputData2_R == 0  )  // 表示被按下
			{
				do
				{
					timeCount --;
					tusb_delay_ms(1000);
				}while( ( GPIOC->IDR.InputData2_R == 0 )&&( timeCount != 0 )  );
			}
		}
		
		if( timeCount == 0 )
			{  // 进入升级模式
				{ // 判断成功，则进入升级模式，就是写标志位
						writeUpgradeFlag( );  // 把升级标志改一下，不知道为什么改不成功？
						{
							LCD_DispalyColor( WHITE );
							if( HighDensity_FlashPage(7)->IndexLen16[0] == 0x55 )
							{
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,16+12*9,12,RED,"  进入升级模式成功！" );
								}
								else
								{
									LCD_ShowString(0,16+12*9,12,RED,"Successfully entered upgrade mode!" );
								}
							}
							else
							{
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,16+12*9,12,RED,"  进入升级模式失败！" );
								}
								else
								{
									LCD_ShowString(0,16+12*9,12,RED,"Failed to enter upgrade mode!" );
								}
							}
						}
					}
					ComPrintString( UART4,"Go to IAP Mode.....\n");
					NVIC_SystemReset();
			}
	}
	{
			{  // 启动1.5K电阻
			if( isHIDVersion == 1 )
			{  // 否则会和mini的引脚存在冲突
				RCC->APB2ENR.AFIOClockEnable_RW = ENABLE;
				AFIO->MAPR.SerialWireJTAGConfiguration_W = 0x2;
				RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
				GPIOA->CRH.InOutMode15_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
				GPIOA->CRH.PinConfig15_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
				GPIOA->ODR.OutputData15_RW = 1;
			}
		}
		
		memset(tmpBuf,0,sizeof(tmpBuf));
		dev = tusb_get_device(TEST_APP_USB_CORE);
		tusb_open_device(dev);
		tusb_delay_ms(200);
	}
	{ //W25QXX初始化和检查，简单检查~
		W25QXX_Init();			
		if( W25QXX_TYPE != W25Q64 )
		{
			if( L_ENGLISH_VER != 0x55AA )
			{
				LCD_ShowString(0,16+12*8,12,RED," 错误：W25QFLASH检测错误" );
			}
			else
			{
				LCD_ShowString(0,16+12*8,12,RED,"  ERR：W25Q Check err " );
			}
		}
	}
	{	// 就相当于：SCB->VTOR = 0x08000000|0x2800  后面的判断是防止地址出界，但是可能多此一举。
		if( ENABLE_APP_ISSUE == 1 )  // 只能当使能发布版本的时候，才启用。
		{
			checkOrFixAPPVersion();   // 设置软件版本号
			dealWithIlegal();         // 检查有没有违法信息，写在这里的原因是：可以用LCD显示存在不存在违法信息。
		}
	}
	{	// 检查读写保护，如果没有启用读保护，则自己启用读保护，如果启用写保护，则需要自己删除
		// 实测中发现：必须要重新上电才能显示正常，否则不正常！！！
		if( ENABLE_APP_ISSUE == 1 ) 
		{
			//                  ==0：表读保护               != 0xFFFFFFFF 无写保护
			if( (FLASH->OBR.ReadProtection_R == 0)||( FLASH->WRPR.WriteProtect_R != 0xFFFFFFFF ) ) // 如果没有使能读保护，或者使能了写保护
			{
				FLASH_ReadOutProtection( ENABLE );  // 使能读保护，同时清除写保护，
			}
		}
	}
	{  // 硬件版本号控制引脚
	}
	{
		timerHardDelyaInit();
	}
	OSInit(&err);		//系统初始化,注意用的参数是地址!
	if( err != OS_ERR_NONE ) {
		ComPrintString(UART4,"μC/OS Init Fail\n");
		while(1);
		/*显然在这里需要检查问题.*/
	}
	
	OS_CRITICAL_ENTER();//进入临界区,官方书中没有这个
	//创建开始任务,注意总共有13个参数!
	OSTaskCreate(	 (OS_TCB 	*   )&StartTaskTCB,		//任务控制块的地址!!用的也是指针,指针必须有类型,所以需要强制转换.
								 (CPU_CHAR	* )"start task", 		//任务名字,可由任意的ASCII值构成!
								 (OS_TASK_PTR )start_task, 			//任务函数,在下面有定义!!!
	               (void		*   )0,					//传递给任务函数的参数,是指针型数据!!注意:可以是任意类型的,因为原型函数中就是用void*来声明的
                 (OS_PRIO	    )START_TASK_PRIO,     //任务优先级(这个是开始任务的)
                 (CPU_STK   * )&START_TASK_STK[0],//任务堆栈基地址,这个是必须的.
                 (CPU_STK_SIZE)START_STK_SIZE/10,	//任务堆栈深度限位,也就是栈溢出警告的的极限深度,一般都是/10.也就是剩余10%栈时.
                 (CPU_STK_SIZE)START_STK_SIZE,		//任务堆栈大小
                 (OS_MSG_QTY  )0,					//任务内部消息队列能够接收的最大消息数目,为0时禁止接收消息
                 (OS_TICK	    )0,					//当使能时间片轮转时的时间片长度，为0时为默认长度，
                 (void   	*   )0,					//用户补充的存储区
								 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR, //定义任务的可选项!!这两个表示:任务栈全部清零.
                 (OS_ERR 	*   )&err);				//存放该函数错误时的返回值,若创建成功,则返回OS_ERR_NONE,通过参数可检查出错的原因.
	OS_CRITICAL_EXIT();	//退出临界区,官方书中没有这个

	OSStart(&err);  //开启UCOSIII,注:ucos会先运行调用这个函数之前已经创建最高优先的任务!
	if( err != OS_ERR_NONE ) {
		while(1);
		/* Your code is NEVER supposed to come back to this point - 永远不应该回到 */
	}
	while(1);//官方书中没有这个
}

/**
 * @brief  开始任务
 */
void start_task(void *p_arg) 
{
	OS_ERR err;
	CPU_SR_ALLOC();
	p_arg = p_arg;
	CPU_Init(); 
#if OS_CFG_STAT_TASK_EN > 0u
   OSStatTaskCPUUsageInit(&err);  	//统计任务                
#endif
	
#ifdef CPU_CFG_INT_DIS_MEAS_EN		//如果使能了测量中断关闭时间
    CPU_IntDisMeasMaxCurReset();	
#endif
	
#if	OS_CFG_SCHED_ROUND_ROBIN_EN  //当使用时间片轮转的时候
	 //使能时间片轮转调度功能,时间片长度为1个系统时钟节拍，既1*5=5ms
	OSSchedRoundRobinCfg(DEF_ENABLED,1,&err);  
#endif		
	{ // 建立串口发送互斥信号量
		OSMutexCreate( 	(OS_MUTEX  *)&USART2_SendUseRight_Mutex,
										(CPU_CHAR  *)"USART2_SendUseRight_Mutex",
										(OS_ERR    *)&err );
		if( err != OS_ERR_NONE ) {  // 初始化错误就卡住
			while(1);
		}
	}
	{ // 建立LCD显示，信号量
		OSMutexCreate( 	(OS_MUTEX  *)&LCD_ShowsrtingUseRight_Mutex,
										(CPU_CHAR  *)"LCD_ShowsrtingUseRight_Mutex",
										(OS_ERR    *)&err );
		if( err != OS_ERR_NONE ) {  // 初始化错误就卡住
			while(1);
		}
	}
	OSSemCreate(	(OS_SEM     *)&USART2_HadReceiveData_Sem,
								(CPU_CHAR   *)"USART2_HadReceiveData_Sem",
								(OS_SEM_CTR  )0,
								(OS_ERR     *)&err);
	if( err != OS_ERR_NONE ) {  // 初始化错误就卡住
		while(1);
	}

		
	OSTaskCreate(	 (OS_TCB 	*   )&SystemTimerOperate_Task_TCB,		
								 (CPU_CHAR	* )"SystemTimerOperate", 		
                 (OS_TASK_PTR )SystemTimerOperate_Task, 			
                 (void		*   )0,					
                 (OS_PRIO	    )SystemTimerOperate_Task_PRIO,     
                 (CPU_STK   * )&SystemTimerOperate_Task_STK,	
                 (CPU_STK_SIZE)SystemTimerOperate_Task_SIZE/10,	
                 (CPU_STK_SIZE)SystemTimerOperate_Task_SIZE,		
                 (OS_MSG_QTY  )0,					
                 (OS_TICK	    )0,  					
                 (void   	*   )0,					
                 (OS_OPT      )OS_OPT_TASK_STK_CHK+OS_OPT_TASK_STK_CLR,
                 (OS_ERR 	*   )&err);	
	if( err != OS_ERR_NONE ) {  // 初始化错误就卡住
		while(1);
	}
	
	OSTaskCreate(	 (OS_TCB 	*   )&USARTDataProcess_Task_TCB,		
								 (CPU_CHAR	* )"USARTDataProcess_Task", 		
                 (OS_TASK_PTR )USARTDataProcess_Task, 			
                 (void		*   )0,					
                 (OS_PRIO	    )USARTDataProcess_Task_PRIO,     
                 (CPU_STK   * )&USARTDataProcess_Task_STK,	
                 (CPU_STK_SIZE)USARTDataProcess_Task_SIZE/10,	
                 (CPU_STK_SIZE)USARTDataProcess_Task_SIZE,		
                 (OS_MSG_QTY  )0,					
                 (OS_TICK	    )0,  					
                 (void   	*   )0,					
                 (OS_OPT      )OS_OPT_TASK_STK_CHK+OS_OPT_TASK_STK_CLR,
                 (OS_ERR 	*   )&err);	
	if( err != OS_ERR_NONE ) {  // 初始化错误就卡住
		while(1);
	}				

	OSTaskCreate(	 (OS_TCB 	*   )&AutoDownloader_Task_TCB,		
								 (CPU_CHAR	* )"AutoDownloader_Task", 		
                 (OS_TASK_PTR )AutoDownloader_Task, 			
                 (void		*   )0,					
                 (OS_PRIO	    )AutoDownloader_Task_PRIO,     
                 (CPU_STK   * )&AutoDownloader_Task_STK,	
                 (CPU_STK_SIZE)AutoDownloader_Task_SIZE/10,	
                 (CPU_STK_SIZE)AutoDownloader_Task_SIZE,		
                 (OS_MSG_QTY  )0,					
                 (OS_TICK	    )0,  					
                 (void   	*   )0,					
                 (OS_OPT      )OS_OPT_TASK_STK_CHK+OS_OPT_TASK_STK_CLR,
                 (OS_ERR 	*   )&err);	
	if( err != OS_ERR_NONE ) {  // 初始化错误就卡住
		while(1);
	}	
	
	OSTaskCreate(	 (OS_TCB 	*   )&Key_Task_TCB,		
								 (CPU_CHAR	* )"Key_Task", 		
                 (OS_TASK_PTR )Key_Task, 			
                 (void		*   )0,					
                 (OS_PRIO	    )Key_Task_PRIO,     
                 (CPU_STK   * )&Key_Task_STK,	
                 (CPU_STK_SIZE)Key_Task_SIZE/10,	
                 (CPU_STK_SIZE)Key_Task_SIZE,		
                 (OS_MSG_QTY  )0,					
                 (OS_TICK	    )0,  					
                 (void   	*   )0,					
                 (OS_OPT      )OS_OPT_TASK_STK_CHK+OS_OPT_TASK_STK_CLR,
                 (OS_ERR 	*   )&err);	
	if( err != OS_ERR_NONE ) {  // 初始化错误就卡住
		while(1);
	}		
	
	OS_TaskSuspend((OS_TCB*)&StartTaskTCB,&err);		//挂起开始任务			 
	OS_CRITICAL_EXIT();	//退出临界区
}



/**
 * @brief 一个系统定时处理的任务，暂时不知道，保留这个任务用作什么！暂时就用作LED程序吧
 */

void SystemTimerOperate_Task( void *p_arg )
{
	OS_ERR err;
	u16 countTimer = 0;
	u16 ADCalibrationValue = 0; // STM32的ADC需要校正
	vu32 ADCValue_MV = 0;	// 电压MV值
	u8 forDisplayIOIfo = 0;
	{ // 初始化SWD引脚！
		SWDInit( );
	}
	{ // 用于开始时候显示文件信息！
		freashDisplayInformation( MenuManager );
	}
	{ // 初始化ADC，手册规定ADC时钟，不能超过14MHZ，那么APB2的时钟是：72MHZ，也就是最小5.1分频，所以可以选择6分频
		OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_HMSM_NON_STRICT,&err);  // 先等待上电时间200ms，全系统稳定，防止ADC不稳定的情况！
		
		if( isHIDVersion == TRUE )
		{
			RCC->APB2ENR.IOPBClockEnable_RW = ENABLE;
			GPIOB->CRL.InOutMode1_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOB->CRL.PinConfig1_RW = GPIO_CR_PINCONFG_IN_ANALOG;
		}
		else
		{
			RCC->APB2ENR.IOPCClockEnable_RW = ENABLE;
			GPIOC->CRL.InOutMode1_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOC->CRL.PinConfig1_RW = GPIO_CR_PINCONFG_IN_ANALOG;
		}
		
		RCC->APB2ENR.ADC1ClockEnable_RW = ENABLE;
		RCC->CFGR.ADCPrescale_RW = 2;  // ADC6分频，也就是：72MHZ/6 = 12MHZ
		RCC->APB2RSTR.ADC1Reset_RW = ENABLE;
		RCC->APB2RSTR.ADC1Reset_RW = DISABLE;
		{
			ADC1->SQR1.RegularChannelSequenceLengthMinusOne_RW = 0;  // 拥有一个转换
			ADC1->CR2.ADONEnable_RW = ENABLE; // 开启ADC转换
			OSTimeDlyHMSM(0,0,0,10,OS_OPT_TIME_HMSM_NON_STRICT,&err);  // 校准前，要保持一定时间
			ADC1->CR2.ADCCalibration_RW = ENABLE;
			while(ADC1->CR2.ADCCalibration_RW == 1); //等待转换结束
			ADCalibrationValue = ADC1->DR.RegularData_R&0xFFF; // 获取校正值
		}
		
		OSTimeDlyHMSM(0,0,0,20,OS_OPT_TIME_HMSM_NON_STRICT,&err);  // 等待20ms用来启动传输
		if( isHIDVersion == TRUE )
		{
			ADC1->SQR3.RegularSequence1_RW = 9;
		}
		else
		{
			ADC1->SQR3.RegularSequence1_RW = 11; // 第11个通道。
		}
		
		{ // 利用ADC来生成随机数，用于生成随机码~
			for( u8 i = 0;i<8;i++ )
			{
				u8 tmp=0;
				u8 mask=0x80;
				do {
					ADC1->CR2.ADONEnable_RW = ENABLE;  // 开启一次转换
					while(ADC1->SR.EndOfConversion_RCW0 == 0); // 等待转换完成
					tmp |= (ADC1->DR.RegularData_R & 0x01)?mask:0; //set tmp based on adc's lsb
					mask = mask >> 1;  //shift to the next bit
				} while (mask);
				AES_TableUnlockFunction[i+8] = tmp;
				randomCode[i] = AES_TableUnlockFunction[i+8];
			}
		}
	}
	{ // 初始化LED引脚
		if( isHIDVersion == 0 )
		{
			RCC->APB2ENR.IOPCClockEnable_RW = ENABLE;
			GPIOC->CRH.InOutMode8_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
			GPIOC->CRH.PinConfig8_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
			NO_HID_LED1 = LED_OFF;
		}
		else
		{
			RCC->APB2ENR.IOPCClockEnable_RW = ENABLE;
			GPIOC->CRL.InOutMode1_RW = GPIO_CR_INOUNTMODE_OUTPUT_50MHZ;
			GPIOC->CRL.PinConfig1_RW = GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL;
			HID_LED = LED_OFF;
		}
		
		if( HARDWARE_VERSION == MINI_VERSION )  // MINI版本
		{
		}
	}	
	{ // 初始化蜂鸣器功能！
		
		if(  HARDWARE_VERSION == MINI_VERSION )
		{
		}
		else
		{
			BeepCtrol(1,650);
		}
	}
	ADC1->CR2.ADONEnable_RW = ENABLE;  // 开启一次转换
	while( DEF_ON )
	{
		{ // ADC使用滤波算法
			while(ADC1->SR.EndOfConversion_RCW0 == 0); // 等待转换完成
			ADCValue_MV = (2* (((( ADC1->DR.RegularData_R&0xFFF) + ADCalibrationValue)*3300 ) /0xFFF));
			ADC1->CR2.ADONEnable_RW = ENABLE;  // 开启一次转换
		}
		OSTimeDlyHMSM(0,0,0,20,OS_OPT_TIME_HMSM_NON_STRICT,&err);  // 20MS的时基
		countTimer++;
		if( countTimer % 50 == 0 )  // 1000ms
		{
			char ADCValueChar[8];  // 定义显示电压的字符串，这个变量务必要大，防止数组溢出，出现错误！！实际情况已经出现了这个问题！而且不止一次，可能编译后都有可能出现！
			char DisPlayVoltage[50];
			if( L_ENGLISH_VER != 0x55AA )
			{
				strcpy(DisPlayVoltage, "  参考电压:");
			}
			else
			{
				strcpy(DisPlayVoltage, "     Vref:");
			}
						
			sprintf( ADCValueChar,"%4d",ADCValue_MV);
			strcat( DisPlayVoltage,ADCValueChar);
			strcat( DisPlayVoltage," mV");
			
			if( HARDWARE_VERSION != MINI_VERSION )
			{
				LCD_ShowString(0,12*12,12,RED,DisPlayVoltage);  // 只有LCD版本可以显示
			}
			
			{  // 不断变换引脚描述的引脚
				if( MenuManager.PageNumberIndex == 8 )  // 引脚描述
				{
					if( forDisplayIOIfo == 0 )
					{
						LCD_ShowString(8+6+8,          80,12,RED,"S\nW\nD\nI\nO");
						LCD_ShowString(8+6+8+6+8,      80,12,RED,"S\nW\nC\nL\nK");
					}
					else if( forDisplayIOIfo == 4 )
					{
						LCD_ShowString(8+6+8,      80,12,BLUE,"R\nE\nS\nE\nT");
						LCD_ShowString(8+6+8+6+8,  80,12,BLUE,"S\nW\nI\nM\n ");
						
					}
					forDisplayIOIfo ++;
					forDisplayIOIfo %= 8;
				}
				
			}
		}
		if( countTimer == 50 )
		{
			countTimer = 0;
			{
				if( HARDWARE_VERSION == MINI_VERSION )
				{
				}
				else
				{
					if( isHIDVersion == 0 )
					{
						NO_HID_LED1 = ~NO_HID_LED1;
					}
					else
					{
						HID_LED = ~HID_LED;
					}
				}
			}
		}
		if( countTimer == 25 )
		{
			if( HARDWARE_VERSION == MINI_VERSION )
				{
				}
				else
				{
					if( isHIDVersion == 0 )
					{
						NO_HID_LED1 = ~NO_HID_LED1;
					}
					else
					{
						HID_LED = ~HID_LED;
					}
				}
		}
	}
}

/**
 * @brief 处理串口接收到命令的函数,显然需要用信号量同步处理
 */
void  USARTDataProcess_Task( void *p_arg )
{
	OS_ERR err;
	CPU_TS ts;
	u8 dataBuf[COMMSG_MINLEN];  // 定义了数组
	u32 pWriteToW25Q = 0;
	memoryManageInit();
	
	while( DEF_ON )
	{
		OSTimeDlyHMSM(0,0,0,1,OS_OPT_TIME_HMSM_NON_STRICT,&err);  // 这个的具体性能未知！但是实测因为是最低优先级，所以可以不需要延时！需要具体的测试！
			{
				if(data_cnt) { // 表示接收到了数据
						for(int i=0;i<data_cnt;i++)
						{  // 把数据提取出来
							Com4.Buf[Com4.pWrite++] = buf[i];
							Com4.pWrite %= COMBUF_MAXBUFF_SIZE;
						}
						data_cnt = 0; 
						tusb_set_rx_valid(dev, RX_EP);
					}
			}
			{
				// 报文处理函数,大于报文开始处理，报文处理函数优先级是除了SWD函数的第二高的优先级
				if( ((( Com4.pWrite + COMBUF_MAXBUFF_SIZE ) - Com4.pRead ) % COMBUF_MAXBUFF_SIZE ) >= COMMSG_MINLEN )
				{
					do	{
							for( u8 i=0;i<COMMSG_MINLEN;i++ )
							{
								dataBuf[i] = Com4.Buf[(Com4.pRead+i)%COMBUF_MAXBUFF_SIZE];
							}
							if( (dataBuf[0] == 0x55)&&(dataBuf[1] == 0xAA)&&(dataBuf[COMMSG_MINLEN-1]==0x88) ) //报文开始和结束正确
							{
								{
									MenuManager.lastDisplayWindows = 0;
									switch( dataBuf[2] )  // 这样的方式，需要
									{
										case 0x00: // 暂时保留
											break;
										case 0x01: // 请求下位机状态信息，包括基本信息和文件信息
												{ // 更新并上传状态信息
													W25QFlashProgramFileInformation();  // 执行一次文件信息检查
													UpStatusInfo.fileNumbers = W25QFlashInformation.FlieNumbers;
													UpStatusInfo.RemainingSize = W25QFlashInformation.RemainingSize;
													UpStatusInfo.RemainingContinuousMaxSize = W25QFlashInformation.RemainingContinuousMaxSize;
													for(u8 i=0;i<8;i++ )
													{
														UpStatusInfo.radomCode[i] = AES_TableUnlockFunction[i+8];  // 更新随机码~
													}
													UpStatusInfo.bootloaderVersion = L_BOOTLOADER_VERSION;
													UpStatusInfo.APPVersion = L_APP_VERSION;
													UpStatusInfo.HardWareVersion = 10;   // 硬件版本，日后要控制这个硬件版本！
													UpStatusInfo.SWDFunction = L_STM32SWDFUN;  // STM32的SWD功能，开启位
													UpStatusInfo.SWINFunction = L_STM8SWINFUM; // SWIN的功能开启位
													UpStatusInfo.sum = GetSum(UpStatusInfo.Buf,sizeof(UpStatusInfo.Buf) - 2 );
													ComSendBuf( UART4,UpStatusInfo.Buf,sizeof(UpStatusInfo.Buf) );
													tusb_send_data(dev, TX_EP, UpStatusInfo.Buf, 64, 0);
													OSTimeDlyHMSM(0,0,0,1,OS_OPT_TIME_HMSM_NON_STRICT,&err); // 延时1ms，防止没有发送完，如果后面可以紧紧跟着HID数据的话！
												}
												{ // 上传文件信息，最多120文件！
													for( u8 i = 0;i<100;i++ )
													{
														W25QXX_Read( filex.Buf,W25QXX_FlieDensityAddress( i/8,(i%8)*2,0 ),sizeof(filex.Buf));
														if( ( filex.Status == 0x55 ) && ( GetSum(filex.Buf,sizeof(filex.Buf) - 1) == filex.SumValue ) ) // 表示这是一个正常的文件而且校验和正确！
														{
															UpFileInfo.fileNO = i+1; // 文件编号从1开始！
															UpFileInfo.ChipTypePrefix = filex.ChipTypePrefix;
															UpFileInfo.ChipTypeSuffix = filex.ChipTypeSuffix;
															for( u8 j=0;j<20;j++ ) // 更新文件名~
															{
																UpFileInfo.fileName[j] = filex.FileName[j];
															}
															UpFileInfo.sum = GetSum(UpFileInfo.Buf,sizeof(UpFileInfo.Buf) - 2 );
															ComSendBuf( UART4,UpFileInfo.Buf,sizeof(UpFileInfo.Buf) );
															tusb_send_data(dev, TX_EP, UpFileInfo.Buf, 64, 0);
															OSTimeDlyHMSM(0,0,0,1,OS_OPT_TIME_HMSM_NON_STRICT,&err); // 延时1ms，防止没有发送完
														}
														else // 没有发现正确的文件编号：则退出循环检查！
														{		 // 由于可能会丢失某个文件，所以一直循环下去，进行文件的上传。
//															break;
															
														}
													}
												}
											break;
										case 0x02: // 删除文件或整理内存
											switch(dataBuf[3])
											{
												case 0x1:  // 删除单个文件
													deleteFile( 0, dataBuf[4]  + (dataBuf[5]<<8) );
													break;
												case 0x2:  // 删除全部文件
													deleteFile( 1,0 );
													break;
												case 0x3:  // 整理文件
													arrangeTheProgramFile( );
													break;
												default:
													break;
											}
												// 因为涉及到操作时间的关系，这个文件时间比较长！所以要反馈！
											{ // 交换数据~再发送，因为报文的关系！
													u8 tmp = dataBuf[0];
													dataBuf[0] = dataBuf[1];
													dataBuf[1] = tmp;
													ComSendBuf( UART4,dataBuf,40 );  // 数据返回给上位机，表示接收成功！防止文件信息出错！
													tusb_send_data(dev, TX_EP, dataBuf, 64, 0);
													OSTimeDlyHMSM(0,0,0,1,OS_OPT_TIME_HMSM_NON_STRICT,&err); // 延时1ms，防止没有发送完
											}
											break;
										case 0x03:
											{
												if( dataBuf[3] == 0x00 )  // 开启SWD引脚
													MaskSWDPin( TRUE );
												else if( dataBuf[3] == 0x01 ) // 关闭SWD引脚
													MaskSWDPin( FALSE );
											}
											break;
										case 0x04: // 软件复位下位机
											NVIC_SystemReset();
											break;
										case 0x05: // 通知表示要开始传输文件信息，显然这个时候要准备SPIFLASH内存
										{ // 关于屏蔽按键的操作
											maskKey = TRUE;
										}
										{
											W25QFlashProgramFileInformation();  // 执行一次文件信息检查，发现不执行话，文件信息没有。如果一开始配置，会导致提示：没有足够的存储空间！
										}
										{
											if( HARDWARE_VERSION == 0 )
											{ // 现在允许。最多两个文件。因为功能测试上，需要测试STM32和STM8
											}
										}
											memset( &filex,0,sizeof(_FileInformation) );
											{ // 交换数据~再发送，因为报文的关系！
												u8 tmp = dataBuf[0];
												dataBuf[0] = dataBuf[1];
												dataBuf[1] = tmp;
												ComSendBuf( UART4,dataBuf,40 );  // 数据返回给上位机，表示接收成功！防止文件信息出错！
												tusb_send_data(dev, TX_EP, dataBuf, 64, 0);
												OSTimeDlyHMSM(0,0,0,1,OS_OPT_TIME_HMSM_NON_STRICT,&err); // 延时1ms，防止没有发送完
											}
											{ // 准备文件的加密工作，配置密钥，同时初始化！
												for( u8 temp3 = 0;temp3<16;temp3++ )
												{
													AES_Key_Table[temp3] = AES_TableSPIFlash[temp3];  // 使用升级的AES密钥
												}
												aesEncInit();
												aesDecInit();
											}
											pWriteToW25Q = 0;  // 用于把接收到的数据存储入W25q的数据指针！
											break;
										case 0x06: // 表示传输文件信息
											for( u8 i=0;i<32;i++ )
												{
													filex.Buf[filex.pWrite++] = dataBuf[4+i]; // 把接收到数据先放入缓冲区中，这个缓冲区是256字节！
												}
												{ // 当数据量达到256的时候，认为可以开始处理这个256数据了！
													if( filex.pWrite == 256 ) // 表示数据正确！
													{
														{ // 第一步：可行性判断：是否有足够空间！
															if( ( ( filex.ProgramSize + filex.ProgramSize_STM8 ) > W25QFlashInformation.RemainingContinuousMaxSize )
																	|| ( W25QFlashInformation.FlieNumbers >= 100 ))
															{
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,16+12*9,12,RED," 错误：没有足够的空间！" );
																}
																else
																{
																	LCD_ShowString(0,16+12*9,12,RED," ERR: not enough space!" );
																}																	
																
																return;
															}
														}
														{ // 当能执行到这一步的时候，则找到25QFlash中的数据来安排一下，写入文件信息，同时安排存储程序文件。
															// 在这个时候，我们认为可以不用擦除而直接写。因为之前的每个操作都OK。当然，最好还是要操作一下！
															
														}
														{ // 找W25Q可以存放程序的地址！并且擦除这些地址的数据，方便写入！
															// 1：长度要够，2：找到首地址！
															W25QXX_Read( W25Q4KBuf.databuf4K , W25QXX_FlieDensityAddress(15,0,0) , 1792); // 读出这个扇区的数据
															{
																u32 size = 0;      // 大小计算
																u16 pSubscript= 0; // 找到下标
																for( u16 i=0;i<1792;i++ )
																	{
																		if( W25Q4KBuf.databuf4K[i] == 0xFF ) // 表示空闲(只有两种状态：一个空闲，一个使用！)
																		{
																			size += 4096;
																			if( ( filex.ProgramSize + filex.ProgramSize_STM8) <= size ) // 则认为找到合适的存储区了！这时候根据i值来判断。
																			{
																				pSubscript = i - (size/4096-1);   // 找到一开始对应的下标值
																				filex.ProgramSaveAtW25QAddress = 1 * 0x100000 + pSubscript * 0x1000;  // 找到这个地址！
																				{ // 清理，对应的区为0xFF，方便写入！
																					for( u32 j=0;j<size/4096;j++ )
																					{
																						W25QXX_Erase_Sector( filex.ProgramSaveAtW25QAddress + j * 0x1000 ); // 清除新地址所在的文件区域
																					}
																				}
																				{ // 整理指示区
																					W25QXX_Erase_Sector(W25QXX_FlieDensityAddress(15,0,0));
																					for( u32 j=0;j<size/4096;j++ )
																					{
																						W25Q4KBuf.databuf4K[pSubscript+j] = 0x55;  // 表示使用中！
																					}
																					W25QXX_Write_NoCheck( W25Q4KBuf.databuf4K,W25QXX_FlieDensityAddress(15,0,0),1792 );  // 更新4K区域！
																					break;  // 此时可以退出循环！
																				}
																			}
																		}
																		else
																		{
																			size = 0;
																		}
																	}
															}
														}
													}
												}
												{ // 交换数据~再发送，因为报文的关系！
													u8 tmp = dataBuf[0];
													dataBuf[0] = dataBuf[1];
													dataBuf[1] = tmp;
												
													ComSendBuf( UART4,dataBuf,40 );  // 数据返回给上位机，表示接收成功！防止文件信息出错！
													tusb_send_data(dev, TX_EP, dataBuf, 64, 0);
													OSTimeDlyHMSM(0,0,0,2,OS_OPT_TIME_HMSM_NON_STRICT,&err); // 延时1ms，防止没有发送完
												}
											break;
										case 0x07: // 表示传输数据文件。一种文件是：非远程文件，明文传输，加密存储！
																// 
												{ // 处理接收的数据！
													u8 flashBuffer1[16] = { dataBuf[4], dataBuf[5], dataBuf[6], dataBuf[7], dataBuf[8], dataBuf[9], dataBuf[10],dataBuf[11],
																									dataBuf[12],dataBuf[13],dataBuf[14],dataBuf[15],dataBuf[16],dataBuf[17],dataBuf[18],dataBuf[19],};
													u8 flashBuffer2[16] = { dataBuf[20],dataBuf[21],dataBuf[22],dataBuf[23],dataBuf[24],dataBuf[25],dataBuf[26],dataBuf[27],
																									dataBuf[28],dataBuf[29],dataBuf[30],dataBuf[31],dataBuf[32],dataBuf[33],dataBuf[34],dataBuf[35],};
//													AES_Dec(flashBuffer1);// 解密数据，数据不一定是加密传输的，不一定需要解密数据，当然远程文件应该需要这个功能！
//													AES_Dec(flashBuffer2);// 解密数据
													
													// 下面是把信息放入W25q中，其中的关键就是边界信息的处理，虽然边界处理下的信息能遇到的情况概率比较小，但还是要小心处理~
													if( ( pWriteToW25Q + 16 ) <= ( filex.ProgramSize + filex.ProgramSize_STM8 ) ) // 数据指针只有这三种状况！根据推演，下面的问题不大，但是日后需要检查！
													{
														W25QXX_Write_NoCheck(flashBuffer1,filex.ProgramSaveAtW25QAddress+pWriteToW25Q,16);
														pWriteToW25Q += 16;
														{ // 求AES加密和，这样可以快速验证程序的正确性！暂时没有使用AES加密，但是AES加密和应该是上位机提供，下位机进行文件检测的时候，再做对比，这样会比较好！
															// 根据实测情况，下面的加密和存在问题！！和不正确，日后更新版本的时候检查！！
															for( u8 tmp = 0;tmp<16;tmp++ )
																filex.SumValueOfProgramAESSecure += flashBuffer1[tmp];
														}
													}
													else if( pWriteToW25Q > ( filex.ProgramSize + filex.ProgramSize_STM8 ) )
													{
														if( L_ENGLISH_VER != 0x55AA )
														{
															LCD_ShowString(0,16+12*9,12,RED," 警告：超出存储范围" );
														}
														else
														{
															LCD_ShowString(0,16+12*9,12,RED," Warning: out of storage range" );
														}
													}
													else
													{
														W25QXX_Write_NoCheck(flashBuffer1,filex.ProgramSaveAtW25QAddress+pWriteToW25Q,( filex.ProgramSize + filex.ProgramSize_STM8) - pWriteToW25Q);
														{
															for( u8 tmp = 0;tmp<( filex.ProgramSize + filex.ProgramSize_STM8) - pWriteToW25Q;tmp++ )
																filex.SumValueOfProgramAESSecure += flashBuffer1[tmp];
														}
														pWriteToW25Q = ( filex.ProgramSize + filex.ProgramSize_STM8);
													}
													
													if( ( pWriteToW25Q + 16 ) <= ( filex.ProgramSize + filex.ProgramSize_STM8) ) // 数据指针只有这三种状况！
													{
														W25QXX_Write_NoCheck(flashBuffer2,filex.ProgramSaveAtW25QAddress+pWriteToW25Q,16);
														pWriteToW25Q += 16;
														{ // 求AES加密和，这样可以快速验证程序的正确性！
															for( u8 tmp = 0;tmp<16;tmp++ )
																filex.SumValueOfProgramAESSecure += flashBuffer2[tmp];
														}
													}
													else if( pWriteToW25Q > ( filex.ProgramSize + filex.ProgramSize_STM8) )
													{
														if( L_ENGLISH_VER != 0x55AA )
														{
															LCD_ShowString(0,16+12*9,12,RED," 警告：超出存储范围" );
														}
														else
														{
															LCD_ShowString(0,16+12*9,12,RED," Warning: out of storage range" );
														}
													}
													else
													{
														W25QXX_Write_NoCheck(flashBuffer2,filex.ProgramSaveAtW25QAddress+pWriteToW25Q,( filex.ProgramSize + filex.ProgramSize_STM8) - pWriteToW25Q);
														{
															for( u8 tmp = 0;tmp<( filex.ProgramSize + filex.ProgramSize_STM8) - pWriteToW25Q;tmp++ )
																filex.SumValueOfProgramAESSecure += flashBuffer1[tmp];
														}
														pWriteToW25Q = ( filex.ProgramSize + filex.ProgramSize_STM8);
													}
												}
												
												{ // 交换数据~再发送，因为报文的关系！
													u8 tmp = dataBuf[0];
													dataBuf[0] = dataBuf[1];
													dataBuf[1] = tmp;
												
													ComSendBuf( UART4,dataBuf,40 );  // 数据返回给上位机，表示接收成功！防止文件信息出错！
													tusb_send_data(dev, TX_EP, dataBuf, 64, 0);
													OSTimeDlyHMSM(0,0,0,2,OS_OPT_TIME_HMSM_NON_STRICT,&err); // 延时1ms，防止没有发送完
												}
												
											break;
										case 0x08: // 表示传输结束，这个时候写入文件信息。
											{ // 这个时候需要安排位置，来写文件信息~根据之前的操作，理论上，没有被安排的文件信息上全部都被擦除为0xFF
												// 但是，因为开发阶段的不确定性，而且为了保证其稳定性，也就是必须要有清除操作。
												filex.Status = 0x55;
												filex.SumValue = GetSum(filex.Buf,sizeof(filex.Buf) - 1);  // 求和
												for( u8 i = 0;i<100;i++ ) // 最多120个文件！当找到可以写的时候，就可以开始写入信息了！
													{
														W25QXX_Read( fileTemp.Buf,W25QXX_FlieDensityAddress( i/8,(i%8)*2,0 ),sizeof(fileTemp.Buf));
														if( fileTemp.Status != 0x55 ) // 这个时候认为没有被使用！只允许有两种状态：0x55，使用中，其它状态，不正常状态！
														{
															W25QXX_Write_NoCheck(filex.Buf,W25QXX_FlieDensityAddress( i/8,(i%8)*2,0 ),sizeof(filex.Buf));
															{ // 写入已下载次数!
																u8 tmpBuf[4] = {0,0,0,0};
																W25QXX_Write_NoCheck(tmpBuf,W25QXX_FlieDensityAddress( i/8,(i%8)*2+1,0 ),4);
															}
															break;  // 使用Break退出循环！此时已经完成一个程序文件！
														}
													}
											}
											{ // 交换数据~再发送，因为报文的关系！
													u8 tmp = dataBuf[0];
													dataBuf[0] = dataBuf[1];
													dataBuf[1] = tmp;
												
													ComSendBuf( UART4,dataBuf,40 );  // 数据返回给上位机，表示接收成功！防止文件信息出错！
													tusb_send_data(dev, TX_EP, dataBuf, 64, 0);
													OSTimeDlyHMSM(0,0,0,2,OS_OPT_TIME_HMSM_NON_STRICT,&err); // 延时1ms，防止没有发送完
											}
											
											{ // 每次配置完成后，要进行一次文件检查。
												// 只有mini需要检查，非mini版本，可能会存在删除全部的文件可能。
												if( HARDWARE_VERSION == MINI_VERSION )
												{
												}
											}
											{  // 传输结束
												maskKey = FALSE;
												W25QFlashProgramFileInformation(); // 检查一次文件信息，因为本次配置后也需要使用其中信息，用在这里靠谱！
											}
											break;
										case 0x09: // 进入升级模式
										{
											if( 	( dataBuf[3]  == 0x11 )
													&&( dataBuf[4]  == 0x11 ) 
													&&( dataBuf[5]  == 0x11 )
													&&( dataBuf[6]  == 0x11 )
													&&( dataBuf[7]  == 0x11 )
													&&( dataBuf[8]  == 0x11 )
													&&( dataBuf[9]  == 0x11 )
													&&( dataBuf[10] == 0x11 )
													&&( dataBuf[11] == 0x11 ) )
												{ // 判断成功，则进入升级模式，就是写标志位
													if( ENABLE_APP_ISSUE == 1 )  // 只能当使能发布版本的时候，才启用。
													{
														writeUpgradeFlag( );  // 把升级标志改一下，不知道为什么改不成功？
														{
															LCD_DispalyColor( WHITE );
															if( HighDensity_FlashPage(7)->IndexLen16[0] == 0x55 )
															{
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,16+12*9,12,RED,"  进入升级模式成功！" );
																}
																else
																{
																	LCD_ShowString(0,16+12*9,12,RED,"Successfully entered upgrade mode!" );
																}
															}
															else
															{
																if( L_ENGLISH_VER != 0x55AA )
																{
																	LCD_ShowString(0,16+12*9,12,RED,"  进入升级模式失败！" );
																}
																else
																{
																	LCD_ShowString(0,16+12*9,12,RED,"Failed to enter upgrade mode!" );
																}
															}
														}
													}
													ComPrintString( UART4,"Go to IAP Mode.....\n");
													NVIC_SystemReset();
												}
										}
											break;
										case 0x0A: // 表示要解锁新功能，利用随机码来操作的，通过密钥解锁16个数据全为0x11，表示正确！
															 // 这个过程是：电脑首先加密数组(16个0x11)，然后下位机来解密，要求解密出数组！
										{
											u8 flashBuffer[16] = {  dataBuf[5], dataBuf[6], dataBuf[7], dataBuf[8], dataBuf[9], dataBuf[10],dataBuf[11],dataBuf[12],
																							dataBuf[13],dataBuf[14],dataBuf[15],dataBuf[16],dataBuf[17],dataBuf[18],dataBuf[19], dataBuf[20],};
											{ // 初始化AES
												for( u8 i = 0;i<16;i++ )
													{
														AES_Key_Table[i] = AES_TableUnlockFunction[i];  //
													}
													aesEncInit();  // AES加密初始化，实际上只用到了解密服务，这个应该可以不用使用
													aesDecInit();  // AES解密初始化
											}
											AES_Dec(flashBuffer);
											
											if(		(	flashBuffer[0] == 0x11 )
													&&( flashBuffer[1] == 0x22 )
													&&( flashBuffer[2] == 0x33 )
													&&( flashBuffer[3] == 0x44 )
													&&( flashBuffer[4] == 0x55 )
													&&( flashBuffer[5] == 0x66 )
													&&( flashBuffer[6] == 0x77 )
													&&( flashBuffer[7] == 0x88 )
													&&( flashBuffer[8] == 0x11 )
													&&( flashBuffer[9] == 0x22 )
													&&( flashBuffer[10] == 0x33 )
													&&( flashBuffer[11] == 0x44 )
													&&( flashBuffer[12] == 0x55 )
													&&( flashBuffer[13] == 0x66 )
													&&( flashBuffer[14] == 0x77 )
													&&( flashBuffer[15] == 0x88 )		)
											{
												if( L_ENGLISH_VER != 0x55AA )
												{
													LCD_ShowString(0,16+12*9,12,RED," 解锁码正确   " );
												}
												else
												{
													LCD_ShowString(0,16+12*9,12,RED," Correct unlock code" );
												}
												switch(dataBuf[3])
												{
													case 0x01: // 解锁STM32功能
														if( L_STM32SWDFUN != 0x5A )
														{
															u16 tempBuffer[3] = {0};
															tempBuffer[0] = HighDensity_FlashPage(7)->IndexLen16[0];
															tempBuffer[1] = dataBuf[4];  // 这里是实际的开头，需要上位机发送！
															tempBuffer[2] = L_STM8SWINFUM;
															Flash_ErasePage((u32)HighDensity_FlashPage(7)); // 擦除Flash
															{
																FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
																FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2; // 解锁
																FLASH->CR.Programming_RW = ENABLE;  // 编程使能
																for( u16 i=0;i<3;i++ )
																{
																	 HighDensity_FlashPage(7)->IndexLen16[i] = tempBuffer[i];
																}
																FLASH->CR.Programming_RW = DISABLE; // 编程禁能
																FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
															}
														}
														break;
													case 0x02: // 解锁STM8功能
														if( L_STM8SWINFUM != 0x5A )
														{
															u16 tempBuffer[3] = {0};
															tempBuffer[0] = HighDensity_FlashPage(7)->IndexLen16[0];
															tempBuffer[1] = L_STM32SWDFUN;
															tempBuffer[2] = dataBuf[4];  // 这里是实际的开头，需要上位机发送！
															Flash_ErasePage((u32)HighDensity_FlashPage(7)); // 擦除Flash
															{
																FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
																FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2; // 解锁
																FLASH->CR.Programming_RW = ENABLE;  // 编程使能
																for( u16 i=0;i<3;i++ )
																{
																	 HighDensity_FlashPage(7)->IndexLen16[i] = tempBuffer[i];
																}
																FLASH->CR.Programming_RW = DISABLE; // 编程禁能
																FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
															}
														}
														break;
													default:
														break;
												}
											}
											else
											{
												if( L_ENGLISH_VER != 0x55AA )
												{
													LCD_ShowString(0,16+12*9,12,RED," 解锁码错误   " );
												}
												else
												{
													LCD_ShowString(0,16+12*9,12,RED," Unlock code error" );
												}
											}
										}
										break;
									case 0x0b: // 实现按键通过命令来控制：单击，双击，多击，长按。
									{
										switch( dataBuf[3] )
										{
											case 0: // 上
												{
													switch( dataBuf[4] )
													{
														case 0:
															KeyUp.Status = keyLongPressV2;
															break;
														case 1:
															KeyUp.Status = keyFirstPress;
															break;
														case 2:
															KeyUp.Status = keyDoublePressed;
															break;
														case 3:
															KeyUp.Status = keyThirdPress;
															break;
														default:
															break;
													}
												}
												break;
											case 1: // 下
												{
													switch( dataBuf[4] )
													{
														case 0:
															KeyDown.Status = keyLongPressV2;
															break;
														case 1:
															KeyDown.Status = keyFirstPress;
															break;
														case 2:
															KeyDown.Status = keyDoublePressed;
															break;
														case 3:
															KeyDown.Status = keyThirdPress;
															break;
														default:
															break;
													}
												}
												break;
											case 2: // 左
												{
													switch( dataBuf[4] )
													{
														case 0:
															KeyLeft.Status = keyLongPressV2;
															break;
														case 1:
															KeyLeft.Status = keyFirstPress;
															break;
														case 2:
															KeyLeft.Status = keyDoublePressed;
															break;
														case 3:
															KeyLeft.Status = keyThirdPress;
															break;
														default:
															break;
													}
												}
												break;
											case 3: // 右
												{
													switch( dataBuf[4] )
													{
														case 0:
															KeyRight.Status = keyLongPressV2;
															break;
														case 1:
															KeyRight.Status = keyFirstPress;
															break;
														case 2:
															KeyRight.Status = keyDoublePressed;
															break;
														case 3:
															KeyRight.Status = keyThirdPress;
															break;
														default:
															break;
													}
												}
												break;
											case 4: // 中
												{
													switch( dataBuf[4] )
													{
														case 0:
															KeyMedium.Status = keyLongPressV2;
															break;
														case 1:
															KeyMedium.Status = keyFirstPress;
															break;
														case 2:
															KeyMedium.Status = keyDoublePressed;
															break;
														case 3:
															KeyMedium.Status = keyThirdPress;
															break;
														default:
															break;
													}
												}
												break;
											case 5: // 返回
												{
													switch( dataBuf[4] )
													{
														case 0:
															KeyLock.Status = keyLongPressV2;
															break;
														case 1:
															KeyLock.Status = keyFirstPress;
															break;
														case 2:
															KeyLock.Status = keyDoublePressed;
															break;
														case 3:
															KeyLock.Status = keyThirdPress;
															break;
														default:
															break;
													}
												}
												break;
											default: //
												break;
										}
									}
										break;
									default:
										break;
									}
									Com4.pRead += COMMSG_MINLEN;
									Com4.pRead %= COMBUF_MAXBUFF_SIZE;
								}
//								else // 报文特征和不正确
//								{
//									Com4.pRead ++;
//									Com4.pRead %= COMBUF_MAXBUFF_SIZE;
//								}
							}
							else  // 报文特征不正确,减少一位后
							{
								Com4.pRead ++;
								Com4.pRead %= COMBUF_MAXBUFF_SIZE;
							}
						} while(((( Com4.pWrite + COMBUF_MAXBUFF_SIZE ) - Com4.pRead ) % COMBUF_MAXBUFF_SIZE ) >= COMMSG_MINLEN );  // 仍在报文长度内继续处理
					}
			}
		}
}

/**
 * @brief 自动下载的任务管理！
 */
void AutoDownloader_Task( void *p_arg )
{
	OS_ERR err;
	while( DEF_ON ) {
		OSTimeDlyHMSM(0,0,0,500,OS_OPT_TIME_HMSM_NON_STRICT,&err);  // 每500ms用于检测
		if( AutoDownloadEnable == ENABLE )
		{ // 使能了自动下载任务
			{ // 以下程序是从：按键下载直接复制过来的。
				// 先深度读取文件，当进入1级深度时，默认是文件成功读取后，才可以的！
				u32 fileNumIndex = ( MenuManager.PageNumberIndex - 10 ) * 10 + MenuManager.FileNumberIndexInPage - 1;  // 注意：fileNumIndex值为：0-119
				if( HARDWARE_VERSION == 0 )
				{ 
				}
				
				W25QXX_Read( filex.Buf,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2  ,0 ),sizeof(filex.Buf)); // 这次要全部提取出来！
				W25QXX_Read( filex.DownloaderNumbers,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2+1,0 ),sizeof(filex.DownloaderNumbers)); // 读取文件次数区
				filex.AlreadyProgrammedCarrayBit = 	( filex.DownloaderNumbers[0]<<24 )
																					+ ( filex.DownloaderNumbers[1]<<16 )
																					+	( filex.DownloaderNumbers[2]<<8  )
																					+ ( filex.DownloaderNumbers[3]<<0  );  // 由于文件结构变动，现在使用这一种方法
//				W25QXX_Read( filex.Buf,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2  ,0 ),sizeof(filex.Buf)+sizeof(filex.DownloaderNumbers)); // 这次要全部提取出来！
				{
					if( HARDWARE_VERSION == MINI_VERSION )
						{
						}
						{  // 下载之前判断次数，之后就是下载之后更新次数。
							{ // 先判断有没有操作次数！
								if( filex.ProgrammeTimes == 0xFFFFFFFF ) // 不限制次数
								{
									filex.DownloaderNumbers[4] = 0x68;  // 0x68表示可以继续下载
								}
								else
								{
									if( filex.DownloaderNumbers[255] == 0x55 ) // 表示指示区已经满
									{
										if( filex.ProgrammeTimes <= filex.AlreadyProgrammedCarrayBit*(251+1) + 251 )  // 表明已经不能在下载了
										{
												if( L_ENGLISH_VER != 0x55AA )
												{
														LCD_ShowString(0,12*11,12,RED, "  达到最大次数限制   ");
												}
												else
												{
													LCD_ShowString(0,12*11,12,RED,   "    time limited    ");
												}
												filex.DownloaderNumbers[4] = 0xFF;  // 表示本次不能再下载
										}
										else
										{
											filex.DownloaderNumbers[4] = 0x68;  // 0x68表示可以继续下载
										}
									}
									else
										{ // 先找出操作的指示区次数！
											for( u32 tmp0 = 5;tmp0<256;tmp0 ++)
											{
												if( filex.DownloaderNumbers[tmp0] == 0xFF )
												{
													if( filex.ProgrammeTimes < filex.AlreadyProgrammedCarrayBit*(251+1) + tmp0 - 5 + 1 )
													{
														if( L_ENGLISH_VER != 0x55AA )
														{
																LCD_ShowString(0,12*11,12,RED, "  达到最大次数限制   ");
														}
														else
														{
															LCD_ShowString(0,12*11,12,RED,   "    time limited    ");
														}
														filex.DownloaderNumbers[4] = 0xFF;  // 表示本次不能再下载
														break;
													}
													else
													{
														filex.DownloaderNumbers[4] = 0x68;  // 0x68表示可以继续下载
														break;
													}
												}
											}
										}
									}
							}
					}
					if( 0x55AA == DownloadFlash( filex ) )  //表示操作成功，此时需要更新已操作次数
					{
							{  // 在下载之后务必保持引脚为浮空模式。
								if( HARDWARE_VERSION == MINI_VERSION )
								{ // PA0 PA1 PA2 PA3 PA8 PB1 PB6 
								}
								else
								{ // PA0 PA1 PA8 PB4 PB6
									GPIOA->CRL.InOutMode0_RW = GPIO_CR_INOUNTMODE_INPUT;
									GPIOA->CRL.PinConfig0_RW = GPIO_CR_PINCONFG_IN_FLOATING;
									
									GPIOA->CRL.InOutMode1_RW = GPIO_CR_INOUNTMODE_INPUT;
									GPIOA->CRL.PinConfig1_RW = GPIO_CR_PINCONFG_IN_FLOATING;
									
									GPIOA->CRH.InOutMode8_RW = GPIO_CR_INOUNTMODE_INPUT;
									GPIOA->CRH.PinConfig8_RW = GPIO_CR_PINCONFG_IN_FLOATING;
									
									GPIOB->CRL.InOutMode4_RW = GPIO_CR_INOUNTMODE_INPUT;
									GPIOB->CRL.PinConfig4_RW = GPIO_CR_PINCONFG_IN_FLOATING;
									
//									GPIOB->CRL.InOutMode6_RW = GPIO_CR_INOUNTMODE_INPUT;
//									GPIOB->CRL.PinConfig6_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
//									GPIOB->ODR.OutputData6_RW = 1;
								}
							}

						if( HARDWARE_VERSION == MINI_VERSION )
						{
						}
						if( L_ENGLISH_VER != 0x55AA )
						{
							LCD_ShowString(0,12*11,12, BLUE,"     下载成功！     ");  // 用蓝色提示下载成功
						}
						else
						{
							LCD_ShowString(0,12*11,12, BLUE,"  Download sucess!  "); 
						}
						BeepCtrol(1,650);  // 成功下载响一声
						if( (filex.ProgrammeTimes != 0xFFFFFFFF) || (filex.RollingCodeFunction == 0x55) )  // 当这个文件使能编程次数或者使能滚码操作的时候，才需要进行更新！
							{
								if( filex.DownloaderNumbers[255] == 0xFF ) // 说明指示区没有写满，这个时候要找到指示尾，然后写入！
								{ 
									for( u32 tmp0 = 5;tmp0<256 ;tmp0++ )
									{
										if( filex.DownloaderNumbers[tmp0] == 0xFF )
										{ // 找到程序指示的尾巴，进行更新！
											u8 tmp1 = 0x55;
//											W25QXX_Write_NoCheck((u8*)&tmp1,fileNumIndex*512 + 256 + tmp0,1);  // 写入
											W25QXX_Write_NoCheck((u8*)&tmp1,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2+1,0 )+tmp0,1);
											break;
										}
									}
								}
								else
								{ // 如果都不0x55，则认为已经写满，需要重新擦除后，重写！
									{ // 更改fileNumIndex所在内容
										filex.AlreadyProgrammedCarrayBit++; // 暂时不判断，我就不相信能超过0xFFFFFFFFF。
										for( u32 tmp1 = 5;tmp1<256 ;tmp1++ ) // 清除为0xFF
										{
											filex.DownloaderNumbers[tmp1] = 0xFF;
										}
										{ // 更新文件下载次数
											filex.DownloaderNumbers[0] = filex.AlreadyProgrammedCarrayBit>>24;
											filex.DownloaderNumbers[1] = filex.AlreadyProgrammedCarrayBit>>16;
											filex.DownloaderNumbers[2] = filex.AlreadyProgrammedCarrayBit>>8;
											filex.DownloaderNumbers[3] = filex.AlreadyProgrammedCarrayBit>>0;
											filex.DownloaderNumbers[4] = 0xFF;  // 不能下载（好像这个判断没有用！）
										}
										W25QXX_Erase_Sector( W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2+1,0 ) );// 清除所在文件区域：现在是1个4K区
										W25QXX_Write_NoCheck( filex.DownloaderNumbers,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2+1,0 ),256);
									}
								}
								{ // 进行更新后，需要重新显示下载次数！
									W25QXX_Read( filex.Buf,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2,0 ),sizeof(filex.Buf)); // 重新提取一次文件后，显示出来！否则显示的为上一次的次数！
									W25QXX_Read( filex.DownloaderNumbers,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2+1,0 ),sizeof(filex.DownloaderNumbers));
									freashDisplayProgramTimes( filex );
								}
							}
							else
							{
								ComPrintString(UART4,"\r\n本文件不限制次数！\r\n");
							}
						OSTimeDlyHMSM(0,0,0,3500,OS_OPT_TIME_HMSM_NON_STRICT,&err);  // 4S的延时用于处理（1500ms+500ms的延时检测，提供2000ms的时间）
					}
				}
			}
		}
	}
}

/**
 * @brief 检查并设置APP版本号
 */
void checkOrFixAPPVersion( void )
{
	if( L_APP_VERSION != APPVersion )
	{
		for( u16 i=0;i<1024;i++ )
		{
			BufferForFlashOp[i] = HighDensity_FlashPage(6)->IndexLen16[i];  //读出Flash中其它数据
		}
		BufferForFlashOp[0] = APPVersion;	// 更改版本号所在Flash位置
		
		Flash_ErasePage((u32)HighDensity_FlashPage(6)); // 擦除Flash
		{
			FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
			FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2; // 解锁
			FLASH->CR.Programming_RW = ENABLE;  // 编程使能
			for( u16 i=0;i<1024;i++ )
			{
				 HighDensity_FlashPage(6)->IndexLen16[i] = BufferForFlashOp[i];
			}
			FLASH->CR.Programming_RW = DISABLE; // 编程禁能
			FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
		}
	}
}


/**
 * @brief 检查96位ID值是否一致，如果不一致，则输出调试信息后回到Bootloader中。
 * 
 */
u8 CheckUniqueID(  )
{
	{ // 初始化AES
		for( u8 temp1 = 0;temp1<16;temp1++ )
			{
				AES_Key_Table[temp1] = AES_TableForUniqueID[temp1];  // 使用AES的密钥
			}
			aesEncInit();  // AES初始化
			aesDecInit();  // AES解密初始化
	}
	
	{
		u8 uniqueIDOfEightBitTable[16] = {  L_UNIQUE_ID0,L_UNIQUE_ID0>>8,
																				L_UNIQUE_ID1,L_UNIQUE_ID1>>8,
																				L_UNIQUE_ID2,L_UNIQUE_ID2>>8,
																				L_UNIQUE_ID3,L_UNIQUE_ID3>>8,
																				L_UNIQUE_ID4,L_UNIQUE_ID4>>8,
																				L_UNIQUE_ID5,L_UNIQUE_ID5>>8,
																				L_UNIQUE_ID_SUPPLEMENT_FOR_AES0,L_UNIQUE_ID_SUPPLEMENT_FOR_AES0>>8,
																				L_UNIQUE_ID_SUPPLEMENT_FOR_AES1,L_UNIQUE_ID_SUPPLEMENT_FOR_AES1>>8,
																			};  // 恢复加密表
		AES_Dec( uniqueIDOfEightBitTable );  // 解密出来的值不一样
		if(    uniqueIDOfEightBitTable[0]  != UniqueID->IndexLen8[0] 
			  || uniqueIDOfEightBitTable[1]  != UniqueID->IndexLen8[1] 
				|| uniqueIDOfEightBitTable[2]  != UniqueID->IndexLen8[2] 
				|| uniqueIDOfEightBitTable[3]  != UniqueID->IndexLen8[3] 
				|| uniqueIDOfEightBitTable[4]  != UniqueID->IndexLen8[4] 
				|| uniqueIDOfEightBitTable[5]  != UniqueID->IndexLen8[5] 
				|| uniqueIDOfEightBitTable[6]  != UniqueID->IndexLen8[6] 
				|| uniqueIDOfEightBitTable[7]  != UniqueID->IndexLen8[7] 
				|| uniqueIDOfEightBitTable[8]  != UniqueID->IndexLen8[8] 
				|| uniqueIDOfEightBitTable[9]  != UniqueID->IndexLen8[9] 
				|| uniqueIDOfEightBitTable[10] != UniqueID->IndexLen8[10] 
				|| uniqueIDOfEightBitTable[11] != UniqueID->IndexLen8[11] 
				|| uniqueIDOfEightBitTable[12] != 0xFF 
				|| uniqueIDOfEightBitTable[13] != 0xFF
				|| uniqueIDOfEightBitTable[14] != 0xFF
				|| uniqueIDOfEightBitTable[15] != 0xFF                    ) // 如果存在不相等的，则认为密钥对应不上
		{
			return FAIL;  // ID值检查失败
		}
		else
		{
			return TRUE;  // ID值对应正确
		}
	}
}


/**
 * \B 检查Bootloader是不是满足设定的校验值，确保不是非法Bootloader。
 */

u8 checkBootloaderCRCValue( void )
{
	u16 temp1 =  CRC16_OfCheckTable( (u8 *)HighDensity_FlashPage(0),( (L_BOOTLOADER_SIZE_H<<16) + (L_BOOTLOADER_SIZE_L) ));
	if( L_BOOTLOADER_CRC_VALUE == temp1 )
		return SUCCESS;
	else
	{
		return FAIL;
	}
}

/**
 * \B 用于写升级标志
 */

void writeUpgradeFlag( void )
{
	for( u16 i=0;i<1024;i++ )
	{
		BufferForFlashOp[i] = HighDensity_FlashPage(7)->IndexLen16[i];  //读出Flash中其它数据
	}
	BufferForFlashOp[0] = 0x55;	// 写升级标志
	
	Flash_ErasePage((u32)HighDensity_FlashPage(7)); // 擦除Flash
	{
		FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
		FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2; // 解锁
		FLASH->CR.Programming_RW = ENABLE;  // 编程使能
		for( u16 i=0;i<1024;i++ )
		{
			 HighDensity_FlashPage(7)->IndexLen16[i] = BufferForFlashOp[i];
		}
		FLASH->CR.Programming_RW = DISABLE; // 编程禁能
		FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
	}
}

/**
 * \B 用于处理验证违法的函数，关于验证只有：惟一ID值验证，和检查BOOT的验证。
 */

void dealWithIlegal( void )
{
	if( checkBootloaderCRCValue( ) == FAIL )
	{
		if( L_ENGLISH_VER !=0x55AA )
		{
			LCD_ShowString(0,16+12*9,12,RED," 违法的BOOT！    " );
		}
		else
		{
			LCD_ShowString(0,16+12*9,12,RED," illegal BOOT！    " );
		}
		for( vu32 tmp0 = 0;tmp0 < 0xFFFFF;tmp0++ );  // 短暂延时
				// 警告：本来这是有延时函数的。但是使用的μc/OS-Ⅲ，但是使用在开始处，系统根本就没有启用，所以删除。
		writeUpgradeFlag( );   // 则需要重新更新这个程序！
		NVIC_SystemReset();
	}
	else if( CheckUniqueID( ) == FAIL )
	{
			if( L_ENGLISH_VER !=0x55AA )
			{
				LCD_ShowString(0,16+12*9,12,RED," 违法的ID           " );
			}
			else
			{
				LCD_ShowString(0,16+12*9,12,RED," illegal ID    " );
			}
		for( vu32 tmp0 = 0;tmp0 < 0xFFFFF;tmp0++ );  // 短暂延时
				// 警告：本来这是有延时函数的。但是使用的μc/OS-Ⅲ，但是使用在开始处，系统根本就没有启用，所以删除。
		for( vu32 tmp0 = 0;tmp0 < 0xFFFFF;tmp0++ );
		writeUpgradeFlag( );
		NVIC_SystemReset();
	}
}


/**
 * \B 按键处理函数，总共5个按键！然后要根据状态机来显示！
 */
void Key_Task( void *p_arg )
{
	OS_ERR err;
	u8 forminiDownloadSecondFileIndex = 0; // 当这个标志位为1的时候，表示mini下载器需要下载第二个文件！
	u8 doubleHitTime = 0,longHitTime = 0;
	if( HARDWARE_VERSION == 0 )
	{
	}
	{  // 保证下载相关的引脚处理接收浮空模式，每次检查按键的时候用于复位操作。
		if( HARDWARE_VERSION == MINI_VERSION )
		{ // PA0 PA1 PA2 PA3 PA8 PB1 PB6 
		}
		else
		{ // PA0 PA1 PA8 PB4 PB6
			// HID的设备也是这样的，主要是下载接口保持这个默认的状态。
			GPIOA->CRL.InOutMode0_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOA->CRL.PinConfig0_RW = GPIO_CR_PINCONFG_IN_FLOATING;
			
			GPIOA->CRL.InOutMode1_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOA->CRL.PinConfig1_RW = GPIO_CR_PINCONFG_IN_FLOATING;
			
			GPIOA->CRH.InOutMode8_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOA->CRH.PinConfig8_RW = GPIO_CR_PINCONFG_IN_FLOATING;
			
			GPIOB->CRL.InOutMode4_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOB->CRL.PinConfig4_RW = GPIO_CR_PINCONFG_IN_FLOATING;
			
//			GPIOB->CRL.InOutMode6_RW = GPIO_CR_INOUNTMODE_INPUT;
//			GPIOB->CRL.PinConfig6_RW = GPIO_CR_PINCONFG_IN_FLOATING;
		}
	}

	keyInitV2( &KeyUp,effectAtPress,effectInstant,maxDoubleHitIs8,0,50); 
	keyInitV2( &KeyDown,effectAtPress,effectInstant,maxDoubleHitIs8,0,50);
	keyInitV2( &KeyLeft,effectAtPress,effectWaitOpreationConfirm,maxDoubleHitIs8,0,0);
	keyInitV2( &KeyRight,effectAtPress,effectWaitOpreationConfirm,maxDoubleHitIs8,0,0);
	keyInitV2( &KeyMedium,effectAtPress,effectWaitOpreationConfirm,maxDoubleHitIs8,doubleHitTime,longHitTime);  // 
	keyInitV2( &KeyLock,effectAtPress,effectWaitOpreationConfirm,maxDoubleHitIs8,0,150);
	
	{  // 根据硬件来初始化按键
		if( HARDWARE_VERSION == LCD_VERSION )  // 
		{
		}
		else
		{  // mini 版本只用了一个按键:PA15，此外，冲突的只有PA2，被用作了SWCLK引脚
				RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
				GPIOA->CRH.InOutMode15_RW = GPIO_CR_INOUNTMODE_INPUT;
				GPIOA->CRH.PinConfig15_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
				GPIOA->ODR.OutputData15_RW = 1;
		}
		
		if( isHIDVersion == 0 )
		{  // 不是HID版本的时候，才能使用！否则不操作！
			RCC->APB2ENR.IOPBClockEnable_RW = ENABLE;
			GPIOB->CRH.InOutMode11_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOB->CRH.PinConfig11_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
			GPIOB->ODR.OutputData11_RW = 1;
			
			GPIOC->CRL.InOutMode5_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOC->CRL.PinConfig5_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
			GPIOC->ODR.OutputData5_RW = 1;
		}
		else
		{
			RCC->APB2ENR.IOPAClockEnable_RW = ENABLE;
			GPIOA->CRL.InOutMode5_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOA->CRL.PinConfig5_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
			GPIOA->ODR.OutputData5_RW = 1;
			
			GPIOA->CRL.InOutMode6_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOA->CRL.PinConfig6_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
			GPIOA->ODR.OutputData6_RW = 1;
			
			GPIOA->CRL.InOutMode7_RW = GPIO_CR_INOUNTMODE_INPUT;
			GPIOA->CRL.PinConfig7_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
			GPIOA->ODR.OutputData7_RW = 1;
		}
		
		// 下面三个引脚是通用的
		RCC->APB2ENR.IOPCClockEnable_RW = ENABLE;
		GPIOC->CRL.InOutMode2_RW = GPIO_CR_INOUNTMODE_INPUT;
		GPIOC->CRL.PinConfig2_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
		GPIOC->ODR.OutputData2_RW = 1;
		
		GPIOC->CRL.InOutMode3_RW = GPIO_CR_INOUNTMODE_INPUT;
		GPIOC->CRL.PinConfig3_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
		GPIOC->ODR.OutputData3_RW = 1;
		
		GPIOC->CRL.InOutMode4_RW = GPIO_CR_INOUNTMODE_INPUT;
		GPIOC->CRL.PinConfig4_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
		GPIOC->ODR.OutputData4_RW = 1;
		

	}

	while(1)  // 按键处理任务，日后需要不需要改，等待第一版本做出来后，再考虑
	{

		OSTimeDlyHMSM(0,0,0,20,OS_OPT_TIME_HMSM_NON_STRICT,&err);  // 20ms定时处理函数

		switch( KeyUp.Status )   // 操作文件的上下选择
		{
			case keyFirstPress:
			BeepCtrol(1,50);
			cycle1:
			AutoDownloadEnable = DISABLE;
			if( MenuManager.menuDepth == 0 ) // 0级深度
			{
				if( ( MenuManager.PageNumberIndex >=10 ) && ( MenuManager.PageNumberIndex <= 21 ) )  // 表示在文件信息之内！
					// 这个操作是向上减！
				{
					{	// 第一步，先判断这一页有没有信息！
						W25QFlashProgramFileInformation();
						if( W25QFlashInformation.FlieNumbers >= (MenuManager.PageNumberIndex - 10)*10+1 )  // 表明该页有文件！
						{ 
							if( ( MenuManager.FileNumberIndexInPage == 1 ) || ( MenuManager.FileNumberIndexInPage == 0 ) )
							{
								MenuManager.FileNumberIndexInPage = 1;
							}
							else if( MenuManager.FileNumberIndexInPage <= 10 )
							{
								MenuManager.FileNumberIndexInPage-- ; // 进行减法操作
							}
							else
							{
								MenuManager.FileNumberIndexInPage = 10;  // 这个等于1可能会比较好一点！
							}
						}
						else // 认为没有文件！
						{
							MenuManager.FileNumberIndexInPage = 0;  // 就是没有文件，不允许有指示！
						}
					}
					
					freashDisplayInformation( MenuManager );
				}
				else if( MenuManager.PageNumberIndex == 9 )
				{
					opreta_cfg(0,2,0); // 改变蜂鸣器状态
				}
			}
			else if( MenuManager.menuDepth == 1) // 1级深度
			{
				MenuManager.FileOperationIndex --;
				MenuManager.FileOperationIndex = MenuManager.FileOperationIndex%2;
				freashDisplayInformation( MenuManager );
			}
				break;
			case keyLongPressV2:
				OSTimeDlyHMSM(0,0,0,50,OS_OPT_TIME_HMSM_NON_STRICT,&err);
				goto cycle1;
			
//			{ // 这里用于检测输出引脚有没有损坏！通过输出高低电平，再返回检测，验证！
//				SWD_SWDIO_MODE_OUT;  // 关于这三个引脚都是输出模式。
//				for( u8 tmp2 = 0;tmp2<5;tmp2++ )
//				{
//					GPIOA->ODR.OutputData8_RW = 0;
//					GPIOB->ODR.OutputData4_RW = 0;
//					GPIOB->ODR.OutputData6_RW = 0;
//					
//					OSTimeDlyHMSM(0,0,0,20,OS_OPT_TIME_HMSM_NON_STRICT,&err);
//					
//					if( ( GPIOA->IDR.InputData8_R != 0 )||( GPIOB->ODR.OutputData4_RW != 0 )||( GPIOB->ODR.OutputData6_RW != 0 ) )
//					{
//						LCD_ShowString(0,12+12*10,12,RED," SWD引脚值检查错误！ " );
//																						 
//					}
//					OSTimeDlyHMSM(0,0,0,1000,OS_OPT_TIME_HMSM_NON_STRICT,&err);
//					
//					GPIOA->ODR.OutputData8_RW = 1;
//					GPIOB->ODR.OutputData4_RW = 1;
//					GPIOB->ODR.OutputData6_RW = 1;
//					
//					OSTimeDlyHMSM(0,0,0,20,OS_OPT_TIME_HMSM_NON_STRICT,&err);
//					
//					if( ( GPIOA->IDR.InputData8_R != 1 )||( GPIOB->ODR.OutputData4_RW != 1 )||( GPIOB->ODR.OutputData6_RW != 1 ) )
//					{
//						LCD_ShowString(0,12+12*10,12,RED," SWD引脚值检查错误！ " );
//					}
//					OSTimeDlyHMSM(0,0,0,1000,OS_OPT_TIME_HMSM_NON_STRICT,&err);
//				}
//			}
//				break;
			case keyDoublePressed:
				break;
			default:
				break;
		}
		switch( KeyDown.Status )  // 操作文件的上下选择
		{
			case keyFirstPress:
					BeepCtrol(1,50);
					cycle2:
					AutoDownloadEnable = DISABLE;
					if( MenuManager.menuDepth == 0 ) // 0级深度
					{
						if( ( MenuManager.PageNumberIndex >=10 ) && ( MenuManager.PageNumberIndex <= 21 ) )  // 表示在文件信息之内！
						{ // 这个操作是向下减加！
							{	// 第一步，先判断这一页有没有信息！
								W25QFlashProgramFileInformation( );
								u32 indexMaxValue0 = 1;
								if( W25QFlashInformation.FlieNumbers >= (MenuManager.PageNumberIndex - 10)*10+1 )  // 表明该页有文件！
								{
									
									{ // 找到能指示的最大值！
										if( W25QFlashInformation.FlieNumbers >= (MenuManager.PageNumberIndex - 10)*10+10 )
										{
											indexMaxValue0 = 10;  // 可以指示到最大文件！
										}
										else
										{
											indexMaxValue0 = W25QFlashInformation.FlieNumbers%10;
										}
									}
								}
								
								if( MenuManager.FileNumberIndexInPage == 10 )
								{
									MenuManager.FileNumberIndexInPage = 10;
								}
								else
								{
									MenuManager.FileNumberIndexInPage++ ; // 进行减法操作
								}
								if( MenuManager.FileNumberIndexInPage > indexMaxValue0 )
									MenuManager.FileNumberIndexInPage = indexMaxValue0;
								freashDisplayInformation( MenuManager );
						}
					}
					else if( MenuManager.PageNumberIndex == 9 )
					{
						opreta_cfg(0,1,0); // 改变蜂鸣器状态
					}
				}
				else if( MenuManager.menuDepth == 1) // 1级深度
				{
					MenuManager.FileOperationIndex ++;
					MenuManager.FileOperationIndex = MenuManager.FileOperationIndex%2;
					freashDisplayInformation( MenuManager );
				}
				break;
			case keyLongPressV2:
				OSTimeDlyHMSM(0,0,0,50,OS_OPT_TIME_HMSM_NON_STRICT,&err);
				goto cycle2;
//				break;
			case keyDoublePressed:
				break;
			default:
				break;
			}
		switch( KeyLeft.Status ) // 左右移动界面，包括状态。
		{
			case keyFirstPress:
					 BeepCtrol(1,50);
				AutoDownloadEnable = DISABLE;
				if( MenuManager.menuDepth == 0 ) // 0级深度
				{
					if( ( MenuManager.PageNumberIndex >=8 ) && ( MenuManager.PageNumberIndex <= 21 ) )  // 表示在文件信息之内！
					{
						if( MenuManager.PageNumberIndex!= 8 )
						{
							MenuManager.PageNumberIndex -- ;
						}
						freashDisplayInformation( MenuManager );
					}
				}
				else if( MenuManager.menuDepth == 1) // 1级深度
				{
					
				}
				break;
			case keyLongPressV2:
				break;
			case keyDoublePressed:
				break;
			default:
				break;
		}
		switch( KeyRight.Status ) // 左右移动界面，包括状态。
		{
			case keyFirstPress:
					BeepCtrol(1,50);
					AutoDownloadEnable = DISABLE;
					if( MenuManager.menuDepth == 0 ) // 0级深度
					{
						u32 PageNumberIndexMaxValue = 0;
						{ //向右移动不能到没有文件的地方！
							W25QFlashProgramFileInformation( );  // 更新状态信息！
							if( W25QFlashInformation.FlieNumbers == 0 )
								PageNumberIndexMaxValue = 10;
							else
							{
								PageNumberIndexMaxValue = (W25QFlashInformation.FlieNumbers - 1)/10 + 10;
							}
						}
						MenuManager.PageNumberIndex ++ ;  // 先向上加！
						if( ( MenuManager.PageNumberIndex >=8 ) && ( MenuManager.PageNumberIndex <= PageNumberIndexMaxValue ) )  // 表示在文件信息之内！且在最大值之内！
						{
							// NULL
						}
						else
						{
							MenuManager.PageNumberIndex = PageNumberIndexMaxValue;
						}
						freashDisplayInformation( MenuManager );
					}
					else if( MenuManager.menuDepth == 1) // 1级深度
					{
						
					}
					
				break;
			case keyLongPressV2:
				break;
			case keyDoublePressed:
				break;
			default:
				break;
		}
		switch( KeyMedium.Status ) // 用于深层次的选择文件
		{
			case keyFirstPress:
				forminiDownloadSecondFile:	// mini下载器，下载第二个文件也调用这个位置。
					BeepCtrol(1,50);
					AutoDownloadEnable = DISABLE;
					W25QFlashProgramFileInformation( );  // 更新状态信息！
					if( HARDWARE_VERSION == 0 )  // 表示为mini下载器，只能选中一个文件进行下载！
					{
					}
					if( MenuManager.menuDepth == 0 ) // 0级深度
					{
						if(		(W25QFlashInformation.FlieNumbers != 0)	// 存在文件
								&&(MenuManager.FileNumberIndexInPage != 0)   // 文件指示没有指向0
								&&( MenuManager.PageNumberIndex >=10 ) && ( MenuManager.PageNumberIndex <= 21 )   // 在文件列表之中
								)  // 判断能不能进入1级深度，只有当选择的文件的时候，而且这个文件存在的时候才能进入1级深度！
									 // 可以这样认为：之前的操作指针只要确保不是NoFile，且文件没有指向NO FILE就应该可以进入
						{
							MenuManager.menuDepth = 1;
							freashDisplayInformation( MenuManager );
							{ // 用于切换语言
							}
						}
						else
						{
							{ // 用于切换语言
								// 新版本文件全新更换：在开机显示界面下：上下键调整亮度，返回键调整蜂鸣器，且都可以进行保存。
								// 亮度：10%-100%。蜂鸣器：打开与关闭！
								// 也就是总共有三个参数，另外：亮度仅仅在，LCD版本中的HID版本中才有。
								opreta_cfg(1,0,0);
							}
						}
					}
					else if( MenuManager.menuDepth == 1 ) // 1级深度，这个时候就是文件操作了！暂时不写！
					{
						u32 fileNumIndex = ( MenuManager.PageNumberIndex - 10 ) * 10 + MenuManager.FileNumberIndexInPage - 1;  // 注意：fileNumIndex值为：0-119
						if( HARDWARE_VERSION == 0 )
						{  // 表示mini下载器中不存在文件！则报错，并且返回
						}
						// 先深度读取文件，当进入1级深度时，默认是文件成功读取后，才可以的！
						
						W25QXX_Read( filex.Buf,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2  ,0 ),sizeof(filex.Buf)); // 这次要全部提取出来！
						W25QXX_Read( filex.DownloaderNumbers,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2+1,0 ),sizeof(filex.DownloaderNumbers)); // 读取文件次数区
						filex.AlreadyProgrammedCarrayBit = 	( filex.DownloaderNumbers[0]<<24 )
																							+ ( filex.DownloaderNumbers[1]<<16 )
																							+	( filex.DownloaderNumbers[2]<<8  )
																							+ ( filex.DownloaderNumbers[3]<<0  );  // 由于文件结构变动，现在使用这一种方法
						if( MenuManager.FileOperationIndex == 0 ) // 下载程序
						{
							if( HARDWARE_VERSION == MINI_VERSION )
							{
							}
							{  // 下载之前判断次数，之后就是下载之后更新次数。
									{ // 先判断有没有操作次数！
										if( filex.ProgrammeTimes == 0xFFFFFFFF ) // 不限制次数
										{
											filex.DownloaderNumbers[4] = 0x68;  // 0x68表示可以继续下载
										}
										else
										{
											if( filex.DownloaderNumbers[255] == 0x55 ) // 表示指示区已经满
											{
												if( filex.ProgrammeTimes <= filex.AlreadyProgrammedCarrayBit*(251+1) + 251 )  // 表明已经不能在下载了
												{
														if( L_ENGLISH_VER != 0x55AA )
														{
																LCD_ShowString(0,12*11,12,RED, "  达到最大次数限制   ");
														}
														else
														{
															LCD_ShowString(0,12*11,12,RED,   "    time limited    ");
														}
														filex.DownloaderNumbers[4] = 0xFF;  // 表示本次不能再下载
												}
												else
												{
													filex.DownloaderNumbers[4] = 0x68;  // 0x68表示可以继续下载
												}
											}
											else
												{ // 先找出操作的指示区次数！
													for( u32 tmp0 = 5;tmp0<256;tmp0 ++)
													{
														if( filex.DownloaderNumbers[tmp0] == 0xFF )
														{
															if( filex.ProgrammeTimes < filex.AlreadyProgrammedCarrayBit*(251+1) + tmp0 - 5 + 1 )
															{
																if( L_ENGLISH_VER != 0x55AA )
																{
																		LCD_ShowString(0,12*11,12,RED, "  达到最大次数限制   ");
																}
																else
																{
																	LCD_ShowString(0,12*11,12,RED,   "    time limited    ");
																}
																filex.DownloaderNumbers[4] = 0xFF;  // 表示本次不能再下载
																break;
															}
															else
															{
																filex.DownloaderNumbers[4] = 0x68;  // 0x68表示可以继续下载
																break;
															}
														}
													}
												}
											}
									}
							}
							if( 0x55AA == DownloadFlash( filex ) )  //表示操作成功，此时需要更新已操作次数
							{
								{  // 在下载之后务必保持引脚为浮空模式。
									if( HARDWARE_VERSION == MINI_VERSION )
									{ // PA0 PA1 PA2 PA3 PA8 PB1 PB6 
									}
									else
									{ // PA0 PA1 PA8 PB4 PB6
										GPIOA->CRL.InOutMode0_RW = GPIO_CR_INOUNTMODE_INPUT;
										GPIOA->CRL.PinConfig0_RW = GPIO_CR_PINCONFG_IN_FLOATING;
										
										GPIOA->CRL.InOutMode1_RW = GPIO_CR_INOUNTMODE_INPUT;
										GPIOA->CRL.PinConfig1_RW = GPIO_CR_PINCONFG_IN_FLOATING;
										
										GPIOA->CRH.InOutMode8_RW = GPIO_CR_INOUNTMODE_INPUT;
										GPIOA->CRH.PinConfig8_RW = GPIO_CR_PINCONFG_IN_FLOATING;
										
										GPIOB->CRL.InOutMode4_RW = GPIO_CR_INOUNTMODE_INPUT;
										GPIOB->CRL.PinConfig4_RW = GPIO_CR_PINCONFG_IN_FLOATING;
										
//										GPIOB->CRL.InOutMode6_RW = GPIO_CR_INOUNTMODE_INPUT;
//										GPIOB->CRL.PinConfig6_RW = GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN;
//										GPIOB->ODR.OutputData6_RW = 1;
									}
								}
								if( HARDWARE_VERSION == MINI_VERSION )
								{
								}
								if( L_ENGLISH_VER != 0x55AA )
								{
									LCD_ShowString(0,12*11,12, BLUE,"     下载成功！     ");  // 用蓝色提示下载成功
								}
								else
								{
									LCD_ShowString(0,12*11,12, BLUE,"  Download sucess!  "); 
								}
								BeepCtrol(1,650);  // 成功下载响一声
								if( (filex.ProgrammeTimes != 0xFFFFFFFF) || (filex.RollingCodeFunction == 0x55) )  // 当这个文件使能编程次数或者使能滚码操作的时候，才需要进行更新！
								{
									if( filex.DownloaderNumbers[255] == 0xFF ) // 说明指示区没有写满，这个时候要找到指示尾，然后写入！
									{ 
										for( u32 tmp0 = 5;tmp0<256 ;tmp0++ )
										{
											if( filex.DownloaderNumbers[tmp0] == 0xFF )
											{ // 找到程序指示的尾巴，进行更新！
												u8 tmp1 = 0x55;
//											W25QXX_Write_NoCheck((u8*)&tmp1,fileNumIndex*512 + 256 + tmp0,1);  // 写入
												W25QXX_Write_NoCheck((u8*)&tmp1,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2+1,0 )+tmp0,1);
												break;
											}
										}
									}
									else
									{ // 如果都不0x55，则认为已经写满，需要重新擦除后，重写！
										{ // 更改fileNumIndex所在内容
											filex.AlreadyProgrammedCarrayBit++; // 暂时不判断，我就不相信能超过0xFFFFFFFFF。
											for( u32 tmp1 = 5;tmp1<256 ;tmp1++ ) // 清除为0xFF
											{
												filex.DownloaderNumbers[tmp1] = 0xFF;
											}
											{ // 更新文件下载次数
												filex.DownloaderNumbers[0] = filex.AlreadyProgrammedCarrayBit>>24;
												filex.DownloaderNumbers[1] = filex.AlreadyProgrammedCarrayBit>>16;
												filex.DownloaderNumbers[2] = filex.AlreadyProgrammedCarrayBit>>8;
												filex.DownloaderNumbers[3] = filex.AlreadyProgrammedCarrayBit>>0;
												filex.DownloaderNumbers[4] = 0xFF;  // 不能下载（好像这个判断没有用！）
											}
											W25QXX_Erase_Sector( W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2+1,0 ) );// 清除所在文件区域：现在是1个4K区
											W25QXX_Write_NoCheck( filex.DownloaderNumbers,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2+1,0 ),256);
										}
									}
									{ // 进行更新后，需要重新显示下载次数！
										W25QXX_Read( filex.Buf,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2,0 ),sizeof(filex.Buf)); // 重新提取一次文件后，显示出来！否则显示的为上一次的次数！
										W25QXX_Read( filex.DownloaderNumbers,W25QXX_FlieDensityAddress( fileNumIndex/8,(fileNumIndex%8)*2+1,0 ),sizeof(filex.DownloaderNumbers));
										freashDisplayProgramTimes( filex );
									}
								}
								else
								{
									ComPrintString(UART4,"\r\n本文件不限制次数！\r\n");
								}
							}
						}
						else  // 这个是智能下载！暂时先不管，因为当前问题还是比较多的。
						{  // 自动下载方案
							AutoDownloadEnable = ENABLE;
//							LCD_ShowString(0,12*11,12, BLUE,"     Developing...    ");  // 用蓝色提示下载成功
						}
					}
				break;
			case keyLongPressV2:
				BeepCtrol(1,500);
				if( ( HARDWARE_VERSION == 0 ) && ( W25QFlashInformation.FlieNumbers != 0 ) )
				{
				}
			
				break;
			case keyDoublePressed:
				break;
			case keyThirdPress:  // 只存在mini下载器，下载第二个文件
				{
					forminiDownloadSecondFileIndex = 1;
					goto forminiDownloadSecondFile;
				}
//				break;
			case keyFivthPressed:  // 这个也只存在mini下载器，用于改变蜂鸣器的状态，部分情况下，可能不需要蜂鸣器！
				{
					if( HARDWARE_VERSION == MINI_VERSION )
					{
					}
				}
				break;
			default:
				break;
		}
		switch( KeyLock.Status ) // 用于返回文件信息
		{
			case keyFirstPress:
				BeepCtrol(1,50);
				AutoDownloadEnable = DISABLE;
				if( MenuManager.menuDepth == 0 ) // 0级深度
					{
						if( MenuManager.PageNumberIndex == 9 )
						{
							opreta_cfg(0,0,1); // 改变蜂鸣器状态
						}
					}
					else if( MenuManager.menuDepth == 1) // 1级深度，这个时候就是文件操作了！暂时不写！
					{
						MenuManager.menuDepth = 0;
						freashDisplayInformation( MenuManager );
					}
				break;
			case keyLongPressV2:
				AutoDownloadEnable = DISABLE;
				NVIC_SystemReset();
				break;
			case keyDoublePressed:
				break;
			default:
				break; 
		}
		// 为了可以使：电脑软件也可以控制按键。
		if( maskKey == FALSE )
		{
			if( HARDWARE_VERSION == LCD_VERSION )
			{
				if( isHIDVersion == 1 )
				{
					keyDetectV2( &KeyUp,     GPIOA->IDR.InputData5_R );
					keyDetectV2( &KeyDown,   GPIOA->IDR.InputData7_R );
					keyDetectV2( &KeyLeft,   GPIOC->IDR.InputData4_R );
					keyDetectV2( &KeyRight,  GPIOC->IDR.InputData3_R );
					keyDetectV2( &KeyMedium, GPIOA->IDR.InputData6_R );
					keyDetectV2( &KeyLock,   GPIOC->IDR.InputData2_R );
				}
				else
				{
					keyDetectV2( &KeyUp, GPIOC->IDR.InputData4_R );
					keyDetectV2( &KeyDown, GPIOC->IDR.InputData3_R );
					keyDetectV2( &KeyLeft, GPIOC->IDR.InputData2_R );
					keyDetectV2( &KeyRight, GPIOC->IDR.InputData5_R );
					keyDetectV2( &KeyMedium, GPIOA->IDR.InputData2_R );
					keyDetectV2( &KeyLock, GPIOB->IDR.InputData11_R );
				}
			}
			else
			{
				keyDetectV2( &KeyMedium, GPIOA->IDR.InputData15_R );
			}
		}
	}
}

/**
// @language：0：不变，1：变化
// @light_lv：0：不变，1：减少，其它：增加
// @beepEnable 0：不变 1：变化
**/
void opreta_cfg( u16 language,u16 light_lv,u16 beepEnable )
{
	{ // 用于切换语言
		// 新版本文件全新更换：在开机显示界面下：上下键调整亮度，返回键调整蜂鸣器，且都可以进行保存。
		// 亮度：10%-100%。蜂鸣器：打开与关闭！
		// 也就是总共有三个参数，另外：亮度仅仅在，LCD版本中的HID版本中才有。
		if( ( MenuManager.PageNumberIndex == 9 ) || ( HARDWARE_VERSION == MINI_VERSION ) )  // mini版本的只是为了改变蜂鸣器的状态
		{
			u16 sysCfg[3] = {0,0,0};
			sysCfg[0] = L_ENGLISH_VER;
			sysCfg[1] = L_LIGNT_LV;
			sysCfg[2] = L_BEEP_ENABLE;
			
			if( sysCfg[1] > 9 )
				sysCfg[1] = 9;  // 9为最大亮度
			
			if( language == 1 )
			{
				if( sysCfg[0] == 0x55aa )  // 55aa：英文，其它值：中文
				{
					sysCfg[0] = 0xFFFF;
				}
				else
				{
					sysCfg[0] = 0x55AA;
				}
			}
			if( light_lv != 0 )
			{
				if( light_lv == 1 )
				{  // 减少
					if( sysCfg[1] != 0 )
						sysCfg[1]--;
				}
				else
				{  // 增加
					if( sysCfg[1] < 9 )
						sysCfg[1]++;
				}
				{  // 进行亮度变换操作
					TIM5->CCR4.CaptureCompareValue_RW = (TIM5->ARR.AutoReloadValue_RW/10)*(sysCfg[1]+1);
				}
				{  // 进行亮度的显示
						char DisPlayBuffer[50] = {0}; 
						char Value[30] = {0}; ;  // 定义显示电压的字符串
						if( isHIDVersion == 1 )
						{
							if( L_ENGLISH_VER != 0X55AA )
							{
								strcpy(DisPlayBuffer,"     亮度:");
							}
							else
							{
								strcpy(DisPlayBuffer,"  Brightness: ");
							}
							u8 beishu = (sysCfg[1]+1)*10;
							if( beishu > 100 ) { beishu = 100; }
							sprintf( Value,"%02d%% ",beishu);  // 
							strcat( DisPlayBuffer,Value);
							LCD_ShowString(0,12*9,12,BLUE,DisPlayBuffer);
						}
				}
			}
			if( beepEnable == 1 )
			{
				if( sysCfg[2] == 0x55aa )  // 55aa：不响，其它值：响
				{
					sysCfg[2] = 0xFFFF;
				}
				else
				{
					sysCfg[2] = 0x55AA;
				}
				
				{ // 改为蜂鸣器
					if( sysCfg[2] == 0x55AA )
					{ // 关闭蜂鸣器
						LCD_ShowString_Language(6,12*10,12,BLUE,"  蜂鸣器已关闭","    Buzzer off");
					}
					else
					{ // 开启蜂鸣器
						LCD_ShowString_Language(6,12*10,12,BLUE,"  蜂鸣器已开启","    Buzzer on ");
					}
				}

			}
			{ // 操作Flash
					FLASH->SR.All = 0;
					Flash_ErasePage((u32)HighDensity_FlashPage(255)); // 擦除Flash
					{
						FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY1;
						FLASH->KEYR.FlashProgramAndEraseControllerKey_W = FLASH_KEY2; // 解锁
						FLASH->CR.Programming_RW = ENABLE;  // 编程使能
							HighDensity_FlashPage(255)->IndexLen16[0] = sysCfg[0];
							while( FLASH->SR.Busy_R == 1 );
							HighDensity_FlashPage(255)->IndexLen16[1] = sysCfg[1];
							while( FLASH->SR.Busy_R == 1 );
							HighDensity_FlashPage(255)->IndexLen16[2] = sysCfg[2];
							while( FLASH->SR.Busy_R == 1 );
						FLASH->CR.Programming_RW = DISABLE; // 编程禁能
						FLASH->CR.LockOnlyCanWriteOne_RW = 1;  // 锁定操作
					}
			}
			if( language == 1 )  // 此时需要复位。
			{
				NVIC_SystemReset();
			}
		}
	}
}

/**
	@level :0-9的数字，分别表示：10%-100%的亮度
**/
void setLEDPWM( u8 level )
{ // 使用的是PA3，TIM5_CH4这个通道。（其中的TIM2被SWIM接口使用了）
	// 注意PA3:mini下载器使用了。但是之前的版本没有使用。
}

