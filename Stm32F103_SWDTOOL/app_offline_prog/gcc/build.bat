@echo off
chcp 936 >nul
setlocal
cd /d "%~dp0"

rem ====================================================================
rem  build.bat — 用 arm-none-eabi-gcc 编译 STM32F103 离线编程器
rem
rem  工具链默认位置 (可用环境变量覆盖):
rem    TOOLCHAIN_BIN   arm-none-eabi-gcc 的 bin 目录
rem    MAKE_BIN        GNU make 的 bin 目录
rem
rem  用法: build.bat            编译全部 5 个 target
rem        build.bat boot       只编译 boot
rem        build.bat clean      清理
rem        build.bat flash      编译 boot + app-cdc 并用 J-Link 烧录
rem ====================================================================

if not defined TOOLCHAIN_BIN set "TOOLCHAIN_BIN=D:\keilwork\tools\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin"
if not defined MAKE_BIN      set "MAKE_BIN=D:\keilwork\tools\make-4.4.1-win32\bin"

if not exist "%TOOLCHAIN_BIN%\arm-none-eabi-gcc.exe" (
    echo [ERROR] 找不到 arm-none-eabi-gcc: %TOOLCHAIN_BIN%
    echo         请设置环境变量 TOOLCHAIN_BIN 指向工具链 bin 目录
    exit /b 1
)
if not exist "%MAKE_BIN%\make.exe" (
    echo [ERROR] 找不到 make.exe: %MAKE_BIN%
    echo         请设置环境变量 MAKE_BIN 指向 GNU make 的 bin 目录
    exit /b 1
)

set "PATH=%TOOLCHAIN_BIN%;%MAKE_BIN%;%PATH%"

if "%~1"=="" (
    make all
) else (
    make %*
)
endlocal
