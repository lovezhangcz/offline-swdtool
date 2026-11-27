# debug.ps1 — 一键启动 J-Link GDB Server 并用 arm-none-eabi-gdb 调试 app-cdc
#
#   使用方法（在 gcc 目录下）:
#       powershell -ExecutionPolicy Bypass -File tools\debug.ps1
#       powershell -ExecutionPolicy Bypass -File tools\debug.ps1 -Elf build\app-msc\offline_prog_app_msc.elf
#
# 说明:
#   * J-Link 软件 V4.74b 的 GDB Server 不支持 -nogui 参数（会弹警告并卡住），
#     这里只用 -device / -if / -speed / -port。
#   * 目标机 J-Link ARM-OB 默认就是 SWD，无需 SelectInterface。
#   * J-Link 的 GDB Server 不支持 detach，调试结束用 monitor go 让目标继续跑。

param(
    [string]$Elf = "build\app-cdc\offline_prog_app_cdc.elf",
    [string]$ToolchainBin = "D:\keilwork\tools\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin",
    [string]$GdbServer = "C:\Program Files (x86)\SEGGER\JLinkARM_V474b\JLinkGDBServerCL.exe",
    [int]$Port = 2331,
    [string]$Device = "STM32F103C8",
    [int]$Speed = 1000
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not (Test-Path $Elf)) { throw "找不到 ELF: $Elf （先 build.bat app-cdc）" }
if (-not (Test-Path (Join-Path $ToolchainBin "arm-none-eabi-gdb.exe"))) { throw "找不到 arm-none-eabi-gdb.exe: $ToolchainBin" }
if (-not (Test-Path $GdbServer)) { throw "找不到 JLinkGDBServerCL.exe: $GdbServer" }

# 清掉可能残留的 GDB Server
Get-Process JLinkGDBServerCL -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800

Write-Host "[debug] 启动 J-Link GDB Server :$Port ($Device, SWD, ${Speed}kHz)"
Start-Process -FilePath $GdbServer `
    -ArgumentList @("-device", $Device, "-if", "SWD", "-speed", "$Speed", "-port", "$Port") `
    -WorkingDirectory $root -WindowStyle Hidden

# 等待端口就绪
$ok = $false
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 500
    $c = Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue |
         Where-Object { $_.LocalPort -eq $Port }
    if ($c) { $ok = $true; break }
}
if (-not $ok) {
    Get-Process JLinkGDBServerCL -ErrorAction SilentlyContinue | Stop-Process -Force
    throw "GDB Server 未能在端口 $Port 监听"
}
Write-Host "[debug] 端口已就绪，启动 GDB"

$env:Path = "$ToolchainBin;" + $env:Path

# 按 -Elf 参数替换脚本里的 file 行（gdb 里 \ 是转义符，路径统一用 /）
$elfFwd = $Elf.Replace("\", "/")
$lines = Get-Content "tools\debug.gdb" -Encoding UTF8 | ForEach-Object {
    if ($_ -match '^\s*file\s') { "file $elfFwd" } else { $_ }
}
$tmpScript = "tools\.debug_run.gdb"
[IO.File]::WriteAllText((Join-Path $root $tmpScript), ($lines -join "`r`n") + "`r`n", (New-Object Text.UTF8Encoding($false)))

& (Join-Path $ToolchainBin "arm-none-eabi-gdb.exe") -q -x $tmpScript
Remove-Item $tmpScript -Force -ErrorAction SilentlyContinue

Write-Host "[debug] 关闭 GDB Server"
Get-Process JLinkGDBServerCL -ErrorAction SilentlyContinue | Stop-Process -Force

# GDB 的 monitor reset 只复位内核，USB/RCC 等外设状态会残留（调试后 CDC 不再枚举），
# 这里用 J-Link Commander 做一次整机复位（SYSRESETREQ）+ 运行，恢复到正常固件状态
$jlink = "C:\Program Files (x86)\SEGGER\JLinkARM_V474b\JLink.exe"
if (-not (Test-Path $jlink)) { Write-Host "[debug] (skip full reset: JLink.exe not found)"; exit 0 }
$tmpJlink = Join-Path $root "tools\.debug_reset.jlink"
[IO.File]::WriteAllText($tmpJlink, "speed 1000`r`ndevice $Device`r`nr`r`ng`r`nq`r`n", (New-Object Text.ASCIIEncoding))
& $jlink -CommanderScript $tmpJlink | Out-Null
Remove-Item $tmpJlink -Force -ErrorAction SilentlyContinue

Write-Host "[debug] done (full reset + run issued, target back to app)"
