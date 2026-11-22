/**
  * @file  usb_cdc.h
  * @brief USB CDC ACM (虚拟串口) 类实现
  */
#ifndef __USB_CDC_H
#define __USB_CDC_H

#include <stdint.h>

void USB_CDC_Init(void);

/* 主机是否打开了串口 (DTR=1) */
uint8_t  USB_CDC_HostActive(void);

/* 发送数据: 返回实际排队字节数; 返回 0 表示发送忙, 稍后重试 */
uint16_t USB_CDC_Write(const uint8_t *data, uint16_t len);

/* 主循环调用: 处理待发送队列 */
void     USB_CDC_Poll(void);

#endif
