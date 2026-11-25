/**
  * @file  boot.h
  * @brief USB CDC Bootloader 公共定义
  *
  *   复位后先跑 boot:
  *     - 有有效应用且 3 秒内主机没有联系 -> 直接跳转应用
  *     - 应用写 BKP->DR1 = BOOT_FORCE_MAGIC 后软复位 -> 常驻 boot 等待升级
  *     - 无有效应用 -> 常驻 boot
  *     - 连续 BOOT_TRY_MAX 次复位都没等到应用确认启动 -> 常驻 boot (坏应用不会锁死设备)
  *
  *   升级协议为 ASCII 文本行 (详见 boot_proto.c)
  */
#ifndef __BOOT_H
#define __BOOT_H

#include <stdint.h>
#include "board.h"

#define BOOT_VERSION_STR      "1.0.0"
/* 上电等待主机联系的时间 (ms) */
#define BOOT_WAIT_MS          3000u
/* 曾经跳转失败过 (计数非 0) 时, 把等待窗口拉长, 给上位机留出抓取/STAY 的机会 */
#define BOOT_RETRY_WAIT_MS    10000u
/* 应用主动要求进 boot 后, 若主机一直不联系则回退到应用的时间 (ms) */
#define BOOT_FORCED_WAIT_MS   30000u

/* 升级标志页内容 */
typedef struct {
    uint32_t magic;      /* BOOT_FLAG_MAGIC */
    uint32_t len;        /* 应用有效载荷长度 */
    uint32_t crc32;      /* 载荷 CRC32 (与 zlib.crc32 一致) */
    uint32_t flag;       /* BOOT_FLAG_VALID 表示可用 */
} boot_flag_t;

/* ---- 跳转与状态 ---- */
void    Boot_JumpToApp(void);
uint8_t Boot_AppValid(void);          /* 标志有效且整片 CRC32 通过 */
uint8_t Boot_TakeForceFlag(void);     /* 读取并清除 BKP 进 boot 标记 (同时载入重试计数) */
uint8_t Boot_TooManyRetries(void);    /* 连续跳转失败达到 BOOT_TRY_MAX */
void    Boot_ArmRetry(void);          /* 跳转前计数 +1 */
void    Boot_ClearRetry(void);        /* 计数清零 */

/* ---- 升级协议 ---- */
void    Boot_Init(void);
void    Boot_Poll(void);              /* 主循环: 解析命令行/执行命令 */
void    Boot_SendBanner(uint8_t app_ok, uint8_t tries);
uint8_t Boot_HostSeen(void);          /* 主机是否已经发过命令 */
uint8_t Boot_Busy(void);              /* 正在擦/写 */

/* ---- flash 操作 (boot_flash.c) ---- */
uint8_t BootFlash_EraseApp(uint32_t len);
uint8_t BootFlash_Write(uint32_t addr, const uint8_t *data, uint32_t len);
uint32_t BootFlash_Crc32(uint32_t addr, uint32_t len);
uint8_t BootFlag_Read(boot_flag_t *f);
uint8_t BootFlag_Write(const boot_flag_t *f);

#endif /* __BOOT_H */
