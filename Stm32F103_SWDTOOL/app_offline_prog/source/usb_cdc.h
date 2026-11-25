/**
  * @file  usb_cdc.h
  * @brief USB CDC ACM (虚拟串口) 类实现
  */
#ifndef __USB_CDC_H
#define __USB_CDC_H

#include <stdint.h>
#include "board.h"

#if USB_MODE == USB_MODE_CDC
/* 单功能 CDC 时由本类负责初始化 (含底层 USB 初始化) */
void USB_CDC_Init(void);
#endif

/* 主机是否打开了串口 (DTR=1) */
uint8_t  USB_CDC_HostActive(void);

/* 发送数据: 返回实际排队字节数; 返回 0 表示发送忙, 稍后重试 */
uint16_t USB_CDC_Write(const uint8_t *data, uint16_t len);

/* 主循环调用: 处理待发送队列 */
void     USB_CDC_Poll(void);

/* 上层接收回调 (应用由 proto.c 实现, boot 由 boot_proto.c 实现) */
void     CDC_RxBytes(const uint8_t *data, uint16_t len);

/* 主机的 DTR 状态 (SET_CONTROL_LINE_STATE) */
void     USB_CDC_SetDtr(uint8_t dtr);

#if USB_MODE == USB_MODE_COMP
/* 复合设备: 由 usb_comp.c 转发底层事件与类请求 */
void CDC_CB_Reset(void);
void CDC_CB_BulkOut(const uint8_t *data, uint16_t len);
void CDC_CB_BulkInDone(void);
#endif

#endif
