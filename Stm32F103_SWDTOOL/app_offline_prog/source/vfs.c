/**
  * @file  vfs.c
  * @brief 极简虚拟 FAT12 文件系统 (L2精简, 单文件模式)
  *
  *   设计要点:
  *     - FAT12, 1 个 FAT 表, 16 个根目录项, 1 扇区/簇
  *     - 单文件模式: 只保存一个 .bin 固件文件
  *     - 不假设固定簇映射: Windows 把文件放哪个簇, 我们就记哪个簇
  *     - 写入时: 数据按簇号原样存到 Flash, 从根目录提取文件名/大小/首簇
  *     - 读回时: 用保存的首簇和大小构造 FAT 链, 数据从原簇号位置读出
  *     - 支持最长 20 字节文件名 (通过 LFN 长文件名项显示)
  *     - 只识别 .bin 扩展名文件, 多个 bin 取第一个
  *
  *   存储布局 (内部 Flash):
  *     第 0 页 (1KB):
  *       偏移  0-31 : 镜像头 (img_hdr_t, 32字节)
  *       偏移 32-51 : 文件名 (20字节)
  *       偏移 52-53 : 首簇号 (uint16_t, 小端)
  *     第 1 页起:      数据区 (按磁盘簇号存放, 与虚拟磁盘布局一致)
  *
  *   磁盘布局 (虚拟 FAT12):
  *     LBA 0   : BPB 引导扇区
  *     LBA 1   : FAT 表
  *     LBA 2   : 根目录
  *     LBA 3..N: 数据区 (簇 2, 3, 4, ...)
  */
#include "vfs.h"
#include "usb_msc.h"
#include "storage.h"
#include "crc.h"
#include "board.h"

#if USB_MODE == USB_MODE_MSC

#include <string.h>

/* ---- 磁盘参数 ---- */

#define VFS_SECTOR_SIZE       512u
#define VFS_RESERVED_SECTORS  1u
#define VFS_FAT_COUNT         1u
#define VFS_ROOT_ENTRIES      16u
#define VFS_SECTORS_PER_CLUSTER  1u

/* 数据簇总数 = 存储区数据部分 / 每簇大小
 * 存储区数据部分 = 总存储区 - 第0页(头) */
#define VFS_DATA_CLUSTERS     ((IMG_STORE_SIZE - FLASH_PAGE_SIZE) / VFS_SECTOR_SIZE)
#define VFS_TOTAL_SECTORS     (VFS_RESERVED_SECTORS + VFS_FAT_COUNT + 1 + VFS_DATA_CLUSTERS)

#define VFS_DATA_START_LBA    3u
#define VFS_FIRST_CLUSTER     2u

/* 数据在存储区中的物理起始偏移 (第 0 页给头) */
#define VFS_DATA_OFFSET       FLASH_PAGE_SIZE

/* 文件名存在第 0 页偏移 32 处, 最长 20 字节 (ASCII) */
#define FW_NAME_OFFSET        32u
#define FW_NAME_LEN           20u

/* 首簇号存在第 0 页偏移 52 处 (uint16_t, 小端) */
#define FW_FIRST_CLUSTER_OFFSET  52u

/* ---- 静态数据 ---- */

static const uint8_t bpb_data[] = {
    0xEB, 0x3C, 0x90,
    'M','S','D','O','S','5','.','0',
    0x00, 0x02,
    0x01,
    0x01, 0x00,
    0x01,
    0x10, 0x00,
    0x00, 0x00,
    0xF8,
    0x01, 0x00,
    0x3F, 0x00,
    0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00,
    0x00,
    0x29,
    0x12, 0x34, 0x56, 0x78,
    'S','W','D','P','R','O','G',' ',' ',' ',' ',
    'F','A','T','1','2',' ',' ',' ',
};

/* 数据区页数 (1 页 = 1KB = 2 个 512B 簇) */
#define VFS_DATA_PAGES        ((VFS_DATA_CLUSTERS + 1u) / 2u)

/* ---- 写入状态 ---- */

static uint8_t  g_write_started;
static uint8_t  g_header_written;
static uint32_t g_write_bytes;             /* 本轮传输已写入的字节数 (按扇区对齐) */
static uint8_t  g_pages_erased[(VFS_DATA_PAGES + 7u) / 8u];  /* 本轮传输已擦除的数据页位图 */
static uint8_t  g_lba_valid;               /* g_last_lba 是否有效 */
static uint32_t g_last_lba;                /* 本轮传输最后写入的数据扇区号 */
static uint8_t  g_flushed;                 /* 本轮写入已被 finalize 收尾 */
static uint16_t g_burst_first_cluster;     /* 本轮传输中最早写入的数据簇 */
static uint8_t  g_fw_name[FW_NAME_LEN];   /* 固件文件名 (ASCII) */
static uint32_t g_fw_size;                /* 固件实际大小 (从根目录项读取) */
static uint16_t g_fw_first_cluster;       /* 固件首簇号 (从根目录项读取) */
static uint8_t  g_fw_found;               /* 是否找到了 .bin 文件 */

