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
  *   Flash 布局 (单 boot, 所有容量统一; 只有镜像区大小按容量区分):
  *     0x08000000 - 0x080017FF : boot 6KB (独立 target, 只烧一次)
  *     0x08001800 - 0x08001FFF : 升级标志区 2KB (只用前 1KB, 兼容 2KB 页)
  *     0x08002000 - 0x08006FFF : 应用 20KB (可经 USB 由 boot 升级)
  *     0x08007000 - 0x080077FF : 烧录流水记录区 2KB (页A流水 / 页B计数)
  *     0x08007800 - 0x0800FFFF : 镜像区 34KB  (64KB 芯片),
  *                               0x0801FFFF / 0x08022FFF 分别 98KB / 110KB
  *
  *   boot / 标志区 / 应用的地址对所有容量完全一致, 两种板共用同一个 boot;
  *   容量差异只体现在应用 target 的镜像区大小 (FLASH_256K / FLASH_128K 宏)。
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

/* ------------------------------------------------------------ 调试开关 ---- */
/* DBG_TOOLS = 1 : 保留只用于开发排查的功能, 会占约 1.5KB 代码
 *      - 示波器时钟测试 (CMD_CLK_TEST / 主循环方波输出)
 *      - 本机 Flash 实际容量自检 (CMD_FLASH_TEST, 会访问不存在的地址)
 *      - 探针里的引脚电平/时钟网络/原始波形/引脚组合扫描/DHCSR/GPIO 采样诊断
 * DBG_TOOLS = 0 : 以上全部裁掉, 只保留握手+读 IDCODE 的简洁探针 (量产用)
 * 也可以不改源码, 在 Keil 的 C/C++ Define 里加 DBG_TOOLS=1 临时打开。 */
#ifndef DBG_TOOLS
#define DBG_TOOLS           0
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
/* TARGET_CHIP_AUTO     : 自动识别 (默认)
 *   运行期按 SWD DPIDR + DBGMCU IDCODE (+ FMC_PID) 判定芯片, 并选用对应
 *   烧录算法; 四个算法 blob 全部编入 (约 +1KB Flash)。
 *   已实测通过的特征:
 *     GD32F303 : DPIDR 0x2BA01477 (M4)  DEV 0x414  REV 0x2104  2KB 扇区/512KB
 *     GD32 1KB : DPIDR 0x1BA01477 (M3)  DEV 0x410  PID 高字节 0x41
 *                GD32F103 与 GD32F1x0 的 DEV/REV 完全相同 (0x410/0x1303),
 *                只能靠 FMC_PID 区分 (GD32F103=0x41424333, GD32F130=0x41384330);
 *                两者共用 GD32F1x0 算法 (均已实测)
 *     STM32F1  : DPIDR 0x1BA01477 (M3)  DEV 0x410/412/414/418/420/428/430
 *                (DEV 0x410 且 FMC_PID 读不到 0x41424333 时判为 STM32F1)
 * TARGET_CHIP_STM32F1  : 仅 STM32F101/103 系列
 * TARGET_CHIP_GD32F1X0 : 仅 GD32F1x0 (GD32F130/150, 1KB 页)
 *   GD32F1x0 与 STM32F1 共用 DEV_ID 0x410, FMC 寄存器/密钥/位定义兼容;
 *   GD32F103 的 DEV_ID/REV_ID 也一样, 只能读 FMC_PID 区分。
 * TARGET_CHIP_GD32F103 : 仅 GD32F103 系列 (Cortex-M3, 如 GD32F103C8T6)
 * TARGET_CHIP_GD32F303 : 仅 GD32F303 系列 (Cortex-M4, DEV_ID 0x414)
 *   FMC 与 F1 兼容, 扇区 2KB (编程页 1KB), 512KB 版本用 GD32F30x_HD.FLM。 */
#define TARGET_CHIP_AUTO        0
#define TARGET_CHIP_STM32F1     1
#define TARGET_CHIP_GD32F1X0    2
#define TARGET_CHIP_GD32F103    3
#define TARGET_CHIP_GD32F303    4

#ifndef TARGET_CHIP
#define TARGET_CHIP             TARGET_CHIP_AUTO   /* 当前: 自动识别 */
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
 *   0x08000000 - 0x080017FF : boot 6KB
 *   0x08001800 - 0x080067FF : 应用 20KB (由 boot 通过 USB 升级)
 *   0x08006800 - 0x0800FBFF : 目标固件镜像存储区 37KB
 *   0x0800FC00 - 0x0800FFFF : boot 升级标志页
 *
 * BUILD_BOOT : 编译 boot (Bootloader)
 * BUILD_APP  : 编译可被 boot 升级的应用 target (APP_CDC / APP_MSC)
 * 未定义     : 老 target (offline_prog_CDC / MSC), 保持原有布局做回退
 */
#define FLASH_PAGE_SIZE     1024u

#define BOOT_BASE           0x08000000u
#define BOOT_SIZE           (6u * 1024u)

