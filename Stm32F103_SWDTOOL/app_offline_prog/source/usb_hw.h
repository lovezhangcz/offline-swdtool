/**
  * @file  usb_hw.h
  * @brief STM32F103 USB FS 设备控制器底层驱动
  *        - 寄存器/PMA 直接操作, 不依赖 ST USB 库
  *        - 端点缓冲表(BTABLE=0) 静态布局
  *        - 上层通过回调接收: 总线复位 / EP0 SETUP / EP0 数据完成 / 批量端点收发
  *
  *   端点分配 (按 USB_MODE 切换):
  *     CDC 模式   : EP0=Ctrl, EP1=IntIn(通知), EP2=BulkOut, EP3=BulkIn
  *     MSC 模式   : EP0=Ctrl, EP1=BulkOut, EP2=BulkIn
  *     复合模式   : EP0=Ctrl, EP1=IntIn(CDC通知), EP2=CDC BulkOut, EP3=CDC BulkIn,
  *                  EP4=MSC BulkOut, EP5=MSC BulkIn
  */
#ifndef __USB_HW_H
#define __USB_HW_H

#include <stdint.h>
#include "board.h"

/* 端点号 */
#define USB_EP0       0u

#if USB_HAS_CDC
  #define USB_CDC_NOTIFY_EP  1u   /* CDC 通知 (interrupt IN, 仅声明不发送) */
  #define USB_CDC_OUT_EP     2u   /* CDC 数据 OUT (bulk) */
  #define USB_CDC_IN_EP      3u   /* CDC 数据 IN  (bulk) */
#endif

#if USB_HAS_MSC
  #if USB_MODE == USB_MODE_MSC
    #define USB_BULK_OUT_EP  1u   /* MSC 数据 OUT (bulk) */
    #define USB_BULK_IN_EP   2u   /* MSC 数据 IN  (bulk) */
  #else /* 复合模式: MSC 用 EP4/EP5 */
    #define USB_BULK_OUT_EP  4u
    #define USB_BULK_IN_EP   5u
  #endif
#endif

/* 状态值 (EPnR STAT_RX/STAT_TX) */
#define USB_STAT_DISABLED   0u
#define USB_STAT_STALL      1u
#define USB_STAT_NAK        2u
#define USB_STAT_VALID      3u

void USB_HW_Init(void);          /* 上电初始化 (不产生总线事件, 枚举由复位中断启动) */

/* 端点配置 */
void USB_EP_Config(uint8_t ep, uint16_t type, uint16_t rx_addr, uint16_t rx_size,
                   uint16_t tx_addr);
void USB_EP_SetRxStatus(uint8_t ep, uint16_t stat);
void USB_EP_SetTxStatus(uint8_t ep, uint16_t stat);
void USB_EP_ClearRxCTR(uint8_t ep);
void USB_EP_ClearTxCTR(uint8_t ep);
void USB_SetAddress(uint8_t addr);

/* PMA 数据读写 (len 字节) */
void USB_PMA_Write(uint16_t pma_off, const uint8_t *buf, uint16_t len);
void USB_PMA_Read (uint16_t pma_off, uint8_t *buf, uint16_t len);

/* 端点缓冲描述符 */
void USB_EP_SetTxCount(uint8_t ep, uint16_t cnt);
uint16_t USB_EP_GetRxCount(uint8_t ep);
uint16_t USB_EP_GetTxAddr(uint8_t ep);
uint16_t USB_EP_GetRxAddr(uint8_t ep);

/* ---- 上层回调 (CDC 由 usb_cdc.c 实现, MSC 由 usb_msc.c 实现, 复合由 usb_comp.c 分发) ---- */
void USB_CB_Reset(void);
void USB_CB_Setup(const uint8_t *pkt);                 /* 8 字节 SETUP 包 */
void USB_CB_EP0_TxDone(void);
void USB_CB_EP0_RxDone(uint16_t len);
void USB_CB_BulkOut(uint8_t ep, const uint8_t *data, uint16_t len);  /* 批量 OUT 数据 */
void USB_CB_BulkInDone(uint8_t ep);                                  /* 批量 IN 发送完成 */

#endif
