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
#define CMD_REC_TEST    0x27u   /* DBG_TOOLS: 烧录流水记录自检 */
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

/* main.c 中固定在 APP_FW_INFO_ADDR 处的应用固件识别码/版本 */
extern const app_fw_info_t g_app_fw_info;

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
    uint8_t  dirty = (uint8_t)(p[0] != 0xFFFFu || p[511] != 0xFFFFu);

    /* 非空白页: 只接受"上次自检留下的图案"(说明该页存在且是我们写的),
       其它内容一律判为不可用, 绝不误擦真正的数据页 */
    if (dirty && p[0] != pat[0]) return 4;

    FLASH->KEYR = 0x45670123u;
    FLASH->KEYR = 0xCDEF89ABu;
    if (FLASH->CR & FLASH_CR_LOCK) return 1;

    /* 残留图案: 先擦除 (该页已确认存在), 恢复空白后再测 */
    if (dirty) {
        FLASH->SR = 0xFFFFFFFFu;
        FLASH->CR |= FLASH_CR_PER;
        FLASH->AR = page;
        FLASH->CR |= FLASH_CR_STRT;
        if (flash_wait(1000u)) { FLASH->CR |= FLASH_CR_LOCK; return 1; }
        FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_STRT);
        FLASH->SR = 0xFFFFFFFFu;
        if (p[0] != 0xFFFFu || p[511] != 0xFFFFu) { FLASH->CR |= FLASH_CR_LOCK; return 3; }
    }

    /* 只做"空页写入->回读": 地址不存在时擦除会把 FPEC 卡在 BSY,
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

    /* 清掉测试图案, 把该页恢复为空白, 便于反复自检 */
    if (r == 0) {
        FLASH->SR = 0xFFFFFFFFu;
        FLASH->CR |= FLASH_CR_PER;
        FLASH->AR = page;
        FLASH->CR |= FLASH_CR_STRT;
        (void)flash_wait(1000u);
        FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_STRT);
        FLASH->SR = 0xFFFFFFFFu;
    }
    FLASH->CR |= FLASH_CR_LOCK;
    return r;
}
/* 页大小探测 (只看, 不改布局):
   擦 0x08010000 后往 0x08010400 写一个半字, 再擦 0x08010000,
   0x08010400 被一起擦掉 => 物理页 2KB; 仍是 0x1234 => 1KB 页 */
static uint8_t flash_pagesize_test(void)
{
    volatile uint16_t *b = (volatile uint16_t *)0x08010400u;
    uint8_t r;

    FLASH->KEYR = 0x45670123u;
    FLASH->KEYR = 0xCDEF89ABu;
    if (FLASH->CR & FLASH_CR_LOCK) return 2u;

    FLASH->SR = 0xFFFFFFFFu;
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = 0x08010000u;
    FLASH->CR |= FLASH_CR_STRT;
    if (flash_wait(1000u)) { FLASH->CR |= FLASH_CR_LOCK; return 3u; }
    FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_STRT);
    FLASH->SR = 0xFFFFFFFFu;

    if (*b != 0xFFFFu) {                 /* 1KB 页: 后半个页还是旧数据, 单独擦 */
        FLASH->CR |= FLASH_CR_PER;
        FLASH->AR  = 0x08010400u;
        FLASH->CR |= FLASH_CR_STRT;
        if (flash_wait(1000u)) { FLASH->CR |= FLASH_CR_LOCK; return 3u; }
        FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_STRT);
        FLASH->SR = 0xFFFFFFFFu;
        if (*b != 0xFFFFu) { FLASH->CR |= FLASH_CR_LOCK; return 4u; }
    }

    FLASH->CR |= FLASH_CR_PG;
    *b = 0x1234u;
    if (flash_wait(100u)) { FLASH->CR &= ~FLASH_CR_PG; FLASH->CR |= FLASH_CR_LOCK; return 5u; }
    FLASH->CR &= ~FLASH_CR_PG;
    if (*b != 0x1234u) { FLASH->CR |= FLASH_CR_LOCK; return 6u; }

    FLASH->SR = 0xFFFFFFFFu;
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = 0x08010000u;
    FLASH->CR |= FLASH_CR_STRT;
    if (flash_wait(1000u)) { FLASH->CR |= FLASH_CR_LOCK; return 7u; }
    FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_STRT);
    FLASH->SR = 0xFFFFFFFFu;

    r = (*b == 0xFFFFu) ? 1u : 0u;       /* 1 = 2KB 页, 0 = 1KB 页 */
    FLASH->CR |= FLASH_CR_LOCK;
    return r;
}
/* 上半个芯片的页扫描: 每 4KB 取一页, 擦除+确认空白,
   返回第一个擦不掉的页地址 (0xFFFFFFFF = 全部可用) */
