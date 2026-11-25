/**
  * @file  usb_msc.h
  * @brief USB MSC (Mass Storage Class) Bulk-Only Transport + 最小 SCSI 命令集
  *        L2精简: 仅实现 Windows/Linux/macOS 正常读写U盘所需的最少命令
  *
  *   对接层:
  *     底层: usb_hw.c 提供的批量端点回调
  *     上层: vfs.c   提供的扇区级读写 (虚拟 FAT)
  *
  *   暴露给上层 (vfs / msc_user):
  *     USB_MSC_SendData(data, len)  - 发送数据到主机 (DATA_IN 阶段)
  *     USB_MSC_EndTransfer(success) - 结束当前命令, 发送 CSW
  */
#ifndef __USB_MSC_H
#define __USB_MSC_H

#include <stdint.h>
#include "board.h"

#if USB_HAS_MSC

/* MSC 扇区大小 (必须 512, 与 FAT 一致) */
#define MSC_SECTOR_SZ       512u

/* ---- 供上层 (vfs / msc_user) 调用的接口 ---- */

/**
 * @brief  在 DATA_IN 阶段发送数据到主机
 *         可多次调用, 每次最多 64 字节 (全速 USB 包长)
 *         发完最后一包后会自动进入 CSW 阶段
 * @param  data  数据指针
 * @param  len   数据长度 (0 表示结束, 会发 CSW)
 */
void USB_MSC_SendData(const uint8_t *data, uint16_t len);

/**
 * @brief  结束当前 CBW 命令, 发送 CSW 状态包
 * @param  success  1=成功, 0=失败 (会设置 sense key)
 */
void USB_MSC_EndTransfer(uint8_t success);

/**
 * @brief  获取当前命令正在写入的扇区数据 (DATA_OUT 阶段)
 *         由 VFS_WriteSector 的回调使用者读取
 * @return 指向 512 字节扇区缓冲的指针
 */
uint8_t *USB_MSC_GetWriteBuf(void);

/* ---- 描述符初始化 (仅单功能 MSC 模式) ---- */
#if USB_MODE == USB_MODE_MSC
void USB_MSC_InitDesc(void);
#endif

#if USB_MODE == USB_MODE_COMP
/* 复合设备: 由 usb_comp.c 转发底层事件 */
void MSC_CB_Reset(void);
void MSC_CB_BulkOut(const uint8_t *data, uint16_t len);
void MSC_CB_BulkInDone(void);
#endif

/* ---- VFS 层函数约定 (由 vfs.c 实现) ---- */
/*
 * void VFS_ReadSector(uint32_t lba)    - 读一个扇区, 数据写入 USB_MSC_GetWriteBuf()
 * void VFS_WriteSector(uint32_t lba)   - 写一个扇区, 数据来自 USB_MSC_GetWriteBuf()
 * void VFS_WriteDone(void)             - 写操作全部完成 (SYNCHRONIZE CACHE 时)
 * uint32_t VFS_GetTotalSectors(void)   - 返回磁盘总扇区数
 */

#endif /* USB_MODE == USB_MODE_MSC */
#endif /* __USB_MSC_H */