/* ================================================================
 *                       辅助: 目录项构造
 * ================================================================ */

static void make_dir_entry(uint8_t *entry,
                           const uint8_t *name11,
                           uint8_t attr, uint16_t time, uint16_t date,
                           uint16_t start_cluster, uint32_t size)
{
    uint8_t i;
    for (i = 0; i < 11; i++) {
        entry[i] = name11[i];
    }
    entry[11] = attr;
    entry[12] = 0;
    entry[13] = 0;
    entry[14] = (uint8_t)(time & 0xFF);
    entry[15] = (uint8_t)(time >> 8);
    entry[16] = (uint8_t)(date & 0xFF);
    entry[17] = (uint8_t)(date >> 8);
    entry[18] = (uint8_t)(date & 0xFF);
    entry[19] = (uint8_t)(date >> 8);
    entry[20] = 0;
    entry[21] = 0;
    entry[22] = (uint8_t)(time & 0xFF);
    entry[23] = (uint8_t)(time >> 8);
    entry[24] = (uint8_t)(date & 0xFF);
    entry[25] = (uint8_t)(date >> 8);
    entry[26] = (uint8_t)(start_cluster & 0xFF);
    entry[27] = (uint8_t)(start_cluster >> 8);
    entry[28] = (uint8_t)(size & 0xFF);
    entry[29] = (uint8_t)((size >> 8) & 0xFF);
    entry[30] = (uint8_t)((size >> 16) & 0xFF);
    entry[31] = (uint8_t)((size >> 24) & 0xFF);
}

/* ================================================================
 *                     LFN 长文件名辅助
 * ================================================================ */

static uint8_t lfn_checksum(const uint8_t *short_name)
{
    uint8_t sum = 0;
    uint8_t i;
    for (i = 0; i < 11; i++) {
        sum = (uint8_t)(((sum & 1) << 7) + (sum >> 1) + short_name[i]);
    }
    return sum;
}

static void make_short_name(const uint8_t *name, uint8_t name_len, uint8_t *short_name)
{
    uint8_t base[8];
    uint8_t ext[3];
    uint8_t base_len = 0;
    uint8_t ext_len = 0;
    uint8_t i, j;
    int8_t dot_pos = -1;

    for (i = 0; i < name_len; i++) {
        if (name[i] == '.') dot_pos = (int8_t)i;
    }

    uint8_t base_src_len = (dot_pos >= 0) ? (uint8_t)dot_pos : name_len;
    if (base_src_len > 6) base_src_len = 6;
    for (i = 0; i < base_src_len; i++) {
        uint8_t c = name[i];
        if (c >= 'a' && c <= 'z') c = (uint8_t)(c - 32);
        base[i] = c;
    }
    base[i++] = '~';
    base[i++] = '1';
    base_len = i;
    while (base_len < 8) base[base_len++] = ' ';

    if (dot_pos >= 0 && dot_pos < name_len - 1) {
        uint8_t ext_src_start = (uint8_t)(dot_pos + 1);
        ext_len = (uint8_t)(name_len - ext_src_start);
        if (ext_len > 3) ext_len = 3;
        for (j = 0; j < ext_len; j++) {
            uint8_t c = name[ext_src_start + j];
            if (c >= 'a' && c <= 'z') c = (uint8_t)(c - 32);
            ext[j] = c;
        }
    }
    while (ext_len < 3) ext[ext_len++] = ' ';

    for (i = 0; i < 8; i++) short_name[i] = base[i];
    for (i = 0; i < 3; i++) short_name[8 + i] = ext[i];
}

