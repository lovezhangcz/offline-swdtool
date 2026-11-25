@echo off
chcp 65001 >nul
rem ============================================================
rem  clear.bat - 清除 STM32F103_SWDTOOL 工程编译后产生的无用文件
rem
rem  清除内容:
rem    1. app_offline_prog\project\obj* / list*   各 target 的编译中间件
rem    2. testprj\project\obj / list              测试工程编译中间件
rem       (保留 obj 下的 *.bin 测试固件, 需要一起清理时加参数 all)
rem    3. 已废弃 target 遗留的目录 obj_cdc/obj_msc/list_cdc/list_msc
rem    4. 编译日志 build_*.log / *_build_log.txt / JLinkLog.txt
rem    5. host_tool\out 里的 .bin/.hex 产物, host_tool\__pycache__
rem
rem  不删除: 源码、工程文件(.uvproj/.uvopt/.uvgui)、host_tool\logs 运行日志
rem
rem  用法: clear.bat        清理(保留测试固件 bin)
rem        clear.bat all    连测试固件 bin 一起清理
rem ============================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "KEEP_BIN=1"
if /i "%~1"=="all" set "KEEP_BIN=0"

echo [CLEAN] 清理 Keil 编译中间件 ...
for %%D in (
    "app_offline_prog\project\obj"
    "app_offline_prog\project\obj_app_cdc"
    "app_offline_prog\project\obj_app_msc"
    "app_offline_prog\project\obj_boot"
    "app_offline_prog\project\list"
    "app_offline_prog\project\list_app_cdc"
    "app_offline_prog\project\list_app_msc"
    "app_offline_prog\project\list_boot"
    "testprj\project\list"
) do (
    if exist "%%~D" (
        pushd "%%~D"
        del /f /q /a *.* >nul 2>nul
        popd
    )
)

echo [CLEAN] 清理测试工程 obj ...
if exist "testprj\project\obj" (
    if "!KEEP_BIN!"=="1" (
        for %%F in ("testprj\project\obj\*") do (
            if /i not "%%~xF"==".bin" del /f /q /a "%%~F" >nul 2>nul
        )
    ) else (
        pushd "testprj\project\obj"
        del /f /q /a *.* >nul 2>nul
        popd
    )
)

echo [CLEAN] 删除废弃 target 遗留目录 ...
for %%D in (
    "app_offline_prog\project\obj_cdc"
    "app_offline_prog\project\obj_msc"
    "app_offline_prog\project\list_cdc"
    "app_offline_prog\project\list_msc"
) do (
    if exist "%%~D" rd /s /q "%%~D" >nul 2>nul
)

echo [CLEAN] 删除编译日志 ...
del /f /q "app_offline_prog\project\build_*.log"      >nul 2>nul
del /f /q "app_offline_prog\project\*_build_log.txt"  >nul 2>nul
del /f /q "app_offline_prog\project\JLinkLog.txt"     >nul 2>nul
del /f /q "testprj\project\JLinkLog.txt"              >nul 2>nul
del /f /q "host_tool\build_*.log"                     >nul 2>nul

echo [CLEAN] 清理上位机产物 ...
if exist "host_tool\out" (
    pushd "host_tool\out"
    del /f /q /a *.* >nul 2>nul
    popd
)
if exist "host_tool\__pycache__" rd /s /q "host_tool\__pycache__" >nul 2>nul

echo.
echo [CLEAN] 完成.
if /i not "%~1"=="nopause" pause
endlocal
