/**
  * @file  proto.c
  * @brief 上位机通信协议实现
  */
#include <string.h>
#include "proto.h"
#include "usb_cdc.h"
#include "storage.h"
#include "target_prog.h"
#include "crc.h"

#if USB_MODE == USB_MODE_CDC

#define FRAME_HDR0   0xAAu
#define FRAME_HDR1   0x55u

#define CMD_PING        0x01u
#define CMD_GET_INFO    0x02u
#define CMD_IMG_BEGIN   0x10u
#define CMD_IMG_DATA    0x11u
#define CMD_IMG_END     0x12u
#define CMD_PROG_NOW    0x20u
#define CMD_GET_STATUS  0x21u
#define CMD_TGT_PROBE   0x22u

#define RSP_OK          0x00u
#define RSP_ERR_ARG     0x01u
#define RSP_ERR_FLASH   0x02u
#define RSP_ERR_CRC     0x03u
#define RSP_ERR_STATE   0x04u

/* 最大帧载荷: IMG_DATA = offset(4) + data(508) */
#define MAX_PAYLOAD     520u

/* ------------------------- 接收字节环 (USB ISR 写入) --------------------- */
#define RX_RING_SIZE    1024u
static uint8_t  s_rx_ring[RX_RING_SIZE];
static volatile uint16_t s_rx_head, s_rx_tail;

void Proto_RxBytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        uint16_t next = (uint16_t)((s_rx_head + 1u) % RX_RING_SIZE);
        if (next == s_rx_tail) return;              /* 满则丢弃 (主机按帧等应答, 不会发生) */
        s_rx_ring[s_rx_head] = data[i];
        s_rx_head = next;
    }
}

/* ------------------------- 帧解析状态机 ---------------------------------- */
typedef enum { RS_H0, RS_H1, RS_CMD, RS_L0, RS_L1, RS_PAYLOAD, RS_C0, RS_C1 } rx_state_t;
static rx_state_t s_rs;
static uint8_t  s_cmd;
static uint16_t s_len, s_pos;
static uint8_t  s_payload[MAX_PAYLOAD];
static uint16_t s_crc_calc, s_crc_recv;

/* ------------------------- 发送 ------------------------------------------ */
static uint8_t s_tx_frame[80];             /* 发送组帧缓冲 (所有应答 < 64B) */
static volatile uint8_t s_prog_req_flag;   /* PROG_NOW 手动触发标志 */

static void proto_respond(uint8_t cmd, uint8_t status, const uint8_t *payload, uint16_t len)
{
    uint8_t  *f = s_tx_frame;
    uint16_t crc;
    uint16_t i;
    uint16_t total = (uint16_t)(2 + 1 + 2 + (len + 1u) + 2); /* 头2+命令1+长度2+状态1+载荷len+CRC2 */

    if (total > 64u) return;    /* 单包上限 (设计上所有应答均 < 64B) */

    f[0] = FRAME_HDR0; f[1] = FRAME_HDR1;
    f[2] = (uint8_t)(cmd | 0x80u);
    f[3] = (uint8_t)(len + 1u); f[4] = (uint8_t)((len + 1u) >> 8);
    f[5] = status;
    for (i = 0; i < len; i++) f[6 + i] = payload[i];
    crc = CRC16_Update(0xFFFFu, &f[2], (uint16_t)(3 + 1 + len));
    f[6 + len] = (uint8_t)crc;
    f[7 + len] = (uint8_t)(crc >> 8);

    /* 发送忙时稍候重试几次; 主机按帧等待, 队列足够 */
    for (i = 0; i < 100u; i++) {
        if (USB_CDC_Write(f, total) == total) return;
    }
}

/* ------------------------- 镜像下载会话 ---------------------------------- */
static uint32_t s_img_len, s_img_crc, s_img_addr, s_img_received;
static uint8_t  s_img_active;

