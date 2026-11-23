/**
  * @file  vfs.c
  * @brief 极简虚拟 FAT12 文件系统 (L2精简)
  *
  *   设计要点:
  *     - FAT12, 1 个 FAT 表, 16 个根目录项, 1 扇区/簇
  *     - 2 个固定文件: README.TXT (只读) + FIRMWARE.BIN (可写)
  *     - FAT/目录 动态构造 (不占 RAM, 直接在扇区缓冲里拼)
  *     - 写入 FAT/目录 被接受但忽略 (下次读取动态构造的版本)
  *     - FIRMWARE.BIN 数据直接读写镜像存储区
  *
  *   磁盘布局:
  *     LBA 0   : BPB 引导扇区
  *     LBA 1   : FAT 表
  *     LBA 2   : 根目录
  *     LBA 3   : 数据区, 簇 2 = README.TXT
  *     LBA 4..N: 数据区, 簇 3.. = FIRMWARE.BIN
  */
#include "vfs.h"
#include "usb_msc.h"
#include "storage.h"
#include "crc.h"

#if USB_MODE == USB_MODE_MSC

#include <string.h>

/* ---- 磁盘参数 ---- */

#define VFS_SECTOR_SIZE       512u
#define VFS_RESERVED_SECTORS  1u
#define VFS_FAT_COUNT         1u
#define VFS_ROOT_ENTRIES      16u     /* 16 * 32 = 512B = 1 扇区 */
#define VFS_SECTORS_PER_CLUSTER  1u

/* 总扇区数 = 镜像区大小 / 512 + 3 (保留+FAT+根目录) */
#define VFS_DATA_CLUSTERS     (IMG_STORE_SIZE / VFS_SECTOR_SIZE - 1)  /* 留 1 簇给 README */
#define VFS_TOTAL_SECTORS     (VFS_RESERVED_SECTORS + VFS_FAT_COUNT + 1 + VFS_DATA_CLUSTERS)

/* 数据区起始 LBA = 保留(1) + FAT(1) + 根目录(1) = 3 */
#define VFS_DATA_START_LBA    3u
#define VFS_FIRST_CLUSTER     2u     /* FAT12 中簇号从 2 开始 */

/* 文件簇号 */
#define CLUSTER_README        2u
#define CLUSTER_FIRMWARE      3u

/* ---- 静态数据 (Flash) ---- */

/* README.TXT 内容 */
static const char readme_text[] =
    "SWD Offline Programmer\r\n"
    "======================\r\n"
    "\r\n"
    "Usage:\r\n"
    "  1. Copy firmware.bin to this drive\r\n"
    "  2. Wait for the LED to stop blinking\r\n"
    "  3. Connect target board, programming starts automatically\r\n"
    "\r\n"
    "Supported targets: GD32F130/F150 (default)\r\n"
    "Max firmware size: ~31KB\r\n"
    "\r\n";

/* BPB (BIOS Parameter Block) - FAT12 */
static const uint8_t bpb_data[] = {
    0xEB, 0x3C, 0x90,       /* BS_jmpBoot */
    'M','S','D','O','S','5','.','0', /* BS_OEMName */
    0x00, 0x02,             /* BPB_BytsPerSec = 512 (小端) */
    0x01,                   /* BPB_SecPerClus = 1 */
    0x01, 0x00,             /* BPB_RsvdSecCnt = 1 */
    0x01,                   /* BPB_NumFATs = 1 */
    0x10, 0x00,             /* BPB_RootEntCnt = 16 */
    0x00, 0x00,             /* BPB_TotSec16 (运行时填充) */
    0xF8,                   /* BPB_Media = 0xF8 (硬盘) */
    0x01, 0x00,             /* BPB_FATSz16 = 1 扇区 */
    0x3F, 0x00,             /* BPB_SecPerTrk = 63 */
    0xFF, 0x00,             /* BPB_NumHeads = 255 */
    0x00, 0x00, 0x00, 0x00, /* BPB_HiddSec = 0 */
    0x00, 0x00, 0x00, 0x00, /* BPB_TotSec32 = 0 */
    0x00,                   /* BS_DrvNum = 0 */
    0x00,                   /* BS_Reserved1 */
    0x29,                   /* BS_BootSig = 0x29 */
    0x12, 0x34, 0x56, 0x78, /* BS_VolID */
    /* 卷标 11 字节 (运行时填充) */
    'S','W','D','P','R','O','G',' ',' ',' ',' ',
    /*  FS Type 8 字节 */
    'F','A','T','1','2',' ',' ',' ',
};

