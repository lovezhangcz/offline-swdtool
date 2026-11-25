/**
  * @file  storage.h
  * @brief 目标固件镜像在本机内部 flash 中的存储管理
  */
#ifndef __STORAGE_H
#define __STORAGE_H

#include <stdint.h>
#include "board.h"

#define STORAGE_OK        0
#define STORAGE_ERR_ARG   1
#define STORAGE_ERR_FLASH 2
#define STORAGE_ERR_CRC   3

/* 读取镜像头 (直接从 flash 映射, 返回指针, 无需拷贝) */
const img_hdr_t *Storage_Header(void);

/* 镜像是否有效 (magic/长度/地址范围检查) */
uint8_t  Storage_ImageValid(void);

/* 擦除整个存储区 */
uint8_t  Storage_Erase(void);

/* 只擦除 offset 所在的一页 (offset 相对存储区起点, 需页对齐) */
uint8_t  Storage_ErasePage(uint32_t offset);

/* 仅擦除并更新头部所在的第一页 (不影响数据区) */
uint8_t  Storage_UpdateHeader(const img_hdr_t *hdr);

/* 向存储区写数据: offset 相对存储区起点(含头), 半字对齐自动补 0xFF */
uint8_t  Storage_Write(uint32_t offset, const uint8_t *data, uint32_t len);

/* 取镜像载荷起始地址/指针 */
uint32_t        Storage_PayloadAddr(void);
const uint8_t  *Storage_PayloadPtr(void);
void            Storage_SetImageName(const char *name, uint32_t len);

#endif