static uint32_t flash_scan_pages(void)
{
    uint32_t addr, i;
    uint8_t  r = 0;

    FLASH->KEYR = 0x45670123u;
    FLASH->KEYR = 0xCDEF89ABu;
    if (FLASH->CR & FLASH_CR_LOCK) return 0xFFFFFFFEu;

    for (addr = 0x08010000u; addr < 0x08040000u; addr += 0x1000u) {
        FLASH->SR = 0xFFFFFFFFu;
        FLASH->CR |= FLASH_CR_PER;
        FLASH->AR  = addr;
        FLASH->CR |= FLASH_CR_STRT;
        r = flash_wait(1000u);
        FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_STRT);
        FLASH->SR = 0xFFFFFFFFu;
        if (r != 0) break;
        for (i = 0; i < FLASH_PAGE_SIZE; i += 4u) {
            if (*(volatile uint32_t *)(addr + i) != 0xFFFFFFFFu) { r = 9; break; }
        }
        if (r != 0) break;
    }
    FLASH->CR |= FLASH_CR_LOCK;
    return (r == 0) ? 0xFFFFFFFFu : addr;
}

#endif /* DBG_TOOLS */

/* ------------------------- 镜像下载会话 ---------------------------------- */
static uint32_t s_img_len, s_img_crc, s_img_addr, s_img_received;
static uint32_t s_img_trailer;      /* 载荷尾部随文件携带的信息长度 (不烧进目标) */
static uint8_t  s_img_active;

/* 上位机在线判定: 3 秒内收到过任何一条命令即视为在线。
   上位机 (online_prog.py) 平时以 4Hz 轮询状态, 天然维持心跳。 */