static void make_lfn_entry(uint8_t *entry, uint8_t seq,
                           const uint16_t *chars, uint8_t n_chars,
                           uint8_t checksum)
{
    uint8_t i;
    uint16_t buf[13];

    for (i = 0; i < 13; i++) {
        if (i < n_chars) buf[i] = chars[i];
        else if (i == n_chars) buf[i] = 0x0000;
        else buf[i] = 0xFFFF;
    }

    memset(entry, 0, 32);
    entry[0] = seq;
    entry[1]  = (uint8_t)(buf[0] & 0xFF);
    entry[2]  = (uint8_t)(buf[0] >> 8);
    entry[3]  = (uint8_t)(buf[1] & 0xFF);
    entry[4]  = (uint8_t)(buf[1] >> 8);
    entry[5]  = (uint8_t)(buf[2] & 0xFF);
    entry[6]  = (uint8_t)(buf[2] >> 8);
    entry[7]  = (uint8_t)(buf[3] & 0xFF);
    entry[8]  = (uint8_t)(buf[3] >> 8);
    entry[9]  = (uint8_t)(buf[4] & 0xFF);
    entry[10] = (uint8_t)(buf[4] >> 8);
    entry[11] = 0x0F;
    entry[12] = 0x00;
    entry[13] = checksum;
    entry[14] = (uint8_t)(buf[5] & 0xFF);
    entry[15] = (uint8_t)(buf[5] >> 8);
    entry[16] = (uint8_t)(buf[6] & 0xFF);
    entry[17] = (uint8_t)(buf[6] >> 8);
    entry[18] = (uint8_t)(buf[7] & 0xFF);
    entry[19] = (uint8_t)(buf[7] >> 8);
    entry[20] = (uint8_t)(buf[8] & 0xFF);
    entry[21] = (uint8_t)(buf[8] >> 8);
    entry[22] = (uint8_t)(buf[9] & 0xFF);
    entry[23] = (uint8_t)(buf[9] >> 8);
    entry[24] = (uint8_t)(buf[10] & 0xFF);
    entry[25] = (uint8_t)(buf[10] >> 8);
    entry[26] = 0x00;
    entry[27] = 0x00;
    entry[28] = (uint8_t)(buf[11] & 0xFF);
    entry[29] = (uint8_t)(buf[11] >> 8);
    entry[30] = (uint8_t)(buf[12] & 0xFF);
    entry[31] = (uint8_t)(buf[12] >> 8);
}

static uint8_t build_lfn_entries(uint8_t *buf, const uint8_t *name, uint8_t name_len,
                                 const uint8_t *short_name)
{
    uint8_t checksum = lfn_checksum(short_name);
    uint8_t n_entries;
    uint8_t i, e;
    uint16_t unicode_buf[13];
    uint8_t entry_idx;

    n_entries = (name_len + 12) / 13;
    if (n_entries == 0) n_entries = 1;
    if (n_entries > 2) n_entries = 2;

    entry_idx = 0;
    for (e = n_entries; e >= 1; e--) {
        /* FAT 规范: LFN 项按序号倒序物理排列, 序号最高的项最先出现,
         * 存放名字的最后 13 个字符; 序号 1 的项紧邻短目录项,
         * 存放名字的前 13 个字符; 0x40 标志位属于最高序号项。 */
        uint8_t start_char = (uint8_t)((e - 1u) * 13u);
        uint8_t n_chars = (uint8_t)(name_len - start_char);
        if (n_chars > 13) n_chars = 13;

        for (i = 0; i < n_chars; i++) {
            unicode_buf[i] = (uint16_t)name[start_char + i];
        }

        uint8_t seq = e;
        if (e == n_entries) seq |= 0x40;

        make_lfn_entry(buf + entry_idx * 32, seq, unicode_buf, n_chars, checksum);
        entry_idx++;
    }

    return (uint8_t)(n_entries * 32);
}

/* ================================================================
 *                    从镜像头读取保存的信息
 * ================================================================ */

static uint8_t fw_get_saved_name(uint8_t *out_buf)
{
    if (Storage_ImageValid()) {
        const uint8_t *p = (const uint8_t *)IMG_STORE_ADDR + FW_NAME_OFFSET;
        uint8_t i;
        for (i = 0; i < FW_NAME_LEN; i++) {
            if (p[i] == 0 || p[i] == 0xFF) break;
            out_buf[i] = p[i];
        }
        return i;
    }
    return 0;
}

static uint16_t fw_get_saved_first_cluster(void)
{
    if (Storage_ImageValid()) {
        const uint8_t *p = (const uint8_t *)IMG_STORE_ADDR + FW_FIRST_CLUSTER_OFFSET;
        uint16_t c = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        if (c >= VFS_FIRST_CLUSTER && c < VFS_FIRST_CLUSTER + VFS_DATA_CLUSTERS) {
            return c;
        }
    }
    return VFS_FIRST_CLUSTER;  /* 默认值 */
}

/* ================================================================
 *                         读扇区
 * ================================================================ */

static void read_bpb(uint8_t *buf)
{
    memcpy(buf, bpb_data, sizeof(bpb_data));
    uint16_t total = VFS_TOTAL_SECTORS;
    buf[19] = (uint8_t)(total & 0xFF);
    buf[20] = (uint8_t)(total >> 8);
    buf[510] = 0x55;
    buf[511] = 0xAA;
}

