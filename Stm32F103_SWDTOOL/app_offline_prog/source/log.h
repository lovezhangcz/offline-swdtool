/**
  * @file  log.h
  * @brief 应用诊断日志环形缓冲 (由上位机通过 LOG_POLL 命令拉取)
  */
#ifndef __LOG_H
#define __LOG_H

#include <stdint.h>

void     Log_Init(void);
void     Log_Str(const char *s);
void     Log_Dec(uint32_t v);       /* 十进制 */
void     Log_Hex(uint32_t v);       /* 8 位十六进制, 无 0x 前缀 */
uint16_t Log_Read(uint8_t *buf, uint16_t max);   /* 取出最多 max 字节 */

#endif
