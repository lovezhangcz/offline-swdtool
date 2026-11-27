/**
  ******************************************************************************
  * @file    gcc_compat.h
  * @brief   让 Keil(ARMCC) 专有语法在 GCC 下可用, 且不改动原有源码
  *
  *  唯一的 ARMCC 专有写法在 main.c:
  *      const app_fw_info_t g_app_fw_info __attribute__((at(APP_FW_INFO_ADDR)))
  *
  *  ARMCC 的 at(addr) 是"绝对地址定位"; GCC 没有该属性, 改用
  *  "自定义段 + 链接脚本固定地址" 实现:
  *      gcc_compat.h  :  at(x) -> section(".app_fw_info")
  *      stm32f103c8_app.ld : .app_fw_info 固定在 ORIGIN(FLASH)+0x100
  *
  *  本头文件由 Makefile 用 -include 强制包含, 不影响 Keil 工程。
  ******************************************************************************
  */
#ifndef __GCC_COMPAT_H
#define __GCC_COMPAT_H

#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
  /* at(addr): 地址参数在链接脚本里体现, 这里只负责把对象放进专用段 */
  #define at(addr)  section(".app_fw_info"), used
#endif

#endif /* __GCC_COMPAT_H */
