/**
  * @file  usb_msc.c
  * @brief USB MSC Bulk-Only Transport + 最小 SCSI 命令集 + USB 枚举处理
  *
  *   L2精简: 只保留 Win/Linux/macOS 读写U盘必需的最少命令
  *
  *   包含:
  *     - USB 设备/配置/字符串描述符
  *     - EP0 标准控制请求 (枚举用)
  *     - MSC 类请求 (BOT Reset / Get Max LUN)
  *     - BOT 状态机 (CBW -> DATA -> CSW)
  *     - 12 条最小 SCSI 命令
  *
  *   端点使用: EP1=Bulk OUT, EP2=Bulk IN
  */
#include "usb_msc.h"
#include "usb_hw.h"
#include "board.h"

#if USB_MODE == USB_MODE_MSC

#include <string.h>

/* ================================================================
 *                        描述符
 * ================================================================ */

static const uint8_t desc_device[18] = {
    0x12, 0x01,             /* bLength, bDescriptorType=DEVICE */
    0x00, 0x02,             /* bcdUSB 2.00 */
    0x00, 0x00, 0x00,       /* class/subclass/protocol = 0 (defined by interface) */
    0x40,                   /* bMaxPacketSize0 = 64 */
    0x83, 0x04,             /* VID 0x0483 (ST) */
    0x41, 0x57,             /* PID 0x5741 (自定义 MSC) */
    0x00, 0x01,             /* bcdDevice 1.00 */
    0x01, 0x02, 0x03,       /* iManufacturer/iProduct/iSerial */
    0x01                    /* bNumConfigurations */
};

static const uint8_t desc_config[] = {
    /* Configuration */
    0x09, 0x02, 32, 0x00,   /* wTotalLength = 32 */
    0x01,                   /* bNumInterfaces */
    0x01,                   /* bConfigurationValue */
    0x00,                   /* iConfiguration */
    0xC0,                   /* bmAttributes: self powered */
    0x32,                   /* bMaxPower 100mA */
    /* Interface 0: MSC Bulk-Only */
    0x09, 0x04, 0x00, 0x00, 0x02, 0x08, 0x06, 0x50, 0x00,
    /*   class=08(mass storage), subclass=06(SCSI), protocol=50(BOT) */
    /* Endpoint 1 OUT (bulk, 64B) */
    0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00,
    /* Endpoint 2 IN (bulk, 64B) */
    0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00,
};

static const uint8_t desc_lang[4] = { 0x04, 0x03, 0x09, 0x04 };

/* 字符串描述符 (在 RAM 中构建, 序列号用芯片 UID) */
static uint8_t desc_str_man[2 + 10 * 2];
static uint8_t desc_str_prd[2 + 22 * 2];
static uint8_t desc_str_ser[2 + 24 * 2];

static void build_str(uint8_t *desc, const char *ascii, uint8_t hex_serial)
{
    uint8_t n = 0;
    if (hex_serial) {
        const volatile uint8_t *uid = (const volatile uint8_t *)0x1FFFF7E8u;
        uint8_t k;
        desc[0] = 2 + 24 * 2; desc[1] = 0x03;
        for (k = 0; k < 12; k++) {
            uint8_t b = uid[k];
            uint8_t hi = (uint8_t)((b >> 4) < 10 ? '0'+(b>>4)   : 'A'+(b>>4)-10);
            uint8_t lo = (uint8_t)((b & 15) < 10 ? '0'+(b&15)   : 'A'+(b&15)-10);
            desc[2 + (2*k)*2]   = hi;
            desc[3 + (2*k)*2]   = 0;
            desc[2 + (2*k+1)*2] = lo;
            desc[3 + (2*k+1)*2] = 0;
        }
        return;
    }
    while (ascii[n]) n++;
    desc[0] = (uint8_t)(2 + n * 2); desc[1] = 0x03;
    for (uint8_t i = 0; i < n; i++) {
        desc[2 + i*2] = (uint8_t)ascii[i];
        desc[3 + i*2] = 0;
    }
}

