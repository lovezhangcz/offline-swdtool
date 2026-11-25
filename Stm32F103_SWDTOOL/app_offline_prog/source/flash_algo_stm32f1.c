#if !defined(BUILD_BOOT)
/* 由 tools/flm2blob.py 自动生成, 请勿手改
 * 来源: STM32F10x_128.FLM
 * 设备: STM32F10x Med-density Flash  flash=0x08000000+0x20000  page=1024B
 * sectors: 0x0+0x400
 *
 * L1优化: 仅在 TARGET_CHIP == TARGET_CHIP_STM32F1 / TARGET_CHIP_GD32F103 时编译
 *   (GD32F103 的 FMC 寄存器布局与解锁密钥和 STM32F1 相同, 可复用同一 blob)
 */
#include "board.h"          /* TARGET_CHIP 选择 */
#include "flash_blob.h"

#if (TARGET_CHIP == TARGET_CHIP_STM32F1) || (TARGET_CHIP == TARGET_CHIP_GD32F103) || \
    (TARGET_CHIP == TARGET_CHIP_AUTO)

static const uint32_t flash_code[] = {
    0xE00ABE00, 0x062D780D, 0x24084068, 0xD3000040, 0x1E644058, 0x1C49D1FA, 0x2A001E52, 0x4770D1F2,
    0x4603B510, 0x4C442000, 0x48446020, 0x48446060, 0x46206060, 0xF01069C0, 0xD1080F04, 0x5055F245,
    0x60204C40, 0x60602006, 0x70FFF640, 0x200060A0, 0x4601BD10, 0x69004838, 0x0080F040, 0x61104A36,
    0x47702000, 0x69004834, 0x0004F040, 0x61084932, 0x69004608, 0x0040F040, 0xE0036108, 0x20AAF64A,
    0x60084930, 0x68C0482C, 0x0F01F010, 0x482AD1F6, 0xF0206900, 0x49280004, 0x20006108, 0x46014770,
    0x69004825, 0x0002F040, 0x61104A23, 0x61414610, 0xF0406900, 0x61100040, 0xF64AE003, 0x4A2120AA,
    0x481D6010, 0xF01068C0, 0xD1F60F01, 0x6900481A, 0x0002F020, 0x61104A18, 0x47702000, 0x4603B510,
    0xF0201C48, 0xE0220101, 0x69004813, 0x0001F040, 0x61204C11, 0x80188810, 0x480FBF00, 0xF01068C0,
    0xD1FA0F01, 0x6900480C, 0x0001F020, 0x61204C0A, 0x68C04620, 0x0F14F010, 0x4620D006, 0xF04068C0,
    0x60E00014, 0xBD102001, 0x1C921C9B, 0x29001E89, 0x2000D1DA, 0x0000E7F7, 0x40022000, 0x45670123,
    0xCDEF89AB, 0x40003000, 0x00000000,
};

const flash_algo_t FLASH_ALGO_STM32F1 = {
    0x20000021u,  /* Init */
    0x20000053u,  /* UnInit */
    0x20000065u,  /* EraseChip (0xFFFFFFFF=不支持) */
    0x2000009Fu,  /* EraseSector */
    0x200000DDu,  /* ProgramPage */
    { 0x20000001u, 0x20000148u, 0x2000094Cu },  /* BKPT / static_base(RW) / SP */
    0x2000014Cu,  /* program_buffer */
    0x20000000u,  /* algo_start */
    sizeof(flash_code),
    flash_code,
    0x00000400u,  /* program_buffer_size = page_size */
    0x08000000u,  /* flash_start */
    0x00020000u,  /* flash_size */
    0x00000400u,  /* sector_size (首扇区) */
    100u,        /* prog_timeout_ms */
    500u,        /* erase_timeout_ms */
    "STM32F10x Med-density Flash",
};

#endif /* STM32F1 / GD32F103 */

#if (TARGET_CHIP == TARGET_CHIP_GD32F103) || (TARGET_CHIP == TARGET_CHIP_AUTO)
/* GD32F103C8T6: 64KB, 1KB 页; FMC 与 STM32F1 兼容, 但页擦除更慢 */
const flash_algo_t FLASH_ALGO_GD32F103 = {
    0x20000021u,  /* Init */
    0x20000053u,  /* UnInit */
    0x20000065u,  /* EraseChip (0xFFFFFFFF=不支持) */
    0x2000009Fu,  /* EraseSector */
    0x200000DDu,  /* ProgramPage */
    { 0x20000001u, 0x20000148u, 0x2000094Cu },  /* BKPT / static_base(RW) / SP */
    0x2000014Cu,  /* program_buffer */
    0x20000000u,  /* algo_start */
    sizeof(flash_code),
    flash_code,
    0x00000400u,  /* program_buffer_size = page_size */
    0x08000000u,  /* flash_start */
    0x00010000u,  /* flash_size = 64KB (GD32F103C8T6) */
    0x00000400u,  /* sector_size (首扇区) */
    200u,         /* prog_timeout_ms */
    2000u,        /* erase_timeout_ms (GD32 页擦除明显慢于 STM32) */
    "GD32F103C8 64KB Flash",
};
#endif
#endif
