# OfflineDownloader 离线下载器 — 硬件与驱动交接文档

> 用途：本文档面向接手本套硬件做二次开发的工程师，快速掌握主控资源分配、引脚功能、LCD 屏与外部 Flash 驱动现状，可在此基础上裁剪/替换业务逻辑，复用底层板级驱动。
> 依据：`原理图.pdf`（SWD_OfflineDownloader.SchDoc，2019/11/29）+ `下载器开源.代码-2022.11.03` 源码交叉核对。
> 工程：Keil MDK 工程 `OfflineDownloader.uvprojx`，目标芯片 **STM32F103RET6**（LQFP64，Cortex-M3，512KB Flash / 64KB RAM）。

---

## 1. 产品与硬件概览

### 1.1 产品定位
本板是一台**离线编程器（脱机下载器）**：通过 USB 从 PC 接收固件文件存入板载 SPI Flash，脱机后通过 SWD / SWIM 接口给目标芯片（STM32 / STM8 / 蓝牙等）烧录。板载 1.8" LCD 做菜单与状态显示，6 个按键操作，蜂鸣器提示，状态 LED 指示。

### 1.2 板上主要器件
| 位号 | 器件 | 说明 |
|---|---|---|
| U5 | STM32F103RET6 | 主控，LQFP64 |
| U8 | W25Q64 | 板载 SPI NOR Flash，64Mbit = **8MB**（源码兼容 W25Q80/16/32/64/128，按 JEEC/ID 自动识别） |
| O1 | 16MHz 无源晶振 | HSE 外部时钟，配 22pF 负载电容 C16/C17 |
| U1/U4 | ASM1117-3.3 | LDO，USB_5V → 3.3VD |
| F1 | 400mA 自恢复保险丝 | USB 输入过流保护 |
| U3 | USBLC6 / U6 SRV05-4 | USB 口 ESD 保护 |
| U2 | TYPE-C-16P | USB-C 座，USB_D+ / USB_D- |
| U7 | TXB0102 | 电平转换（LCD 侧） |
| Q1/Q2/Q3 | SS8050 | NPN，分别驱动状态 LED、背光/LED_CTL、蜂鸣器 |
| U9 | 无源蜂鸣器 | BEEP_CTL 经三极管驱动 |
| F2/F3/F4 | FPC 1.0 / 0.8 座 | LCD 屏、按键等排线接口 |

### 1.3 软件运行环境
- **RTOS**：µC/OS-III（源码目录 `UCOSIII/`），多任务调度；任务定义与优先级见 `main.c` 顶部。
- **USB 协议栈**：TeenyUSB（目录 `12_teenyUSBStack/`），枚举为 USB HID 设备，FS 全速。
- **时钟**：HSE 16MHz → PLL 倍频至 **SYSCLK 72MHz**；APB1 36MHz，APB2 72MHz。时钟配置见 `11_rj_lib/ClockConfigurationByCRJ.c`。
- **版本判别（运行时）**：固件通过读 RCC_CFGR 的 PLL 倍频位/分频位判别 `isHIDVersion`（HID 版=1，LED/老版=0）；`HARDWARE_VERSION`（1=LCD 版，0=mini 版）在 `10_app/stm32f10x.h` 编译期宏定义。

### 1.4 关键编译/运行配置宏（`10_app/stm32f10x.h`）
| 宏 | 当前值 | 含义 |
|---|---|---|
| `HARDWARE_VERSION` | 1 | 1=LCD 屏版，0=mini 精简版 |
| `MINI_VERSION` | 0 | mini 版标识 |
| `FORCE_VERSION` | 0 | 强制版本（0 不强制） |
| `ENABLE_APP_ISSUE` | 0 | 是否使能 APP 发布（影响中断向量偏移） |
| `CLOSE_SWDPIN` | 0 | 是否关闭板载 SWD 调试引脚 |
| `L_LIGNT_LV` | 配置区 Flash 字 | LCD 背光等级 0–9（10%–100%） |
| `L_BEEP_ENABLE` | 配置区 Flash 字 | 蜂鸣器开关（0x55AA 为关闭） |

---

## 2. 主控引脚功能总表（核心交付）

