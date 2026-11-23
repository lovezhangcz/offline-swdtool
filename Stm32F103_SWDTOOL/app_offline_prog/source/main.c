/**
  * @file  main.c
  * @brief 离线编程器主程序
  *
  *   工作流程:
  *   1. 固件镜像写入本机 flash 镜像区
  *      - CDC 模式: 上位机经 USB CDC 下发
  *      - MSC 模式: 用户把 .bin 文件拖入 U 盘
  *   2. 主循环每 DETECT_INTERVAL_MS 探测 SWCLK/SWDIO 上是否有目标芯片
  *   3. 探测到新目标且镜像有效 -> 自动擦除/烧录/校验/复位
  *   4. 一次接入只烧录一次, 目标拔出后重新待命
  *
  *   LED (PB12 低电平亮):
  *     无镜像        : 1Hz 慢闪
  *     待命(有镜像)  : 0.3s 亮 / 1.7s 灭 心跳
  *     烧录中        : 10Hz 快闪
  *     成功等待拔出  : 常亮
  *     失败等待拔出  : 三短闪循环
  */
#include "board.h"
#include "bsp.h"
#include "storage.h"
#include "swd_host.h"
#include "target_prog.h"

#if USB_MODE == USB_MODE_CDC
  #include "usb_cdc.h"
  #include "proto.h"
#else /* USB_MODE_MSC */
  #include "usb_msc.h"
  #include "vfs.h"
  #include "usb_hw.h"
#endif

/* 状态字节定义 (与 proto.h 中的定义一致, 这里也保留一份避免依赖) */
#define ST_NO_IMAGE        0u
#define ST_ARMED           1u
#define ST_PROGRAMMING     2u
#define ST_DONE_WAIT       3u
#define ST_FAIL_WAIT       4u

prog_status_t g_status;

static uint8_t s_attached;      /* 目标当前是否在线 */
static uint8_t s_state;

/* ---------------- LED 模式 ---------------- */
static void led_update(void)
{
    uint32_t ms = BSP_Millis();
    switch (s_state) {
    case ST_NO_IMAGE:                       /* 1Hz, 50% */
        if ((ms % 1000u) < 500u) LED_ON(); else LED_OFF();
        break;
    case ST_ARMED:                          /* 心跳: 0.2s 亮 / 1.8s 灭 */
        if ((ms % 2000u) < 200u) LED_ON(); else LED_OFF();
        break;
    case ST_PROGRAMMING:                    /* 10Hz 快闪 */
        if ((ms % 100u) < 50u) LED_ON(); else LED_OFF();
        break;
    case ST_DONE_WAIT:
        LED_ON();
        break;
    case ST_FAIL_WAIT: {                    /* 三短闪, 1.5s 周期 */
        uint32_t t = ms % 1500u;
        if ((t < 100u) || (t >= 200u && t < 300u) || (t >= 400u && t < 500u))
            LED_ON();
        else
            LED_OFF();
        break;
    }
    default:
        LED_OFF();
        break;
    }
}

/* ---------------- 烧录动作 ---------------- */
static void do_program(void)
{
    uint32_t err_addr = 0;
    uint8_t r;

#if USB_MODE == USB_MODE_CDC
    (void)Proto_TakeProgRequest();          /* 清掉烧录期间可能积下的手动触发 */
#endif
    s_state = ST_PROGRAMMING;
    g_status.state = ST_PROGRAMMING;
    g_status.progress = 0;

    r = TargetProg_Run(&err_addr);

    SWD_GPIO_Release();
    g_status.last_error = ((uint32_t)r) | (err_addr ? (err_addr & 0xFFFFFF00u) : 0u);

    if (r == TP_OK) {
        g_status.last_result = 1;
        s_state = ST_DONE_WAIT;
        s_attached = 1;                     /* 等拔出后再待命 */
    } else {
        g_status.last_result = 2;
        s_state = ST_FAIL_WAIT;
        s_attached = 1;
    }
    g_status.state = s_state;
}

/* ---------------- 主函数 ---------------- */
int main(void)
{
    uint32_t t_detect = 0;

    BSP_Init();

#if USB_MODE == USB_MODE_CDC
    Proto_Init();
    USB_CDC_Init();
#else /* USB_MODE_MSC */
    USB_MSC_InitDesc();
    VFS_Init();
    /* USB_HW_Init 在 USB_CDC_Init / MSC 初始化中调用?
     * 不, CDC 模式下 USB_CDC_Init 内部会调 USB_HW_Init.
     * MSC 模式下, 我们直接调用 USB_HW_Init. */
    USB_HW_Init();
#endif

    g_status.state = ST_NO_IMAGE;
    g_status.last_result = 0;
    g_status.last_error = 0;
    g_status.target_dpidr = 0;
    g_status.target_idcode = 0;
    g_status.progress = 0;
    g_status.swd_err = 0;

    s_state = Storage_ImageValid() ? ST_ARMED : ST_NO_IMAGE;
    g_status.state = s_state;

    for (;;) {
#if USB_MODE == USB_MODE_CDC
        Proto_Poll();                       /* 处理上位机命令 */
        USB_CDC_Poll();
#endif
        led_update();

        /* 镜像状态可能因上位机/U盘写入而改变 */
        if (s_state == ST_NO_IMAGE && Storage_ImageValid()) {
            s_state = ST_ARMED;
            g_status.state = s_state;
        } else if (s_state == ST_ARMED && !Storage_ImageValid()) {
            s_state = ST_NO_IMAGE;
            g_status.state = s_state;
        }

        if (s_state == ST_PROGRAMMING) continue;

#if USB_MODE == USB_MODE_CDC
        /* 手动触发 (CDC 模式下上位机可发 PROG_NOW 命令) */
        if (Proto_TakeProgRequest()) {
            if (Storage_ImageValid()) { do_program(); continue; }
        }
#endif

        /* 每 DETECT_INTERVAL_MS 探测目标 */
        if ((BSP_Millis() - t_detect) < DETECT_INTERVAL_MS) continue;
        t_detect = BSP_Millis();

        if (s_state == ST_ARMED && !s_attached) {
            if (TargetProg_Detect() == TP_OK) {
                SWD_GPIO_Release();
                s_attached = 1;
                do_program();               /* 新目标接入 -> 自动烧录 */
            } else {
                SWD_GPIO_Release();
            }
        } else if ((s_state == ST_DONE_WAIT || s_state == ST_FAIL_WAIT) && s_attached) {
            if (TargetProg_Detect() == TP_OK) {
                SWD_GPIO_Release();         /* 目标仍在线, 保持等待 */
            } else {
                SWD_GPIO_Release();
                s_attached = 0;             /* 已拔出 -> 重新待命 */
                s_state = Storage_ImageValid() ? ST_ARMED : ST_NO_IMAGE;
                g_status.state = s_state;
            }
        }
    }
}