/* ---- 写入状态 ---- */

static uint8_t  g_write_started;   /* 是否已开始写入 (首次写入时擦除) */
static uint32_t g_write_bytes;     /* 已写入的固件字节数 */

/* ================================================================
 *                       辅助: 目录项构造
 * ================================================================ */

/* 构造一个 32 字节的 FAT 目录项 */
static void make_dir_entry(uint8_t *entry,
                           const char *name8, const char *ext3,
                           uint8_t attr, uint16_t time, uint16_t date,
                           uint16_t start_cluster, uint32_t size)
{
    uint8_t i;
    /* 8.3 文件名 (空格补齐) */
    for (i = 0; i < 8; i++) {
        entry[i] = (name8[i] != 0) ? name8[i] : ' ';
    }
    for (i = 0; i < 3; i++) {
        entry[8 + i] = (ext3[i] != 0) ? ext3[i] : ' ';
    }
    entry[11] = attr;                 /* 属性 */
    entry[12] = 0;                    /* 保留 */
    entry[13] = 0;                    /* 创建时间 10ms */
    entry[14] = (uint8_t)(time & 0xFF);
    entry[15] = (uint8_t)(time >> 8);
    entry[16] = (uint8_t)(date & 0xFF);
    entry[17] = (uint8_t)(date >> 8);
    entry[18] = (uint8_t)(date & 0xFF);  /* 访问日期 (随便填) */
    entry[19] = (uint8_t)(date >> 8);
    entry[20] = (uint8_t)((start_cluster >> 16) & 0xFF);  /* 首簇高 16 位 (FAT12 用不到) */
    entry[21] = (uint8_t)((start_cluster >> 16) >> 8);
    entry[22] = (uint8_t)(time & 0xFF);  /* 写入时间 */
    entry[23] = (uint8_t)(time >> 8);
    entry[24] = (uint8_t)(date & 0xFF);  /* 写入日期 */
    entry[25] = (uint8_t)(date >> 8);
    entry[26] = (uint8_t)(start_cluster & 0xFF);
    entry[27] = (uint8_t)(start_cluster >> 8);
    entry[28] = (uint8_t)(size & 0xFF);
    entry[29] = (uint8_t)((size >> 8) & 0xFF);
    entry[30] = (uint8_t)((size >> 16) & 0xFF);
    entry[31] = (uint8_t)((size >> 24) & 0xFF);
}

/* ================================================================
 *                         读扇区
 * ================================================================ */

static void read_bpb(uint8_t *buf)
{
    memcpy(buf, bpb_data, sizeof(bpb_data));
    /* 填充总扇区数 */
    uint16_t total = VFS_TOTAL_SECTORS;
    buf[19] = (uint8_t)(total & 0xFF);
    buf[20] = (uint8_t)(total >> 8);
    /* 扇区末尾签名 */
    buf[510] = 0x55;
    buf[511] = 0xAA;
}