> 说明：
> - 表中"原理图网络名"来自 `原理图.pdf`；"源码宏/复用"来自固件。
> - 本板存在**两套硬件分支**（HID 并口版 / LED-SPI 版），源码用 `isHIDVersion` 在运行时分流；引脚在两套分支下功能不同的，已在备注列分别列出。
- `OutputDataN_RW` 是源码自定义的位带别名，等价于 `GPIOx->ODR` 的第 N 位，即 **PxN 引脚**；`InputDataN_R` 等价于 `GPIOx->IDR` 第 N 位。
> - 带 JTAG 默认复用的引脚（PA13/PA14/PA15/PB3/PB4），固件已执行 `AFIO->MAPR = 2`（关闭 JTAG、保留 SWD），释放为普通 GPIO 使用，见 `SWD_Module.c` 的 `MaskSWDPin()`。

### 2.1 电源 / 时钟 / 复位 / 启动
| 脚 | 引脚 | 连接 | 方向 | 备注 |
|---|---|---|---|---|
| 1 | VBAT | 3.3VD | 电源 | 后备电源，本板接 3.3V |
| 12 | VSSA | GND | 电源 | 模拟地 |
| 13 | VDDA | 3.3VD | 电源 | 模拟电源 |
| 18 | VSS | GND | 电源 | |
| 19 | VDD | 3.3VD | 电源 | |
| 31 | VSS | GND | 电源 | |
| 32 | VDD | 3.3VD | 电源 | |
| 47 | VSS | GND | 电源 | |
| 48 | VDD | 3.3VD | 电源 | |
| 63 | VSS | GND | 电源 | |
| 64 | VDD | 3.3VD | 电源 | |
| 5 | PD0 / OSCIN | 16MHz 晶振 | 输入 | HSE 输入 |
| 6 | PD1 / OSCOUT | 16MHz 晶振 | 输出 | HSE 输出 |
| 7 | NRST | 复位按键/排针 | 输入 | 低电平复位 |
| 28 | PB2 / BOOT1 | BOOT1 | — | 启动模式配置 |
| 60 | BOOT0 | 启动模式配置（配 R10/R11 上下拉） | 输入 | BOOT0=0 从主 Flash 启动 |

### 2.2 USB 通信（内置 USB-FS Device）
| 脚 | 引脚 | 网络 | 方向/复用 | 备注 |
|---|---|---|---|---|
| 45 | PA12 | USB_D+ | 复用推挽 | USBDP，经 22R 串联 + ESD |
| 44 | PA11 | USB_D- | 复用推挽 | USBDM，经 22R 串联 + ESD |

### 2.3 调试 / 目标烧录接口（对目标芯片输出）
| 脚 | 引脚 | 原理图网络 | 源码符号 | 方向/复用 | 备注 |
|---|---|---|---|---|---|
| 20 | PA4 | RESET_CTR | SWD_RESET / LED 版 W25Q_CS | 推挽输出 | 目标芯片复位线 |
| 41 | PA8 | SWCLK_CTR (SWIM_OUT) | SWD_SWCLK_PIN / SWIM 输出 | 输出 / TIM1_CH1 | SWD 时钟；兼 STM8 SWIM 波形输出（DMA1_Ch2） |
| 56 | PB4 | SWDIO_CTR (SWIM_RST) | SWD_SWDIO_PIN / SWIM_RST | 双向 / 可切输入上拉 | SWD 数据；JNTRST，需关闭 JTAG 后可用 |
| 14 | PA0 | SWIM_IN | SWIM_IN1 | 输入 / TIM2_CH1 捕获 | STM8 SWIM 输入，DMA1_Ch5 |
| 15 | PA1 | SWIM_IN | SWIM_IN2 | 输入 / TIM2_CH2 捕获 | STM8 SWIM 输入，DMA1_Ch7 |
| 46 | PA13 | JTMS/SWDIO | — | 内部调试口 | 板上 SWD 调试器接口（保留） |
| 49 | PA14 | JTCK/SWCLK | — | 内部调试口 | 板上 SWD 调试器接口（保留） |

