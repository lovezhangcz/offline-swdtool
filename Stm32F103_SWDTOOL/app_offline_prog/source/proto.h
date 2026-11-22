/**
  * @file  proto.h
  * @brief 上位机通信协议 (CDC 虚拟串口上的二进制帧)
  *
  *   帧格式:  [0xAA][0x55][CMD][LEN_L][LEN_H][PAYLOAD...][CRC_L][CRC_H]
  *   CRC16-CCITT(init 0xFFFF) 覆盖 CMD..PAYLOAD。
  *   应答: CMD | 0x80, PAYLOAD 首字节为状态码 (0=OK)。
  *
  *   CMD:
  *     0x01 PING       -> "SWDPROG1"
  *     0x02 GET_INFO   -> valid, len, crc32, load_addr, capacity, state, result
  *     0x10 IMG_BEGIN  <- len(4) crc32(4) load_addr(4)   擦除存储区
  *     0x11 IMG_DATA   <- offset(4) data[..<=508]        写镜像数据
  *     0x12 IMG_END    -> 校验 CRC, 生效镜像
  *     0x20 PROG_NOW   -> 手动触发一次烧录
  *     0x21 GET_STATUS -> state, result, err, dpidr, target_id, progress
  *
  *   数据接收在 USB 中断里只做入环, 解析与执行在 Proto_Poll() (主循环)。
  */
#ifndef __PROTO_H
#define __PROTO_H

#include <stdint.h>

/* 运行状态 (proto 与 main 共享) */
typedef struct {
    volatile uint8_t  state;        /* 见 main.c 中 PROG_STATE_xxx */
    volatile uint8_t  last_result;  /* 0=无 1=成功 2=失败 */
    volatile uint32_t last_error;   /* target_prog 错误码 */
    volatile uint32_t target_dpidr; /* 目标 DP IDR */
    volatile uint32_t target_idcode;/* 目标 DBGMCU_IDCODE */
    volatile uint32_t progress;     /* 已烧录字节数 */
    volatile uint32_t swd_err;      /* SWD 层子错误码 (SWD_ERR_xxx, 0=OK) */
} prog_status_t;

extern prog_status_t g_status;

void Proto_Init(void);
void Proto_RxBytes(const uint8_t *data, uint16_t len);   /* USB ISR 上下文 */
void Proto_Poll(void);                                    /* 主循环上下文 */

/* 手动烧录请求: 由 PROG_NOW 置位, main 读取后清零 */
uint8_t Proto_TakeProgRequest(void);

#endif
