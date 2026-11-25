/**
  * @file  usb_comp.h
  * @brief USB 复合设备 (MSC + CDC): 初始化入口
  */
#ifndef __USB_COMP_H
#define __USB_COMP_H

#include "board.h"

#if USB_MODE == USB_MODE_COMP
void USB_Comp_Init(void);
#endif

#endif /* __USB_COMP_H */
