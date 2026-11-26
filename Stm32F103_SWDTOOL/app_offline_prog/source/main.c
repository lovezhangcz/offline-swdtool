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
#define ST_TARGET_SEEN     5u      /* 已识别目标, 稳定电源中 (LED 双闪, 仅本地显示) */

prog_status_t g_status;

/* 应用固件版本信息: 用 ARMCC 的 at() 属性固定在镜像偏移 0x100 处
 * (APP_BASE + 0x100), 上位机升级前从新固件的同一偏移读出识别码/版本,
 * 再与本机运行中的应用经 GET_INFO 上报的值比较。该对象被 proto.c 引用,
 * 不会被链接器裁掉。 */
const app_fw_info_t g_app_fw_info __attribute__((at(APP_FW_INFO_ADDR))) = {
    APP_FW_MAGIC, APP_FW_VERSION, { 0u, 0u }
};

static uint8_t s_attached;      /* 目标当前是否在线 */
static uint8_t s_state;
static uint8_t s_led_state = 0xFFu;   /* 上一次送到 LED 的状态, 0xFF 强制首刷 */
static uint32_t s_done_crc;     /* 最近一次烧录用到的镜像 CRC32 */
static uint32_t s_attach_ms;    /* 首次探测到目标的时刻 (0=无目标) */

/* 目标插入后等待供电稳定再自动烧录 (电机板等大电容负载, 插入瞬间电源最差,
   实测 40ms 就抢烧会死在编程中途; 等 500ms 成功率显著提高) */
#define ATTACH_SETTLE_MS    500u

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

/* LED 模式已移至 bsp.c (SysTick 中断驱动的 LED_SetState),
 * 即使主循环被 TargetProg_Run 长时间阻塞, 灯也会持续按节奏闪烁。
 * 状态->灯效 映射见 bsp.h。 */

