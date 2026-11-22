/**
  * @file  flash_algo.h
  * @brief 在目标 RAM 中执行 Keil FLM 烧录算法
  */
#ifndef __FLASH_ALGO_H
#define __FLASH_ALGO_H

#include <stdint.h>
#include "flash_blob.h"

#define FA_OK            0u
#define FA_ERR_SWD       1u   /* SWD 通信失败   */
#define FA_ERR_TIMEOUT   2u   /* 等待停机超时   */
#define FA_ERR_RETVAL    3u   /* 算法返回非 0   */

/* 把算法 blob 下载到目标 RAM (目标须已停机) */
uint8_t  FlashAlgo_Load(const flash_algo_t *algo);

/* 调用算法函数, 返回 FA_xxx; *ret 带出 R0 (0=成功)
   timeout_ms 为等待停机的最长时间                         */
uint8_t  FlashAlgo_Call(const flash_algo_t *algo, uint32_t entry,
                        uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
                        uint32_t timeout_ms, uint32_t *ret);

#endif