### 2.4 LCD 屏接口（8080 并口，HID 版；LED 版走 SPI2，见 3.1）
| 脚 | 引脚 | 原理图网络 | 源码宏（`lcd1_8inch.h`） | 方向 | 备注 |
|---|---|---|---|---|---|
| 40 | PC9 | LCD_D0 | 并口数据口（GPIOC 高 8 位一次性输出） | 输出 | 数据位 0 |
| 39 | PC8 | LCD_D1 | 同上 | 输出 | 数据位 1；源码状态灯宏 `NO_HID_LED1` 也配在此脚 |
| 51 | PC10 | LCD_D2 | 同上 | 输出 | 数据位 2；复用 UART4_TX |
| 52 | PC11 | LCD_D3 | 同上 | 输出 | 数据位 3；复用 UART4_RX |
| 53 | PC12 | LCD_D4 | 同上 | 输出 | 数据位 4；复用 UART5_TX |
| 2 | PC13 | LCD_D5 | 同上 | 输出 | 数据位 5 |
| 3 | PC14 | LCD_D6 | 同上 | 输出 | 数据位 6 |
| 4 | PC15 | LCD_D7 | 同上 | 输出 | 数据位 7 |
| 59 | PB7 | LCD_RD | `HID_LCD_RD` | 输出 | 读选通（本设计基本不用读） |
| 58 | PB6 | LCD_WR | `HID_LCD_WR` | 输出 | 写选通，下降沿锁存数据 |
| 57 | PB5 | LCD_D/C | `HID_LCD_DC` | 输出 | 命令/数据选择（RS）：0=命令，1=数据 |
| 55 | PB3 | LCD_RST | 源码 `HID_LCD_RST` 定义在 PB4（见 §7 差异 D1） | 输出 | 屏硬件复位，低电平复位 |
| 54 | PD2 | LCD_CS | `HID_LCD_CS` | 输出 | 片选，低有效 |
| 17 | PA3 | LED_CTL / LCD 背光 | TIM5_CH4 PWM | 复用 PWM 输出 | 背光调光，等级 0–9；源码 `LED_CTL` |

### 2.5 按键（6 键，低电平有效，内部上拉）
| 功能 | HID/LCD 版引脚（源码） | 原理图网络 | LED/老版引脚（源码） | 备注 |
|---|---|---|---|---|
| 上 / KeyUp | PA5 | K-Up | PC4 | 菜单上移 |
| 下 / KeyDown | PA7 | K-Down | PC3 | 菜单下移 |
| 左 / KeyLeft | PC4 | K-Left | PC2 | 菜单左移 |
| 右 / KeyRight | PC3 | K-Right（原理图在 PC2，见 §7 D2） | PC5 | 菜单右移 |
| 中 / KeyMedium | PA6 | K-Medium | PA2 | 确认/选中 |
| 锁 / KeyLock | PC2 | K-Lock（原理图在 PC1，见 §7 D2） | PB11 | 长按进入锁定/信息页 |
| （mini 版） | PA15 | — | — | mini 版仅 1 个中键 |

按键检测：20ms 周期扫描（`main.c` 的 `Key_Task`），支持单击、双击、长按，状态机见 `1_Key/key_state_By_CRJ_V2.c`。

