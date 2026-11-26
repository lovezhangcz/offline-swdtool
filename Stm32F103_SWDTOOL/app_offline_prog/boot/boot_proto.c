#if defined(BUILD_BOOT)
/**
  * @file  boot_proto.c
  * @brief boot 升级协议 (USB CDC 上的 ASCII 文本行 + 二进制数据块)
  *
  *   所有命令以 '\n' 结尾, 应答为一行 "OK" / "ERR n" / "INFO ..."。
  *
  *   INFO                    -> INFO ver=1.0.0 app=<0|1> len=<n> crc=<hex> cap=<n>
  *   ERASE <len>             -> 擦除应用区前 len 字节覆盖的页
  *   W <addr> <len> <crc16>  -> 随后跟 len 字节原始数据 (crc16-CCITT, init 0xFFFF)
  *   F <len> <crc32>         -> 校验整片 CRC32, 通过则写升级标志
  *   RUN                     -> 跳转应用
  *   STAY                    -> 留在 boot (不自动跳转)
  *
  *   错误码: 1=参数 2=flash 3=CRC 9=无有效应用
  */
#include <string.h>
#include "boot.h"
#include "bsp.h"
#include "usb_cdc.h"
#include "crc.h"

#define RX_RING_SIZE      1024u
#define LINE_MAX          96u
#define BOOT_CHUNK_MAX    1024u

/* ------------------------- 接收环 (USB ISR 写入) ------------------------- */
static uint8_t  s_ring[RX_RING_SIZE];
static volatile uint16_t s_head, s_tail;

void CDC_RxBytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        uint16_t next = (uint16_t)((s_head + 1u) % RX_RING_SIZE);
        if (next == s_tail) return;             /* 满则丢弃 */
        s_ring[s_head] = data[i];
        s_head = next;
    }
}

/* ------------------------- 行/裸数据缓冲 -------------------------------- */
static char     s_line[LINE_MAX];
static uint8_t  s_line_len;
static uint8_t  s_host_seen;
static volatile uint8_t s_busy;

static uint8_t  s_raw_active;
static uint32_t s_raw_addr, s_raw_len, s_raw_pos;
static uint16_t s_raw_crc;
static uint8_t  s_raw_buf[BOOT_CHUNK_MAX];

/* ------------------------- 发送 ----------------------------------------- */
static void send_bytes(const uint8_t *d, uint16_t n)
{
    uint32_t guard = 200000u;

    while (n && guard--) {
        uint16_t w = USB_CDC_Write(d, (n > 64u) ? 64u : n);
        if (w) { d += w; n = (uint16_t)(n - w); }
    }
}

static void send_str(const char *s)
{
    send_bytes((const uint8_t *)s, (uint16_t)strlen(s));
}

static void send_dec(uint32_t v)
{
    char b[11];
    uint8_t i = 0, k;

    if (v == 0) { send_bytes((const uint8_t *)"0", 1); return; }
    while (v) { b[i++] = (char)('0' + (v % 10u)); v /= 10u; }
    for (k = 0; k < i / 2u; k++) {
        char t = b[k]; b[k] = b[i - 1u - k]; b[i - 1u - k] = t;
    }
    send_bytes((const uint8_t *)b, i);
}

static const char hex_digits[] = "0123456789ABCDEF";

static void send_hex8(uint32_t v)
{
    char b[8];
    uint8_t i;

    for (i = 0; i < 8u; i++) b[i] = hex_digits[(v >> (28u - 4u * i)) & 0xFu];
    send_bytes((const uint8_t *)b, 8);
}

static void send_ok(void)  { send_str("OK\n"); }
static void send_err(uint8_t code)
{
    send_str("ERR ");
    send_dec(code);
    send_str("\n");
}

/* ------------------------- 参数解析 ------------------------------------- */
static char *next_tok(char **pp)
{
    char *p = *pp;
    char *s;

    while (*p == ' ') p++;
    if (*p == 0) { *pp = p; return 0; }
    s = p;
    while (*p && *p != ' ') p++;
    if (*p) { *p++ = 0; }
    *pp = p;
    return s;
}

static uint8_t parse_num(const char *s, uint32_t *out)
{
    uint32_t v = 0;
    uint8_t any = 0, hex = 0;

    if (!s) return 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { hex = 1; s += 2; }
    while (*s) {
        uint8_t d;
        if (*s >= '0' && *s <= '9') d = (uint8_t)(*s - '0');
        else if (hex && *s >= 'a' && *s <= 'f') d = (uint8_t)(*s - 'a' + 10);
        else if (hex && *s >= 'A' && *s <= 'F') d = (uint8_t)(*s - 'A' + 10);
        else return 0;
        v = hex ? ((v << 4) | d) : (v * 10u + d);
        any = 1;
        s++;
    }
    *out = v;
    return any;
}

/* ------------------------- 命令执行 ------------------------------------- */
static void cmd_info(void)
{
    boot_flag_t f;

    if (!BootFlag_Read(&f)) { f.len = 0; f.crc32 = 0; }

    send_str("INFO ver=");
    send_str(BOOT_VERSION_STR);
    send_str(" app=");
    send_dec(Boot_AppValid());
    send_str(" len=");
    send_dec(f.len);
    send_str(" crc=");
    send_hex8(f.crc32);
    send_str(" cap=");
    send_dec(APP_SIZE);
    /* 应用基址: 64KB/128KB 布局是 0x08001800, 大容量布局是 0x08001C00,
       上位机据此判断烧录地址, 避免把应用写到旧位置 */
    send_str(" base=0x");
    send_hex8(APP_BASE);
    send_str("\n");
}

