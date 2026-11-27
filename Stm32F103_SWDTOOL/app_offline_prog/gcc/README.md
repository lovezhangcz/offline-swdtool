# STM32F103 SWD 离线编程器 — GNU Arm Embedded (GCC) 工程

这是 `project/offline_prog.uvproj`（Keil MDK / ARMCC）之外新增的 **GCC 编译链工程**。

两份工程共用同一套源码，原 Keil 工程与源码**不做任何修改**；GCC 专属的启动文件、

链接脚本、`at()` 属性兼容头、构建与烧录脚本都放在本目录。

## 与 Keil target 的对应关系



| make target    | Keil target                 | 预定义宏                              | Flash 起始 / 长度      |
| -------------- | --------------------------- | --------------------------------- | ------------------ |
| `boot`         | `offline_prog_BOOT`         | `BUILD_BOOT USB_MODE=1`           | 0x08000000 / 6 KB  |
| `app-cdc`      | `offline_prog_APP_CDC`      | `BUILD_APP USB_MODE=1`            | 0x08002000 / 22 KB |
| `app-msc`      | `offline_prog_APP_MSC`      | `BUILD_APP USB_MODE=3`            | 0x08002000 / 22 KB |
| `app-cdc-256k` | `offline_prog_APP_CDC_256K` | `BUILD_APP USB_MODE=1 FLASH_256K` | 0x08002000 / 22 KB |
| `app-msc-256k` | `offline_prog_APP_MSC_256K` | `BUILD_APP USB_MODE=3 FLASH_256K` | 0x08002000 / 22 KB |

整片 Flash 布局（与 `boot.h`、Keil scatter 文件一致）：



```
0x08000000  6KB    boot

0x08001800  2KB    升级标志页 (magic / len / crc32 / valid)

0x08002000  22KB   应用 (APP)

0x08007800  ...    记录区 + 目标镜像区 (由应用运行时管理)
```

RAM：0x20000000，20 KB（栈顶 `_estack = 0x20005000`）。

## 目录



```
gcc/

├── Makefile                      # 全部构建规则（5 个 variant）

├── build.bat                     # Windows 一键编译（自动配 PATH）

├── startup\_stm32f10x\_md.s        # GCC 语法启动文件 + 59 向量表 + 弱定义

├── stm32f103c8\_boot.ld           # boot 链接脚本 (6KB @ 0x08000000)

├── stm32f103c8\_app.ld            # app  链接脚本 (22KB @ 0x08002000, .app\_fw\_info @ +0x100)

├── gcc\_compat.h                  # 把 ARMCC 的 \_\_attribute\_\_((at(x))) 映射为自定义段

└── tools/

&#x20;   ├── gen\_flag.py               # 由 app.bin 计算 len/CRC32，生成升级标志页 flag.bin

&#x20;   ├── flash.jlink               # J-Link 烧录脚本（boot + app + flag + 回读校验 + 复位运行）

&#x20;   ├── program.bat               # gen\_flag.py + J-Link 一键烧录（按日志判定成功）

&#x20;   ├── debug.gdb                 # GDB 调试脚本模板（reset → break main → continue → 单步）

&#x20;   └── debug.ps1                 # 一键启动 GDB Server + GDB（UTF-8 with BOM）
```

编译选项要点（`Makefile`）：



```
CFLAGS : -mcpu=cortex-m3 -mthumb -mfloat-abi=soft -std=gnu11

&#x20;        -Oz -g3 -gdwarf-4 -flto                   # LTO 是 MSC 变体不超 22KB 的关键

&#x20;        -ffunction-sections -fdata-sections -fno-common

&#x20;        -fno-unwind-tables -fno-asynchronous-unwind-tables

LDFLAGS: -nostartfiles -flto -T\<linker.ld> -Wl,--gc-sections

&#x20;        -Wl,-Map=...,--print-memory-usage --specs=nano.specs --specs=nosys.specs
```

> 改 CFLAGS 后一定先 
>
> `build.bat clean`
>
>  再编，否则旧的 
>
> `.o`
>
>  会被复用。
> 想临时加选项可以用 
>
> `make BUILD_VARIANT=app-msc _build EXTRA_CFLAGS=... EXTRA_LDFLAGS=...`
> （Makefile 末尾预留了这两个变量）。

## 编译



