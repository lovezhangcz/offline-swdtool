/**
  * @file  usb_comp.c
  * @brief USB 复合设备: MSC (U盘) + CDC (虚拟串口) 同时工作
  *
  *   VID:PID = 0483:5742
  *   接口布局:
  *     IF0  CDC Control (EP1 interrupt IN)
  *     IF1  CDC Data    (EP2 OUT / EP3 IN)
  *     IF2  MSC         (EP4 OUT / EP5 IN)
  *
  *   CDC 用于调试/命令通道 (版本、日志、触发烧录),
  *   MSC 用于把 .bin 拖进 U 盘更新待烧镜像, 两条路互不影响。
  */
#include "usb_hw.h"
#include "usb_cdc.h"
#include "usb_msc.h"
#include "usb_comp.h"
#include "board.h"

#if USB_MODE == USB_MODE_COMP

#include <string.h>

/* ================================================================
 *                          描述符
 * ================================================================ */

static const uint8_t desc_device[18] = {
    0x12, 0x01,             /* bLength, DEVICE */
    0x00, 0x02,             /* bcdUSB 2.00 */
    0xEF, 0x02, 0x01,       /* class=0xEF (misc), IAD 复合设备 */
    0x40,                   /* bMaxPacketSize0 = 64 */
    0x83, 0x04,             /* VID 0x0483 */
    0x42, 0x57,             /* PID 0x5742 */
    0x00, 0x01,             /* bcdDevice 1.00 */
    0x01, 0x02, 0x03,       /* iManufacturer/iProduct/iSerial */
    0x01                    /* bNumConfigurations */
};

static const uint8_t desc_config[] = {
    /* Configuration: 98 字节, 3 个接口 */
    0x09, 0x02, 98, 0x00,
    0x03,                   /* bNumInterfaces */
    0x01,                   /* bConfigurationValue */
    0x00,                   /* iConfiguration */
    0xC0,                   /* self powered */
    0x32,                   /* 100 mA */
    /* IAD: CDC 功能占用 IF0..IF1 */
    0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00,
    /* IF0: CDC Control */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01,          /* Header functional */
    0x05, 0x24, 0x01, 0x00, 0x01,          /* Call management (数据接口 = 1) */
    0x04, 0x24, 0x02, 0x02,                /* ACM functional */
    0x05, 0x24, 0x06, 0x00, 0x01,          /* Union: 0 + 1 */
    0x07, 0x05, 0x81, 0x03, 0x10, 0x00, 0x0A,  /* EP1 IN (interrupt, 16B, 10ms) */
    /* IF1: CDC Data */
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,  /* EP2 OUT (bulk 64B) */
    0x07, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00,  /* EP3 IN  (bulk 64B) */
    /* IF2: MSC Bulk-Only */
    0x09, 0x04, 0x02, 0x00, 0x02, 0x08, 0x06, 0x50, 0x00,
    0x07, 0x05, 0x04, 0x02, 0x40, 0x00, 0x00,  /* EP4 OUT (bulk 64B) */
    0x07, 0x05, 0x85, 0x02, 0x40, 0x00, 0x00,  /* EP5 IN  (bulk 64B) */
};

static const uint8_t desc_lang[4] = { 0x04, 0x03, 0x09, 0x04 };

static uint8_t desc_str_man[2 + 10 * 2];
static uint8_t desc_str_prd[2 + 24 * 2];
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
    for (n = 0; n < (uint8_t)((desc[0] - 2) / 2); n++) {
        desc[2 + n*2] = (uint8_t)ascii[n];
        desc[3 + n*2] = 0;
    }
}

/* ================================================================
 *                        EP0 状态机
 * ================================================================ */

