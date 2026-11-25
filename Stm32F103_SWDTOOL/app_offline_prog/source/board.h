/**
  ******************************************************************************
  * @file    board.h
  * @brief   离线编程器硬件资源配置 (STM32F103C8T6)
  *
  *   硬件连接 (两版板子, 通过 BOARD_VARIANT 切换):
  *     PA11 / PA12 : USB DM / DP
  *     PB12        : 状态 LED, 低电平点亮
  *     GND         : 必须与目标板共地
  *
  *     BOARD_VARIANT_A (原板):
  *       PA3  : SWCLK 输出 (13脚), 备用 PB13(26脚) 见 SWCLK_USE_PB13
  *       PA4  : SWDIO 发送 (14脚)
  *       PA7  : SWDIO 采样 (17脚, PA4/PA7 在板上并成一根 SWDIO)
  *       PA9  : USB D+ 1.5k 上拉控制
  *
  *     BOARD_VARIANT_B (新主板):
  *       PA8  : SWCLK 输出
  *       PB4  : SWDIO  (与 JTAG NJTRST 复用, 需关 JTAG)
  *       PA15 : USB D+ 1.5k 上拉控制 (与 JTAG JTDI 复用, 需关 JTAG)
  *
  *   Flash 布局 (C8T6 = 64KB, 1KB/页):
  *     0x08000000 - 0x08003FFF : boot (16KB, 独立 target, 只烧一次)
  *     0x08004000 - 0x0800BFFF : 应用 (32KB, 可经 USB 由 boot 升级)
  *     0x0800C000 - 0x0800FFFF : 目标固件镜像存储区 (16KB)
  *
  *   老 target (offline_prog_CDC / MSC) 不带 BUILD_APP 宏, 仍保持
  *   0x08000000 起 32KB 程序 + 0x08008000 起 32KB 镜像区, 作为回退。
  *   若芯片实际为 128KB (很多 C8T6 实为 CBT6 同硅片), 可打开
  *   FLASH_128K 宏扩大镜像区, 但不能再保证 64KB 版芯片兼容。
  ******************************************************************************
  */
#ifndef __BOARD_H
#define __BOARD_H

#include "stm32f10x.h"

/* ------------------------------------------------------------ 硬件板型 ---- */
/* BOARD_VARIANT_A : 原板   USB 上拉 PA9  , SWCLK PA4 / SWDIO PA3
 * BOARD_VARIANT_B : 新主板 USB 上拉 PA15 , SWCLK PA8 / SWDIO PB4 (需关 JTAG)
 *
 * 换板只改下面这一行即可; 也可以不动源码, 在 Keil 的 C/C++ Define 里
 * 加一个 BOARD_VARIANT=2 (命令行/宏优先, 下面的 #ifndef 会尊重它)。 */
#define BOARD_VARIANT_A     1
#define BOARD_VARIANT_B     2
#ifndef BOARD_VARIANT
#define BOARD_VARIANT       BOARD_VARIANT_A
#endif

/* ---------------------------------------------------------------- LED ---- */
#define LED_GPIO            GPIOB
#define LED_PIN             12
#define LED_RCC             RCC_APB2Periph_GPIOB
/* 低电平点亮 */
#define LED_ON()            (LED_GPIO->BRR  = (1u << LED_PIN))
#define LED_OFF()           (LED_GPIO->BSRR = (1u << LED_PIN))
#define LED_TOGGLE()        do { if (LED_GPIO->ODR & (1u<<LED_PIN)) LED_ON(); else LED_OFF(); } while(0)

/* ------------------------------------------------------- USB 模式选择 ---- */
/* USB_MODE_CDC  : 虚拟串口 + 二进制协议 (需上位机 swdtool.py)
 * USB_MODE_MSC  : U 盘拖放烧录 (把 .bin 文件拖入 U 盘即更新镜像)
 * USB_MODE_COMP : MSC + CDC 复合设备, 同时出 U 盘和 COM 口 (APP_MSC 调试用) */
#define USB_MODE_CDC        1
#define USB_MODE_MSC        2
#define USB_MODE_COMP       3

#ifndef USB_MODE
#define USB_MODE            USB_MODE_MSC
#endif

/* 该构建是否包含某一路功能 (复合模式下两者都为真) */
#define USB_HAS_CDC         (USB_MODE == USB_MODE_CDC || USB_MODE == USB_MODE_COMP)
#define USB_HAS_MSC         (USB_MODE == USB_MODE_MSC || USB_MODE == USB_MODE_COMP)

/* ------------------------------------------------------- USB D+ 上拉 ----- */
/* F103 的 USB 外设没有片内 D+ 上拉开关。
 * 若板子上 1.5k 上拉电阻固定接 3.3V: 保持 USB_PULLUP_PIN 未定义即可。
 * 若上拉由三极管控制 (常见于下载器小板): 定义下面的端口/引脚/有效电平,
 * 固件会在 USB 初始化时打开上拉, 主机才能检测到设备。                 */