/* ================================================================
 *                      EP0 控制状态机
 * ================================================================ */

static void msc_reset(void);   /* 前向声明, 在 BOT 部分定义 */

static const uint8_t *s_ctrl_ptr;
static uint16_t       s_ctrl_len;
static uint8_t        s_set_address;

static void ep0_tx(const uint8_t *d, uint16_t n)
{
    if (n) {
        USB_PMA_Write(USB_EP_GetTxAddr(USB_EP0), d, n);
        USB_EP_SetTxCount(USB_EP0, n);
    } else {
        USB_EP_SetTxCount(USB_EP0, 0);
    }
    USB_EP_SetTxStatus(USB_EP0, USB_STAT_VALID);
}

static void ep0_stall(void)
{
    USB_EP_SetTxStatus(USB_EP0, USB_STAT_STALL);
    USB_EP_SetRxStatus(USB_EP0, USB_STAT_STALL);
}

static void ep0_read_start(const uint8_t *d, uint16_t len, uint16_t host_len)
{
    if (len > host_len) len = host_len;
    s_ctrl_ptr   = d;
    s_ctrl_len   = len;
    {
        uint16_t n = (s_ctrl_len > 64u) ? 64u : s_ctrl_len;
        ep0_tx(s_ctrl_ptr, n);
        s_ctrl_ptr += n;
        s_ctrl_len -= n;
    }
}

void USB_CB_Setup(const uint8_t *pkt)
{
    uint8_t  bmType = pkt[0];
    uint8_t  bReq   = pkt[1];
    uint16_t wValue = (uint16_t)(pkt[2] | (pkt[3] << 8));
    uint16_t wIndex = (uint16_t)(pkt[4] | (pkt[5] << 8));
    uint16_t wLength= (uint16_t)(pkt[6] | (pkt[7] << 8));
    (void)wIndex;

    s_set_address = 0;
    s_ctrl_len = 0;

    /* 准备好接收后续 OUT (数据或状态阶段) */
    USB_EP_SetRxStatus(USB_EP0, USB_STAT_VALID);

    if ((bmType & 0x60u) == 0x00u) {                 /* 标准请求 */
        switch (bReq) {
        case 0x06:                                   /* GET_DESCRIPTOR */
            if (pkt[3] == 0x01)      ep0_read_start(desc_device, sizeof(desc_device), wLength);
            else if (pkt[3] == 0x02) ep0_read_start(desc_config, sizeof(desc_config), wLength);
            else if (pkt[3] == 0x03) {
                if (pkt[2] == 0)      ep0_read_start(desc_lang, sizeof(desc_lang), wLength);
                else if (pkt[2] == 1) ep0_read_start(desc_str_man, desc_str_man[0], wLength);
                else if (pkt[2] == 2) ep0_read_start(desc_str_prd, desc_str_prd[0], wLength);
                else if (pkt[2] == 3) ep0_read_start(desc_str_ser, desc_str_ser[0], wLength);
                else ep0_stall();
            } else ep0_stall();
            break;
        case 0x05:                                   /* SET_ADDRESS */
            if (wValue < 128u) { s_set_address = (uint8_t)wValue; ep0_tx(0, 0); }
            else ep0_stall();
            break;
        case 0x09:                                   /* SET_CONFIGURATION */
            ep0_tx(0, 0);
            break;
        case 0x08: {                                 /* GET_CONFIGURATION */
            static const uint8_t cfg = 1;
            ep0_read_start(&cfg, 1, wLength);
            break;
        }
        case 0x00: {                                 /* GET_STATUS */
            static const uint8_t st[2] = { 0x01, 0x00 };   /* self-powered */
            ep0_read_start(st, 2, wLength);
            break;
        }
        case 0x01: case 0x03:                        /* CLEAR/SET_FEATURE */
            ep0_tx(0, 0);
            break;
        default:
            ep0_stall();
            break;
        }
    } else if ((bmType & 0x60u) == 0x20u) {          /* MSC 类请求 */
        switch (bReq) {
        case 0xFF:                                   /* Bulk-Only Mass Storage Reset */
            msc_reset();
            ep0_tx(0, 0);
            break;
        case 0xFE: {                                 /* Get Max LUN */
            static const uint8_t lun = 0;
            ep0_read_start(&lun, 1, wLength);
            break;
        }
        default:
            ep0_stall();
            break;
        }
    } else {
        ep0_stall();
    }
}