### 2.6 蜂鸣器 / 状态指示 / 其他
| 脚 | 引脚 | 网络 | 源码符号 | 方向/复用 | 备注 |
|---|---|---|---|---|---|
| 16 | PA2 | BEEP_CTL | HID 版蜂鸣器，TIM5_CH3 PWM | 复用 PWM 输出 | LED 版此脚为 KeyMedium |
| 26 | PB0 | ADSample | LED 版蜂鸣器，TIM3_CH3 | 复用 PWM 输出 | 本板网络名 ADSample，可做 ADC 采样预留 |
| 8 | PC0 | LED_1 | 状态指示（源码 `NO_HID_LED1` 配 PC8，见 §7 D3） | 推挽输出 | 运行指示灯 |
| 9 | PC1 | K-Lock | 源码 `HID_LED` 状态灯 | 双向/上拉 | 与锁定键复用，见 §7 D3 |
| 29 | PB10 | Comm | 预留通信口（复用 UART3_TX） | 复用 | 原理图注"Comm 接口用于日后电池应用" |
| 30 | PB11 | 预留 | 复用 UART3_RX；LED 版 KeyLock | — | |
| 37 | PC6 | FLASH_CS | `W25QXX_HID_CS` | 推挽输出 | 外部 Flash 片选（HID 版） |
| 34 | PB13 | FLASH_SCK | SPI2_SCK | 复用 | 外部 Flash 时钟 |
| 35 | PB14 | FLASH_MISO | SPI2_MISO | 复用输入 | 主机输入 |
| 36 | PB15 | FLASH_MOSI | SPI2_MOSI | 复用 | 主机输出 |
| 27 | PB1 | 预留 | 源码 `MINI_RESET_2` | — | |
| 25 | PC5 | 预留 | LED 版 KeyRight | — | |
| 38 | PC7 | 预留 | 源码 `LED_MINI_2` | — | |
| 50 | PA15 | 预留 | mini 版 KeyMedium；JTDI | — | 关闭 JTAG 后可用 |
| 61 | PB8 | 版本识别（接 GND = 新版本） | 输入（上下拉读取） | 原理图注"PB8 接 GND 表示为新版本" |
| 62 | PB9 | 预留 | — | — | |

### 2.7 串口（固件实际使用）
| 串口 | 引脚 | 波特率 | 用途 | 源码 |
|---|---|---|---|---|
| UART4 | PC10 = TX，PC11 = RX | 460800 | 与上位机/调试通信（带中断接收环形缓冲，400 字节） | `USART/STM32_USART.c`，`InitPeripheral()` 初始化 |

> 注意：PC10/PC11 同时被 LCD 并口 D2/D3 占用（§2.4），两套功能在不同硬件版本二选一。PB10/PB11 为原理图预留 Comm 口（UART3），固件未初始化。

---

## 3. LCD 屏驱动现状

### 3.1 基本参数
| 项目 | 参数 |
|---|---|
| 尺寸 | 1.8 英寸彩色 TFT |
| 驱动 IC | **ST7735S / ST7735R**（初始化序列见 `lcd1_8inch.c` 的 `LCDInitOfST7735CtlIC()`） |
| 分辨率 | **128（宽）× 160（高）**（`Width=128, Height=160`） |
| 色深 | 16 位 RGB565（命令 0x3A=0x05，65K 色） |
| 显存方向 | 命令 0x36 = 0xC0（MX/MY 翻转，按本板排线方向配置） |
| 接口 | 两套并存：**HID 版 = 8080 8 位并口**；**LED/老版 = SPI2 串行接口** |
| 背光 | PA3，TIM5_CH4 PWM 调光，等级 0–9（对应占空比 10%–100%） |

### 3.2 两种接口的引脚与时序
**A. LED/老版 — SPI2 串行（`isHIDVersion == 0`）**
| 信号 | 引脚 | 源码宏 |
|---|---|---|
| SCK | PB13 | `LCD_SCK` |
| MOSI | PB15 | `LCD_MOSI` |
| CS | PB12 | `LCD_CS` |
| D/C（RS） | PA9 | `LCD_DataOrCommand` |
| RST | PA10 | `LCD_RST` |
- SPI 主机模式，8 位数据，MSB first，模式 **CPOL=1 / CPHA=0**（`PolarityType10`），波特率分频 2（APB1 36MHz / 2）。
- 无 MISO 接线（只写屏）。

**B. HID 版 — 8080 并口（`isHIDVersion == 1`，当前原理图板）**
- 8 位数据总线 PC8–PC15（见 §2.4），控制脚 PB7=RD、PB6=WR、PB5=D/C、PB3=RST、PD2=CS。
- 写时序：拉低 DC（命令/数据）→ 拉低 WR → 数据送上 GPIOC 高 8 位 → WR 上升沿锁存 → CS 拉高。
- 初始化流程：RST 低 300ms → 高 300ms → 0x11 退出睡眠（等 500ms）→ 帧率/电源/VCOM/Gamma 配置 → 0x3A 设 16 位色 → 0x29 开显示。