```
cd app\_offline\_prog\gcc

build.bat                 :: 全部 5 个 target

build.bat boot            :: 只编 boot

build.bat app-cdc         :: 只编 CDC 应用

build.bat clean
```

产物在 `build/<variant>/`：



```
offline\_prog\_boot.elf / .bin / .hex / .map

offline\_prog\_app\_cdc.elf / .bin / .hex / .map

...
```

也可以用 GNU make（需自行把工具链和 make 加入 PATH）：



```
make            # 全部

make app-msc

make clean
```

工具链位置通过环境变量覆盖：



```
set TOOLCHAIN\_BIN=D:\keilwork\tools\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin

set MAKE\_BIN=D:\keilwork\tools\make-4.4.1-win32\bin
```

## 烧录（J-Link）



```
build.bat flash
```

等价于 `make flash`，流程：



1. `tools\gen_flag.py` 读取 `build\app-cdc\offline_prog_app_cdc.bin`，

   按 `Boot_AppValid()` 的规则算 `len` 与 `CRC32`，生成 16 字节的 `flag.bin`

   （由 J-Link `loadbin` 写到 0x08001800）。**这一步不能省**：直接烧 app 而不写标志页，

   boot 会判定应用无效并停在 bootloader。

2. `JLink.exe -CommanderScript tools\flash.jlink`：

* 先把当前 64 KB Flash 备份到 `backup\flash_prev.bin`（可回滚）

* `loadbin` 烧 boot /app/flag（J-Link 只擦除被写到的扇区，不整片擦除）

* `verifybin` 回读校验

* 复位并运行

> J-Link 路径默认找 
>
> `C:\Program Files (x86)\SEGGER\JLinkARM_V474b\JLink.exe`
>
> ，
> 找不到会依次尝试 
>
> `C:\Program Files\SEGGER\JLink\JLink.exe`
>
> ；也可以设置环境变量 
>
> `JLINK`
>
> 。

## 调试

`build/*/**.elf` 带 `-g3 -gdwarf-4` 调试信息，用 J-Link GDB Server + arm-none-eabi-gdb：



```
powershell -ExecutionPolicy Bypass -File tools\debug.ps1

powershell -ExecutionPolicy Bypass -File tools\debug.ps1 -Elf build\app-msc\offline\_prog\_app\_msc.elf
```

脚本会：启动 `JLinkGDBServerCL.exe`（SWD/1 MHz，端口 2331）→ 等端口就绪 →

用 `tools\debug.gdb` 生成临时脚本 → 加载 app 的调试符号 → `monitor reset` →

在 `main` 下断点 → `continue`（boot 3 s 后跳 app 并命中 main）→ 打印 PC/SP/LR/ 调用栈 / 局部变量 →

`next` 单步 → `monitor go` 交还目标机 → 关闭 GDB Server。

手工调试等价命令：



```
"C:\Program Files (x86)\SEGGER\JLinkARM\_V474b\JLinkGDBServerCL.exe" ^

&#x20;   -device STM32F103C8 -if SWD -speed 1000 -port 2331

arm-none-eabi-gdb build/app-cdc/offline\_prog\_app\_cdc.elf

(gdb) target extended-remote :2331

(gdb) monitor reset

(gdb) break main

(gdb) continue
```

### J-Link 软件 V4.74b 的坑（本机实测）



| 现象                                                        | 原因 / 正确写法                                                                                                                                                                     |
| --------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 脚本报 `Unknown command`，`JLink.exe` 返回 1                    | ① 没有 `si SWD`，接口选择命令叫 `SelectInterface`；ARM-OB 默认就是 SWD，可不写 ② 没有 `connect` 命令，Commander 启动即自动连接 ③ **成功时退出码也是 1**，所以 `program.bat` 改成解析日志（3 次 `Verify successful` + 无 `Error`） |
| GDB Server 弹窗 "Unknown command line parameter -nogui" 并卡死 | V4.74b 不支持 `-nogui`，只传 `-device/-if/-speed/-port`                                                                                                                             |
| gdb 里 `file build\app-cdc\x.elf` 报文件不存在                   | gdb 把 `\` 当转义符，路径要用 `/`                                                                                                                                                       |
| 调试结束 `detach` 报 "Remote doesn't know how to detach"       | 用 `monitor go` 让目标继续跑，再退出 gdb                                                                                                                                                 |
| 调试后 CDC 不再枚举（COM 口消失）                                     | `monitor reset` 只复位内核，USB/RCC 外设状态残留。`debug.ps1` 在 gdb 退出后会自动用 `JLink.exe` 做一次整机复位（`r` + `g`），恢复后 COM 口重新枚举                                                                   |
| `.ps1` 脚本乱码 / 语法报错                                        | Windows PowerShell 5.1 按 ANSI 读无 BOM 的 UTF-8；`debug.ps1` 已保存为 **UTF-8 with BOM**                                                                                              |
| `.bat` 中文乱码                                               | `build.bat` / `program.bat` 为 GBK (936) + CRLF，并在开头 `chcp 936 >nul`                                                                                                           |

烧录产物与回滚：



```
backup\flash\_prev.bin     # 烧录前自动备份的整片 64 KB Flash（可回滚）

