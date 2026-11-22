/**
  * @file  swd_host.h
  * @brief SWD (Serial Wire Debug) 主机驱动, GPIO 位带实现
  *        SWCLK = PA3 (输出), SWDIO = PA4 (输入/输出切换)
  */
#ifndef __SWD_HOST_H
#define __SWD_HOST_H

#include <stdint.h>

#define SWD_OK           0u
#define SWD_ERR_WAIT     1u     /* 目标持续 WAIT */
#define SWD_ERR_FAULT    2u     /* 目标返回 FAULT */
#define SWD_ERR_PARITY   3u     /* 读数据奇偶校验错 */
#define SWD_ERR_NORESP   4u     /* 无目标应答 */
#define SWD_ERR_IDCODE   5u     /* DPIDR 不匹配 */
#define SWD_ERR_POWER    6u     /* 调试电源域未就绪 */

#define SWD_RETRY        8u

/* 初始化 SWD 引脚 (空闲状态: SWCLK 低, SWDIO 释放; 若启用 NRST 则拉住复位) */
void    SWD_GPIO_Init(void);
/* 释放 SWD 引脚 (检测目标拔出后降低干扰) */
void    SWD_GPIO_Release(void);
/* 释放目标 NRST (若 board.h 启用 SWD_USE_NRST) */
void    SWD_ReleaseNRST(void);

/* 连接目标: 线复位 + JTAG->SWD 切换 + 读 DPIDR + 上电调试域
   成功返回 SWD_OK 且 *dpidr 为 DP 的 IDR (期望 0x1BA01477) */
uint8_t SWD_Connect(uint32_t *dpidr);

/* DP 寄存器访问: reg = 0x0/0x4/0x8/0xC */
uint8_t SWD_DP_Read (uint8_t reg, uint32_t *val);
uint8_t SWD_DP_Write(uint8_t reg, uint32_t val);

/* AP 寄存器访问 (AP0): reg 含 bank (bit[7:4]), 自动维护 SELECT */
uint8_t SWD_AP_Read (uint8_t reg, uint32_t *val);
uint8_t SWD_AP_Write(uint8_t reg, uint32_t val);

/* AHB-AP 存储器访问 */
uint8_t SWD_Mem_Read32 (uint32_t addr, uint32_t *val);
uint8_t SWD_Mem_Write32(uint32_t addr, uint32_t val);
uint8_t SWD_Mem_Write16(uint32_t addr, uint16_t val);

/* 连续读 (地址自增, 用于校验回读): nwords 个 32 位字 */
uint8_t SWD_Mem_ReadBlock(uint32_t addr, uint32_t *buf, uint32_t nwords);

#endif
