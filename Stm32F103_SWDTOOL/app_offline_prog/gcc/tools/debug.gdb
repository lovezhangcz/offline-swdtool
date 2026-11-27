# GDB 调试脚本 — J-Link GDB Server 已监听 :2331 后使用
#   arm-none-eabi-gdb -x tools/debug.gdb
#
# 流程：加载 app 的调试符号 -> 连 GDB Server -> 复位 -> 在 main 下断点 -> 运行到 main
#       打印现场 -> 继续运行（boot 3s 后会跳进 app）-> 交还目标机

set pagination off
set confirm off
set print pretty on

file build/app-cdc/offline_prog_app_cdc.elf

target extended-remote :2331

# 复位并停在复位向量（boot 从这里开始跑）
monitor reset

break main

# 运行：boot 等待约 3 s 后校验标志页并跳转到 0x08002000 的 app，随后命中 main
continue

info registers pc sp lr
bt
info locals

# 再跑几步，确认能单步调试
next
next
info registers pc

# 让目标继续运行（J-Link 的 GDB Server 不支持 detach，用 monitor go 交还）
monitor go
quit