static void cmd_exec(uint8_t cmd, const uint8_t *pl, uint16_t len)
{
    uint8_t p[24];

    switch (cmd) {
    case CMD_PING:
        proto_respond(cmd, RSP_OK, (const uint8_t *)"SWDPROG1", 8);
        break;

    case CMD_GET_INFO: {
        const img_hdr_t *h = Storage_Header();
        uint8_t valid = Storage_ImageValid();
        p[0] = valid;
        memcpy(&p[1],  &h->length,    4);
        memcpy(&p[5],  &h->crc32,     4);
        memcpy(&p[9],  &h->load_addr, 4);
        {
            uint32_t cap = IMG_MAX_PAYLOAD;
            memcpy(&p[13], &cap, 4);
        }
        p[17] = g_status.state;
        p[18] = g_status.last_result;
        proto_respond(cmd, RSP_OK, p, 19);
        break;
    }

    case CMD_IMG_BEGIN:
        if (len != 12u) { proto_respond(cmd, RSP_ERR_ARG, 0, 0); break; }
        if (g_status.state == 2u) {      /* 烧录中禁止更新镜像 */
            proto_respond(cmd, RSP_ERR_STATE, 0, 0);
            break;
        }
        memcpy(&s_img_len,  &pl[0], 4);
        memcpy(&s_img_crc,  &pl[4], 4);
        memcpy(&s_img_addr, &pl[8], 4);
        if (s_img_len == 0 || s_img_len > IMG_MAX_PAYLOAD ||
            (s_img_addr & 0xFF000000u) != 0x08000000u) {
            proto_respond(cmd, RSP_ERR_ARG, 0, 0);
            break;
        }
        if (Storage_Erase() != STORAGE_OK) {
            proto_respond(cmd, RSP_ERR_FLASH, 0, 0);
            break;
        }
        s_img_received = 0;
        s_img_active = 1;
        proto_respond(cmd, RSP_OK, 0, 0);
        break;

    case CMD_IMG_DATA: {
        uint32_t off;
        uint8_t r;
        if (!s_img_active || len < 4u) { proto_respond(cmd, RSP_ERR_STATE, 0, 0); break; }
        memcpy(&off, pl, 4);
        if (off != s_img_received || off + (len - 4u) > s_img_len) {
            proto_respond(cmd, RSP_ERR_ARG, 0, 0);
            break;
        }
        /* 写载荷区: 存储偏移 = 头 + off */
        r = Storage_Write(sizeof(img_hdr_t) + off, &pl[4], (uint32_t)(len - 4u));
        if (r != STORAGE_OK) { proto_respond(cmd, RSP_ERR_FLASH, 0, 0); break; }
        s_img_received += (uint32_t)(len - 4u);
        memcpy(p, &s_img_received, 4);
        proto_respond(cmd, RSP_OK, p, 4);
        break;
    }

    case CMD_IMG_END: {
        uint32_t crc;
        img_hdr_t hdr;
        if (!s_img_active || s_img_received != s_img_len) {
            s_img_active = 0;
            proto_respond(cmd, RSP_ERR_STATE, 0, 0);
            break;
        }
        /* 校验镜像 CRC32 */
        crc = CRC32_Update(0xFFFFFFFFu, Storage_PayloadPtr(), s_img_len) ^ 0xFFFFFFFFu;
        if (crc != s_img_crc) {
            s_img_active = 0;
            proto_respond(cmd, RSP_ERR_CRC, 0, 0);
            break;
        }
        /* 写入头使镜像生效 */
        hdr.magic      = IMG_MAGIC;
        hdr.length     = s_img_len;
        hdr.crc32      = s_img_crc;
        hdr.load_addr  = s_img_addr;
        hdr.prog_count = 0;
        hdr.reserved[0] = hdr.reserved[1] = hdr.reserved[2] = 0;
        if (Storage_Write(0, (const uint8_t *)&hdr, sizeof(hdr)) != STORAGE_OK) {
            proto_respond(cmd, RSP_ERR_FLASH, 0, 0);
            break;
        }
        s_img_active = 0;
        proto_respond(cmd, RSP_OK, 0, 0);
        break;
    }

    case CMD_PROG_NOW:
        s_prog_req_flag = 1;
        proto_respond(cmd, RSP_OK, 0, 0);
        break;

    case CMD_GET_STATUS:
        p[0] = g_status.state;
        p[1] = g_status.last_result;
        memcpy(&p[2],  (const void *)&g_status.last_error,   4);
        memcpy(&p[6],  (const void *)&g_status.target_dpidr, 4);
        memcpy(&p[10], (const void *)&g_status.target_idcode,4);
        memcpy(&p[14], (const void *)&g_status.progress,     4);
        memcpy(&p[18], (const void *)&g_status.swd_err,      4);
        proto_respond(cmd, RSP_OK, p, 22);
        break;

    case CMD_TGT_PROBE: {
        uint32_t dpidr = 0, idcode = 0;
        uint8_t sr;
        if (g_status.state == 2u) {      /* 烧录中禁止探测 */
            proto_respond(cmd, RSP_ERR_STATE, 0, 0);
            break;
        }
        sr = TargetProg_Probe(&dpidr, &idcode);
        p[0] = sr;
        memcpy(&p[1], &dpidr,  4);
        memcpy(&p[5], &idcode, 4);
        proto_respond(cmd, RSP_OK, p, 9);
        break;
    }

    default:
        proto_respond(cmd, RSP_ERR_ARG, 0, 0);
        break;
    }
}

