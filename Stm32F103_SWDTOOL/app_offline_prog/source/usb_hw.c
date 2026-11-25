/**
  * @file  usb_hw.c
  * @brief STM32F103 USB FS 设备控制器底层驱动实现
  *
  *   PMA (512B, BTABLE=0) 布局:
  *     CDC 模式:
  *       0x00 - 0x3F : BTABLE
  *       0x40 - 0x7F : EP0 TX (64B)
  *       0x80 - 0xBF : EP0 RX (64B)
  *       0xC0 - 0xCF : EP1 TX (16B, interrupt IN)
  *       0xD0 - 0x10F: EP2 RX (64B, bulk OUT)
  *       0x110- 0x14F: EP3 TX (64B, bulk IN)
  *     MSC 模式:
  *       0x00 - 0x3F : BTABLE
  *       0x40 - 0x7F : EP0 TX (64B)
  *       0x80 - 0xBF : EP0 RX (64B)
  *       0xC0 - 0xFF : EP1 RX (64B, bulk OUT)
  *       0x100- 0x13F: EP2 TX (64B, bulk IN)
  *
  *   注意: F103 的 D+ 上拉电阻没有片内开关, 若板子上有 GPIO 控制的三极管
  *   上拉, 需要在 USB_HW_Init 里自行拉高对应引脚; 固定上拉则无需处理。
  */
#include "usb_hw.h"
#include "board.h"
#include "bsp.h"

/* ------------------------------------------------ 寄存器定义 ------------- */
#define USB_REG_BASE        0x40005C00u
#define USB_PMA_BASE        0x40006000u

#define USB_EPnR(n)         (*(volatile uint16_t *)(USB_REG_BASE + 4u * (n)))
#define USB_CNTR            (*(volatile uint16_t *)(USB_REG_BASE + 0x40u))
#define USB_ISTR            (*(volatile uint16_t *)(USB_REG_BASE + 0x44u))
#define USB_FNR             (*(volatile uint16_t *)(USB_REG_BASE + 0x48u))
#define USB_DADDR           (*(volatile uint16_t *)(USB_REG_BASE + 0x4Cu))
#define USB_BTABLE          (*(volatile uint16_t *)(USB_REG_BASE + 0x50u))

/* CNTR 位 */
#define CNTR_FRES           0x0001u
#define CNTR_PDWN           0x0002u
#define CNTR_RESETM         0x0400u
#define CNTR_SUSPM          0x0800u
#define CNTR_WKUPM          0x1000u
#define CNTR_CTRM           0x8000u

/* ISTR 位 */
#define ISTR_DIR            0x0010u
#define ISTR_RESET          0x0400u
#define ISTR_SUSP           0x0800u
#define ISTR_WKUP           0x1000u
#define ISTR_CTR            0x8000u

/* EPnR 位 */
#define EPR_CTR_RX          0x8000u
#define EPR_DTOG_RX         0x4000u
#define EPR_STAT_RX         0x3000u
#define EPR_SETUP           0x0800u
#define EPR_EP_TYPE         0x0600u
#define EPR_EP_KIND         0x0100u
#define EPR_CTR_TX          0x0080u
#define EPR_DTOG_TX         0x0040u
#define EPR_STAT_TX         0x0030u
#define EPR_EA              0x000Fu

#define EP_TYPE_BULK        0x0000u
#define EP_TYPE_CONTROL     0x0200u
#define EP_TYPE_INTERRUPT   0x0600u

/* PMA 访问: 每 16 位半字占一个 32 位对齐地址 */
#define PMA_U16(off)        (*(volatile uint16_t *)(USB_PMA_BASE + 2u * (off)))

/* 端点缓冲描述符 (BTABLE=0, 每个端点 4 个半字) */
#define BTAB_TX_ADDR(ep)    PMA_U16((ep) * 8u + 0u)
#define BTAB_TX_CNT(ep)     PMA_U16((ep) * 8u + 2u)
#define BTAB_RX_ADDR(ep)    PMA_U16((ep) * 8u + 4u)
#define BTAB_RX_CNT(ep)     PMA_U16((ep) * 8u + 6u)

