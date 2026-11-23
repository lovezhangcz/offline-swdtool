/**
 * @brief 为了简化main函数中代码过长,将部分不必过度关心的函数封装到这里
 */
 
 
#ifndef __FORSAMPLEMAIN_H
#define __FORSAMPLEMAIN_H

#define NO_HID_LED1 GPIOC->ODR.OutputData8_RW
#define HID_LED     GPIOC->ODR.OutputData1_RW

#define LED_ON  (0)
#define LED_OFF (1)

#include "stm32f10x.h"
#include "STM32_USART.h"

/**
 * @brief 定义结构体联合体,其中联合体对齐,方便写函数
 * @data
 */
typedef union {
	
	struct {
	__IO	u8 RecvBuf[30];  // 30帧报文
	};
// 下面为功能码(01-04)
	struct {
	__IO	u8 Reserved0[2];
	__IO	u8 FunctionCode;
	};

// 下面为工作模式(功能码01)
	struct {
	__IO	u8 Reserved1[3];
	__IO	u8 WorkMode;
	__IO	u8 tBS1;
	__IO	u8 tBS2;
	__IO	u8 BRP_H;
	__IO	u8 BRP_L;
	__IO	u8 ForbidAutoRetransfer;
	};
	
// 下面为滤波器设置(功能码02)
	struct {
	__IO	u8 Reserved2[3];
	__IO	u8 FliterIndex;
	__IO  u8 CorrespondingFliterEnable;   // 对应有宏定义
	__IO	u8 FliterID_H;
	__IO	u8 FliterID_MH;
	__IO	u8 FliterID_ML;
	__IO	u8 FliterID_L;
	__IO	u8 FliterMask_H;
	__IO	u8 FliterMask_MH;
	__IO	u8 FliterMask_ML;
	__IO	u8 FliterMask_L;
	};
// 下面为发送设置(功能码03)
	struct {
	__IO	u8 Reserved3[3];
	__IO	u8 TIR_H;
	__IO	u8 TIR_MH;
	__IO	u8 TIR_ML;
	__IO	u8 TIR_L;
	__IO	u8 TDTR_H;
	__IO	u8 TDTR_MH;
	__IO	u8 TDTR_ML;
	__IO	u8 TDTR_L;
	__IO	u8 TDHR_DATA7_H;
	__IO	u8 TDHR_DATA6_MH;
	__IO	u8 TDHR_DATA5_ML;
	__IO	u8 TDHR_DATA4_L;
	__IO	u8 TDLR_DATA3_H;
	__IO	u8 TDLR_DATA2_MH;
	__IO	u8 TDLR_DATA1_ML;
	__IO	u8 TDLR_DATA0_L;
	};
	
// 下面为复位设置(功能码04)
	struct {
	__IO	u8 Reserved4[3];
	};
	
}USART2RecvMsg_Typedef;

/**
 * @brief 上传状态报文
 */
typedef union {

// 为了上传方便
	struct {
	__IO	u8 UpLoadStatusDataBuf[30];
	};
// 下面为功能码
	struct {
	__IO	u8 Reserved0[2];
	__IO	u8 FunctionCode;
	};
	
// 下面为定时发送的状态
	struct {
	__IO	u8 Reserved1[3];
	__IO	u8 FilterEnabledSum;
	__IO	u8 ErrorWarningFlag;
	__IO	u8 ErrorPassiveFlag;
	__IO	u8 OffLineFlag;
	__IO	u8 LastErrorCode;
	__IO	u8 TransferErrorSum;
	__IO	u8 RecvErrorSum;
	__IO	u8 Baudrate_H;
	__IO	u8 Baudrate_M;
	__IO	u8 Baudrate_L;
	__IO	u8 TransferOrderSum_H;
	__IO	u8 TransferOrderSum_MH;
	__IO	u8 TransferOrderSum_ML;
	__IO	u8 TransferOrderSum_L;
	__IO	u8 TransferSucessedSum_H;
	__IO	u8 TransferSucessedSum_MH;
	__IO	u8 TransferSucessedSum_ML;
	__IO	u8 TransferSucessedSum_L;
	__IO	u8 TransferFailedSum_H;
	__IO	u8 TransferFailedSum_MH;
	__IO	u8 TransferFailedSum_ML;
	__IO	u8 TransferFailedSum_L;
	
	__IO	u8 CANWorkMode_25;
	__IO	u8 AutoRetransfer_26;
	};
}ToUpperStatusMsg_TypeDef;


extern ToUpperStatusMsg_TypeDef ToUpperStatusMsg;
extern u8 RunLEDFre;

void InitPeripheral( void ); // 初始基本外设函数
u8 GetSum( u8* DataBuf,u16 Length );

#endif