#if BOARD_VARIANT == BOARD_VARIANT_B
#define USB_PULLUP_PORT     GPIOA
#define USB_PULLUP_PIN      15u     /* 新主板: PA15 (JTDI, 已关 JTAG 释放) */
#else
#define USB_PULLUP_PORT     GPIOA
#define USB_PULLUP_PIN      9u      /* 原板: PA9 */
#endif
#define USB_PULLUP_ACTIVE   1u      /* 1=高电平接入, 0=低电平接入; 若不枚举改成 0 试 */

/* ------------------------------------------------------- MSC 磁盘参数 ---- */
#if USB_HAS_MSC
/* 虚拟磁盘容量 (必须是 512 字节扇区的整数倍)
 * 比镜像存储区略小即可, 因为 FAT 表和目录项要占几个扇区 */
#define MSC_DISK_SIZE       IMG_STORE_SIZE
#define MSC_SECTOR_SIZE     512u
#define MSC_VOLUME_LABEL    "SWDPROG   "   /* 11 字符, 不足补空格 */
#endif

/* ------------------------------------------------------- 目标芯片选择 ---- */
/* TARGET_CHIP_STM32F1  : STM32F101/103 系列 (DEV_ID 0x410/412/414/418/430..)
 * TARGET_CHIP_GD32F1X0 : GD32F130/150 (GD32F1x0 系列, Cortex-M4)
 *   GD32F1x0 与 STM32F1 共用 DEV_ID 0x410, FMC 寄存器/密钥/位定义兼容,
 *   仅 REV_ID 不同 (0x1303); 选择后按对应规则校验目标身份。
 * TARGET_CHIP_GD32F103 : GD32F103 系列 (Cortex-M3, 如 GD32F103C8T6)
 *   DEV_ID 0x410 与 STM32F103 中容量相同; FMC 寄存器布局/解锁密钥与
 *   STM32F1 一致, 但擦除更慢 (1KB 页, 擦除超时放宽), 且 FMC_PID
 *   (0x40022100) 读出 0x41424333 可确认为 GD32。                     */
#define TARGET_CHIP_STM32F1     1
#define TARGET_CHIP_GD32F1X0    2
#define TARGET_CHIP_GD32F103    3

#ifndef TARGET_CHIP
#define TARGET_CHIP             TARGET_CHIP_GD32F103   /* 当前目标: GD32F103C8T6 */
#endif

/* ---------------------------------------------------------------- SWD ---- */
/* SWDIO 可能由"发送脚"和"采样脚"两个引脚共同完成 (A 板就是这种接法),
 * 所以输出/输入分开定义; 若两个方向共用一个引脚, 把 SWDIO_SHARED 置 1。
 *   注: 若目标无应答/异常, 可交换 SWCLK/SWDIO 的引脚定义再试。          */
#if BOARD_VARIANT == BOARD_VARIANT_B
#define SWCLK_GPIO          GPIOA
#define SWCLK_PIN           8u      /* 新主板 PA8 */
#define SWDIO_OUT_GPIO      GPIOB
#define SWDIO_OUT_PIN       4u      /* 新主板 PB4 (NJTRST): 收发同脚 */
#define SWDIO_IN_GPIO       GPIOB
#define SWDIO_IN_PIN        4u
#define SWDIO_SHARED        1
#define SWD_RCC             (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB)
#define SWD_HAS_CLK_ALT     0
#define SWD_CLK_ALT_GPIO    SWCLK_GPIO
#define SWD_CLK_ALT_PIN     SWCLK_PIN
#else
/* A 板: SWCLK=PA3(13脚), SWDIO 发送=PA4(14脚), SWDIO 采样=PA7(17脚),
 *       两根线在板上并到同一个 SWDIO 网络 (对应目标 PA13) */
#define SWCLK_USE_PB13      0       /* 改成 1 用备用时钟脚 PB13(26脚) */
#if SWCLK_USE_PB13
#define SWCLK_GPIO          GPIOB
#define SWCLK_PIN           13u
#else
#define SWCLK_GPIO          GPIOA
#define SWCLK_PIN           3u
#endif
#define SWDIO_OUT_GPIO      GPIOA
#define SWDIO_OUT_PIN       4u
#define SWDIO_IN_GPIO       GPIOA
#define SWDIO_IN_PIN        7u
#define SWDIO_SHARED        0
#define SWD_RCC             (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB)
/* A 板备用时钟脚 PB13(26脚): 握手诊断时两个时钟脚都会试一遍
 * (两个脚不能同时驱动, 程序里同一时刻只把一个配成输出) */
#define SWD_HAS_CLK_ALT     1
#define SWD_CLK_ALT_GPIO    GPIOB
#define SWD_CLK_ALT_PIN     13u
#endif

/* 新板 PB4/PA15 与 JTAG 引脚复用: 关闭 JTAG、保留 SWD (PA13/PA14 仍可下载) */
#if BOARD_VARIANT == BOARD_VARIANT_B
#define BOARD_DISABLE_JTAG  1
#else
#define BOARD_DISABLE_JTAG  0
#endif