### 3.3 上层 API（`3_LCD1.8/lcd1_8inch.h`、`lcdDisplay.h`）
| 函数 | 功能 |
|---|---|
| `LCDInitOfST7735CtlIC()` | 屏初始化（上电复位 + 全套 ST7735 寄存器序列） |
| `LCD_DispalyColor(u16 RGB)` | 全屏填充单色 |
| `LCD_DispalyRGB()` | 红/绿/蓝三色彩条测试 |
| `LCD_DispalyImage(u8 *pic)` | 全屏显示 16 位图（数组长度 = 128×160×2） |
| `LCD_DispalyImage_Size(pic, x, y, w, h)` | 局部区域贴图 |
| `LCD_ShowString(x, y, size, color, str)` | 显示 ASCII 字符串；`size` 支持 12/16 号字 |
| `LCD_ShowString_Language(x,y,size,color,pC,pE)` | 中英双语串（按语言设置择一显示） |
| `LCD_DispalyWindows()` | 设置全屏窗口（列地址 0–0x7F，行地址 0–0x9F） |
| `freashDisplayInformation(...)` | 菜单/文件列表界面刷新（`lcdDisplay.c`） |

### 3.4 字体与资源
- 字库：`lcdFont.c`（约 1.28MB，ASCII 12 号/16 号点阵 + 中文字模），`lcdFont.h` 声明取模数组。
- 颜色常量：`RED 0xF800`、`GREEN 0x07E0`、`BLUE 0x001F`、`YELLOW 0xFFE0`、`WHITE 0xFFFF`、`BLACK 0x0000`、`PURPLE 0xF81F`。
- 并发保护：`LCD_ShowsrtingUseRight_Mutex`（µC/OS 互斥量），多任务刷屏前需获取。

### 3.5 二次开发提示
- 换屏：若沿用 ST7735 兼容控制器，只需核对初始化序列与行列地址偏移；若换控制器（如 ILI9341），需重写 `LCD_WriteCommand/LCD_WriteData` 与初始化序列。
- 刷屏性能：并口版快于 SPI 版；`lcdDisplay.c` 按菜单局部刷新，避免全屏重绘。
- 背光等级存于内部 Flash 配置页（`L_LIGNT_LV`），上电读出后配 TIM5_CH4 占空比。

---

## 4. 外部 SPI Flash 驱动现状

### 4.1 器件与连接
| 项目 | 内容 |
|---|---|
| 器件 | Winbond W25Q64（原理图 U8），64Mbit = **8MB**；源码 ID 表兼容 W25Q80(1MB)/16(2MB)/32(4MB)/64(8MB)/128(16MB) |
| 总线 | **HID 版：SPI2**（PB13 SCK / PB14 MISO / PB15 MOSI，CS=PC6）；**LED 版：SPI1**（PA5 SCK / PA6 MISO / PA7 MOSI，CS=PA4） |
| SPI 配置 | 主机、8 位、MSB first、模式 **CPOL=0 / CPHA=0**（`PolarityType00`）；HID 版分频 2，LED 版分频 4；关闭中断轮询等待忙 |
| 寻址 | 24 位地址（最大 16MB，兼容 W25Q128） |
| 页 / 扇区 / 块 | 页 256B；扇区 4KB；块 64KB；W25Q64 共 128 块 × 16 扇区 × 256 页 |

### 4.2 支持的命令集（`W25Qxx/w25qxx.h`）
| 命令码 | 功能 |
|---|---|
| 0x06 / 0x04 | 写使能 / 写禁止 |
| 0x05 / 0x01 | 读状态寄存器 / 写状态寄存器 |
| 0x03 / 0x0B | 读数据 / 快速读 |
| 0x3B | 双线快速读（预留） |
| 0x02 | 页编程（256B/页） |
| 0x20 | 扇区擦除（4KB） |
| 0xD8 | 块擦除（64KB） |
| 0xC7 | 整片擦除 |
| 0xB9 / 0xAB | 掉电 / 唤醒（DeviceID） |
| 0x90 / 0x9F | 读器件 ID / 读 JEDEC ID |