/* ---- PMA 缓冲区布局 (按功能裁剪 / 按 USB_MODE 切换) ---- */
#define EP0_TX_BUF   0x040u
#define EP0_RX_BUF   0x080u

#if USB_HAS_CDC
  #define EP_CDC_NOTIFY_TX  0x0C0u   /* 16B interrupt IN */
  #define EP_CDC_OUT_RX     0x0D0u   /* 64B bulk OUT */
  #define EP_CDC_IN_TX      0x110u   /* 64B bulk IN  */
#endif

#if USB_HAS_MSC
  #if USB_MODE == USB_MODE_MSC
    #define EP_MSC_OUT_RX   0x0C0u   /* 64B bulk OUT */
    #define EP_MSC_IN_TX    0x100u   /* 64B bulk IN  */
  #else /* 复合模式: 放在 CDC 之后 */
    #define EP_MSC_OUT_RX   0x150u   /* 64B bulk OUT */
    #define EP_MSC_IN_TX    0x190u   /* 64B bulk IN  */
  #endif
#endif

/* RX 缓冲尺寸描述: 64B -> BL_SIZE=1, NUM_BLOCK=2 */
#define RXSIZE_64           0x8800u
#define RXSIZE_16           0x8400u

/* ------------------------------------------------ 内部工具 --------------- */
static uint16_t ep_rd(uint8_t ep) { return USB_EPnR(ep); }

/* 写 EPnR: CTR_RX/CTR_TX 写 1 保持不变 (写 0 才是清除), 切换位由调用者构造 */
static void ep_wr(uint8_t ep, uint16_t v)
{
    USB_EPnR(ep) = (uint16_t)(v | EPR_CTR_RX | EPR_CTR_TX);
}

/* EPnR 写语义: DTOG/STAT 位写 1 翻转(写 0 保持), CTR 位写 0 清除(写 1 保持)。
 * 因此: 不想动的翻转位必须写 0, 不想清的 CTR 位必须写 1。              */

void USB_EP_SetRxStatus(uint8_t ep, uint16_t stat)
{
    uint16_t r = ep_rd(ep);
    uint16_t v = (uint16_t)(r & ~(EPR_DTOG_RX | EPR_STAT_RX |
                                  EPR_DTOG_TX | EPR_STAT_TX));   /* 其余翻转位写 0 保持 */
    v |= (uint16_t)((r ^ (uint16_t)(stat << 12)) & EPR_STAT_RX); /* 只翻转到目标状态 */
    ep_wr(ep, v);
}

void USB_EP_SetTxStatus(uint8_t ep, uint16_t stat)
{
    uint16_t r = ep_rd(ep);
    uint16_t v = (uint16_t)(r & ~(EPR_DTOG_RX | EPR_STAT_RX |
                                  EPR_DTOG_TX | EPR_STAT_TX));
    v |= (uint16_t)((r ^ (uint16_t)(stat << 4)) & EPR_STAT_TX);
    ep_wr(ep, v);
}

/* 清 CTR_RX: CTR_TX 写 1 保持, 翻转位写 0 保持, 保留 TYPE/KIND/EA */
void USB_EP_ClearRxCTR(uint8_t ep)
{
    USB_EPnR(ep) = (uint16_t)(ep_rd(ep) &
                     (EPR_CTR_TX | EPR_EP_TYPE | EPR_EP_KIND | EPR_EA));
}

/* 清 CTR_TX: CTR_RX 写 1 保持, 翻转位写 0 保持 */
void USB_EP_ClearTxCTR(uint8_t ep)
{
    USB_EPnR(ep) = (uint16_t)(ep_rd(ep) &
                     (EPR_CTR_RX | EPR_EP_TYPE | EPR_EP_KIND | EPR_EA));
}