#define HOST_TIMEOUT_MS   3000u
static uint32_t s_host_ms;
static uint8_t  s_host_on;

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
    uint8_t p[40];

    /* 收到上位机命令 -> 判定上位机在线 (超时由 Proto_Poll 复位) */
    s_host_ms = BSP_Millis();
    if (!s_host_on) {
        s_host_on = 1;
        g_status.host_online = 1;
        Log_Str("HOST 1\n");
    }

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
        {   /* 当前离线固件的累计成功烧录次数 (2KB 流水记录区) */
            uint32_t cnt = Storage_RecordCount();
            memcpy(&p[23], &cnt, 4);
        }
        {   /* 镜像尾部随文件携带的信息: 目标芯片代码 + 尾部长度
               (U盘写入的老镜像没有该信息, 芯片代码报 0xFFFFFFFF 表示"未携带") */
            uint32_t tr = h->reserved[0];
            uint32_t chip = 0xFFFFFFFFu;
            if (!valid || tr > 512u) tr = 0u;
            if (valid && tr >= 4u && tr <= IMG_MAX_PAYLOAD - h->length) {
                memcpy(&chip, Storage_PayloadPtr() + h->length, 4);
            }
            memcpy(&p[27], &chip, 4);
            p[31] = (uint8_t)tr;
        }
        {   /* 应用固件识别码 + 版本 (镜像固定偏移 0x100 处, 上位机升级前比较) */
            memcpy(&p[32], &g_app_fw_info.version, 4);
            memcpy(&p[36], &g_app_fw_info.magic,   4);
        }
        proto_respond(cmd, RSP_OK, p, 40);
        break;
    }

    case CMD_IMG_BEGIN:
        /* 16B 头: len(4) crc(4) load(4) trailer(4) + 可选文件名
           trailer = 载荷尾部随镜像携带的信息长度 (芯片代码 4B + 保留 64B),
           只存储不烧录; 老上位机不带该字段时按 0 处理 */
        if (len < 16u) { proto_respond(cmd, RSP_ERR_ARG, 0, 0); break; }
        if (g_status.state == 2u) {      /* 烧录中禁止更新镜像 */
            proto_respond(cmd, RSP_ERR_STATE, 0, 0);
            break;
        }
        memcpy(&s_img_len,  &pl[0], 4);
        memcpy(&s_img_crc,  &pl[4], 4);
        memcpy(&s_img_addr, &pl[8], 4);
        memcpy(&s_img_trailer, &pl[12], 4);
        if (s_img_len == 0 || s_img_len > IMG_MAX_PAYLOAD ||
            (s_img_addr & 0xFF000000u) != 0x08000000u ||
            s_img_trailer >= s_img_len || s_img_trailer > 512u) {
            proto_respond(cmd, RSP_ERR_ARG, 0, 0);
            break;
        }
        /* 只擦除本镜像用到的页 (头页 + 数据), 不整片擦除:
           256KB 布局整片擦除要好几秒, 且没有必要 */
        {
            uint32_t off, need = IMG_DATA_OFFSET + s_img_len;
            for (off = 0; off < need; off += FLASH_PAGE_SIZE) {
                if (Storage_ErasePage(off) != STORAGE_OK) break;
            }
            if (off < need) {
                proto_respond(cmd, RSP_ERR_FLASH, 0, 0);
                break;
            }
        }
        /* 换了离线固件: 该固件的烧录流水记录从头开始 */
        (void)Storage_RecordErase();
        /* 文件名: 与 U 盘写入保持一致, 便于在 U 盘里看到同一文件名 */
        if (len > 16u) {
            Storage_SetImageName((const char *)&pl[16], (uint32_t)(len - 16u));
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
        uint32_t crc, img_len, chip = 0;
        img_hdr_t hdr;
        if (!s_img_active || s_img_received != s_img_len) {
            s_img_active = 0;
            proto_respond(cmd, RSP_ERR_STATE, 0, 0);
            break;
        }
        /* 载荷 = 镜像 + 尾部信息; CRC 只覆盖真正要烧录的镜像部分 */
        img_len = s_img_len - s_img_trailer;
        crc = CRC32_Update(0xFFFFFFFFu, Storage_PayloadPtr(), img_len) ^ 0xFFFFFFFFu;
        if (crc != s_img_crc) {
            s_img_active = 0;
            proto_respond(cmd, RSP_ERR_CRC, 0, 0);
            break;
        }
        /* 尾部信息: 前 4 字节是目标芯片代码 (0=自动识别) */
        if (s_img_trailer >= 4u) {
            memcpy(&chip, Storage_PayloadPtr() + img_len, 4);
        }
        /* 写入头使镜像生效 */
        hdr.magic      = IMG_MAGIC;
        hdr.length     = img_len;           /* 只算镜像: SWD 烧录与校验都用它 */
        hdr.crc32      = s_img_crc;
        hdr.load_addr  = s_img_addr;
        hdr.prog_count = 0;
        hdr.reserved[0] = s_img_trailer;    /* 尾部信息长度 (0=没有) */
        hdr.reserved[1] = chip;             /* 目标芯片代码 */
        hdr.reserved[2] = 0;
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
        /* 镜像已删除: 该镜像的烧录流水记录一并清掉, 计数从 0 开始 */
        (void)Storage_RecordErase();
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
        /* 扩展: 烧录尝试序号 + 上位机在线标志 (老上位机只读前 22 字节) */
        memcpy(&p[22], (const void *)&g_status.burn_seq,     4);
        p[26] = g_status.host_online;
        proto_respond(cmd, RSP_OK, p, 27);
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
        g_status.burn_seq++;            /* 流式烧录也是一次烧录尝试 */
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
            /* 流式烧录成功也计入同一套流水记录, 便于上位机做生产计数 */
            (void)Storage_RecordBurn();
            Log_Str("BURN cnt=");
            Log_Dec(Storage_RecordCount());
            Log_Str("\n");
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
    case CMD_REC_TEST: {
        /* 自检: 连续记 n 次成功烧录, 返回累计次数 (验证流水页/进位) */
        uint32_t n = (len >= 2u) ? ((uint32_t)pl[0] | ((uint32_t)pl[1] << 8)) : 1u;
        uint32_t i, cnt;
        for (i = 0; i < n; i++) (void)Storage_RecordBurn();
        cnt = Storage_RecordCount();
        memcpy(&p[0], &cnt, 4);
        proto_respond(cmd, RSP_OK, p, 4);
        break;
    }

    case CMD_FLASH_TEST: {
        uint32_t fsz = (uint32_t)(*(volatile uint16_t *)0x1FFFF7E0u);
        uint32_t idc = *(volatile uint32_t *)0xE0042000u;   /* DBGMCU_IDCODE */
        uint32_t pid = *(volatile uint32_t *)0x40022100u;   /* FMC/FMC_PID */
        p[0] = (uint8_t)(fsz > 255u ? 255u : fsz);   /* 芯片自报 KB */
        p[1] = flash_page_test(0x08010000u);         /* 64KB 之上第一页 */
        p[2] = flash_page_test(0x08020000u);         /* 128KB 之上第一页 */
        /* 256KB 之上第一页: 只有 128KB 之上可用才测 (读不存在的地址会总线异常) */
        p[3] = (p[2] == 0u) ? flash_page_test(0x08040000u) : 4u;
        memcpy(&p[4], &idc, 4);
        memcpy(&p[8], &pid, 4);
        p[12] = flash_pagesize_test();   /* 0=1KB 页, 1=2KB 页, >1=出错 */
        {
            uint32_t bad = flash_scan_pages();
            uint32_t wrp = FLASH->WRPR;
            memcpy(&p[13], &bad, 4);
            memcpy(&p[17], &wrp, 4);
        }
        proto_respond(cmd, RSP_OK, p, 21);
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
    s_host_on = 0;
    s_host_ms = 0;
    g_status.host_online = 0;
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

    /* 上位机超时未发命令 -> 判定离线 (拔掉 USB/上位机退出) */
    if (s_host_on && (BSP_Millis() - s_host_ms) > HOST_TIMEOUT_MS) {
        s_host_on = 0;
        g_status.host_online = 0;
        Log_Str("HOST 0\n");
    }
}

#endif /* USB_HAS_CDC */
#endif