/* ---------------- 烧录动作 ---------------- */
static void do_program(void)
{
    uint32_t err_addr = 0;
    uint8_t r;

#if USB_HAS_CDC
    (void)Proto_TakeProgRequest();          /* 清掉烧录期间可能积下的手动触发 */
#endif
    g_status.burn_seq++;                    /* 烧录尝试序号: 上位机据此发现新结果 */
    s_state = ST_PROGRAMMING;
    g_status.state = ST_PROGRAMMING;
    g_status.progress = 0;
    LED_SetState(ST_PROGRAMMING);           /* 立即切快闪, 提醒操作者勿拔 (主循环将被阻塞) */

    r = TargetProg_Run(&err_addr);

    /* 记录本次烧录对应的镜像, 供"U盘换固件后重烧"判断 */
    s_done_crc = Storage_ImageValid() ? Storage_Header()->crc32 : 0u;

    SWD_GPIO_Release();
    g_status.last_error = ((uint32_t)r) | (err_addr ? (err_addr & 0xFFFFFF00u) : 0u);

    if (r == TP_OK) {
        g_status.last_result = 1;
        s_state = ST_DONE_WAIT;
        s_attached = 1;                     /* 等拔出后再待命 */
        LED_SetState(ST_DONE_WAIT);
        Log_Str("PROG OK\n");
        /* 成功烧录流水: 记录区里标记一次, 满页自动进位到页B计数 */
        (void)Storage_RecordBurn();
        Log_Str("BURN cnt=");
        Log_Dec(Storage_RecordCount());
        Log_Str("\n");
    } else {
        g_status.last_result = 2;
        s_state = ST_FAIL_WAIT;
        s_attached = 1;
        LED_SetState(ST_FAIL_WAIT);
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
    /* 应用链接在 APP_BASE, 必须在开启任何中断前重定位向量表 */
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
    g_status.burn_seq = 0;
    g_status.host_online = 0;

    s_state = Storage_ImageValid() ? ST_ARMED : ST_NO_IMAGE;
    g_status.state = s_state;
    Log_Str("IMG ");
    Log_Dec(s_state == ST_ARMED);
    Log_Str("\n");

    WDT_Init();                             /* 初始化完成后再启动看门狗 */

    for (;;) {
        WDT_Feed();                         /* 独立看门狗: 卡死 3.2s 自动复位 */
#if USB_HAS_CDC
        Proto_Poll();                       /* 处理上位机命令 */
        USB_CDC_Poll();
#endif
        /* 把"更紧急"的状态送给 LED:
         *   本地自动流程改 s_state; 主机触发的流式/擦除烧录改 g_status.state
         *   (proto.c 直接写 2/3/4)。取烧录相关的优先, 这样任何路径开始烧录
         *   灯都会立刻切到快闪。只在变化时调用, 避免每 40ms 重置闪烁相位。 */
        {
            uint8_t led_st = s_state;
            if (g_status.state == ST_PROGRAMMING ||
                g_status.state == ST_DONE_WAIT ||
                g_status.state == ST_FAIL_WAIT)
                led_st = g_status.state;
            if (led_st != s_led_state) {
                s_led_state = led_st;
                LED_SetState(led_st);
            }
        }

#if USB_HAS_CDC && DBG_TOOLS
        /* 示波器时钟测试模式: 持续输出 SWCLK/SWDIO 方波, 暂停目标探测 */
        if (g_clk_test) {
            SWD_ClkTestBurst(10u);
            continue;
        }
#endif

        /* 镜像状态可能因上位机/U盘写入或清除而改变 */
        if (!Storage_ImageValid()) {
            /* 无镜像: 无论之前是待命/成功等待/失败等待, 一律回到"无镜像", 不再烧录 */
            if (s_state != ST_NO_IMAGE && s_state != ST_PROGRAMMING) {
                s_state = ST_NO_IMAGE;
                g_status.state = s_state;
                Log_Str("IMG 0\n");
            }
        } else if (s_state == ST_NO_IMAGE) {
            s_state = ST_ARMED;
            g_status.state = s_state;
            Log_Str("IMG 1\n");
        }

        if (s_state == ST_PROGRAMMING) continue;

        /* 流式烧录会话中 (协议层占用 SWD): 主循环不要探测/释放引脚,
           否则会把正在进行的 SWD 会话打断 */
        if (g_status.state == ST_PROGRAMMING) continue;

#if USB_HAS_CDC
        /* 手动触发 (上位机可发 PROG_NOW 命令) */
        if (Proto_TakeProgRequest()) {
            if (Storage_ImageValid()) { do_program(); continue; }
        }
#endif

        /* 目标已识别: 等供电稳定再烧录 (大电容负载插入瞬间电源最差,
         * 实测立即烧会死在编程中途)。这段不参与探测节流, 每轮都检查到点没有;
         * LED 此时是双闪, 告诉操作者"已看到目标, 别拔"。 */
        if (s_state == ST_TARGET_SEEN) {
            if ((BSP_Millis() - s_attach_ms) >= ATTACH_SETTLE_MS)
                do_program();
            continue;
        }

        /* 每 DETECT_INTERVAL_MS 探测目标 */
        if ((BSP_Millis() - t_detect) < DETECT_INTERVAL_MS) continue;
        t_detect = BSP_Millis();

        if (s_state == ST_ARMED && !s_attached) {
            if (TargetProg_Detect() == TP_OK) {
                SWD_GPIO_Release();
                s_attached = 1;
                s_attach_ms = BSP_Millis();
                s_state = ST_TARGET_SEEN;   /* 先稳定 500ms 再烧, LED 切双闪 */
                g_status.state = s_state;   /* 上位机可见: 已识别目标, 稳定电源中 */
                Log_Str("TGT SEEN DPIDR=0x");
                Log_Hex(g_status.target_dpidr);
                Log_Str(" ID=0x");
                Log_Hex(g_status.target_idcode);
                Log_Str(" swd=");
                Log_Dec(g_status.swd_err);
                Log_Str("\n");
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