### 4.3 底层 API（`w25qxx.c`）
| 函数 | 功能 |
|---|---|
| `W25QXX_Init()` | 初始化对应 SPI 与片选，读 ID 识别型号存入 `W25QXX_TYPE` |
| `W25QXX_ReadID()` | 读器件 ID（0x90） |
| `W25QXX_ReadSR() / Write_SR(sr)` | 读/写状态寄存器（BUSY/WEL/保护位） |
| `W25QXX_Read(buf, addr, len)` | 连续读（0x03） |
| `W25QXX_Write_Page(buf, addr, len)` | 页编程（≤256B，自动等忙） |
| `W25QXX_Write_NoCheck(buf, addr, len)` | 跨页写（不擦除，要求目的区已为 0xFF） |
| `W25QXX_Write(buf, addr, len)` | 带擦除写：按 4KB 扇区读出→改写→擦除→回写 |
| `W25QXX_Erase_Sector(addr) / Erase_Block(addr) / Erase_Chip()` | 扇区/块/整片擦除 |
| `W25QXX_Wait_Busy()` | 轮询 BUSY 位等待操作完成 |
| `W25QXX_PowerDown() / WAKEUP()` | 低功耗 / 唤醒 |

### 4.4 文件管理与存储布局（`W25Qxx/memoryManage.c/.h`）
固件在裸 Flash 上实现了一套简单的"文件"索引管理（非 FATFS）：

| 区域 | 地址范围（W25Q64） | 内容 |
|---|---|---|
| 用户数据区 | 0x000000 – 0x0EFFFF | 各固件文件数据，按索引表顺序存放 |
| 文件索引区 | Block15：`0x0F0000` – `0x0FFFFF`（64KB） | 文件描述符数组，每条 512B（`_FileInformation` 结构体），最多约 120 个文件；含芯片型号、起止地址、长度、校验和、加密校验、烧录次数、文件名、选项字节、滚动配置等 |
| 4KB 缓存 | RAM `W25Q4KBuf.databuf4K[4096]` | 扇区改写缓存 |

- 文件描述符结构体 `_FileInformation`（1 字节对齐）：状态字、芯片型号前后缀、程序起始地址、在 Flash 保存地址、程序长度、复位方式、擦除方式、校验方式、烧录次数、16 位校验和、AES 安全校验、烧录位掩码、SWD 低速使能、文件名[34]、选项字节、STM8 配置区、滚动码配置等。
- 管理 API：`memoryManageInit()`、`W25QFlashProgramFileInformation()`（扫描索引区统计剩余容量/文件数）、`arrangeTheProgramFile()`（整理文件）、`deleteFile(fileType, fileIndex)`、`W25QXX_FlieDensityAddress(block, sector, page)`（块/扇区/页 → 24 位地址换算）。
- 上位机状态回包结构：`_UpStatusInfo`（设备状态）、`_UpFileInfo`（文件信息），40 字节定长，帧头 0xAA 0x55。

### 4.5 二次开发提示
- 若新产品只需存少量参数/字库：可直接用 `W25QXX_Read/Write/Erase_Sector`，跳过 `memoryManage` 文件层。
- 若需文件系统：可移植 FATFS 挂到 SPI Flash，替换现有索引层；注意现有索引区在 Block15，移植时避免冲突。
- 写前必须确保目标位为 0xFF；`W25QXX_Write` 已做扇区读出-改写-擦除-回写，大块连续写优先用它。
- 更换更大容量 Flash（如 W25Q128 16MB）：24 位地址已支持，核对 ID 表与容量参数即可。

---

## 5. 片上定时器 / DMA / 中断资源分配