static void read_fat(uint8_t *buf)
{
    uint32_t fw_size;
    uint16_t fw_clusters;
    uint16_t i;

    memset(buf, 0, VFS_SECTOR_SIZE);

    /* FAT[0] = 0xFF8 (媒体描述符), FAT[1] = 0xFFF (保留) */
    buf[0] = 0xF8;        /* FAT[0] 低 8 位 */
    buf[1] = 0xFF;        /* FAT[0] 高 4 位 + FAT[1] 低 4 位 */
    buf[2] = 0x0F;        /* FAT[1] 高 8 位 */

    /* README.TXT: 簇 2 = 0xFFF (单簇文件, 链结束) */
    /* FAT 中第 2 项 (字节 3-4): 12 位 */
    buf[3] = 0xFF;
    buf[4] = 0x0F;        /* 低 4 位是 FAT[3] 的高 4 位 */

    /* FIRMWARE.BIN: 从簇 3 开始的连续链 */
    fw_size = Storage_ImageValid() ? Storage_Header()->length : 0;
    /* 向上取整到簇 */
    fw_clusters = (uint16_t)((fw_size + VFS_SECTOR_SIZE - 1) / VFS_SECTOR_SIZE);
    if (fw_clusters == 0 && fw_size > 0) fw_clusters = 1;
    if (fw_clusters > VFS_DATA_CLUSTERS - 1) fw_clusters = (uint16_t)(VFS_DATA_CLUSTERS - 1);

    /* 构造连续链: 3->4->5->...->(3+N-1)->0xFFF */
    for (i = 0; i < fw_clusters; i++) {
        uint16_t cluster = (uint16_t)(CLUSTER_FIRMWARE + i);
        uint16_t next;
        if (i == fw_clusters - 1) {
            next = 0x0FFF;  /* EOF */
        } else {
            next = (uint16_t)(cluster + 1);
        }
        /* FAT[cluster] 位置: 1.5 字节/项 */
        uint16_t byte_off = cluster + (cluster >> 1);
        if (cluster & 1) {
            /* 奇簇: 跨越 2 字节, 高 4 位在前一字节, 低 8+4=12 位在后两字节...
             * 不对, FAT12 布局:
             *   偶簇 N: 字节 N*1.5 = N + N/2
             *     低 8 位在 byte_off, 高 4 位在 byte_off+1 的低 4 位
             *   奇簇 N:
             *     低 4 位在 byte_off 的高 4 位, 高 8 位在 byte_off+1
             */
            buf[byte_off]     = (uint8_t)((buf[byte_off] & 0x0F) | ((next & 0x0F) << 4));
            buf[byte_off + 1] = (uint8_t)((next >> 4) & 0xFF);
        } else {
            buf[byte_off]     = (uint8_t)(next & 0xFF);
            buf[byte_off + 1] = (uint8_t)((buf[byte_off + 1] & 0xF0) | ((next >> 8) & 0x0F));
        }
    }
}

static void read_rootdir(uint8_t *buf)
{
    uint32_t fw_size;

    memset(buf, 0, VFS_SECTOR_SIZE);

    /* 项 0: README.TXT (只读) */
    make_dir_entry(&buf[0],
                   "README", "TXT",
                   0x01,    /* 只读 */
                   0x4B00u, /* 时间 9:30:00 */
                   0x5421u, /* 日期 2024/1/1 */
                   CLUSTER_README,
                   (uint32_t)sizeof(readme_text) - 1); /* 不含末尾 \0 */

    /* 项 1: FIRMWARE.BIN (存档) */
    fw_size = Storage_ImageValid() ? Storage_Header()->length : 0;
    make_dir_entry(&buf[32],
                   "FIRMWARE", "BIN",
                   0x20,    /* 存档 */
                   0x4B00u,
                   0x5421u,
                   CLUSTER_FIRMWARE,
                   fw_size);

    /* 项 2-15: 空 (首字节 0x00 = 未使用, 且目录项结束) */
    /* memset 已经清零了 */
    buf[64] = 0x00;  /* 第三个目录项的第一个字节 = 0 表示目录结束 */
}

static void read_data_cluster(uint8_t *buf, uint16_t cluster)
{
    memset(buf, 0, VFS_SECTOR_SIZE);

    if (cluster == CLUSTER_README) {
        /* README.TXT */
        uint32_t len = (uint32_t)sizeof(readme_text) - 1;
        if (len > VFS_SECTOR_SIZE) len = VFS_SECTOR_SIZE;
        memcpy(buf, readme_text, len);
        return;
    }

    if (cluster >= CLUSTER_FIRMWARE &&
        cluster < CLUSTER_FIRMWARE + VFS_DATA_CLUSTERS - 1) {
        /* FIRMWARE.BIN: 从镜像存储区读取 */
        uint32_t offset = (uint32_t)(cluster - CLUSTER_FIRMWARE) * VFS_SECTOR_SIZE;
        const uint8_t *payload = Storage_PayloadPtr();
        uint32_t fw_size = Storage_ImageValid() ? Storage_Header()->length : 0;

        if (offset < fw_size) {
            uint32_t copy_len = fw_size - offset;
            if (copy_len > VFS_SECTOR_SIZE) copy_len = VFS_SECTOR_SIZE;
            memcpy(buf, payload + offset, copy_len);
        }
        return;
    }

    /* 其他簇: 返回 0 (空闲) */
}

