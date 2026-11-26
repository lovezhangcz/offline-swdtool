/**
  * @file  target_prog.h
  * @brief 通过 SWD 对目标芯片进行离线烧录
  *
  *   流程: SWD 连接 -> 停机 -> 下载 Keil FLM 烧录算法到目标 RAM ->
  *         Init -> 全片擦除 -> 按页 ProgramPage -> CRC32 回读校验 -> 复位目标
  *
  *   说明:
  *   - 目标 DBGMCU 的 IWDG/WWDG 调试冻结位会被置位, 防止烧录过程被看门狗打断
  *   - 烧录算法 (tools/flm2blob.py 从 Keil .FLM 生成) 在目标 RAM 中执行,
  *     按 TARGET_CHIP 选择 GD32F1x0 / STM32F1 算法
  */
#ifndef __TARGET_PROG_H
#define __TARGET_PROG_H

#include <stdint.h>

#define TP_OK              0u
#define TP_ERR_CONNECT     1u    /* SWD 连接失败/无目标 */
#define TP_ERR_IDCODE      2u    /* 目标身份不符 */
#define TP_ERR_HALT        3u
#define TP_ERR_UNLOCK      4u    /* FPEC 解锁失败 */
#define TP_ERR_PROTECTED   5u    /* 目标有读保护/写保护 */
#define TP_ERR_ERASE       6u
#define TP_ERR_WRITE       7u
#define TP_ERR_VERIFY      8u    /* CRC 校验不一致 */
#define TP_ERR_NOIMAGE     9u
#define TP_ERR_ALGO        10u   /* 烧录算法执行失败 */
#define TP_ERR_SIZE        11u   /* 超出目标 Flash 容量 */
#define TP_ERR_PARAM       12u   /* 参数错误 */
#define TP_ERR_HEX         13u   /* 镜像是 HEX 文本, 不是 bin */
#define TP_ERR_SIZE        11u   /* 超出目标 Flash 容量 */
#define TP_ERR_PARAM       12u   /* 参数错误 (长度/偏移) */

/* 烧录状态 (main.c / target_prog.c / proto.c 共享) */
typedef struct {
    volatile uint8_t  state;
    volatile uint8_t  last_result;  /* 0=无 1=成功 2=失败 */
    volatile uint8_t  host_online;  /* 上位机在线: 3 秒内收到过命令 */
    volatile uint32_t last_error;
    volatile uint32_t target_dpidr;
    volatile uint32_t target_idcode;
    volatile uint32_t progress;     /* 已烧录字节数 */
    volatile uint32_t swd_err;
    volatile uint32_t burn_seq;     /* 每次烧录尝试 +1 (离线/流式都算), 上位机据此发现新结果 */
} prog_status_t;

extern prog_status_t g_status;

/* 最近一次 TargetStream_Begin 读到的目标 Flash 容量 (KB, 0=未知) */
extern uint32_t g_target_flash_kb;

/* ---------------- 流式烧录 (镜像不落本机 Flash) ----------------
 * 上位机分包下发数据, 设备边收边写目标 Flash, 因此镜像大小只受
 * 目标芯片 Flash 容量限制, 不受本机 16KB 镜像区限制。
 * 用法: Begin -> Data(可直接多次, offset 必须连续) -> End
 */
uint8_t TargetStream_Begin(uint32_t total_len, uint32_t crc32,
                           uint32_t load_addr, uint32_t *err_addr);
uint8_t TargetStream_Data(uint32_t offset, const uint8_t *data,
                          uint16_t len, uint32_t *err_addr);
uint8_t TargetStream_End(uint32_t *err_addr);

/**
  * 整片擦除目标芯片 (全片擦除 + 首末字空白校验), 结束后复位运行
  * @return TP_xxx, 目标 Flash 容量见 g_target_flash_kb
  */
uint8_t TargetEraseChip(void);

/**
  * 执行一次完整烧录
  * @param[out] err_addr  出错时的相关地址 (可选 NULL)
  * @return TP_xxx
  */
uint8_t TargetProg_Run(uint32_t *err_addr);

/**
  * 仅探测目标: SWD 连接并读取 DBGMCU_IDCODE
  * @return TP_OK 表示目标在线
  */
uint8_t TargetProg_Detect(void);

/**
  * 诊断探测: 只做 SWD 连接 + 读 ID, 不烧录, 结束后释放引脚
  * @return SWD 层错误码 (SWD_OK/SWD_ERR_xxx), DPIDR/IDCODE 由参数带出
  */
uint8_t TargetProg_Probe(uint32_t *dpidr, uint32_t *idcode);

#endif