| 资源 | 用途 | 关联引脚 / DMA |
|---|---|---|
| SysTick | µC/OS-III 系统节拍 | — |
| TIM1 | STM8 SWIM 发送波形（PWM） | CH1 = PA8；DMA1_Ch2 |
| TIM2 | STM8 SWIM 输入捕获 | CH1=PA0 / CH2=PA1；DMA1_Ch5（CH1）、DMA1_Ch7（CH2） |
| TIM3 | LED/老版蜂鸣器 PWM（约 2.7kHz，50% 占空比） | CH3 = PB0 |
| TIM5 | HID 版蜂鸣器 PWM（CH3）+ LCD 背光 PWM（CH4） | CH3=PA2，CH4=PA3 |
| TIM6 | 蜂鸣器节拍中断（控制鸣响次数/间隔，1ms 时基） | 中断 `TIM6_IRQHandler` |
| DMA1 | SWIM 输入/输出波形搬运 | Ch2 / Ch5 / Ch7 |
| USART4 | 上位机通信，中断接收环形缓冲 | PC10/PC11，460800 |
| USB-FS | HID 设备端点 EP1 收发，64 字节缓冲 | PA11/PA12 |
| CRC / AES | 固件校验、加密（目录 `CRC/`、`AES/`） | — |

### µC/OS-III 主要任务（`main.c`）
| 任务 | 优先级 | 栈大小 | 职责 |
|---|---|---|---|
| `start_task` | 60 | 128 | 启动后创建其余任务 |
| `SystemTimerOperate_Task` | 30 | 2048 | 周期系统操作 |
| `USARTDataProcess_Task` | 13 | 2048 | 上位机协议解析（优先级较高，防止丢包） |
| `AutoDownloader_Task` | 21 | 1024 | 自动脱机烧录流程 |
| `Key_Task` | 12 | 2048 | 20ms 按键扫描与菜单分发 |

---

## 6. 工程目录导航

```
下载器开源.代码-2022.11.03/
├── main.c                     # 主入口、任务创建、按键任务、菜单流程
├── OfflineDownloader.uvprojx  # Keil 工程文件（目标 STM32F103RE）
├── 1_Key/                     # 按键状态机（单击/双击/长按）
├── 2_SPI/                     # SPI1/SPI2 主机/从机通用驱动（轮询，带缓冲）
├── 3_LCD1.8/                  # ST7735 屏驱动、界面刷新、字库
├── 4_STM32_TIMER/             # 软件延时 / 定时器时基
├── 5_SWDModule/               # SWD 协议层（DP/AP 读写、halt/erase/write flash）
├── 6_Beep/                    # 蜂鸣器初始化与鸣响控制（TIM3/TIM5/TIM6）
├── 7_SWIMModule/              # STM8 SWIM 协议层（TIM1/TIM2 + DMA）
├── 8_NewChip/                 # 各目标芯片适配（STM32L0/L4/F7、BlueNRG、STM8 等）
├── AES/  CRC/  Encrypt/        # 加密、校验、选项字保护
├── 10_app/                    # USB HID 类、设备描述符、芯片寄存器位带定义
├── 11_rj_lib/                 # 时钟配置、寄存器层
├── 12_teenyUSBStack/           # TeenyUSB 协议栈（设备/主机，HID/CDC/MSC）
├── 13_STM32F1xx_HAL_Driver/   # HAL 头文件（本工程主要用寄存器层）
├── 14_stm32f1/                # 启动文件、HAL 配置、板级 USB 配置
├── STM32_FLASH/               # 片内 Flash 读写驱动
├── STM32F10X_Lib_UCOSIII/     # CMSIS、µC/OS 移植层
├── UCOSIII/                   # µC/OS-III 内核、CPU 移植、配置、BSP
├── USART/                     # 串口驱动（环形缓冲 + 可选 DMA 发送）
├── W25Qxx/                    # W25Q 驱动 + 文件管理 + 数据手册 PDF
└── ForSampleMain/             # InitPeripheral()、上位机消息结构
```

---

## 7. 已知源码 / 原理图差异与踩坑提示

> 接手时务必先核对实物，以下为源码与原理图/不同硬件版本之间的不一致点：