static void read_fat(uint8_t *buf)
{
    uint32_t fw_size;
    uint16_t fw_clusters;
    uint16_t first_cluster;
    uint16_t i;

    memset(buf, 0, VFS_SECTOR_SIZE);

    /* FAT[0] = 0xFF8, FAT[1] = 0xFFF */
    buf[0] = 0xF8;
    buf[1] = 0xFF;
    buf[2] = 0x0F;

    if (!Storage_ImageValid() || Storage_Header()->length == 0) {
        return;  /* 无固件, FAT 全 0 (空闲) */
    }

    fw_size = Storage_Header()->length;
    first_cluster = fw_get_saved_first_cluster();
    fw_clusters = (uint16_t)((fw_size + VFS_SECTOR_SIZE - 1) / VFS_SECTOR_SIZE);
    if (fw_clusters == 0) fw_clusters = 1;
    if (fw_clusters > VFS_DATA_CLUSTERS) fw_clusters = (uint16_t)VFS_DATA_CLUSTERS;

    /* 安全检查: 首簇 + 链长 不超过总簇数 */
    if (first_cluster + fw_clusters > VFS_FIRST_CLUSTER + VFS_DATA_CLUSTERS) {
        fw_clusters = (uint16_t)(VFS_FIRST_CLUSTER + VFS_DATA_CLUSTERS - first_cluster);
    }

    /* 构造连续簇链 (假设文件连续存放 - Windows 小文件通常连续分配) */
    for (i = 0; i < fw_clusters; i++) {
        uint16_t cluster = (uint16_t)(first_cluster + i);
        uint16_t next;
        if (i == fw_clusters - 1) {
            next = 0x0FFF;
        } else {
            next = (uint16_t)(cluster + 1);
        }
        uint16_t byte_off = cluster + (cluster >> 1);
        if (cluster & 1) {
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
    memset(buf, 0, VFS_SECTOR_SIZE);

    if (!Storage_ImageValid() || Storage_Header()->length == 0) {
        return;  /* 无固件, 根目录为空 */
    }

    uint8_t fw_name[FW_NAME_LEN];
    uint8_t name_len = fw_get_saved_name(fw_name);
    uint8_t short_name[11];
    uint32_t fw_size = Storage_Header()->length;
    uint16_t first_cluster = fw_get_saved_first_cluster();

    if (name_len == 0) {
        static const uint8_t def_name[11] = {
            'F','I','R','M','W','A','R','E','B','I','N'
        };
        memcpy(short_name, def_name, 11);
    } else {
        make_short_name(fw_name, name_len, short_name);
    }

    uint8_t *pos = buf;

    /* LFN 项 (显示完整原名) */
    if (name_len > 0) {
        uint8_t lfn_bytes = build_lfn_entries(pos, fw_name, name_len, short_name);
        pos += lfn_bytes;
    }

    /* 短名目录项 */
    make_dir_entry(pos,
                   short_name,
                   0x20,
                   0x4B00u,
                   0x5421u,
                   first_cluster,
                   fw_size);
}

static void read_data_cluster(uint8_t *buf, uint16_t cluster)
{
    memset(buf, 0, VFS_SECTOR_SIZE);

    if (cluster < VFS_FIRST_CLUSTER) return;
    if (cluster >= VFS_FIRST_CLUSTER + VFS_DATA_CLUSTERS) return;

    /* 数据按簇号存放在 Flash 中: 物理偏移 = 第1页起 + (cluster - 2) * 512 */
    uint32_t rel_cluster = cluster - VFS_FIRST_CLUSTER;
    uint32_t store_offset = VFS_DATA_OFFSET + rel_cluster * VFS_SECTOR_SIZE;

    if (store_offset + VFS_SECTOR_SIZE > IMG_STORE_SIZE) return;

    const uint8_t *src = (const uint8_t *)(IMG_STORE_ADDR + store_offset);

    /* 判断这个簇是否有有效数据: 只看镜像头中的总大小
     * 注意: 由于文件可能从非首簇开始, 我们用 first_cluster 来判断 */
    if (Storage_ImageValid()) {
        uint32_t fw_size = Storage_Header()->length;
        uint16_t first_cluster = fw_get_saved_first_cluster();

        if (cluster >= first_cluster) {
            uint32_t offset_in_file = (uint32_t)(cluster - first_cluster) * VFS_SECTOR_SIZE;
            if (offset_in_file < fw_size) {
                uint32_t copy_len = fw_size - offset_in_file;
                if (copy_len > VFS_SECTOR_SIZE) copy_len = VFS_SECTOR_SIZE;
                memcpy(buf, src, copy_len);
                return;
            }
        }
    }
    /* 非文件数据的簇: 全 0 (擦除状态是 0xFF, 但我们返回 0 更安全) */
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
 *              根目录解析 (提取 .bin 文件名、大小、首簇)
 * ================================================================ */

static uint8_t is_bin_extension(const uint8_t *name11)
{
    uint8_t e0 = name11[8];
    uint8_t e1 = name11[9];
    uint8_t e2 = name11[10];
    if (e0 >= 'a' && e0 <= 'z') e0 = (uint8_t)(e0 - 32);
    if (e1 >= 'a' && e1 <= 'z') e1 = (uint8_t)(e1 - 32);
    if (e2 >= 'a' && e2 <= 'z') e2 = (uint8_t)(e2 - 32);
    return (e0 == 'B' && e1 == 'I' && e2 == 'N');
}

static uint8_t is_user_file_entry(const uint8_t *entry)
{
    uint8_t name0 = entry[0];
    uint8_t attr  = entry[11];
    if (name0 == 0x00 || name0 == 0xE5) return 0;
    if (attr == 0x0F) return 0;
    if (attr & 0x1E) return 0;
    return 1;
}

static uint8_t lfn_extract_chars(const uint8_t *entry, uint8_t *out_buf, uint8_t buf_size)
{
    uint8_t count = 0;
    uint8_t i;
    uint16_t c;
    uint8_t offsets[13] = {1,3,5,7,9, 14,16,18,20,22,24, 28,30};

    for (i = 0; i < 13 && count < buf_size; i++) {
        c = (uint16_t)entry[offsets[i]] | ((uint16_t)entry[offsets[i] + 1] << 8);
        if (c == 0x0000) break;
        if (c == 0xFFFF) break;
        if (c < 0x80 && c >= 0x20) {
            out_buf[count++] = (uint8_t)c;
        }
    }
    return count;
}

static void parse_rootdir_for_fw(const uint8_t *rootdir)
{
    uint8_t lfn_buf[FW_NAME_LEN];
    uint8_t lfn_len = 0;       /* 已拼出的名字长度 */
    uint8_t lfn_ok  = 0;       /* 是否收到序号 1 的 LFN 项 (名字起始段) */
    uint8_t i;

    memset(lfn_buf, 0, sizeof(lfn_buf));

    for (i = 0; i < VFS_ROOT_ENTRIES; i++) {
        const uint8_t *entry = rootdir + i * 32;
        uint8_t name0 = entry[0];
        uint8_t attr  = entry[11];

        if (name0 == 0x00) break;
        if (name0 == 0xE5) {
            lfn_len = 0;
            lfn_ok = 0;
            continue;
        }

        if (attr == 0x0F) {
            /* LFN 项: 序号 = name0 & 0x1F, 项内 13 个字符按序号定位到
             * 名字中的偏移 (序号 1 对应第 0 字节), 与物理出现顺序无关。 */
            uint8_t ord = (uint8_t)(name0 & 0x1Fu);
            uint8_t tmp[13];
            uint8_t n = lfn_extract_chars(entry, tmp, 13);
            uint8_t start = (uint8_t)((ord - 1u) * 13u);

            if (ord >= 1u && ord <= 2u && start < FW_NAME_LEN && n > 0) {
                uint8_t copy = n;
                uint8_t end;
                if (copy > FW_NAME_LEN - start) copy = (uint8_t)(FW_NAME_LEN - start);
                memcpy(lfn_buf + start, tmp, copy);
                end = (uint8_t)(start + copy);
                if (end > lfn_len) lfn_len = end;
                if (ord == 1u) lfn_ok = 1;
            }
            continue;
        }

        if (is_user_file_entry(entry)) {
            if (is_bin_extension(entry)) {
                /* 找到第一个 .bin 文件 */

                if (lfn_ok && lfn_len > 0) {
                    memcpy(g_fw_name, lfn_buf, lfn_len);
                    if (lfn_len < FW_NAME_LEN) g_fw_name[lfn_len] = 0;
                } else {
                    /* 没有 LFN, 从短名构造 */
                    uint8_t j;
                    uint8_t pos = 0;
                    for (j = 0; j < 8 && pos < FW_NAME_LEN; j++) {
                        if (entry[j] == ' ') break;
                        g_fw_name[pos++] = entry[j];
                    }
                    if (entry[8] != ' ' && pos + 1 < FW_NAME_LEN) {
                        g_fw_name[pos++] = '.';
                        for (j = 8; j < 11 && pos < FW_NAME_LEN; j++) {
                            if (entry[j] == ' ') break;
                            g_fw_name[pos++] = entry[j];
                        }
                    }
                    if (pos < FW_NAME_LEN) g_fw_name[pos] = 0;
                }

                /* 文件大小 */
                g_fw_size = (uint32_t)entry[28]
                          | ((uint32_t)entry[29] << 8)
                          | ((uint32_t)entry[30] << 16)
                          | ((uint32_t)entry[31] << 24);

                /* 首簇号 */
                g_fw_first_cluster = (uint16_t)entry[26] | ((uint16_t)entry[27] << 8);

                g_fw_found = 1;
                return;  /* 第一个为准 */
            }
        }
        /* 非目标文件项 (含卷标等): LFN 必须紧邻其短目录项, 断开即作废 */
        lfn_len = 0;
        lfn_ok = 0;
    }
}

/* ================================================================
 *                    镜像头更新 (写入完成时调用)
 * ================================================================ */

static void vfs_update_header(void)
{
    if (g_header_written) return;
    if (!g_fw_found || g_fw_size == 0) return;

    /* 计算 CRC32: 按首簇+连续簇的方式遍历数据 */
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t remaining = g_fw_size;
    uint16_t cluster = g_fw_first_cluster;

    while (remaining > 0) {
        if (cluster < VFS_FIRST_CLUSTER) break;
        if (cluster >= VFS_FIRST_CLUSTER + VFS_DATA_CLUSTERS) break;

        uint32_t rel_cluster = cluster - VFS_FIRST_CLUSTER;
        uint32_t store_offset = VFS_DATA_OFFSET + rel_cluster * VFS_SECTOR_SIZE;
        const uint8_t *src = (const uint8_t *)(IMG_STORE_ADDR + store_offset);

        uint32_t chunk = (remaining > VFS_SECTOR_SIZE) ? VFS_SECTOR_SIZE : remaining;
        crc = CRC32_Update(crc, src, chunk);

        remaining -= chunk;
        cluster++;  /* 假设连续分配 */
    }
    crc ^= 0xFFFFFFFFu;

    /* 构造镜像头 (32 字节) */
    uint8_t hdr_buf[64];
    memset(hdr_buf, 0xFF, 64);

    /* magic */
    hdr_buf[0] = (uint8_t)(IMG_MAGIC & 0xFF);
    hdr_buf[1] = (uint8_t)((IMG_MAGIC >> 8) & 0xFF);
    hdr_buf[2] = (uint8_t)((IMG_MAGIC >> 16) & 0xFF);
    hdr_buf[3] = (uint8_t)((IMG_MAGIC >> 24) & 0xFF);
    /* length */
    hdr_buf[4] = (uint8_t)(g_fw_size & 0xFF);
    hdr_buf[5] = (uint8_t)((g_fw_size >> 8) & 0xFF);
    hdr_buf[6] = (uint8_t)((g_fw_size >> 16) & 0xFF);
    hdr_buf[7] = (uint8_t)((g_fw_size >> 24) & 0xFF);
    /* crc32 */
    hdr_buf[8]  = (uint8_t)(crc & 0xFF);
    hdr_buf[9]  = (uint8_t)((crc >> 8) & 0xFF);
    hdr_buf[10] = (uint8_t)((crc >> 16) & 0xFF);
    hdr_buf[11] = (uint8_t)((crc >> 24) & 0xFF);
    /* load_addr */
    hdr_buf[12] = 0x00;
    hdr_buf[13] = 0x00;
    hdr_buf[14] = 0x00;
    hdr_buf[15] = 0x08;
    /* prog_count */
    hdr_buf[16] = 0;
    hdr_buf[17] = 0;
    hdr_buf[18] = 0;
    hdr_buf[19] = 0;
    /* reserved[0..2] (20-31) = 0xFF */

    /* 文件名 (偏移 32, 20 字节) */
    uint8_t name_len = 0;
    while (name_len < FW_NAME_LEN && g_fw_name[name_len] != 0) name_len++;
    if (name_len > 0) {
        memcpy(&hdr_buf[FW_NAME_OFFSET], g_fw_name, name_len);
    }

    /* 首簇号 (偏移 52, uint16_t 小端) */
    hdr_buf[FW_FIRST_CLUSTER_OFFSET]     = (uint8_t)(g_fw_first_cluster & 0xFF);
    hdr_buf[FW_FIRST_CLUSTER_OFFSET + 1] = (uint8_t)(g_fw_first_cluster >> 8);

    /* 写头 (会擦除第 0 页)
     * 注意: 头只有 32 字节, 但我们擦除了整页 (1KB)
     * 文件名和首簇号也在第 0 页, 所以一起写进去 */
    if (Storage_UpdateHeader((const img_hdr_t *)hdr_buf) != STORAGE_OK) {
        return;     /* 头部没写成功: 保持 g_header_written=0, 下次收尾会再试 */
    }

    /* Storage_UpdateHeader 只写 32 字节 (sizeof img_hdr_t)
     * 我们需要继续写文件名和首簇号 (第 0 页已擦除) */
    if (name_len > 0) {
        if (Storage_Write(FW_NAME_OFFSET, g_fw_name, name_len) != STORAGE_OK) return;
    }
    {
        uint8_t fc_buf[2];
        fc_buf[0] = (uint8_t)(g_fw_first_cluster & 0xFF);
        fc_buf[1] = (uint8_t)(g_fw_first_cluster >> 8);
        if (Storage_Write(FW_FIRST_CLUSTER_OFFSET, fc_buf, 2) != STORAGE_OK) return;
    }

    /* 回读校验: 只有确认头部真的落盘, 才允许标记本轮已完成,
     * 避免重新插拔后 Storage_ImageValid() 为假或首簇号缺失。 */
    {
        const img_hdr_t *h  = Storage_Header();
        const uint8_t *nm  = (const uint8_t *)IMG_STORE_ADDR + FW_NAME_OFFSET;
        const uint8_t *fc  = (const uint8_t *)IMG_STORE_ADDR + FW_FIRST_CLUSTER_OFFSET;
        if (h->magic != IMG_MAGIC || h->length != g_fw_size) return;
        if (fc[0] != (uint8_t)(g_fw_first_cluster & 0xFF)) return;
        if (fc[1] != (uint8_t)(g_fw_first_cluster >> 8)) return;
        if (name_len > 0 && memcmp(nm, g_fw_name, name_len) != 0) return;
    }

    g_header_written = 1;
}

/* ================================================================
 *                         写扇区
 * ================================================================ */

void VFS_WriteSector(uint32_t lba)
{
    const uint8_t *buf = USB_MSC_GetWriteBuf();

    /* ---- 根目录写入: 解析文件名/大小/首簇 ---- */
    if (lba == 2) {
        parse_rootdir_for_fw(buf);
        return;
    }

    if (lba < VFS_DATA_START_LBA) {
        return;  /* BPB / FAT 写入忽略 */
    }

    if (lba >= VFS_TOTAL_SECTORS) {
        return;
    }

    uint16_t cluster = (uint16_t)(lba - VFS_DATA_START_LBA + VFS_FIRST_CLUSTER);

    if (cluster < VFS_FIRST_CLUSTER) return;
    if (cluster >= VFS_FIRST_CLUSTER + VFS_DATA_CLUSTERS) return;

    uint32_t rel_cluster = cluster - VFS_FIRST_CLUSTER;
    uint32_t store_offset = VFS_DATA_OFFSET + rel_cluster * VFS_SECTOR_SIZE;

    if (store_offset + VFS_SECTOR_SIZE > IMG_STORE_SIZE) {
        return;
    }

    /* ---- 判断是否开始"新的一次文件传输" ----
     * 主机在同一次拷贝中可能插入 SYNCHRONIZE CACHE / TEST UNIT READY,
     * 从而把 g_write_started 收尾清零; 此时后续数据扇区仍属于同一次拷贝,
     * 不能重新擦除所在页 (否则会擦掉同页中先写好的另一个扇区)。
     * 只有"上次已收尾 且 主机又回退到更小 LBA 重新写"才是新传输。 */
    if (!g_lba_valid || (g_flushed && lba <= g_last_lba)) {
        memset(g_pages_erased, 0, sizeof(g_pages_erased));
        g_write_bytes = 0;
        g_burst_first_cluster = cluster;
    } else if (cluster < g_burst_first_cluster) {
        g_burst_first_cluster = cluster;
    }
    g_flushed  = 0;
    g_lba_valid = 1;
    g_last_lba  = lba;

    if (!g_write_started) {
        g_write_started = 1;
        g_header_written = 0;
        /* 注意: 不重置 g_fw_* 变量
         * 因为根目录解析可能发生在数据写入之前
         * (Windows 先创建目录项再写数据)
         */
    }

    /* 惰性擦除: 只擦当前数据所在的 1 页 (约 20~40ms),
     * 避免像 Storage_Erase 那样在 USB 中断里一次性擦 32 页 (约 1s) 阻塞 USB。
     * 同一页在本轮传输中只擦一次 (用位图记录)。
     * 注意: 页地址必须加上数据区起始偏移 VFS_DATA_OFFSET,
     * 否则擦的是第 0 页 (镜像头页), 真正的数据页从未擦净,
     * 紧接着的编程会失败, 表现为文件开头 512 字节丢失。 */
    {
        uint32_t page_idx = rel_cluster >> 1;          /* 1KB 页 = 2 个 512B 簇 */
        uint32_t page_off = VFS_DATA_OFFSET + page_idx * FLASH_PAGE_SIZE;
        uint8_t  mask = (uint8_t)(1u << (page_idx & 7u));

        if (!(g_pages_erased[page_idx >> 3] & mask)) {
            uint8_t tries;
            for (tries = 0; tries < 3u; tries++) {
                if (Storage_ErasePage(page_off) == STORAGE_OK) break;
            }
            if (tries >= 3u) {
                /* 擦不干净就绝不能往这页编程 (写了也留不住) */
                return;
            }
            g_pages_erased[page_idx >> 3] |= mask;
        }

        if (Storage_Write(store_offset, buf, VFS_SECTOR_SIZE) != STORAGE_OK) {
            /* 编程失败: 重擦整页再写一次 (正常情况下不会走到这里) */
            if (Storage_ErasePage(page_off) != STORAGE_OK) return;
            g_pages_erased[page_idx >> 3] |= mask;
            if (Storage_Write(store_offset, buf, VFS_SECTOR_SIZE) != STORAGE_OK) {
                return;
            }
        }
    }

    /* 更新写入字节数 (按簇偏移的最大值) */
    uint32_t new_end = (rel_cluster + 1) * VFS_SECTOR_SIZE;
    if (new_end > g_write_bytes) {
        g_write_bytes = new_end;
    }
}

/* ================================================================
 *                     写入状态重置辅助
 * ================================================================ */

static void vfs_reset_write_state(void)
{
    g_write_started  = 0;
    g_header_written = 0;
    g_flushed = 1;    /* 已收尾: 下次若主机回退 LBA 重写, 视为新传输 */
    /* 注意: 保留 g_fw_* 中的根目录解析结果。
     * 同一次拷贝可能被主机中途 finalize 多次, 后续数据写入仍属于同一文件,
     * 若在这里清掉文件名/大小, 最终收尾时就只能退回默认名 FIRMWARE.BIN。 */
}

/* ================================================================
 *                     写入完成 (SYNCHRONIZE CACHE)
 * ================================================================ */

void VFS_WriteDone(void)
{
    if (!g_write_started) return;

    if (g_fw_found && g_fw_size > 0 && !g_header_written) {
        vfs_update_header();
    } else if (!g_header_written && g_write_bytes > 0) {
        /* Fallback: 根目录信息缺失 (例如只写了数据没写目录项)
         * 缺失的字段用写入字节数和实际最早写入的簇兜底 */
        if (g_fw_size == 0) g_fw_size = g_write_bytes;
        if (g_fw_first_cluster == 0) g_fw_first_cluster = g_burst_first_cluster;
        if (g_fw_name[0] == 0) {
            memset(g_fw_name, 0, FW_NAME_LEN);
            memcpy(g_fw_name, "FIRMWARE.BIN", 12);
        }
        g_fw_found = 1;
        vfs_update_header();
    }

    vfs_reset_write_state();
}

/* ================================================================
 *                     尝试完成 (TEST UNIT READY 兜底)
 *
 *   部分主机/系统在写入后不一定发送 SYNCHRONIZE CACHE,
 *   而是在数据写完后直接发送 TEST UNIT READY 确认设备就绪。
 *   此时镜像头还没写入, 重新插拔后 Storage_ImageValid() 为假,
 *   根目录读回为空 -> U盘显示为空盘。
 *
 *   这里作为兜底: 仅在"已找到 .bin 文件且大小已知"时才 finalize,
 *   不做默认名兜底, 避免在根目录尚未写入(数据先于目录)时被误触发。
 * ================================================================ */

void VFS_TryFinalize(void)
{
    if (!g_write_started) return;

    if (g_fw_found && g_fw_size > 0 && !g_header_written) {
        vfs_update_header();
    }

    if (g_header_written) {
        vfs_reset_write_state();
    }
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
    g_write_started  = 0;
    g_header_written = 0;
    g_write_bytes    = 0;
    g_lba_valid      = 0;
    g_last_lba       = 0;
    g_flushed        = 1;
    g_burst_first_cluster = VFS_FIRST_CLUSTER;
    memset(g_pages_erased, 0, sizeof(g_pages_erased));
    g_fw_found = 0;
    g_fw_size = 0;
    g_fw_first_cluster = 0;
    memset(g_fw_name, 0, FW_NAME_LEN);
}

#endif /* USB_MODE == USB_MODE_MSC */
