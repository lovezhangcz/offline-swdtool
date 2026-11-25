#if !defined(BUILD_BOOT)
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
#include "log.h"
#include "storage.h"
#include "swd_host.h"
#include "target_prog.h"

#if USB_HAS_CDC
  #include "usb_cdc.h"
  #include "proto.h"
#endif
#if USB_HAS_MSC
  #include "usb_msc.h"
  #include "vfs.h"
  #include "usb_hw.h"
#endif
#if USB_MODE == USB_MODE_COMP
  #include "usb_comp.h"
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
static uint32_t s_done_crc;     /* 最近一次烧录用到的镜像 CRC32 */

/* ---------------- 异常处理: 跑飞后自动回 boot ---------------- */
/* 应用启动后若发生硬件异常, 唯一的自救通道是 USB 升级, 所以置"进 boot"标记后复位。
   boot 看到标记会常驻 30s 等主机升级, 不会被坏应用锁死在 HardFault 里。 */
static void fault_to_boot(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    BKP_REG_FORCE = BOOT_FORCE_MAGIC;
    NVIC_SystemReset();
    for (;;) { }
}

void HardFault_Handler(void)  { fault_to_boot(); }
void MemManage_Handler(void)  { fault_to_boot(); }
void BusFault_Handler(void)   { fault_to_boot(); }
void UsageFault_Handler(void) { fault_to_boot(); }

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

#if USB_HAS_CDC
    (void)Proto_TakeProgRequest();          /* 清掉烧录期间可能积下的手动触发 */
#endif
    s_state = ST_PROGRAMMING;
    g_status.state = ST_PROGRAMMING;
    g_status.progress = 0;

    r = TargetProg_Run(&err_addr);

    /* 记录本次烧录对应的镜像, 供"U盘换固件后重烧"判断 */
    s_done_crc = Storage_ImageValid() ? Storage_Header()->crc32 : 0u;

    SWD_GPIO_Release();
    g_status.last_error = ((uint32_t)r) | (err_addr ? (err_addr & 0xFFFFFF00u) : 0u);

    if (r == TP_OK) {
        g_status.last_result = 1;
        s_state = ST_DONE_WAIT;
        s_attached = 1;                     /* 等拔出后再待命 */
        Log_Str("PROG OK\n");
    } else {
        g_status.last_result = 2;
        s_state = ST_FAIL_WAIT;
        s_attached = 1;
        Log_Str("PROG ERR code=");
        Log_Dec(r);
        Log_Str(" addr=0x");
        Log_Hex(err_addr);
        Log_Str("\n");
    }
    g_status.state = s_state;
}

/* ---------------- 主函数 ---------------- */
int main(void)
{
    uint32_t t_detect = 0;

#ifdef BUILD_APP
    /* 应用链接在 0x08004000, 必须在开启任何中断前重定位向量表 */
    SCB->VTOR = APP_BASE;
#endif

    BSP_Init();
    Log_Init();
    Log_Str("APP v");
    Log_Str(APP_VERSION_STR);
    Log_Str(" ");
    Log_Str(__DATE__);
    Log_Str(" ");
    Log_Str(__TIME__);
    Log_Str("\n");

#if USB_MODE == USB_MODE_COMP
    /* 复合设备: CDC 命令通道 + MSC U 盘同时可用 */
    Proto_Init();
    VFS_Init();
    USB_Comp_Init();
#elif USB_HAS_CDC
    Proto_Init();
    USB_CDC_Init();
#else /* USB_MODE_MSC */
    USB_MSC_InitDesc();
    VFS_Init();
    USB_HW_Init();
#endif

#ifdef BUILD_APP
    /* USB 已初始化完毕, 认为应用启动成功: 清除 boot 的连续跳转失败计数。
       若应用在到达这里之前跑飞, 计数会保留, boot 连续 3 次后会驻留等待升级。 */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    BKP_REG_BOOTTRY = 0;
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
    Log_Str("IMG ");
    Log_Dec(s_state == ST_ARMED);
    Log_Str("\n");

    for (;;) {
#if USB_HAS_CDC
        Proto_Poll();                       /* 处理上位机命令 */
        USB_CDC_Poll();
#endif
        led_update();

#if USB_HAS_CDC
        /* 示波器时钟测试模式: 持续输出 SWCLK/SWDIO 方波, 暂停目标探测 */
        if (g_clk_test) {
            SWD_ClkTestBurst(10u);
            continue;
        }
#endif

        /* 镜像状态可能因上位机/U盘写入而改变 */
        if (s_state == ST_NO_IMAGE && Storage_ImageValid()) {
            s_state = ST_ARMED;
            g_status.state = s_state;
            Log_Str("IMG 1\n");
        } else if (s_state == ST_ARMED && !Storage_ImageValid()) {
            s_state = ST_NO_IMAGE;
            g_status.state = s_state;
            Log_Str("IMG 0\n");
        }

        if (s_state == ST_PROGRAMMING) continue;

#if USB_HAS_CDC
        /* 手动触发 (上位机可发 PROG_NOW 命令) */
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
            /* U 盘里换了新固件 (CRC 变化): 目标仍在线也重新烧录 */
            if (Storage_ImageValid() && Storage_Header()->crc32 != s_done_crc) {
                do_program();
                continue;
            }
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
#endif