void USB_CB_EP0_TxDone(void)
{
    if (s_set_address) {
        USB_SetAddress(s_set_address);
        s_set_address = 0;
        return;
    }
    if (s_ctrl_len > 0) {
        uint16_t n = (s_ctrl_len > 64u) ? 64u : s_ctrl_len;
        ep0_tx(s_ctrl_ptr, n);
        s_ctrl_ptr += n;
        s_ctrl_len -= n;
    }
    /* 否则是状态阶段零长度包完成, 准备好下一个 SETUP */
    USB_EP_SetRxStatus(USB_EP0, USB_STAT_VALID);
}

void USB_CB_EP0_RxDone(uint16_t len)
{
    (void)len;
    /* 控制 OUT 数据阶段 (MSC 没有需要 OUT 数据的类请求): 回状态 */
    ep0_tx(0, 0);
}

/* ================================================================
 *                   BOT (Bulk-Only Transport)
 * ================================================================ */

/* CBW/CSW 签名 */
#define CBW_SIGNATURE     0x43425355u   /* "USBC" */
#define CSW_SIGNATURE     0x53425355u   /* "USBS" */

/* CSW 状态 */
#define CSW_STATUS_PASS   0x00u
#define CSW_STATUS_FAIL   0x01u
#define CSW_STATUS_PHASE  0x02u

/* SCSI 操作码 */
#define SCSI_TEST_UNIT_READY          0x00u
#define SCSI_REQUEST_SENSE            0x03u
#define SCSI_INQUIRY                  0x12u
#define SCSI_MODE_SENSE_6             0x1Au
#define SCSI_START_STOP_UNIT          0x1Bu
#define SCSI_PREVENT_ALLOW_REMOVAL    0x1Eu
#define SCSI_READ_FORMAT_CAPACITIES   0x23u
#define SCSI_READ_CAPACITY_10         0x25u
#define SCSI_READ_10                  0x28u
#define SCSI_WRITE_10                 0x2Au
#define SCSI_VERIFY_10                0x2Fu
#define SCSI_SYNCHRONIZE_CACHE_10     0x35u

/* Sense Key */
#define SENSE_NO_SENSE          0x00u
#define SENSE_NOT_READY         0x02u
#define SENSE_ILLEGAL_REQUEST   0x05u

/* BOT 状态 */
typedef enum {
    BOT_IDLE = 0,       /* 等待 CBW */
    BOT_DATA_IN,        /* 正在向主机发数据 */
    BOT_DATA_OUT,       /* 正在从主机收数据 */
    BOT_SEND_CSW,       /* 正在发 CSW */
} bot_state_t;

static bot_state_t  g_bot_state;
static uint32_t     g_cbw_tag;
static uint32_t     g_data_remain;        /* 当前阶段剩余字节 */
static uint8_t      g_csw_status;
static uint8_t      g_sense_key;
static uint8_t      g_asc;                /* Additional Sense Code */

/* 512 字节扇区缓冲 (兼作 CBW/CSW 缓冲, 节省 RAM) */
static uint8_t g_sector_buf[MSC_SECTOR_SZ];

/* 多扇区读写状态 */
static uint32_t g_cur_lba;               /* 当前处理的扇区号 */
static uint32_t g_sectors_left;          /* 剩余扇区数 */
static uint16_t g_sector_pos;            /* 当前扇区内字节位置 */