/* 目标 NRST 控制 (可选): 定义后, 连接目标期间自动拉住目标复位,
   抢在目标固件启动 (进低功耗/重映射SWD脚) 之前建立调试连接。
   需要把该引脚接到目标板的 NRST, 并取消注释下面这行:
   (注意: swd_host.c 用 #ifdef 判断, 写成 =0 依然会启用!)            */
/* #define SWD_USE_NRST */       /* 用户无复位线, 保持注释 */
#define SWD_NRST_GPIO       GPIOA
#define SWD_NRST_PIN        5u      /* PA5 -> 目标 NRST, 低电平有效 */

/* ------------------------------------------------------- flash 存储布局 ---- */
/* 64KB flash 分区 (1KB/页):
 *
 *   0x08000000 - 0x08003FFF : boot 16KB
 *       0x08003C00 最后一页 : boot 升级标志 (boot 自己维护, 永不擦)
 *   0x08004000 - 0x0800BFFF : 应用 32KB (由 boot 通过 USB 升级)
 *   0x0800C000 - 0x0800FFFF : 目标固件镜像存储区 16KB
 *
 * BUILD_BOOT : 编译 boot (Bootloader)
 * BUILD_APP  : 编译可被 boot 升级的应用 target (APP_CDC / APP_MSC)
 * 未定义     : 老 target (offline_prog_CDC / MSC), 保持原有布局做回退
 */
#define FLASH_PAGE_SIZE     1024u

#define BOOT_BASE           0x08000000u
#define BOOT_SIZE           (16u * 1024u)
#define BOOT_FLAG_ADDR      (BOOT_BASE + BOOT_SIZE - FLASH_PAGE_SIZE)

#define APP_BASE            0x08004000u
#define APP_SIZE            (32u * 1024u)

/* 备份寄存器分配 (复位/看门狗复位保持; 需备份域写使能 PWR_CR_DBP) */
#define BKP_REG_FORCE       BKP->DR1        /* 应用写 BOOT_FORCE_MAGIC 后软复位 -> boot 直接进升级模式 */
#define BKP_REG_BOOTTRY     BKP->DR2        /* boot 连续跳转失败计数, 应用启动成功后清零 */

#define BOOT_FORCE_MAGIC    0x5A5Au
/* 连续 N 次复位都没等到应用确认启动 -> 常驻 boot, 避免坏应用把设备锁死 */
#define BOOT_TRY_MAX        3u
/* 升级标志页内容 */
#define BOOT_FLAG_MAGIC     0x424F4F54u     /* "BOOT" */
#define BOOT_FLAG_VALID     0xA55A5AA5u

#if defined(BUILD_APP)
  #define IMG_STORE_ADDR    0x0800C000u
  #define IMG_STORE_SIZE    (16u * 1024u)     /* 0x0800C000 - 0x0800FFFF */
#elif defined(FLASH_128K)
  #define IMG_STORE_ADDR    0x08008000u
  #define IMG_STORE_SIZE    (96u * 1024u)     /* 0x08008000 - 0x0801FFFF */
#else
  #define IMG_STORE_ADDR    0x08008000u
  #define IMG_STORE_SIZE    (32u * 1024u)     /* 0x08008000 - 0x0800FFFF */
#endif

/* 镜像头: 放在存储区起始 32 字节 */
#define IMG_MAGIC           0x53574447u       /* "SWDG" */
typedef struct {
    uint32_t magic;        /* IMG_MAGIC                       */
    uint32_t length;       /* 有效载荷字节数 (不含头)         */
    uint32_t crc32;        /* 载荷 CRC32 (IEEE 802.3)         */
    uint32_t load_addr;    /* 目标烧录起始地址, 0x08000000    */
    uint32_t prog_count;   /* 成功烧录次数统计                */
    uint32_t reserved[3];
} img_hdr_t;               /* 共 32 字节 */

/* 系统时钟: 72MHz (HSE 8MHz x PLL9, USB 48MHz = 72/1.5) */
#define SYSCLK_HZ           72000000u

/* 应用版本号 (GET_VERSION 命令用, 确认升级是否真正生效) */
#ifndef APP_VERSION_STR
#define APP_VERSION_STR     "1.0.0"
#endif

/* SWD 速度: 半周期延时循环次数, 越大越慢越稳 */
#ifndef SWD_DELAY_LOOP
#define SWD_DELAY_LOOP      8u
#endif

/* 主循环目标探测间隔 (ms): 越小插入响应越快, 但空板上 SWD 打线更频繁 */
#ifndef DETECT_INTERVAL_MS
#define DETECT_INTERVAL_MS  40u
#endif

/* 载荷在存储区内的起始偏移: 第 0 页整页留给镜像头 + 文件名 + 首簇号
 * (VFS/U 盘路径也要求数据从第 1 页开始, 两条升级路径共用同一套布局) */
#define IMG_DATA_OFFSET     FLASH_PAGE_SIZE

/* 最大镜像大小 = 存储区 - 头所在页 */
#define IMG_MAX_PAYLOAD     (IMG_STORE_SIZE - IMG_DATA_OFFSET)

#endif /* __BOARD_H */
