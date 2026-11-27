# STM32F103 SWD 离线编程器

把 SWD 下载调试器(STM32F103C8T6 主控)改造成**离线烧录器**:
上位机经 USB 把目标固件(.bin)存入编程器内部 flash,之后编程器
每 300ms 探测 SWD 引脚,检测到目标 STM32F103 接入即自动
擦除 → 烧录 → 校验 → 复位,全程无需电脑。

## 硬件连接

| 编程器引脚 | 目标板 | 说明 |
|---|---|---|
| PA3 (SWCLK) | SWCLK / PA13 对应脚 | 时钟 |
| PA4 (SWDIO) | SWDIO / PA14 对应脚 | 数据 |
| GND | GND | **必须共地** |
| (USB) | — | 接电脑下载镜像 |
| PB12 | — | 状态 LED(低电平亮) |

目标板可以自供电,也可以由编程器板子供电,由硬件决定。

## Flash 布局(编程器自身,64KB)

```
0x08000000 - 0x080017FF  boot 6KB
0x08001800 - 0x08001FFF  升级标志区 2KB
0x08002000 - 0x080077FF  应用 22KB
0x08007800 - 0x08007FFF  烧录流水记录区 2KB
0x08008000 - 0x0800FFFF  目标固件镜像存储区 32KB (含 32 字节头)
```

> 很多 C8T6 实际是 128KB 硅片。若确认芯片有 128KB,可在 `board.h`
> 打开 `FLASH_128K` 宏,镜像容量扩大为 96KB。

## 编译与首次烧录

1. 用 Keil MDK5 打开 `project/offline_prog.uvproj`,直接 Build
   (已在 `C:\Keil_v5\ARM\ARMCC` 下用 armcc 5.06 全量编译链接验证,
   0 警告;`build_check/` 里留有参考 axf/bin)。
2. 首次需要把 `offline_prog.hex` 烧进编程器主板:
   用另一支调试器(ST-Link/J-Link)接主板的 SWD 口烧录,
   或用 F103 串口 ISP(BOOT0 拉高 + USART1)。
   之后它就自给自足了。

## 上位机使用(host_tool/swdtool.py)

依赖 `pip install pyserial`。Windows 下设备枚举为 CDC 虚拟串口
(VID:PID = 0483:5740,Win10+ 自动装驱动)。

```bash
python swdtool.py flash -p COM5 app.bin   # 下载固件到编程器(自动擦除+CRC校验)
python swdtool.py info                    # 查看镜像信息(不带 -p 自动扫串口)
python swdtool.py status                  # 查看烧录状态/上次结果/错误码
python swdtool.py prog                    # 手动触发一次烧录
python swdtool.py ping                    # 测试连接
```

固件文件必须是**二进制 .bin**(从 0x08000000 开始的裸镜像)。
Keil 生成 bin:Options → User → After Build 加
`fromelf --bin -o ./obj/app.bin ./obj/app.axf`。

## 工作状态 LED

灯效由 SysTick 中断驱动 (见 bsp.c 的 LED_SetState),即使主循环被烧录流程
长时间阻塞,灯也会持续按节奏闪烁,不会冻住。

| LED | 含义 |
|---|---|
| 极慢单闪(2s 一下) | 无镜像(先用上位机下载固件) |
| 中闪(~2.5Hz) | 有镜像,正在探测目标 |
| 双闪 | 已识别到目标,稳定电源中,**请勿拔出** |
| 快闪(~16Hz) | 正在烧录,**切勿断开连接** |
| 常亮 | 烧录成功,等待拔出目标 |
| 三短闪循环 | 烧录失败,等待拔出目标 |

> 排查自动烧录问题时,可依此判断卡在哪一步:
> 始终"中闪" = 没探测到目标(SWD 接线/目标供电/目标已禁用 SWD);
> "双闪"后回到"中闪" = 目标在稳定窗口内被拔走或连接不稳;
> "快闪"后"三短闪" = 进入烧录但中途失败(看日志/状态码)。

**一次接入只烧录一次**:成功后必须拔出目标再接下一片,
防止对同一片反复擦写。

## 目标芯片切换(board.h 的 TARGET_CHIP)

```c
#define TARGET_CHIP  TARGET_CHIP_GD32F1X0   /* GD32F130/150 */
#define TARGET_CHIP  TARGET_CHIP_STM32F1    /* STM32F101/103 系列 */
```

- **STM32F1**:接受 DEV_ID 0x410/412/414/418/420/428/430
- **GD32F1x0**:要求 DEV_ID=0x410 且 REV_ID=0x1303(GD32 与 STM32F1
  共用 0x410,靠 REV_ID 区分,防止两种板子混线时误烧)

两个系列的 FMC 寄存器、解锁密钥、半字编程流程兼容,切换宏即可,
无需改其他代码。若 GD32 报错码 2(型号不符),用 `status` 查看
`DBGMCU_ID` 的实际值反馈修订 REV 白名单。

## 烧录流程(对目标片)

SWD 位带(PA3/PA4, 约 1MHz)→ 读 DPIDR(0x1BA01477)→ 读
DBGMCU_IDCODE 确认 F1 系列 → 冻结目标看门狗 + 停机 → AHB-AP 直接
操作目标 FPEC 寄存器:解锁 → 全片擦除 → 半字编程 → 回读 CRC32
校验 → 复位运行。32KB 镜像约 10~20 秒。

## 常见问题

- **三短闪 / 错误码 5**:目标片开了读保护(RDP)。需先用
  ST-Link Utility 等工具解除保护(会全片擦除)。
- **错误码 1(SWD 连接失败)**:检查 SWCLK/SWDIO/GND 接线;
  目标如运行了禁用 SWD 脚的固件,可在目标复位瞬间接入,
  或给目标板加复位控制。
- **烧录速度慢**:正常,采用寄存器直写方案(可靠性优先);
  如需提速可改 RAM 下载算法(`target_prog.c` 留有替换空间)。
- **USB 枚举不出来**:确认编程器板子是 8MHz 晶振
  (USB 需要 48MHz 精确时钟);若板上 D+ 上拉电阻由 GPIO 控制,
  需在 `usb_hw.c` 的 `USB_HW_Init()` 里拉高对应引脚。

## 源码结构

```
app_offline_prog/
  project/offline_prog.uvproj   Keil 工程
  source/
    main.c         主状态机:探测/自动烧录/LED
    board.h        引脚与 flash 布局配置
    bsp.c          LED/SysTick/GPIO 配置
    usb_hw.c       F103 USB FS 底层(寄存器+PMA)
    usb_cdc.c      CDC ACM 类与描述符
    proto.c        上位机二进制帧协议(AA 55 ... CRC16)
    storage.c      本机 flash 镜像存储(FPEC)
    swd_host.c     SWD 主机位带驱动(ARM ADIv5)
    target_prog.c  目标 F103 烧录流程
    crc.c          CRC16-CCITT / CRC32-IEEE
host_tool/
  swdtool.py       PC 上位机(pyserial)
```