/* 单 boot 统一布局: boot / 标志区 / 应用 三段的地址与大小对所有容量都相同,
   只有镜像区大小按芯片容量区分, 因此一个 boot 二进制同时兼容 64KB 和 256KB 板:
     0x08000000-0x080017FF  boot 6KB
     0x08001800-0x08001FFF  升级标志区 2KB (实际只用前 1KB; 留 2KB 是为了
                             万一芯片是 2KB 页, 擦标志页不会连带擦掉应用)
     0x08002000-0x08006FFF  应用 20KB
     0x08007000-0x080077FF  烧录流水记录区 2KB (随离线固件更新而擦除)
     0x08007800-........    镜像区 (按容量到芯片可用末尾) */
#define APP_BASE            0x08002000u
#define APP_SIZE            (20u * 1024u)

#define BOOT_FLAG_ADDR      0x08001800u
#define BOOT_FLAG_RESERVE   (2u * 1024u)      /* 标志区预留: 0x08001800-0x08001FFF */

#define RECORD_BASE         0x08007000u       /* 烧录流水记录区 2KB (页A流水 + 页B计数) */
#define RECORD_SIZE         (2u * 1024u)
#define IMG_STORE_ADDR      (RECORD_BASE + RECORD_SIZE)   /* 0x08007800 */
#if defined(FLASH_256K)
  #define IMG_STORE_SIZE    (110u * 1024u)          /* 0x08007800 - 0x08022FFF */
#elif defined(FLASH_128K)
  #define IMG_STORE_SIZE    (98u * 1024u)           /* 0x08007800 - 0x0801FFFF */
#else
  #define IMG_STORE_SIZE    (34u * 1024u)           /* 0x08007800 - 0x0800FFFF */
#endif
/* 备份寄存器分配 (复位/看门狗复位保持; 需备份域写使能 PWR_CR_DBP) */
#define BKP_REG_FORCE       BKP->DR1        /* 应用写 BOOT_FORCE_MAGIC 后软复位 -> boot 直接进升级模式 */
#define BKP_REG_BOOTTRY     BKP->DR2        /* boot 连续跳转失败计数, 应用启动成功后清零 */

#define BOOT_FORCE_MAGIC    0x5A5Au
/* 连续 N 次复位都没等到应用确认启动 -> 常驻 boot, 避免坏应用把设备锁死 */
#define BOOT_TRY_MAX        3u

/* 升级标志区内容 */
#define BOOT_FLAG_MAGIC     0x424F4F54u     /* "BOOT" */
#define BOOT_FLAG_VALID     0xA55A5AA5u

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

/* --------------------------------------------------- 应用固件版本信息 ---- */
/* 版本号只有一个来源: 下面 主/次/修订 三个数字。改版本只改这里, 版本字符串
 * (GET_VERSION 日志用) 与数值版本号 (升级比较用) 会自动同步,
 * 数值版本 = 主*10000 + 次*100 + 修订 (例如 1.1.6 -> 10106)。
 * 三个数字全为 0 即 0 号版本 (调试版, 可覆盖任何版本)。
 *
 * 在应用镜像里"比较靠前的固定位置"放一份识别码 + 版本号, 不涉及 boot:
 * 上位机升级前从新固件镜像的同一偏移读出 (识别码, 版本), 与本机运行中的
 * 应用经 GET_INFO 上报的值比较:
 *   - 识别码 != APP_FW_MAGIC : 不是本项目的应用固件, 拒绝升级
 *   - 版本 == 0              : 调试版, 不受限制, 可覆盖任何版本
 *   - 版本 <= 当前版本        : 拒绝升级
 * 该结构在设备 Flash 里的地址也是 APP_BASE + 0x100, 与镜像偏移完全一致;
 * 0x100 在中断向量表 (约 0xBC) 之后, 不会被向量表占用。 */
#ifndef APP_FW_VERSION_MAJOR
#define APP_FW_VERSION_MAJOR  1
#endif
#ifndef APP_FW_VERSION_MINOR
#define APP_FW_VERSION_MINOR  1
#endif
#ifndef APP_FW_VERSION_PATCH
#define APP_FW_VERSION_PATCH  15
#endif

#define APP_STR_(x)         #x
#define APP_STR(x)          APP_STR_(x)
#define APP_VERSION_STR     APP_STR(APP_FW_VERSION_MAJOR) "." \
                            APP_STR(APP_FW_VERSION_MINOR) "." \
                            APP_STR(APP_FW_VERSION_PATCH)

/* 数值版本号: 上位机升级比较用 (0 = 调试版) */
#define APP_FW_VERSION      ((uint32_t)APP_FW_VERSION_MAJOR * 10000u + \
                             (uint32_t)APP_FW_VERSION_MINOR * 100u +   \
                             (uint32_t)APP_FW_VERSION_PATCH)
#define APP_FW_MAGIC        0x53574450u     /* "SWDP" 本项目应用固件识别码 */
#define APP_FW_INFO_OFFSET  0x100u
#define APP_FW_INFO_ADDR    (APP_BASE + APP_FW_INFO_OFFSET)

typedef struct {
    uint32_t magic;         /* APP_FW_MAGIC                    */
    uint32_t version;       /* APP_FW_VERSION (0 = 调试版)     */
    uint32_t reserved[2];   /* 预留                            */
} app_fw_info_t;            /* 共 16 字节 */

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