/* ---- VFS 层函数声明 (由 vfs.c 实现) ---- */
extern void VFS_ReadSector(uint32_t lba);
extern void VFS_WriteSector(uint32_t lba);
extern void VFS_WriteDone(void);
extern void VFS_TryFinalize(void);
extern uint32_t VFS_GetTotalSectors(void);

/* ---- 内部函数 ---- */
static void scsi_handle_command(const uint8_t *cdb, uint8_t cdb_len,
                                uint32_t data_len, uint8_t dir_in);
static void send_csw(void);
static void send_next_in_packet(void);

/* ================================================================
 *                   SCSI 固定数据 (Flash 中)
 * ================================================================ */

static const uint8_t scsi_inquiry_data[] = {
    0x00,         /* Peripheral Device Type: Direct Access */
    0x80,         /* Removable Media: Yes */
    0x00,         /* Version */
    0x02,         /* Response Data Format: SPC-2 */
    0x1F,         /* Additional Length (31) */
    0x00, 0x00, 0x00,
    'S','W','D',' ',' ',' ',' ',' ',           /* Vendor: "SWD " (8) */
    'S','W','D',' ','P','r','o','g','r','a','m','m','e','r',' ',' ', /* Product (16) */
    '1','.','0','0'                             /* Revision (4) */
};

