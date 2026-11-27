@echo off
chcp 936 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0.."

rem ------------------------------------------------------------------
rem  J-Link 路径: 默认按本机安装位置, 也可用环境变量 JLINK 覆盖
rem ------------------------------------------------------------------
if not defined JLINK set "JLINK=C:\Program Files (x86)\SEGGER\JLinkARM_V474b\JLink.exe"
if not exist "%JLINK%" set "JLINK=C:\Program Files\SEGGER\JLink\JLink.exe"
if not exist "%JLINK%" set "JLINK=C:\Program Files (x86)\SEGGER\JLink\JLink.exe"
if not exist "%JLINK%" (
    echo [ERROR] 找不到 JLink.exe, 请先安装 SEGGER J-Link Software, 或设置环境变量 JLINK
    exit /b 1
)

if not exist "backup" mkdir "backup"

for %%F in (build\boot\offline_prog_boot.bin build\app-cdc\offline_prog_app_cdc.bin) do (
    if not exist "%%F" (
        echo [ERROR] 缺少 %%F, 请先运行 build.bat
        exit /b 1
    )
)

echo [FLASH] 生成升级标志页 ...
python tools\gen_flag.py build\app-cdc\offline_prog_app_cdc.bin build\app-cdc\flag.bin
if errorlevel 1 exit /b 1

echo [FLASH] J-Link 烧录 boot + app + flag ...
set "LOG=%TEMP%\jlink_flash.log"
"%JLINK%" -CommanderScript tools\flash.jlink > "%LOG%" 2>&1

rem J-Link Commander V4.74b 成功时也返回 1, 因此不看 errorlevel, 改为检查日志
findstr /i /c:"Error" "%LOG%" >nul
if not errorlevel 1 (
    echo [ERROR] J-Link 报错:
    type "%LOG%"
    exit /b 1
)

set /a OK=0
for /f %%L in ('findstr /c:"Verify successful" "%LOG%"') do set /a OK+=1
if not "!OK!"=="3" (
    echo [ERROR] 回读校验不完整 ^(Verify successful = !OK!/3^):
    type "%LOG%"
    exit /b 1
)

echo [FLASH] 完成: boot + app-cdc + flag 均已写入并回读校验通过
exit /b 0