/**
  * @file  vfs.h
  * @brief 极简虚拟 FAT12 文件系统 (L2精简)
  *
  *   特性:
  *     - FAT12 格式, 固定磁盘大小 (约等于镜像存储区)
  *     - 2 个文件: README.TXT (只读) + FIRMWARE.BIN (可写)
  *     - 读操作: 动态构造 FAT/目录, 文件数据直接映射
  *     - 写操作: 只接受 FIRMWARE.BIN, 数据写入镜像存储区
  *     - 写入完成后由 SYNCHRONIZE CACHE 触发镜像头更新
  *
  *   磁盘布局:
  *     LBA 0  : 引导扇区 (BPB)
  *     LBA 1  : FAT 表
  *     LBA 2  : 根目录 (16 个目录项)
  *     LBA 3+ : 数据区 (簇 2 开始)
  *       簇 2 (LBA 3) : README.TXT
  *       簇 3..N      : FIRMWARE.BIN
  */
#ifndef __VFS_H
#define __VFS_H

#include <stdint.h>
#include "board.h"

#if USB_MODE == USB_MODE_MSC

/* 初始化 VFS (在 main.c 中调用一次) */
void VFS_Init(void);

#endif /* USB_MODE == USB_MODE_MSC */
#endif /* __VFS_H */