void VFS_ReadSector(uint32_t lba)
{
    uint8_t *buf = USB_MSC_GetWriteBuf();

    if (lba == 0) {
        read_bpb(buf);
    } else if (lba == 1) {
        read_fat(buf);
    } else if (lba == 2) {
        read_rootdir(buf);
    } else if (lba >= VFS_DATA_START_LBA && lba < VFS_TOTAL_SECTORS) {
        uint16_t cluster = (uint16_t)(lba - VFS_DATA_START_LBA + VFS_FIRST_CLUSTER);
        read_data_cluster(buf, cluster);
    } else {
        memset(buf, 0, VFS_SECTOR_SIZE);
    }
}

/* ================================================================
 *                         写扇区
 * ================================================================ */

void VFS_WriteSector(uint32_t lba)
{
    const uint8_t *buf = USB_MSC_GetWriteBuf();

    /* 忽略 FAT/目录/BPB 的写入 (下次读还是动态构造的) */
    if (lba < VFS_DATA_START_LBA) {
        return;
    }

    if (lba >= VFS_TOTAL_SECTORS) {
        return;
    }

    uint16_t cluster = (uint16_t)(lba - VFS_DATA_START_LBA + VFS_FIRST_CLUSTER);

    /* 忽略 README.TXT 的写入 (只读) */
    if (cluster == CLUSTER_README) {
        return;
    }

    /* 只处理 FIRMWARE.BIN 的数据簇 */
    if (cluster >= CLUSTER_FIRMWARE &&
        cluster < CLUSTER_FIRMWARE + VFS_DATA_CLUSTERS - 1) {

        /* 首次写入: 擦除整个存储区 */
        if (!g_write_started) {
            Storage_Erase();
            g_write_started = 1;
            g_write_bytes = 0;
        }

        /* 计算在镜像存储区中的偏移 (含头) */
        uint32_t payload_offset = (uint32_t)(cluster - CLUSTER_FIRMWARE) * VFS_SECTOR_SIZE;
        uint32_t store_offset = sizeof(img_hdr_t) + payload_offset;

        /* 写入数据 */
        Storage_Write(store_offset, buf, VFS_SECTOR_SIZE);

        /* 更新写入字节计数 */
        if (g_write_bytes < payload_offset + VFS_SECTOR_SIZE) {
            g_write_bytes = payload_offset + VFS_SECTOR_SIZE;
        }
    }
}

/* ================================================================
 *                     写入完成 (SYNCHRONIZE CACHE)
 * ================================================================ */

void VFS_WriteDone(void)
{
    if (!g_write_started) return;

    /* 计算实际写入数据的大小 (向下取整到最后一个非全空扇区...
     * 简化: 直接用 g_write_bytes, 但末尾的 0xFF 填充也算)
     * 更精确的做法是从后往前找第一个非 0xFF 字节, 但太费时间 */

    /* 直接用已写入的字节数作为镜像长度
     * 注意: 最后一个扇区可能有多余的 0xFF 填充, 但这不影响烧录 */
    uint32_t actual_len = g_write_bytes;

    if (actual_len > 0) {
        const uint8_t *payload = Storage_PayloadPtr();
        /* 计算 CRC32 */
        uint32_t crc = CRC32_Update(0xFFFFFFFFu, payload, actual_len) ^ 0xFFFFFFFFu;

        /* 写镜像头 (前 32 字节) */
        img_hdr_t hdr;
        hdr.magic     = IMG_MAGIC;
        hdr.length    = actual_len;
        hdr.crc32     = crc;
        hdr.load_addr = 0x08000000u;
        hdr.prog_count = 0;
        hdr.reserved[0] = 0xFFFFFFFFu;
        hdr.reserved[1] = 0xFFFFFFFFu;
        hdr.reserved[2] = 0xFFFFFFFFu;

        Storage_Write(0, (const uint8_t *)&hdr, sizeof(hdr));
    }

    g_write_started = 0;
    g_write_bytes = 0;
}

/* ================================================================
 *                          总扇区数
 * ================================================================ */

uint32_t VFS_GetTotalSectors(void)
{
    return VFS_TOTAL_SECTORS;
}

/* ================================================================
 *                          初始化
 * ================================================================ */

void VFS_Init(void)
{
    g_write_started = 0;
    g_write_bytes = 0;
}

#endif /* USB_MODE == USB_MODE_MSC */
