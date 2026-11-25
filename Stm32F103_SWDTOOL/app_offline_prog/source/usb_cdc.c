/**
  * @file  usb_cdc.c
  * @brief USB CDC ACM 类: 描述符、EP0 控制通道状态机、批量数据通道
  *
  *   USB_MODE_CDC  : 单功能 CDC 设备 (VID:PID = 0483:5740, Windows 自动加载 usbser)
  *   USB_MODE_COMP : 与 MSC 组成复合设备, 本文件只提供 CDC 数据通道 (描述符在 usb_comp.c)
  */
#include "usb_hw.h"
#include "usb_cdc.h"
#include "board.h"

#if USB_HAS_CDC

#include <string.h>

#if USB_MODE == USB_MODE_CDC

/* ------------------------------------------------ 描述符 ----------------- */
static const uint8_t desc_device[18] = {
    0x12, 0x01,             /* bLength, bDescriptorType=DEVICE */
    0x00, 0x02,             /* bcdUSB 2.00 */
    0x02, 0x00, 0x00,       /* class=CDC */
    0x40,                   /* bMaxPacketSize0 = 64 */
    0x83, 0x04,             /* VID 0x0483 */
    0x40, 0x57,             /* PID 0x5740 */
    0x00, 0x01,             /* bcdDevice 1.00 */
    0x01, 0x02, 0x03,       /* iManufacturer/iProduct/iSerial */
    0x01                    /* bNumConfigurations */
};

static const uint8_t desc_config[] = {
    /* Configuration */
    0x09, 0x02, 75, 0x00,   /* wTotalLength = 75 */
    0x02,                   /* bNumInterfaces */
    0x01,                   /* bConfigurationValue */
    0x00,                   /* iConfiguration */
    0xC0,                   /* bmAttributes: self powered */
    0x32,                   /* bMaxPower 100mA */
    /* IAD */
    0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00,
    /* Interface 0: CDC Control */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    /* Header functional */
    0x05, 0x24, 0x00, 0x10, 0x01,
    /* Call management */
    0x05, 0x24, 0x01, 0x00, 0x01,
    /* ACM functional */
    0x04, 0x24, 0x02, 0x02,
    /* Union functional */
    0x05, 0x24, 0x06, 0x00, 0x01,
    /* Endpoint 1 IN (interrupt, 16B, 10ms) */
    0x07, 0x05, 0x81, 0x03, 0x10, 0x00, 0x0A,
    /* Interface 1: CDC Data */
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    /* Endpoint 2 OUT (bulk, 64B) */
    0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
    /* Endpoint 3 IN (bulk, 64B) */
    0x07, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00,
};

static const uint8_t desc_lang[4] = { 0x04, 0x03, 0x09, 0x04 };

/* 字符串描述符在 RAM 中构建 (序列号取自芯片唯一 ID) */
static uint8_t desc_str_man[2 + 10 * 2];
static uint8_t desc_str_prd[2 + 22 * 2];
static uint8_t desc_str_ser[2 + 24 * 2];

static void build_str(uint8_t *desc, const char *ascii, uint8_t hex_serial)
{
    uint8_t i = 0, n = 0;
    if (hex_serial) {
        const volatile uint8_t *uid = (const volatile uint8_t *)0x1FFFF7E8u;
        uint8_t k;
        desc[0] = 2 + 24 * 2; desc[1] = 0x03;
        for (k = 0; k < 12; k++) {
            uint8_t b = uid[k];
            desc[2 + (2*k)*2]   = (uint8_t)((b >> 4) < 10 ? '0'+(b>>4)   : 'A'+(b>>4)-10);
            desc[3 + (2*k)*2]   = 0;
            desc[2 + (2*k+1)*2] = (uint8_t)((b & 15) < 10 ? '0'+(b&15)   : 'A'+(b&15)-10);
            desc[3 + (2*k+1)*2] = 0;
        }
        return;
    }
    while (ascii[n]) n++;
    desc[0] = (uint8_t)(2 + n * 2); desc[1] = 0x03;
    for (i = 0; i < n; i++) { desc[2 + i*2] = (uint8_t)ascii[i]; desc[3 + i*2] = 0; }
}

/* ------------------------------------------------ EP0 状态机 ------------- */
static const uint8_t *s_ctrl_ptr;
static uint16_t       s_ctrl_len;    /* 剩余待发送 */
static uint8_t        s_set_address; /* 状态阶段完成后生效的地址 */
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

/* 控制读: 启动数据阶段 */
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