static void cmd_erase(char **pp)
{
    uint32_t len;

    if (!parse_num(next_tok(pp), &len) || len == 0 || len > APP_SIZE) {
        send_err(1);
        return;
    }
    s_busy = 1;
    if (BootFlash_EraseApp(len) != 0) { s_busy = 0; send_err(2); return; }
    s_busy = 0;
    send_ok();
}

/* 裸数据接收完成: 校验 + 写 flash + 回读校验 */
static void raw_complete(void)
{
    uint16_t crc;
    uint8_t r;

    s_busy = 1;
    crc = CRC16_Update(0xFFFFu, s_raw_buf, s_raw_len);
    if (crc != s_raw_crc) {
        s_busy = 0;
        send_err(3);
        return;
    }
    r = BootFlash_Write(s_raw_addr, s_raw_buf, s_raw_len);
    if (r == 0) {
        crc = CRC16_Update(0xFFFFu, (const uint8_t *)s_raw_addr, s_raw_len);
        if (crc != s_raw_crc) r = 1;
    }
    s_busy = 0;
    if (r) send_err(2); else send_ok();
}

static void cmd_write(char **pp)
{
    uint32_t addr, len, crc;

    if (!parse_num(next_tok(pp), &addr) ||
        !parse_num(next_tok(pp), &len) ||
        !parse_num(next_tok(pp), &crc)) {
        send_err(1);
        return;
    }
    if (len == 0 || len > BOOT_CHUNK_MAX ||
        addr < APP_BASE || addr + len > APP_BASE + APP_SIZE) {
        send_err(1);
        return;
    }
    s_raw_addr = addr;
    s_raw_len  = len;
    s_raw_pos  = 0;
    s_raw_crc  = (uint16_t)crc;
    s_raw_active = 1;
    /* 应答在数据收完后发送 */
}

static void cmd_finish(char **pp)
{
    uint32_t len, crc;
    boot_flag_t f;

    if (!parse_num(next_tok(pp), &len) ||
        !parse_num(next_tok(pp), &crc) ||
        len == 0 || len > APP_SIZE) {
        send_err(1);
        return;
    }
    s_busy = 1;
    if (BootFlash_Crc32(APP_BASE, len) != crc) {
        s_busy = 0;
        send_err(3);
        return;
    }
    f.magic = BOOT_FLAG_MAGIC;
    f.len   = len;
    f.crc32 = crc;
    f.flag  = BOOT_FLAG_VALID;
    if (BootFlag_Write(&f) != 0) { s_busy = 0; send_err(2); return; }
    Boot_ClearRetry();                  /* 新应用已校验通过: 失败计数从头开始 */
    s_busy = 0;
    send_ok();
}

static void handle_line(char *line)
{
    char *p = line;
    char *cmd = next_tok(&p);

    s_host_seen = 1;
    if (!cmd) return;

    if (strcmp(cmd, "INFO") == 0) {
        cmd_info();
    } else if (strcmp(cmd, "ERASE") == 0) {
        cmd_erase(&p);
    } else if (strcmp(cmd, "W") == 0) {
        cmd_write(&p);
    } else if (strcmp(cmd, "F") == 0) {
        cmd_finish(&p);
    } else if (strcmp(cmd, "RUN") == 0) {
        if (!Boot_AppValid()) { send_err(9); return; }
        send_ok();
        BSP_DelayMs(50);
        Boot_JumpToApp();
    } else if (strcmp(cmd, "STAY") == 0) {
        send_ok();
    } else {
        send_err(1);
    }
}

/* ------------------------- 对外接口 ------------------------------------- */
void Boot_Init(void)
{
    s_head = s_tail = 0;
    s_line_len = 0;
    s_host_seen = 0;
    s_busy = 0;
    s_raw_active = 0;
}

void Boot_Poll(void)
{
    /* 收到任何字节就认为主机在线, 不再自动跳转 (避免上位机握手期间跳走) */
    if (s_tail != s_head) s_host_seen = 1;

    while (s_tail != s_head) {
        uint8_t b = s_ring[s_tail];
        s_tail = (uint16_t)((s_tail + 1u) % RX_RING_SIZE);

        if (s_raw_active) {
            s_raw_buf[s_raw_pos++] = b;
            if (s_raw_pos >= s_raw_len) {
                s_raw_active = 0;
                raw_complete();
            }
            continue;
        }
        if (b == '\r') continue;
        if (b == '\n') {
            if (s_line_len) {
                s_line[s_line_len] = 0;
                handle_line(s_line);
                s_line_len = 0;
            }
            continue;
        }
        if (b >= 0x20u && s_line_len < (LINE_MAX - 1u)) {
            s_line[s_line_len++] = (char)b;
        }
    }
}

uint8_t Boot_HostSeen(void) { return s_host_seen; }
uint8_t Boot_Busy(void)     { return s_busy; }

void Boot_SendBanner(uint8_t app_ok, uint8_t tries)
{
    send_str("BOOT ");
    send_str(BOOT_VERSION_STR);
    send_str(" app=");
    send_dec(app_ok);
    send_str(" try=");
    send_dec(tries);
    send_str("\n");
}
#endif
