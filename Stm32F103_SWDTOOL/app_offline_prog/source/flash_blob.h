/**
  * @file  flash_blob.h
  * @brief Keil FLM 烧录算法 blob 描述结构
  *        (由 tools/flm2blob.py 从 .FLM 生成对应的 flash_algo_xxx.c)
  */
#ifndef __FLASH_BLOB_H
#define __FLASH_BLOB_H

#include <stdint.h>

/* 调用约定: 函数返回时 PC 跳到 breakpoint(BKPT) 使 CPU 停机,
   static_base 装入 R9 (RW 段基址), stack_pointer 装入 R13      */
typedef struct {
    uint32_t breakpoint;
    uint32_t static_base;
    uint32_t stack_pointer;
} prog_syscall_t;

typedef struct {
    uint32_t init;               /* Init(adr, clock, fnc)     */
    uint32_t uninit;             /* UnInit(fnc)               */
    uint32_t erase_chip;         /* EraseChip(), 0xFFFFFFFF=无 */
    uint32_t erase_sector;       /* EraseSector(adr)          */
    uint32_t program_page;       /* ProgramPage(adr, sz, buf) */
    prog_syscall_t syscall;
    uint32_t program_buffer;     /* 目标 RAM 页缓冲区地址     */
    uint32_t algo_start;         /* blob 下载基址 (0x20000000) */
    uint32_t algo_size;          /* blob 字节数 (含 32B 头)   */
    const uint32_t *algo_blob;
    uint32_t program_buffer_size;/* 页缓冲区大小 = page_size  */
    uint32_t flash_start;        /* 目标 flash 起始 (0x08000000) */
    uint32_t flash_size;
    uint32_t sector_size;        /* 扇区大小 (无 EraseChip 时按扇区擦) */
    uint32_t prog_timeout_ms;
    uint32_t erase_timeout_ms;
    const char *name;
} flash_algo_t;

extern const flash_algo_t FLASH_ALGO_GD32F1X0;   /* GD32F130/150 64KB */
extern const flash_algo_t FLASH_ALGO_STM32F1;    /* STM32F10x 中容量   */
extern const flash_algo_t FLASH_ALGO_GD32F103;   /* GD32F103C8 64KB    */
extern const flash_algo_t FLASH_ALGO_GD32F303;   /* GD32F303 (2KB 扇区)*/

#endif
