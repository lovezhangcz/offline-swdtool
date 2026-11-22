# 开源参考工程导读

2026-11-21 从 GitHub 下载（codeload zip，均含 LICENSE），用于参考改进
`../Stm32F103_SWDTOOL/app_offline_prog` 的 SWD 离线编程器固件。

## 1. DAPProg（XIVN1987/DAPProg，master）★ 与我们最像

- STM32 上 GPIO 位bang SWD 的"乞丐版"离线下载器，MDK 工程，代码极简。
- 关键文件：
  - `src/SWD_host.c` —— SWD 底层：line reset 51 个时钟、JTAG→SWD 切换 0xE79E、
    8 个空闲时钟后读 IDCODE、`swd_init_debug` 重试（失败先写 ABORT）。
  - `src/SWD_flash.c` —— 把 Keil FLM 烧录算法 blob 下载到目标 RAM 并执行
    （Init/EraseSector/ProgramPage），含作者修正的返回值/Init 调用语义。
  - `src/flash_blob.h`、`doc/FlashAlgo/` —— STM32F10x/F4xx 的算法 blob 与
    生成脚本（flash_algo.py + c_blob.tmpl），要新增 GD32 算法时可照此流程。
- 我们固件的连接序列（复位51clk→0xE79E→复位→空闲8clk→读IDCODE）与该工程
  及 ARM 官方 CMSIS-DAP 完全一致，说明序列本身没问题。

## 2. DAPLink（ARMmbed/DAPLink，main）★ ARM 官方实现，权威参考

- 官方调试器固件，拖拽烧录 + 全家族 flash 算法。
- 关键文件：
  - `source/daplink/interface/swd_host.c` —— 完整 SWD host：
    `swd_init_debug()` 的失败重试策略（最多 4 次，重试前先写 DP_ABORT），
    上电握手（写 CTRL/STAT 的 CDBGPWRUPREQ|CSYSPWRUPREQ 后轮询 ACK）。
  - `source/daplink/interface/swd_host_ca.c` —— connect-under-reset /
    halt-on-reset 等连接模式（需要 NRST 的场景）。
  - `source/family/*/flash_blob.c` —— 各家芯片的烧录算法 blob 实例。
- 我们已在固件里实现"先 ABORT 再连接"，与其策略一致。

## 3. OfflineSWD（wuzhl2018/OfflineSWD，main）

- 基于 ST 官方 CMSIS-DAP 修改的 F103RET6 脱机烧写器：虚拟 U 盘放 HEX/BIN、
  可去除读保护、可切 DAP 仿真模式、OLED 界面。
- 体积大（含 66MB 成品/文档目录 `OfflineSWD-V1.0`），源码在
  `STM32-OffineSWD` 子目录。
- 参考价值：上位机交互形式（U 盘拖拽 vs 我们的串口协议）和读保护解除流程。

## 4. blackmagic（blackmagic-debug/blackmagic，main）★ 时序细节参考

- Black Magic Probe 固件，业界公认的纯软件 SWD host。
- 关键文件：
  - `src/platforms/common/swdptap.c` —— 位bang 时序策略（值得对照我们的
    `swd_host.c`）：
    - 每个原语以下降沿结束；下降沿之后才驱动输出；上升沿之前采样输入；
    - turnaround 做了"方向没变就不翻转"的优化。
  - `src/target/adiv5.c` —— ADIv5 DP 连接/扫描、无 NRST 情况下的
    连接处理、错误恢复。

## 对当前问题（GD32F130 一直 NORESP）的启示

1. 四个工程用的连接序列与我们一致 → 协议层大概率没问题，重点怀疑物理层
   （供电、线序、上拉、目标侧复用）。
2. blackmagic 的"方向没变不翻转 turnaround"和采样时机可作我们时序精修的
   直接参照。
3. DAPProg 的 flash_algo.py 流程可用于把 Keil 的 GD32F1x0 FLM 算法转成
   blob，替代我们现在的逐字 FMC 写，能提速并增加校验能力。
4. 若将来要支持更多芯片，可借鉴 DAPLink 的 family/flash_blob 组织方式。

## 说明

- 同级 `*.zip` 是原始下载包，确认解压无误后可自行删除（OfflineSWD.zip
  约 83MB）。
