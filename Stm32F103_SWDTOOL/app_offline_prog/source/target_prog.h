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
