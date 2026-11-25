#if !defined(BUILD_BOOT)
/**
  * @file  proto.c
  * @brief 上位机通信协议实现
  */
#include <string.h>
#include "proto.h"
#include "usb_cdc.h"
#include "storage.h"
#include "target_prog.h"
#include "swd_host.h"
#include "crc.h"
#include "board.h"
#include "bsp.h"
#include "log.h"

#if USB_HAS_CDC

#define FRAME_HDR0   0xAAu
#define FRAME_HDR1   0x55u

#define CMD_PING        0x01u
#define CMD_GET_INFO    0x02u
#define CMD_GET_VERSION 0x03u
#define CMD_IMG_BEGIN   0x10u
#define CMD_IMG_DATA    0x11u
#define CMD_IMG_END     0x12u
#define CMD_IMG_CLEAR   0x13u
#define CMD_PROG_NOW    0x20u
#define CMD_GET_STATUS  0x21u
#define CMD_TGT_PROBE   0x22u
#define CMD_REBOOT_BOOT 0x23u
#define CMD_LOG_POLL    0x24u
#define CMD_CLK_TEST    0x25u
#define CMD_FLASH_TEST  0x26u
#define CMD_STRM_BEGIN  0x30u
#define CMD_STRM_DATA   0x31u
#define CMD_STRM_END    0x32u
#define CMD_TGT_ERASE   0x33u
#define CMD_TGT_READ    0x34u
#define CMD_TGT_DETECT  0x35u

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