void USB_EP_SetTxCount(uint8_t ep, uint16_t cnt) { BTAB_TX_CNT(ep) = cnt; }
uint16_t USB_EP_GetRxCount(uint8_t ep)           { return (uint16_t)(BTAB_RX_CNT(ep) & 0x03FFu); }
uint16_t USB_EP_GetTxAddr(uint8_t ep)            { return BTAB_TX_ADDR(ep); }
uint16_t USB_EP_GetRxAddr(uint8_t ep)            { return BTAB_RX_ADDR(ep); }

void USB_PMA_Write(uint16_t pma_off, const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i += 2u) {
        uint16_t w = buf[i];
        if ((uint16_t)(i + 1u) < len) w |= (uint16_t)buf[i + 1u] << 8;
        PMA_U16(pma_off + i) = w;
    }
}

void USB_PMA_Read(uint16_t pma_off, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i += 2u) {
        uint16_t w = PMA_U16(pma_off + i);
        buf[i] = (uint8_t)(w & 0xFFu);
        if ((uint16_t)(i + 1u) < len) buf[i + 1u] = (uint8_t)(w >> 8);
    }
}

/**
  * 配置端点: type = EP_TYPE_xxx; rx_size 仅用于 16/64 字节缓冲
  * 端点地址字段 EA = ep, 初始状态 NAK, 单缓冲
  */
void USB_EP_Config(uint8_t ep, uint16_t type, uint16_t rx_addr, uint16_t rx_size,
                   uint16_t tx_addr)
{
    uint16_t r = ep_rd(ep);
    uint16_t v = (uint16_t)(type | ep);
    if (r & EPR_DTOG_RX) v |= EPR_DTOG_RX;
    if (r & EPR_DTOG_TX) v |= EPR_DTOG_TX;
    ep_wr(ep, v);

    BTAB_TX_ADDR(ep) = tx_addr;
    BTAB_TX_CNT(ep)  = 0;
    BTAB_RX_ADDR(ep) = rx_addr;
    BTAB_RX_CNT(ep)  = rx_size;

    USB_EP_SetTxStatus(ep, USB_STAT_NAK);
    USB_EP_SetRxStatus(ep, USB_STAT_NAK);
}

void USB_SetAddress(uint8_t addr)
{
    USB_DADDR = (uint16_t)(0x80u | addr);   /* EF=1 + 地址 */
}

/* ------------------------------------------------ 初始化 ----------------- */
static void usb_endpoints_reset(void)
{
    /* 总线复位后: 重新配置全部端点, 等待主机重新枚举 */
    USB_EP_Config(USB_EP0, EP_TYPE_CONTROL,   EP0_RX_BUF,  RXSIZE_64, EP0_TX_BUF);

#if USB_HAS_CDC
    USB_EP_Config(USB_CDC_NOTIFY_EP, EP_TYPE_INTERRUPT, 0, RXSIZE_16, EP_CDC_NOTIFY_TX);
    USB_EP_Config(USB_CDC_OUT_EP,    EP_TYPE_BULK, EP_CDC_OUT_RX, RXSIZE_64, 0);
    USB_EP_Config(USB_CDC_IN_EP,     EP_TYPE_BULK, 0, RXSIZE_16, EP_CDC_IN_TX);
#endif
#if USB_HAS_MSC
    USB_EP_Config(USB_BULK_OUT_EP,   EP_TYPE_BULK, EP_MSC_OUT_RX, RXSIZE_64, 0);
    USB_EP_Config(USB_BULK_IN_EP,    EP_TYPE_BULK, 0, RXSIZE_16, EP_MSC_IN_TX);
#endif

    /* EP0: RX VALID 准备接收 SETUP, TX NAK */
    USB_EP_SetRxStatus(USB_EP0, USB_STAT_VALID);
    USB_EP_SetTxStatus(USB_EP0, USB_STAT_NAK);
    /* 批量 OUT 端点打开接收 */
#if USB_HAS_CDC
    USB_EP_SetRxStatus(USB_CDC_OUT_EP, USB_STAT_VALID);
#endif
#if USB_HAS_MSC
    USB_EP_SetRxStatus(USB_BULK_OUT_EP, USB_STAT_VALID);
#endif

    USB_SetAddress(0);
    USB_CB_Reset();
}