static const uint8_t scsi_sense_hdr[] = {
    0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

/* ================================================================
 *                    批量端点回调 (底层 -> MSC)
 * ================================================================ */

void USB_CB_Reset(void)
{
    msc_reset();
}

/* 批量 OUT 数据到达 */
void USB_CB_BulkOut(const uint8_t *data, uint16_t len)
{
    if (g_bot_state == BOT_IDLE) {
        /* ---- 期望 CBW (31 字节) ---- */
        if (len < 31u) return;

        uint32_t sig = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                       ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
        if (sig != CBW_SIGNATURE) {
            g_csw_status = CSW_STATUS_PHASE;
            send_csw();
            return;
        }

        g_cbw_tag = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                    ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
        uint32_t data_len = (uint32_t)data[8] | ((uint32_t)data[9] << 8) |
                           ((uint32_t)data[10] << 16) | ((uint32_t)data[11] << 24);
        uint8_t flags  = data[12];
        uint8_t cb_len = (uint8_t)(data[14] & 0x1Fu);
        const uint8_t *cdb = &data[15];

        g_data_remain = data_len;
        g_csw_status  = CSW_STATUS_PASS;
        g_sector_pos   = 0;     /* 每个新命令从缓冲起始位置开始 */
        g_cur_lba      = 0;
        g_sectors_left = 0;

        scsi_handle_command(cdb, cb_len, data_len, (uint8_t)(flags & 0x80u));
        return;
    }

    if (g_bot_state == BOT_DATA_OUT) {
        /* ---- WRITE 数据阶段 ---- */
        uint32_t copy_len = len;
        if (copy_len > g_data_remain) copy_len = g_data_remain;

        /* 写入扇区缓冲 */
        if (g_sector_pos + copy_len <= MSC_SECTOR_SZ) {
            memcpy(&g_sector_buf[g_sector_pos], data, (uint16_t)copy_len);
            g_sector_pos += (uint16_t)copy_len;
            g_data_remain -= copy_len;

            /* 扇区满了? 写入 VFS */
            if (g_sector_pos >= MSC_SECTOR_SZ) {
                VFS_WriteSector(g_cur_lba);
                g_cur_lba++;
                g_sectors_left--;
                g_sector_pos = 0;
            }

            /* 全部收完? */
            if (g_data_remain == 0) {
                /* 最后一个不完整扇区也写入 */
                if (g_sector_pos > 0 && g_sectors_left == 0) {
                    VFS_WriteSector(g_cur_lba);
                }
                send_csw();
            }
        } else {
            /* 异常: 超出扇区缓冲 */
            g_csw_status = CSW_STATUS_FAIL;
            g_sense_key  = SENSE_ILLEGAL_REQUEST;
            g_asc        = 0x24; /* Invalid field in CDB */
            send_csw();
        }
        return;
    }
}

/* 批量 IN 发送完成 */
void USB_CB_BulkInDone(void)
{
    if (g_bot_state == BOT_DATA_IN) {
        if (g_data_remain > 0 || g_sectors_left > 0) {
            send_next_in_packet();
        } else {
            send_csw();
        }
        return;
    }
    if (g_bot_state == BOT_SEND_CSW) {
        g_bot_state = BOT_IDLE;
    }
}

/* ================================================================
 *                       SCSI 命令处理
 * ================================================================ */

static void set_sense(uint8_t key, uint8_t asc)
{
    g_sense_key  = key;
    g_asc        = asc;
    g_csw_status = CSW_STATUS_FAIL;
}

static void scsi_handle_command(const uint8_t *cdb, uint8_t cdb_len,
                                uint32_t data_len, uint8_t dir_in)
{
    uint8_t opcode = cdb[0];
    (void)cdb_len;
    (void)dir_in;

    switch (opcode) {
    /* ---- 无数据命令 ---- */
    case SCSI_TEST_UNIT_READY:
        /* 兜底: 某些系统写完数据不发 SYNCHRONIZE CACHE,
         * 这里在确认就绪时尝试写镜像头, 避免重插后空盘 */
        VFS_TryFinalize();
        send_csw();
        break;
    case SCSI_START_STOP_UNIT:
    case SCSI_PREVENT_ALLOW_REMOVAL:
    case SCSI_VERIFY_10:
        send_csw();
        break;

    case SCSI_SYNCHRONIZE_CACHE_10:
        VFS_WriteDone();
        send_csw();
        break;

    /* ---- IN 数据命令 ---- */
    case SCSI_REQUEST_SENSE: {
        uint8_t alloc_len = cdb[4];
        if (alloc_len > 18) alloc_len = 18;
        memcpy(g_sector_buf, scsi_sense_hdr, 18);
        g_sector_buf[2]  = g_sense_key;
        g_sector_buf[12] = g_asc;
        g_sense_key = SENSE_NO_SENSE;
        g_asc = 0;
        g_sector_pos   = 0;
        g_data_remain = alloc_len;
        if (data_len < alloc_len) g_data_remain = data_len;
        g_bot_state = BOT_DATA_IN;
        send_next_in_packet();
        break;
    }

    case SCSI_INQUIRY: {
        uint8_t alloc_len = (uint8_t)cdb[4];
        if (alloc_len > sizeof(scsi_inquiry_data)) {
            alloc_len = (uint8_t)sizeof(scsi_inquiry_data);
        }
        memcpy(g_sector_buf, scsi_inquiry_data, alloc_len);
        g_sector_pos   = 0;
        g_data_remain = alloc_len;
        if (data_len < alloc_len) g_data_remain = data_len;
        g_bot_state = BOT_DATA_IN;
        send_next_in_packet();
        break;
    }

    case SCSI_MODE_SENSE_6: {
        /* 简化: 只返回 4 字节 header, 无块描述符, 无页 */
        g_sector_buf[0] = 3;    /* Mode Data Length */
        g_sector_buf[1] = 0;    /* Medium Type */
        g_sector_buf[2] = 0;    /* Device-Specific Parameter */
        g_sector_buf[3] = 0;    /* Block Descriptor Length */
        uint8_t ret_len = 4;
        if (data_len < ret_len) ret_len = (uint8_t)data_len;
        g_sector_pos   = 0;
        g_data_remain = ret_len;
        g_bot_state = BOT_DATA_IN;
        send_next_in_packet();
        break;
    }

    case SCSI_READ_FORMAT_CAPACITIES: {
        uint32_t total = VFS_GetTotalSectors();
        /* 4 字节 header + 8 字节 descriptor = 12 字节 */
        g_sector_buf[0] = 0;
        g_sector_buf[1] = 0;
        g_sector_buf[2] = 0;
        g_sector_buf[3] = 8;    /* List Length */
        uint32_t last = total - 1;
        g_sector_buf[4] = (uint8_t)(last >> 24);
        g_sector_buf[5] = (uint8_t)(last >> 16);
        g_sector_buf[6] = (uint8_t)(last >> 8);
        g_sector_buf[7] = (uint8_t)last;
        g_sector_buf[8] = 0;    /* Descriptor Type: Formatted Media */
        g_sector_buf[9] = 0;
        g_sector_buf[10] = (uint8_t)(MSC_SECTOR_SZ >> 8);
        g_sector_buf[11] = (uint8_t)MSC_SECTOR_SZ;
        uint8_t ret_len = 12;
        if (data_len < ret_len) ret_len = (uint8_t)data_len;
        g_sector_pos   = 0;
        g_data_remain = ret_len;
        g_bot_state = BOT_DATA_IN;
        send_next_in_packet();
        break;
    }

    case SCSI_READ_CAPACITY_10: {
        uint32_t total = VFS_GetTotalSectors();
        uint32_t last = total - 1;
        g_sector_buf[0] = (uint8_t)(last >> 24);
        g_sector_buf[1] = (uint8_t)(last >> 16);
        g_sector_buf[2] = (uint8_t)(last >> 8);
        g_sector_buf[3] = (uint8_t)last;
        g_sector_buf[4] = 0;
        g_sector_buf[5] = 0;
        g_sector_buf[6] = (uint8_t)(MSC_SECTOR_SZ >> 8);
        g_sector_buf[7] = (uint8_t)MSC_SECTOR_SZ;
        g_sector_pos   = 0;
        g_data_remain = 8;
        g_bot_state = BOT_DATA_IN;
        send_next_in_packet();
        break;
    }

    case SCSI_READ_10: {
        g_cur_lba = ((uint32_t)cdb[2] << 24) | ((uint32_t)cdb[3] << 16) |
                    ((uint32_t)cdb[4] << 8)  | (uint32_t)cdb[5];
        g_sectors_left = ((uint32_t)cdb[7] << 8) | (uint32_t)cdb[8];
        g_sector_pos = 0;

        if (g_sectors_left == 0) {
            send_csw();
            break;
        }

        /* 读第一个扇区到缓冲 */
        VFS_ReadSector(g_cur_lba);
        g_cur_lba++;
        g_sectors_left--;
        g_data_remain = MSC_SECTOR_SZ;
        g_bot_state = BOT_DATA_IN;
        send_next_in_packet();
        break;
    }

    /* ---- OUT 数据命令 ---- */
    case SCSI_WRITE_10: {
        g_cur_lba = ((uint32_t)cdb[2] << 24) | ((uint32_t)cdb[3] << 16) |
                    ((uint32_t)cdb[4] << 8)  | (uint32_t)cdb[5];
        g_sectors_left = ((uint32_t)cdb[7] << 8) | (uint32_t)cdb[8];
        g_sector_pos = 0;

        if (g_sectors_left == 0) {
            send_csw();
            break;
        }

        g_bot_state = BOT_DATA_OUT;
        break;
    }

    default:
        set_sense(SENSE_ILLEGAL_REQUEST, 0x20); /* Invalid Operation Code */
        send_csw();
        break;
    }
}

/* ================================================================
 *                       数据传输辅助
 * ================================================================ */

/* 发送下一包 IN 数据 (从 g_sector_buf[g_sector_pos] 开始, 最多 64B)
 * 当前扇区发完且还有后续扇区时, 自动读入下一扇区 */
static void send_next_in_packet(void)
{
    /* 当前扇区发完了? */
    if (g_data_remain == 0) {
        if (g_sectors_left > 0) {
            /* 读入下一扇区 */
            VFS_ReadSector(g_cur_lba);
            g_cur_lba++;
            g_sectors_left--;
            g_data_remain = MSC_SECTOR_SZ;
            g_sector_pos = 0;
        } else {
            /* 全部发完 */
            send_csw();
            return;
        }
    }

    uint16_t send_len = 64;
    if (send_len > g_data_remain) send_len = (uint16_t)g_data_remain;

    USB_PMA_Write(USB_EP_GetTxAddr(USB_BULK_IN_EP),
                  &g_sector_buf[g_sector_pos], send_len);
    USB_EP_SetTxCount(USB_BULK_IN_EP, send_len);
    USB_EP_SetTxStatus(USB_BULK_IN_EP, USB_STAT_VALID);

    g_sector_pos  += send_len;
    g_data_remain -= send_len;
}

/* 发送 CSW (13 字节) */
static void send_csw(void)
{
    uint8_t *buf = g_sector_buf;

    buf[0] = (uint8_t)(CSW_SIGNATURE & 0xFFu);
    buf[1] = (uint8_t)((CSW_SIGNATURE >> 8) & 0xFFu);
    buf[2] = (uint8_t)((CSW_SIGNATURE >> 16) & 0xFFu);
    buf[3] = (uint8_t)((CSW_SIGNATURE >> 24) & 0xFFu);

    buf[4] = (uint8_t)(g_cbw_tag & 0xFFu);
    buf[5] = (uint8_t)((g_cbw_tag >> 8) & 0xFFu);
    buf[6] = (uint8_t)((g_cbw_tag >> 16) & 0xFFu);
    buf[7] = (uint8_t)((g_cbw_tag >> 24) & 0xFFu);

    buf[8]  = (uint8_t)(g_data_remain & 0xFFu);
    buf[9]  = (uint8_t)((g_data_remain >> 8) & 0xFFu);
    buf[10] = (uint8_t)((g_data_remain >> 16) & 0xFFu);
    buf[11] = (uint8_t)((g_data_remain >> 24) & 0xFFu);

    buf[12] = g_csw_status;

    g_bot_state = BOT_SEND_CSW;

    USB_PMA_Write(USB_EP_GetTxAddr(USB_BULK_IN_EP), buf, 13);
    USB_EP_SetTxCount(USB_BULK_IN_EP, 13);
    USB_EP_SetTxStatus(USB_BULK_IN_EP, USB_STAT_VALID);
}

/* ================================================================
 *                          重置
 * ================================================================ */

static void msc_reset(void)
{
    g_bot_state    = BOT_IDLE;
    g_cbw_tag      = 0;
    g_data_remain  = 0;
    g_csw_status   = CSW_STATUS_PASS;
    g_sense_key    = SENSE_NO_SENSE;
    g_asc          = 0;
    g_cur_lba      = 0;
    g_sectors_left = 0;
    g_sector_pos   = 0;
}

/* ================================================================
 *                     供上层调用的公共接口
 * ================================================================ */

void USB_MSC_SendData(const uint8_t *data, uint16_t len)
{
    if (len <= MSC_SECTOR_SZ) {
        memcpy(g_sector_buf, data, len);
        g_data_remain = len;
        g_bot_state = BOT_DATA_IN;
        send_next_in_packet();
    }
}

void USB_MSC_EndTransfer(uint8_t success)
{
    if (!success) g_csw_status = CSW_STATUS_FAIL;
    send_csw();
}

uint8_t *USB_MSC_GetWriteBuf(void)
{
    return g_sector_buf;
}

/* ================================================================
 *                    描述符初始化 (供 main.c 调用)
 * ================================================================ */

void USB_MSC_InitDesc(void)
{
    build_str(desc_str_man, "SWD Tools", 0);
    build_str(desc_str_prd, "SWD Offline Programmer", 0);
    build_str(desc_str_ser, NULL, 1);
}

#endif /* USB_MODE == USB_MODE_MSC */