static const uint8_t *s_ctrl_ptr;
static uint16_t       s_ctrl_len;
static uint8_t        s_set_address;
static uint8_t        s_line_coding[7] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 }; /* 115200 8N1 */

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
    s_ctrl_ptr = d;
    s_ctrl_len = len;
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

    s_set_address = 0;
    s_ctrl_len = 0;
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
        case 0x0B: {                                 /* SET_INTERFACE (只有一个备用设置) */
            if (wIndex <= 2u && wValue == 0u) ep0_tx(0, 0);
            else ep0_stall();
            break;
        }
        case 0x0A: {                                 /* GET_INTERFACE */
            static const uint8_t alt = 0;
            ep0_read_start(&alt, 1, wLength);
            break;
        }
        case 0x08: {                                 /* GET_CONFIGURATION */
            static const uint8_t cfg = 1;
            ep0_read_start(&cfg, 1, wLength);
            break;
        }
        case 0x00: {                                 /* GET_STATUS */
            /* 设备: 自供电; 接口/端点: 0 */
            static const uint8_t st_dev[2] = { 0x01, 0x00 };
            static const uint8_t st_other[2] = { 0x00, 0x00 };
            if ((bmType & 0x1Fu) == 0u) ep0_read_start(st_dev, 2, wLength);
            else                        ep0_read_start(st_other, 2, wLength);
            break;
        }
        case 0x01: case 0x03:                        /* CLEAR/SET_FEATURE */
            ep0_tx(0, 0);
            break;
        default:
            ep0_stall();
            break;
        }
    } else if ((bmType & 0x60u) == 0x20u) {          /* 类请求 */
        switch (bReq) {
        case 0x20:                                   /* CDC SET_LINE_CODING: 7 字节 OUT */
            break;                                   /* 数据在 USB_CB_EP0_RxDone 处理 */
        case 0x21:                                   /* CDC GET_LINE_CODING */
            ep0_read_start(s_line_coding, 7, wLength);
            break;
        case 0x22:                                   /* CDC SET_CONTROL_LINE_STATE */
            USB_CDC_SetDtr((uint8_t)(wValue & 0x01u));
            ep0_tx(0, 0);
            break;
        case 0xFE: {                                 /* MSC Get Max LUN */
            static const uint8_t lun = 0;
            ep0_read_start(&lun, 1, wLength);
            break;
        }
        case 0xFF:                                   /* MSC Bulk-Only Reset */
            MSC_CB_Reset();
            ep0_tx(0, 0);
            break;
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
    USB_EP_SetRxStatus(USB_EP0, USB_STAT_VALID);
}

void USB_CB_EP0_RxDone(uint16_t len)
{
    if (len == 7u) {
        /* SET_LINE_CODING 数据阶段: 收下 7 字节后回状态阶段 */
        USB_PMA_Read(USB_EP_GetRxAddr(USB_EP0), s_line_coding, 7u);
        ep0_tx(0, 0);
    } else {
        /* 控制读的状态阶段 (0 长度 OUT): 只关 TX, 不能再发 IN,
           否则会多出一个 ZLP 破坏控制传输 */
        USB_EP_SetTxStatus(USB_EP0, USB_STAT_NAK);
    }
}

/* ================================================================
 *                       事件分发
 * ================================================================ */

void USB_CB_Reset(void)
{
    /* 总线复位: 丢弃可能残留的控制传输状态 */
    s_ctrl_ptr = 0;
    s_ctrl_len = 0;
    s_set_address = 0;
    CDC_CB_Reset();
    MSC_CB_Reset();
}

void USB_CB_BulkOut(uint8_t ep, const uint8_t *data, uint16_t len)
{
    if (ep == USB_CDC_OUT_EP) {
        CDC_CB_BulkOut(data, len);
    } else if (ep == USB_BULK_OUT_EP) {
        MSC_CB_BulkOut(data, len);
    }
}

void USB_CB_BulkInDone(uint8_t ep)
{
    if (ep == USB_CDC_IN_EP) {
        CDC_CB_BulkInDone();
    } else if (ep == USB_BULK_IN_EP) {
        MSC_CB_BulkInDone();
    }
    /* EP1 (CDC 通知) 不发送数据, 忽略 */
}

/* ================================================================
 *                       初始化
 * ================================================================ */

void USB_Comp_Init(void)
{
    build_str(desc_str_man, "OpenSWD", 0);
    build_str(desc_str_prd, "SWD Prog MSC+CDC", 0);
    build_str(desc_str_ser, NULL, 1);
    USB_HW_Init();
}

#endif /* USB_MODE == USB_MODE_COMP */