void USB_HW_Init(void)
{
#ifdef USB_PULLUP_PORT
    /* 打开 D+ 1.5k 上拉 (三极管控制型), 主机由此检测到设备接入 */
    BSP_PinConfig(USB_PULLUP_PORT, USB_PULLUP_PIN, 0x2u);   /* 推挽输出 2MHz */
#if USB_PULLUP_ACTIVE
    USB_PULLUP_PORT->BSRR = (1u << USB_PULLUP_PIN);
#else
    USB_PULLUP_PORT->BRR  = (1u << USB_PULLUP_PIN);
#endif
#endif

    /* 使能 USB 时钟 (APB1), 48MHz 来自 72MHz/1.5 (RCC_CFGR USBPRE=0) */
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;

    /* 复位 USB 外设 */
    USB_CNTR = CNTR_FRES;
    USB_CNTR = 0;
    USB_ISTR = 0;
    USB_BTABLE = 0;

    /* 使能中断: 正确传输 + 总线复位 + 挂起/唤醒 */
    USB_CNTR = CNTR_CTRM | CNTR_RESETM | CNTR_SUSPM | CNTR_WKUPM;
    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

/* ------------------------------------------------ 中断处理 --------------- */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    uint16_t istr;

    while ((istr = USB_ISTR) != 0) {
        if (istr & ISTR_RESET) {
            USB_ISTR = (uint16_t)~ISTR_RESET;
            usb_endpoints_reset();
            continue;
        }
        if (istr & ISTR_SUSP) {
            USB_ISTR = (uint16_t)~ISTR_SUSP;
            continue;
        }
        if (istr & ISTR_WKUP) {
            USB_ISTR = (uint16_t)~ISTR_WKUP;
            USB_CNTR &= ~(uint16_t)0x000Cu;   /* 清 LP_MODE/FSUSP */
            continue;
        }
        if (istr & ISTR_CTR) {
            uint8_t ep  = (uint8_t)(istr & 0x0Fu);
            uint16_t r  = ep_rd(ep);

            if (ep == USB_EP0) {
                if (r & EPR_CTR_RX) {
                    uint16_t cnt = USB_EP_GetRxCount(USB_EP0);
                    uint8_t buf[8];
                    if (r & EPR_SETUP) {
                        USB_PMA_Read(USB_EP_GetRxAddr(USB_EP0), buf, 8u);
                        USB_EP_ClearRxCTR(USB_EP0);
                        USB_CB_Setup(buf);
                    } else {
                        USB_EP_ClearRxCTR(USB_EP0);
                        USB_CB_EP0_RxDone(cnt);
                    }
                }
                if (ep_rd(USB_EP0) & EPR_CTR_TX) {
                    USB_EP_ClearTxCTR(USB_EP0);
                    USB_CB_EP0_TxDone();
                }
            } else if (r & EPR_CTR_RX) {
                uint16_t cnt = USB_EP_GetRxCount(ep);
                uint8_t buf[64];
                USB_PMA_Read(USB_EP_GetRxAddr(ep), buf, cnt);
                USB_EP_ClearRxCTR(ep);
                USB_CB_BulkOut(ep, buf, cnt);
                USB_EP_SetRxStatus(ep, USB_STAT_VALID);
            } else if (r & EPR_CTR_TX) {
                /* 批量 IN 发送完成 (CDC 通知端点不发数据, 由上层忽略) */
                USB_EP_ClearTxCTR(ep);
                USB_CB_BulkInDone(ep);
            }
            continue;
        }
        /* 其余未使能的标志不会置位; 防御性清除可写位 */
        USB_ISTR = (uint16_t)~(istr & ~(ISTR_CTR));
    }
}
