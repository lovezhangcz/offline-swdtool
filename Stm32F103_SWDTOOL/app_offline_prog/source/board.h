/**
  ******************************************************************************
  * @file    board.h
  * @brief   离线编程器硬件资源配置 (STM32F103C8T6)
  *
  *   硬件连接:
  *     PA11 / PA12 : USB DM / DP
  *     PB12        : 状态 LED, 低电平点亮
  *     PA3         : SWCLK 输出 (接目标芯片 SWCLK)
  *     PA4         : SWDIO      (接目标芯片 SWDIO)
  *     GND         : 必须与目标板共地
  *
  *   Flash 布局 (C8T6 = 64KB, 1KB/页):
  *     0x08000000 - 0x08007FFF : 本编程器程序 (32KB)
  *     0x08008000 - 0x0800FFFF : 目标固件镜像存储区 (32KB)
  *
  *   若芯片实际为 128KB (很多 C8T6 实为 CBT6 同硅片), 可打开
  *   FLASH_128K 宏, 镜像区扩大为 96KB, 但不能再保证 64KB 版芯片兼容。
  ******************************************************************************
  */
#ifndef __BOARD_H
#define __BOARD_H

#include "stm32f10x.h"

/* ---------------------------------------------------------------- LED ---- */
#define LED_GPIO            GPIOB
#define LED_PIN             12
#define LED_RCC             RCC_APB2Periph_GPIOB
/* 低电平点亮 */
#define LED_ON()            (LED_GPIO->BRR  = (1u << LED_PIN))
#define LED_OFF()           (LED_GPIO->BSRR = (1u << LED_PIN))
#define LED_TOGGLE()        do { if (LED_GPIO->ODR & (1u<<LED_PIN)) LED_ON(); else LED_OFF(); } while(0)

/* ------------------------------------------------------- USB D+ 上拉 ----- */
/* F103 的 USB 外设没有片内 D+ 上拉开关。
 * 若板子上 1.5k 上拉电阻固定接 3.3V: 保持 USB_PULLUP_PIN 未定义即可。
 * 若上拉由三极管控制 (常见于下载器小板): 定义下面的端口/引脚/有效电平,
 * 固件会在 USB 初始化时打开上拉, 主机才能检测到设备。                 */
#define USB_PULLUP_PORT     GPIOA
#define USB_PULLUP_PIN      9u
#define USB_PULLUP_ACTIVE   1u      /* 1=高电平接入, 0=低电平接入; 若不枚举改成 0 试 */

/* ------------------------------------------------------- 目标芯片选择 ---- */
/* TARGET_CHIP_STM32F1  : STM32F101/103 系列 (DEV_ID 0x410/412/414/418/430..)
 * TARGET_CHIP_GD32F1X0 : GD32F130/150 (GD32F1x0 系列)
 *   GD32F1x0 与 STM32F1 共用 DEV_ID 0x410, FMC 寄存器/密钥/位定义兼容,
 *   仅 REV_ID 不同 (0x1303); 选择后按对应规则校验目标身份。            */
#define TARGET_CHIP_STM32F1     1
#define TARGET_CHIP_GD32F1X0    2

#ifndef TARGET_CHIP
#define TARGET_CHIP             TARGET_CHIP_GD32F1X0
#endif

/* ---------------------------------------------------------------- SWD ---- */
#define SWD_GPIO            GPIOA
#define SWD_RCC             RCC_APB2Periph_GPIOA
/* 注: 若目标无应答/异常, 可交换两引脚定义再试 (3<->4) */
#define SWCLK_PIN           3u
#define SWDIO_PIN           4u

/* 目标 NRST 控制 (可选): 定义后, 连接目标期间自动拉住目标复位,
   抢在目标固件启动 (进低功耗/重映射SWD脚) 之前建立调试连接。
   需要把该引脚接到目标板的 NRST, 并取消注释下面这行:
   (注意: swd_host.c 用 #ifdef 判断, 写成 =0 依然会启用!)            */
/* #define SWD_USE_NRST */       /* 用户无复位线, 保持注释 */
#define SWD_NRST_PIN        5u      /* PA5 -> 目标 NRST, 低电平有效 */

/* ------------------------------------------------------- flash 存储布局 ---- */
#ifdef FLASH_128K
  #define IMG_STORE_ADDR    0x08008000u
  #define IMG_STORE_SIZE    (96u * 1024u)     /* 0x08008000 - 0x0801FFFF */
#else
  #define IMG_STORE_ADDR    0x08008000u
  #define IMG_STORE_SIZE    (32u * 1024u)     /* 0x08008000 - 0x0800FFFF */
#endif
#define FLASH_PAGE_SIZE     1024u

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

/* SWD 速度: 半周期延时循环次数, 越大越慢越稳 */
#ifndef SWD_DELAY_LOOP
#define SWD_DELAY_LOOP      8u
#endif

/* 主循环目标探测间隔 (ms): 越小插入响应越快, 但空板上 SWD 打线更频繁 */
#ifndef DETECT_INTERVAL_MS
#define DETECT_INTERVAL_MS  40u
#endif

/* 最大镜像大小 = 存储区 - 头 */
#define IMG_MAX_PAYLOAD     (IMG_STORE_SIZE - sizeof(img_hdr_t))

#endif /* __BOARD_H */
