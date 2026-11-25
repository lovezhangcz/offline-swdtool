#if !defined(BUILD_BOOT)
/**
  * @file  log.c
  * @brief 诊断日志环形缓冲
  *
  *   写入只在主循环上下文 (main/proto) 发生, 因此无需临界区。
  *   缓冲满时丢弃新内容, 不阻塞业务流程。
  */
#include "log.h"

#define LOG_SIZE   768u

static uint8_t  s_buf[LOG_SIZE];
static uint16_t s_head, s_tail;

void Log_Init(void)
{
    s_head = s_tail = 0;
}

static void log_put(uint8_t b)
{
    uint16_t next = (uint16_t)((s_head + 1u) % LOG_SIZE);
    if (next == s_tail) return;          /* 满则丢 */
    s_buf[s_head] = b;
    s_head = next;
}

void Log_Str(const char *s)
{
    while (*s) log_put((uint8_t)*s++);
}

void Log_Dec(uint32_t v)
{
    char b[11];
    uint8_t i = 0, k;

    if (v == 0) { log_put('0'); return; }
    while (v) { b[i++] = (char)('0' + (v % 10u)); v /= 10u; }
    for (k = 0; k < i; k++) log_put((uint8_t)b[i - 1u - k]);
}

void Log_Hex(uint32_t v)
{
    static const char h[] = "0123456789ABCDEF";
    uint8_t i;

    for (i = 0; i < 8u; i++) log_put((uint8_t)h[(v >> (28u - 4u * i)) & 0xFu]);
}

uint16_t Log_Read(uint8_t *buf, uint16_t max)
{
    uint16_t n = 0;

    while (n < max && s_tail != s_head) {
        buf[n++] = s_buf[s_tail];
        s_tail = (uint16_t)((s_tail + 1u) % LOG_SIZE);
    }
    return n;
}
#endif
