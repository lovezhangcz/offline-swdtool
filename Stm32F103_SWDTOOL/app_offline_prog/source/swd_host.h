/**
  * @file  swd_host.h
  * @brief SWD (Serial Wire Debug) 主机驱动, GPIO 位带实现
  *        SWCLK/SWDIO 的端口与引脚由 board.h 的 BOARD_VARIANT 决定
  *        (原板 PA4/PA3, 新主板 PA8/PB4)
  */
#ifndef __SWD_HOST_H
#define __SWD_HOST_H

#include <stdint.h>
#include "board.h"      /* GPIO_TypeDef 及引脚宏 */

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
/* 调整位延时 (值越大时钟越慢); 默认 SWD_DELAY_LOOP */
void    SWD_SetSpeed(uint32_t loop);
/* 切换 SWCLK 驱动脚 (A 板: PA3 默认 / PB13 备用), 同一时刻只有一个脚输出 */
void    SWD_SetClockPin(GPIO_TypeDef *port, uint16_t pin);
/* 切换 SWDIO 发送/采样脚 (现场排查接反用) */
void    SWD_SetDioPins(GPIO_TypeDef *out_port, uint16_t out_pin,
                       GPIO_TypeDef *in_port, uint16_t in_pin);
/* 引脚自检: 空闲电平 / SWDIO 释放后 1ms 与 100ms 电平 / SWCLK 释放后 1ms 与 100ms 电平 */
void    SWD_PinDiag(uint8_t *idle, uint8_t *io_rel, uint8_t *io_hold,
                    uint8_t *ck_rel, uint8_t *ck_hold);
/* 时钟网络自检: PA3<->PB13 互相驱动/采样是否连通 (A 板) */
void    SWD_ClockDiag(uint8_t *a_drv_b, uint8_t *b_drv_a);
/* 原始监听: 发 DPIDR 读请求后连续采样 32 拍 SWDIO, mode=0 上升沿前/1 上升沿后采样 */
#if DBG_TOOLS
uint32_t SWD_ListenRaw(uint8_t mode);
#endif
/* SWDIO 自回环: bit0/bit1 = 发送脚拉低/拉高时采样脚读到的电平 (跟随正常=2) */
uint8_t  SWD_SelfEcho(void);
#if DBG_TOOLS
/* 示波器时钟测试模式: g_clk_test=1 时主循环持续输出测试方波; burst 输出 ms 毫秒 */
extern volatile uint8_t g_clk_test;
void     SWD_ClkTestBurst(uint32_t ms);
#endif
/* 诊断: 最近一次 SWD 事务读到的 3 位 ACK 原始值 (0/7=无驱动, 1=OK, 2=WAIT, 4=FAULT) */
extern uint8_t g_swd_dbg_ack;
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
/* 连续写 (地址自增, 用于下载烧录算法/数据到目标 RAM) */
uint8_t SWD_Mem_WriteBlock(uint32_t addr, const uint32_t *buf, uint32_t nwords);

#endif