static void CDC_CB_Setup(const uint8_t *pkt)
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
    } else if ((bmType & 0x60u) == 0x20u) {          /* CDC 类请求 */
        switch (bReq) {
        case 0x20:                                   /* SET_LINE_CODING: 7 字节数据阶段 */
            /* 等待 OUT 数据, 见 USB_CB_EP0_RxDone */
            break;
        case 0x21:                                   /* GET_LINE_CODING */
            ep0_read_start(s_line_coding, 7, wLength);
            break;
        case 0x22:                                   /* SET_CONTROL_LINE_STATE */
            USB_CDC_SetDtr((uint8_t)(wValue & 0x01u));
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

static void CDC_CB_EP0_TxDone(void)
{
    if (s_set_address) {
        USB_SetAddress(s_set_address);
        s_set_address = 0;
        return;
    }
    if (s_ctrl_len) {
        /* 控制读后续包 */
        uint16_t n = (s_ctrl_len > 64u) ? 64u : s_ctrl_len;
        ep0_tx(s_ctrl_ptr, n);
        s_ctrl_ptr += n;
        s_ctrl_len -= n;
    }
    /* 本工程所有描述符与应答长度都不是 64 的整数倍, 无需 ZLP */
}

static void CDC_CB_EP0_RxDone(uint16_t len)
{
    if (len == 7u) {
        /* SET_LINE_CODING 的数据阶段 -> 回状态阶段 (0 长度 IN) */
        USB_PMA_Read(USB_EP_GetRxAddr(USB_EP0), s_line_coding, 7u);
        ep0_tx(0, 0);
    } else {
        /* 控制读的状态阶段 (0 长度 OUT): 无需应答, 关闭 TX */
        USB_EP_SetTxStatus(USB_EP0, USB_STAT_NAK);
    }
}

#endif /* USB_MODE == USB_MODE_CDC */

/* ------------------------------------------------ 批量数据通道 ----------- */
/* 简单 4 包发送环形队列, 每包 <=64B */
#define TXQ_DEPTH 4u
static uint8_t  s_txq[TXQ_DEPTH][64];
static uint8_t  s_txq_len[TXQ_DEPTH];
static volatile uint8_t s_txq_head, s_txq_tail;
static volatile uint8_t s_tx_busy;
static volatile uint8_t s_line_dtr;

static void tx_kick(void)
{
    __disable_irq();                    /* 与 BulkInDone(ISR) 互斥 */
    if (!s_tx_busy && s_txq_head != s_txq_tail) {
        s_tx_busy = 1;
        USB_PMA_Write(USB_EP_GetTxAddr(USB_CDC_IN_EP), s_txq[s_txq_tail], s_txq_len[s_txq_tail]);
        USB_EP_SetTxCount(USB_CDC_IN_EP, s_txq_len[s_txq_tail]);
        USB_EP_SetTxStatus(USB_CDC_IN_EP, USB_STAT_VALID);
    }
    __enable_irq();
}

uint16_t USB_CDC_Write(const uint8_t *data, uint16_t len)
{
    uint8_t next;
    if (len == 0 || len > 64u) return 0;
    next = (uint8_t)((s_txq_head + 1u) % TXQ_DEPTH);
    if (next == s_txq_tail) return 0;                /* 队列满 */
    {
        uint16_t i;
        for (i = 0; i < len; i++) s_txq[s_txq_head][i] = data[i];
    }
    s_txq_len[s_txq_head] = (uint8_t)len;
    s_txq_head = next;
    tx_kick();
    return len;
}

void CDC_CB_BulkInDone(void)
{
    s_txq_tail = (uint8_t)((s_txq_tail + 1u) % TXQ_DEPTH);
    s_tx_busy = 0;
    tx_kick();
}

/* 主机 -> 设备数据: 转发给协议层环形缓冲 (ISR 上下文, 只做拷贝) */
void CDC_CB_BulkOut(const uint8_t *data, uint16_t len)
{
    CDC_RxBytes(data, len);
}

void USB_CDC_Poll(void)
{
    /* 队列非空但发送器空闲(异常恢复)时补一脚 */
    if (!s_tx_busy && s_txq_head != s_txq_tail) tx_kick();
}

uint8_t USB_CDC_HostActive(void)
{
    return s_line_dtr;
}

void USB_CDC_SetDtr(uint8_t dtr)
{
    s_line_dtr = dtr;
}

void CDC_CB_Reset(void)
{
    s_txq_head = s_txq_tail = 0;
    s_tx_busy = 0;
    s_line_dtr = 0;
}

#if USB_MODE == USB_MODE_CDC
/* ------------------------------------------------ 初始化 ----------------- */
void USB_CDC_Init(void)
{
    build_str(desc_str_man, "OpenSWD", 0);
    build_str(desc_str_prd, "SWD Offline Programmer", 0);
    build_str(desc_str_ser, "", 1);
    USB_HW_Init();
}

/* 单功能 CDC: 把底层回调接到本文件 */
void USB_CB_Reset(void)              { CDC_CB_Reset(); }
void USB_CB_Setup(const uint8_t *p)  { CDC_CB_Setup(p); }
void USB_CB_EP0_TxDone(void)         { CDC_CB_EP0_TxDone(); }
void USB_CB_EP0_RxDone(uint16_t len) { CDC_CB_EP0_RxDone(len); }
void USB_CB_BulkOut(uint8_t ep, const uint8_t *d, uint16_t n) { (void)ep; CDC_CB_BulkOut(d, n); }
void USB_CB_BulkInDone(uint8_t ep)   { (void)ep; CDC_CB_BulkInDone(); }
#endif

#endif /* USB_HAS_CDC */