/* usb_cdc.c 的接收回调 (boot 工程里由 boot_proto.c 提供同名实现) */
void CDC_RxBytes(const uint8_t *data, uint16_t len)
{
    Proto_RxBytes(data, len);
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

#if DBG_TOOLS
/* ---------------- 本机 Flash 实际容量自检 (仅调试) ----------------
   芯片自报的 F_SIZE 不可靠 (F103C8 常见 64KB 报 64 但实际 128KB),
   所以在 64KB 之上的 1KB 页做一次 写入->回读, 用结果判定。
   返回 0=OK, 1=解锁/擦除失败, 2=编程失败, 3=回读不一致, 4=该页不存在 */
static uint8_t flash_wait(uint32_t ms)
{
    uint32_t t0 = BSP_Millis();
    while (FLASH->SR & FLASH_SR_BSY) {          /* 地址不存在时 BSY 可能不释放, 必须超时 */
        WDT_Feed();
        if ((BSP_Millis() - t0) > ms) return 1;
    }
    return 0;
}

static uint8_t flash_page_test(uint32_t page)
{
    static const uint16_t pat[8] = { 0x1234u, 0x5678u, 0x9ABCu, 0xDEF0u,
                                     0x0F1Eu, 0x2D3Cu, 0x55AAu, 0xAA55u };
    volatile uint16_t *p = (volatile uint16_t *)page;
    uint32_t i;
    uint8_t  r = 0;

    /* 先探测该页首尾是否可读且为空白, 不存在/别名则直接放弃 */
    if (p[0] != 0xFFFFu || p[511] != 0xFFFFu) return 4;

    FLASH->KEYR = 0x45670123u;
    FLASH->KEYR = 0xCDEF89ABu;
    if (FLASH->CR & FLASH_CR_LOCK) return 1;

    /* 只做"空页写入->回读": 不擦除。地址不存在时擦除会把 FPEC 卡在 BSY,
       写入最坏只是置 PGERR 标志, 不会把设备挂死。 */
    FLASH->SR = 0xFFFFFFFFu;
    FLASH->CR |= FLASH_CR_PG;
    for (i = 0; i < 8u; i++) {
        p[i] = pat[i];
        if (flash_wait(100u)) { r = 2; break; }
    }
    FLASH->CR &= ~FLASH_CR_PG;
    if (FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) r = 2;
    FLASH->SR = FLASH_SR_PGERR | FLASH_SR_WRPRTERR;

    if (r == 0) {
        for (i = 0; i < 8u; i++) {
            if (p[i] != pat[i]) { r = 3; break; }
        }
    }
    FLASH->CR |= FLASH_CR_LOCK;
    return r;
}
#endif /* DBG_TOOLS */

/* ------------------------- 镜像下载会话 ---------------------------------- */
static uint32_t s_img_len, s_img_crc, s_img_addr, s_img_received;
static uint8_t  s_img_active;

/* 置标记后软复位: boot 看到标记直接进入升级模式 */
static void app_reboot_to_boot(void)
{
    Log_Str("REBOOT BOOT\n");

    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    BKP_REG_FORCE = BOOT_FORCE_MAGIC;

#ifdef USB_PULLUP_PORT
  #if USB_PULLUP_ACTIVE
    USB_PULLUP_PORT->BRR  = (1u << USB_PULLUP_PIN);
  #else
    USB_PULLUP_PORT->BSRR = (1u << USB_PULLUP_PIN);
  #endif
#endif
}

static void cmd_exec(uint8_t cmd, const uint8_t *pl, uint16_t len)
{
    uint8_t p[24];

    switch (cmd) {
    case CMD_PING:
        proto_respond(cmd, RSP_OK, (const uint8_t *)"SWDPROG1", 8);
        break;

    case CMD_GET_VERSION: {
        static const char vs[] = "v" APP_VERSION_STR " " __DATE__ " " __TIME__;
        proto_respond(cmd, RSP_OK, (const uint8_t *)vs, (uint16_t)(sizeof(vs) - 1u));
        break;
    }

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
        {   /* 本机 Flash 自报容量(KB): F103C8 可能报 64 但实际更大, 仅供参考 */
            uint32_t fsz = (uint32_t)(*(volatile uint16_t *)0x1FFFF7E0u);
            memcpy(&p[19], &fsz, 4);
        }
        proto_respond(cmd, RSP_OK, p, 23);
        break;
    }

    case CMD_IMG_BEGIN:
        /* 12B 头 + 可选文件名 (U 盘模式下显示同名) */
        if (len < 12u) { proto_respond(cmd, RSP_ERR_ARG, 0, 0); break; }
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
        /* 文件名: 与 U 盘写入保持一致, 便于在 U 盘里看到同一文件名 */
        if (len > 12u) {
            Storage_SetImageName((const char *)&pl[12], (uint32_t)(len - 12u));
        } else {
            Storage_SetImageName("FIRMWARE.BIN", 12u);
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
        r = Storage_Write(IMG_DATA_OFFSET + off, &pl[4], (uint32_t)(len - 4u));
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

    case CMD_IMG_CLEAR:
        /* 删除烧录器里存的目标固件: 擦掉镜像头所在页即可让镜像失效,
           主循环下一轮会切到"无镜像"状态, 之后不会再对任何目标自动烧录 */
        if (g_status.state == 2u) {          /* 烧录中不允许删 */
            proto_respond(cmd, RSP_ERR_STATE, 0, 0);
            break;
        }
        if (Storage_ErasePage(0u) != STORAGE_OK) {
            proto_respond(cmd, RSP_ERR_FLASH, 0, 0);
            break;
        }
        s_img_active = 0;
        g_status.last_result = 0;
        g_status.last_error = 0;
        Log_Str("IMG CLEAR\n");
        proto_respond(cmd, RSP_OK, 0, 0);
        break;

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

    case CMD_REBOOT_BOOT:
        proto_respond(cmd, RSP_OK, 0, 0);   /* 先把应答排进发送队列 */
        BSP_DelayMs(80);
        app_reboot_to_boot();
        BSP_DelayMs(120);                   /* 让主机看到 USB 断开 */
        NVIC_SystemReset();
        break;

#if DBG_TOOLS
    case CMD_CLK_TEST:
        /* 示波器测试: payload[0]!=0 开启持续输出 SWCLK/SWDIO 方波, 0 关闭 */
        g_clk_test = (uint8_t)((len >= 1u && pl[0]) ? 1u : 0u);
        proto_respond(cmd, RSP_OK, 0, 0);
        break;
#endif

    case CMD_LOG_POLL: {
        uint8_t lb[48];
        uint16_t n = Log_Read(lb, (uint16_t)sizeof(lb));
        proto_respond(cmd, RSP_OK, lb, n);
        break;
    }

    case CMD_STRM_BEGIN: {
        uint32_t n_len, n_crc, n_addr, err = 0;
        uint8_t sr;
        if (len != 12u) { proto_respond(cmd, RSP_ERR_ARG, 0, 0); break; }
        if (g_status.state == 2u) { proto_respond(cmd, RSP_ERR_STATE, 0, 0); break; }
        memcpy(&n_len,  &pl[0], 4);
        memcpy(&n_crc,  &pl[4], 4);
        memcpy(&n_addr, &pl[8], 4);
        g_status.state = 2u;
        g_status.progress = 0;
        g_status.last_result = 0;
        sr = TargetStream_Begin(n_len, n_crc, n_addr, &err);
        if (sr != TP_OK) {
            SWD_GPIO_Release();
            g_status.last_result = 2;
            g_status.last_error = ((uint32_t)sr) | (err & 0xFFFFFF00u);
            g_status.state = 4u;
        }
        p[0] = sr;
        memcpy(&p[1], &g_target_flash_kb, 4);
        memcpy(&p[5], &err, 4);
        proto_respond(cmd, RSP_OK, p, 9);
        break;
    }

    case CMD_STRM_DATA: {
        uint32_t off, err = 0;
        uint8_t sr;
        if (g_status.state != 2u || len < 5u) {
            proto_respond(cmd, RSP_ERR_STATE, 0, 0);
            break;
        }
        memcpy(&off, pl, 4);
        sr = TargetStream_Data(off, &pl[4], (uint16_t)(len - 4u), &err);
        if (sr != TP_OK) {
            SWD_GPIO_Release();
            g_status.last_result = 2;
            g_status.last_error = ((uint32_t)sr) | (err & 0xFFFFFF00u);
            g_status.state = 4u;
        }
        p[0] = sr;
        memcpy(&p[1], (const void *)&g_status.progress, 4);
        memcpy(&p[5], &err, 4);
        proto_respond(cmd, RSP_OK, p, 9);
        break;
    }

    case CMD_STRM_END: {
        uint32_t err = 0;
        uint8_t sr;
        if (g_status.state != 2u) { proto_respond(cmd, RSP_ERR_STATE, 0, 0); break; }
        sr = TargetStream_End(&err);
        g_status.last_error = ((uint32_t)sr) | (err & 0xFFFFFF00u);
        g_status.last_result = (sr == TP_OK) ? 1u : 2u;
        g_status.state = (sr == TP_OK) ? 3u : 4u;
        if (sr == TP_OK) {
            Log_Str("STRM OK\n");
        } else {
            Log_Str("STRM ERR code=");
            Log_Dec(sr);
            Log_Str("\n");
        }
        p[0] = sr;
        memcpy(&p[1], &err, 4);
        proto_respond(cmd, RSP_OK, p, 5);
        break;
    }

    case CMD_TGT_READ: {
        /* 诊断: 直接读目标存储器 <- addr(4) len(4,<=48), 返回原始字节 */
        uint32_t addr, n, off = 0;
        uint8_t rb[48];
        if (g_status.state == 2u) {
            proto_respond(cmd, RSP_ERR_STATE, 0, 0);
            break;
        }
        if (len != 8u) { proto_respond(cmd, RSP_ERR_ARG, 0, 0); break; }
        memcpy(&addr, &pl[0], 4);
        memcpy(&n,    &pl[4], 4);
        if (n == 0u || n > sizeof(rb) || (addr & 3u) != 0u || (n & 3u) != 0u) {
            proto_respond(cmd, RSP_ERR_ARG, 0, 0);
            break;
        }
        {
            uint32_t dp;
            uint8_t sr = SWD_Connect(&dp);
            if (sr != SWD_OK) {
                SWD_GPIO_Release();
                proto_respond(cmd, RSP_ERR_FLASH, 0, 0);
                break;
            }
            while (off < n) {
                uint32_t nw = (n - off) / 4u;
                if (nw > 12u) nw = 12u;
                if (SWD_Mem_ReadBlock(addr + off, (uint32_t *)(rb + off), nw) != SWD_OK)
                    break;
                off += nw * 4u;
            }
            SWD_GPIO_Release();
        }
        if (off != n) { proto_respond(cmd, RSP_ERR_FLASH, rb, (uint16_t)off); break; }
        proto_respond(cmd, RSP_OK, rb, (uint16_t)n);
        break;
    }

    case CMD_TGT_ERASE: {
        uint8_t sr;
        if (g_status.state == 2u) {
            proto_respond(cmd, RSP_ERR_STATE, 0, 0);
            break;
        }
        g_status.state = 2u;
        g_status.progress = 0;
        g_status.last_result = 0;
        sr = TargetEraseChip();
        g_status.last_error = sr;
        g_status.last_result = (sr == TP_OK) ? 1u : 2u;
        g_status.state = (sr == TP_OK) ? 3u : 4u;
        if (sr == TP_OK) {
            Log_Str("ERASE OK\n");
        } else {
            Log_Str("ERASE ERR code=");
            Log_Dec(sr);
            Log_Str("\n");
        }
        p[0] = sr;
        memcpy(&p[1], &g_target_flash_kb, 4);
        proto_respond(cmd, RSP_OK, p, 5);
        break;
    }

#if DBG_TOOLS
    case CMD_FLASH_TEST: {
        uint32_t fsz = (uint32_t)(*(volatile uint16_t *)0x1FFFF7E0u);
        p[0] = (uint8_t)(fsz > 255u ? 255u : fsz);   /* 芯片自报 KB */
        p[1] = flash_page_test(0x08010000u);         /* 64KB 之上第一页 */
        p[2] = flash_page_test(0x08020000u);         /* 128KB 之上第一页 */
        proto_respond(cmd, RSP_OK, p, 3);
        break;
    }
#endif

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

#endif /* USB_HAS_CDC */
#endif