build\app-cdc\flag.bin    # gen\_flag.py 生成的升级标志页
```

也可以把 elf 交给 Keil / VS Code (Cortex-Debug) 使用 —— 只要指定同一个 J-Link。

## 与 Keil 工程的差异（重要）



1. **启动文件**：Keil 用 ARMCC 语法汇编；GCC 用 `startup_stm32f10x_md.s`，

   向量表 59 项与 Keil 版本一一对应，中断服务函数同名弱定义，源码里的

   `HardFault_Handler`、`SysTick_Handler`、`USB_LP_CAN1_RX0_IRQHandler`

   会覆盖弱定义。

2. `__attribute__((at(APP_FW_INFO_ADDR)))`：ARMCC 专有。GCC 没有 at 属性，

   由 `gcc_compat.h`（`-include` 强制包含）把它改写为

   `__attribute__((section(".app_fw_info"), used))`，再由 `stm32f103c8_app.ld`

   把 `.app_fw_info` 固定在 `0x08002000 + 0x100`，与 Keil 镜像同一地址。

   这样上位机按固定偏移读取 `magic/version` 的逻辑完全不受影响。

3. **C 库**：Keil 用 MicroLIB；GCC 用 `--specs=nano.specs --specs=nosys.specs`

* `-nostartfiles`（启动代码自己写），`--gc-sections` 去掉未用代码。

1. **代码体积**：GCC 产物必须 ≤ 22 KB（应用区）、≤ 6 KB（boot 区）。

   链接结束会打印 `Memory region Used Size`，超了会直接报错。

   **MSC 变体必须开 LTO**：`-Oz` 单独用仍有 23 344 B（超 816 B），

   加 `-flto` 后降到 22 188 B（`-flto` 已写进 Makefile 的 CFLAGS/LDFLAGS）。

   实测（arm-none-eabi-gcc 13.2.1）：



| variant      | GCC (-Oz + -flto)        | Keil ARMCC |
| ------------ | ------------------------ | ---------- |
| boot         | 5 196 B / 6 KB           | 5 640 B    |
| app-cdc      | 17 044 B / 22 KB         | 17 200 B   |
| app-msc      | 22 188 B / 22 KB (98.5%) | 22 116 B   |
| app-cdc-256k | 17 044 B                 | —          |
| app-msc-256k | 22 208 B                 | —          |

> `FLASH_256K`
>
>  只出现在 Makefile/README 里，源码中没有任何地方引用它，
> 所以 
>
> `*-256k`
>
>  与非 256k 变体的镜像内容基本一致（这是源码现状，不是漏编译）。



1. **VTOR**：应用在 `main()` 里自己设置 `SCB->VTOR = APP_BASE`，

   GCC 这边 `SystemInit()` 行为与 Keil 一致（`VECT_TAB_OFFSET=0`），无需改动。

2. **LTO 后 Reset\_Handler 位置会变**：`-flto`/`--gc-sections` 会把 `Reset_Handler`

   排到 `.text` 末尾（例如 app-msc 的复位向量是 0x0800630D 而不是紧跟

   `.isr_vector`）。向量表第 2 个字指向它，功能完全正常，只是反汇编时别误判。

## 一键验证编译结果



```
arm-none-eabi-size build\boot\offline\_prog\_boot.elf

arm-none-eabi-size build\app-cdc\offline\_prog\_app\_cdc.elf

arm-none-eabi-objdump -h build\app-cdc\offline\_prog\_app\_cdc.elf
```