- **D1. LCD_RST 引脚**：原理图 PB3 = LCD_RST、PB4 = SWDIO_CTR；源码 `lcd1_8inch.h` 中 `HID_LCD_RST` 定义在 **PB4**，而 `SWD_Module.h` 中 PB4 又是 SWDIO。移植并口屏时先按原理图确认 RST 实际接脚，再改宏定义。
- **D2. 右键 / 锁键引脚**：原理图 K-Right=PC2、K-Lock=PC1；源码 HID 版 `KeyRight=PC3`、`KeyLock=PC2`。若按键错位，对照 §2.5 表调整 `main.c` 中 `keyDetectV2` 的输入脚。
- **D3. 状态灯引脚**：原理图 LED_1=PC0；源码 `NO_HID_LED1=PC8`、`HID_LED=PC1`。PC8/PC1 同时参与 LCD 并口/按键，复用关系按实物版本确认。
- **D4. 串口与并口复用**：UART4（PC10/PC11）与 LCD 并口 D2/D3 共用引脚，两种功能不能在同一硬件版本同时使用；源码 `InitPeripheral()` 初始化 UART4，若改用并口 LCD，注意串口数据脚被占用。
- **D5. JTAG 引脚释放**：PB3/PB4/PA15 默认是 JTAG 功能，固件在 `MaskSWDPin()` 中写 `AFIO->MAPR=2` 关闭 JTAG 保留 SWD 后才能当 GPIO；新代码若用到这几个脚，必须先执行该重映射。
- **D6. 位带别名命名**：源码寄存器位操作封装为 `OutputDataN_RW` / `InputDataN_R`，N 即引脚号；读代码时不要把它当成独立外设。
- **D7. 双版本分流**：`isHIDVersion` 运行时由时钟配置反推，`HARDWARE_VERSION` 为编译期宏；新增外设代码时注意判断分支，避免在错误版本上配置引脚。
- **D8. 片内 Flash 配置页**：亮度、蜂鸣器、语言等配置存在主控片内 Flash 最后一页（`HighDensity_FlashPage(255)`），擦写片内 Flash 时不要误擦该页。
- **D9. 工程历史配置**：`DebugConfig/` 与 `Objects/` 下残留 C8/RC/VE/RE 等多种芯片调试配置；当前目标以 `OfflineDownloader.uvprojx` 内的 **STM32F103RE** 为准。
- **D10. 编码**：部分中文注释为 GBK 编码，在 UTF-8 环境打开会乱码，不影响编译；改注释时注意保存编码。

---

## 8. 二次开发快速上手步骤

1. **编译环境**：安装 Keil MDK（支持 Cortex-M3 / STM32F1），打开 `OfflineDownloader.uvprojx`，确认器件为 STM32F103RE、晶振 16MHz、仿真器按板上 SWD 口（PA13/PA14）连接。
2. **跑通基线**：先原样编译下载，确认 LCD 点亮、按键菜单可操作、蜂鸣器响、USB 枚举为 HID；再对照本文 §2 引脚表在原理图上逐脚核对。
3. **读代码主线**：`main()` → `InitPeripheral()` → `BeepInit()` → `LCDInitOfST7735CtlIC()` → `start_task` → 各业务任务；底层驱动入口：SPI（`2_SPI`）、Flash（`W25Qxx`）、LCD（`3_LCD1.8`）、按键（`1_Key`）、串口（`USART`）、定时器（`4_STM32_TIMER` / `6_Beep`）。
4. **裁剪业务**：新产品若不需要离线烧录，可删除 `5_SWDModule`、`7_SWIMModule`、`8_NewChip`、`Encrypt`、`AES` 等下载业务模块，保留板级支撑（时钟、GPIO、SPI、Flash、LCD、按键、串口、OS）。
5. **新增功能**：
   - 接新传感器/执行器：先查 §2 表中预留脚（PB1、PC5、PC7、PB9、PB10/PB11 等），在 `InitPeripheral()` 或对应模块初始化里配 GPIO/复用。
   - 存参数：优先复用 W25Q 现成 `W25QXX_Read/Write`；存字库/图片可放 Flash 数据区，LCD 贴图接口已就绪。
   - 通信：USB-HID 协议栈可直接复用；需要串口时用 `ComInit(USARTx, baud, irq)` 框架。
6. **发布前检查**：核对配置宏（§1.4）、关闭未用外设时钟、确认引脚无冲突、备份片内 Flash 配置页、验证低功耗与上电复位时序。

---

*文档生成基于源码 2022.11.03 版本与 2019/11/29 原理图；如硬件改版，请以最新原理图与板上丝印为准并同步更新本文档。*