/* ------------------------- 主循环解析 ------------------------------------ */
uint8_t Proto_TakeProgRequest(void)
{
    uint8_t r = s_prog_req_flag;
    s_prog_req_flag = 0;
    return r;
}

void Proto_Init(void)
{
    s_rs = RS_H0;
    s_img_active = 0;
    s_prog_req_flag = 0;
}

static void proto_feed(uint8_t b)
{
    switch (s_rs) {
    case RS_H0:
        if (b == FRAME_HDR0) s_rs = RS_H1;
        break;
    case RS_H1:
        if (b == FRAME_HDR1) { s_rs = RS_CMD; s_crc_calc = 0xFFFFu; }
        else s_rs = RS_H0;
        break;
    case RS_CMD:
        s_cmd = b;
        s_crc_calc = CRC16_Update(s_crc_calc, &b, 1);
        s_rs = RS_L0;
        break;
    case RS_L0:
        s_len = b;
        s_crc_calc = CRC16_Update(s_crc_calc, &b, 1);
        s_rs = RS_L1;
        break;
    case RS_L1:
        s_len |= (uint16_t)b << 8;
        s_crc_calc = CRC16_Update(s_crc_calc, &b, 1);
        if (s_len > MAX_PAYLOAD) s_rs = RS_H0;      /* 超长丢弃, 重新找头 */
        else { s_pos = 0; s_rs = s_len ? RS_PAYLOAD : RS_C0; }
        break;
    case RS_PAYLOAD:
        s_payload[s_pos++] = b;
        s_crc_calc = CRC16_Update(s_crc_calc, &b, 1);
        if (s_pos >= s_len) s_rs = RS_C0;
        break;
    case RS_C0:
        s_crc_recv = b;
        s_rs = RS_C1;
        break;
    case RS_C1:
        s_crc_recv |= (uint16_t)b << 8;
        if (s_crc_recv == s_crc_calc) cmd_exec(s_cmd, s_payload, s_len);
        s_rs = RS_H0;
        break;
    default:
        s_rs = RS_H0;
        break;
    }
}

void Proto_Poll(void)
{
    while (s_rx_tail != s_rx_head) {
        uint8_t b = s_rx_ring[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1u) % RX_RING_SIZE);
        proto_feed(b);
    }
}

#endif /* USB_MODE == USB_MODE_CDC */
