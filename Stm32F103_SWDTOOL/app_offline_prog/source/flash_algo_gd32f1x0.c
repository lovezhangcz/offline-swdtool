/* 由 tools/flm2blob.py 自动生成, 请勿手改
 * 来源: GD32F1x0_64.FLM
 * 设备: GD32F1x0 64KB  flash=0x08000000+0x10000  page=1024B
 * sectors: 0x0+0x400
 *
 * L1优化: 仅在 TARGET_CHIP == TARGET_CHIP_GD32F1X0 时编译, 节省 Flash
 */
#include "flash_blob.h"

#if TARGET_CHIP == TARGET_CHIP_GD32F1X0

static const uint32_t flash_code[] = {
    0xE00ABE00, 0x062D780D, 0x24084068, 0xD3000040, 0x1E644058, 0x1C49D1FA, 0x2A001E52, 0x4770D1F2,
    0xF36F4939, 0x44490012, 0x48386008, 0x60012100, 0x60414937, 0x60414937, 0x074069C0, 0x4836D408,
    0x5155F245, 0x21066001, 0xF6406041, 0x608171FF, 0x47702000, 0x6901482D, 0x0180F041, 0x20006101,
    0x482A4770, 0xF0416901, 0x61010104, 0xF0416901, 0x61010140, 0x21AAF64A, 0xE0004A27, 0x68C36011,
    0xD1FB07DB, 0xF0216901, 0x61010104, 0x47702000, 0x690A491E, 0x0202F042, 0x6148610A, 0xF0406908,
    0x61080040, 0x20AAF64A, 0xE0004A1B, 0x68CB6010, 0xD1FB07DB, 0xF0206908, 0x61080002, 0x47702000,
    0x1CC9B510, 0x0103F021, 0xE0194B10, 0xF044691C, 0x611C0401, 0x60046814, 0x07E468DC, 0x691CD1FC,
    0x0401F024, 0x68DC611C, 0x0F14F014, 0x68D8D005, 0x0014F040, 0x200160D8, 0x1D00BD10, 0x1D121F09,
    0xD1E32900, 0xBD102000, 0x00000004, 0x40022000, 0x45670123, 0xCDEF89AB, 0x40003000, 0x00000000,
    0x00000000,
};

const flash_algo_t FLASH_ALGO_GD32F1X0 = {
    0x20000021u,  /* Init */
    0x20000055u,  /* UnInit */
    0x20000063u,  /* EraseChip (0xFFFFFFFF=不支持) */
    0x20000091u,  /* EraseSector */
    0x200000C1u,  /* ProgramPage */
    { 0x20000001u, 0x2000011Cu, 0x20000924u },  /* BKPT / static_base(RW) / SP */
    0x20000124u,  /* program_buffer */
    0x20000000u,  /* algo_start */
    sizeof(flash_code),
    flash_code,
    0x00000400u,  /* program_buffer_size = page_size */
    0x08000000u,  /* flash_start */
    0x00010000u,  /* flash_size */
    0x00000400u,  /* sector_size (首扇区) */
    100u,        /* prog_timeout_ms */
    3000u,        /* erase_timeout_ms */
    "GD32F1x0 64KB",
};

#endif /* TARGET_CHIP == TARGET_CHIP_GD32F1X0 */
