#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""实时监控烧录器状态/日志, 用于诊断热插拔自动烧录问题。

复用 swdtool.Prog 的协议实现 (CRC16 / 帧解析), 不自造帧。
每 ~120ms 轮询 GET_STATUS + LOG_POLL:
  - 状态仅在变化时打印 (含 burn_seq / swd_err / 进度 / 目标 ID)
  - 日志文本逐行打印 (LOG_POLL 0x24, 设备环形缓冲 768B)
  - 烧录中设备主循环被阻塞 (连接/擦除阶段), 命令超时 -> "BURN(无响应)"
  - 串口异常 / 设备重枚举 -> 自动重连

用法:
  python monitor_hotplug.py            默认监控 600s, 日志写到 logs\hotplug_<ts>.log
  python monitor_hotplug.py 300        监控 300s
  python monitor_hotplug.py -p COM5    指定串口 (默认自动扫描 0483:5740)
"""
import argparse
import os
import struct
import sys
import time

import swdtool
from swdtool import Prog, find_device, ERR_NAMES, SWD_ERR_NAMES

CMD_GET_STATUS = swdtool.CMD_GET_STATUS      # 0x21
CMD_LOG_POLL   = 0x24
VID, PID = 0x0483, 0x5740

# 状态 5 是 v1.1.15 起上位机可见的"已识别目标, 稳定电源中"
STATE_NAMES = dict(swdtool.STATE_NAMES)
STATE_NAMES[5] = "识别目标(稳定中,勿拔)"
RESULT_NAMES = {0: "-", 1: "成功", 2: "失败"}


def ts_ms(t0):
    return int((time.time() - t0) * 1000)


def parse_status(pl):
    """GET_STATUS 应答负载 (27B): state,result,err[4],dpidr[4],idcode[4],
    progress[4],swd_err[4],burn_seq[4],host_online[1]"""
    if len(pl) < 22:
        return None
    state = pl[0]
    result = pl[1]
    err, dpidr, tid, prog, swd_err = struct.unpack("<IIIII", pl[2:22])
    burn_seq = struct.unpack("<I", pl[22:26])[0] if len(pl) >= 26 else 0
    host_online = pl[26] if len(pl) >= 27 else 0
    return dict(state=state, result=result, err=err, dpidr=dpidr,
                tid=tid, progress=prog, swd_err=swd_err,
                burn_seq=burn_seq, host_online=host_online)


def open_prog(port=None):
    if not port:
        port = find_device(verbose=False)
    if not port:
        return None, None
    pr = Prog(port)
    return pr, port


def main():
    ap = argparse.ArgumentParser(description="SWD 离线编程器热插拔监控")
    ap.add_argument("duration", nargs="?", type=int, default=600,
                    help="监控时长(秒), 默认 600")
    ap.add_argument("-p", "--port", help="串口号, 默认自动扫描")
    ap.add_argument("-o", "--out", help="日志文件路径, 默认 logs\\hotplug_<ts>.log")
    args = ap.parse_args()

    # 日志文件 (同时输出到屏幕与文件)
    logdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
    os.makedirs(logdir, exist_ok=True)
    logpath = args.out or os.path.join(
        logdir, time.strftime("hotplug_%Y%m%d_%H%M%S.log"))
    logf = open(logpath, "w", encoding="utf-8")

    def emit(line):
        print(line, flush=True)
        logf.write(line + "\n")
        logf.flush()

    t0 = time.time()
    emit("=" * 78)
    emit(f"SWD 热插拔监控启动  {time.strftime('%Y-%m-%d %H:%M:%S')}")
    emit(f"日志文件: {logpath}")
    emit("=" * 78)

    pr, port = open_prog(args.port)
    if not pr:
        emit("未找到设备 (0483:5740), 请检查 USB 连接; 3s 后重试...")
        time.sleep(3)
        pr, port = open_prog(args.port)
        if not pr:
            emit("仍未找到设备, 退出"); return

    # 启动上下文: 版本 / 镜像
    try:
        ver = pr.version()
        info = pr.info()
        emit(f"端口 {port}  固件 {ver}")
        if info["valid"]:
            emit(f"镜像: {info['length']}B @0x{info['load']:08X} "
                 f"CRC=0x{info['crc32']:08X}  容量 {info['capacity']}B  "
                 f"已烧 {info.get('burn_count', 0)} 次")
        else:
            emit(f"镜像: 无 (容量 {info['capacity']}B)")
    except Exception as e:
        emit(f"读取设备信息失败: {e}")
    emit("-" * 78)
    emit("状态含义: 0无镜像 1待命探测 5识别目标(稳定中) 2烧录中 3成功 4失败")
    emit("-" * 78)

    last_key = None
    last_prog_print = -1
    last_timeout_print = 0.0
    log_buf = ""        # 跨轮次累积的日志碎片

    def dump_logs():
        nonlocal log_buf
        try:
            st, pl = pr.cmd(CMD_LOG_POLL, timeout=0.2)
        except Exception:
            return
        if not pl:
            return
        log_buf += pl.decode("latin-1")
        # 按行切, 最后一段不完整则留到下轮
        while "\n" in log_buf:
            line, log_buf = log_buf.split("\n", 1)
            line = line.rstrip("\r")
            if line:
                emit(f"[{ts_ms(t0):7d}ms] LOG  {line}")

    while time.time() - t0 < args.duration:
        try:
            st, pl = pr.cmd(CMD_GET_STATUS, timeout=0.4)
            s = parse_status(pl)
            if s is None:
                time.sleep(0.12); continue

            # 进度变化的节流: 每 4KB 打一次, 但状态/错误/序号变化必打
            prog_bucket = s["progress"] // 4096
            key = (s["state"], s["result"], s["err"], s["swd_err"],
                   s["dpidr"], s["tid"], s["burn_seq"], prog_bucket,
                   s["host_online"])
            if key != last_key:
                last_key = key
                code = s["err"] & 0xFF
                addr = s["err"] & 0xFFFFFF00
                name = STATE_NAMES.get(s["state"], str(s["state"]))
                line = (f"[{ts_ms(t0):7d}ms] "
                        f"state={s['state']}({name}) "
                        f"result={s['result']}({RESULT_NAMES.get(s['result'], '?')}) "
                        f"err={code}({ERR_NAMES.get(code, '未知')})" +
                        (f"@0x{addr:08X}" if addr else "") +
                        f" swd={s['swd_err']}({SWD_ERR_NAMES.get(s['swd_err'], '?')[:14]}) "
                        f"prog={s['progress']} "
                        f"DPIDR=0x{s['dpidr']:08X} ID=0x{s['tid']:08X} "
                        f"seq={s['burn_seq']} host={s['host_online']}")
                emit(line)

            dump_logs()

        except RuntimeError:
            # 应答超时: 设备主循环被烧录阻塞 (连接/擦除阶段)
            now = time.time()
            if now - last_timeout_print > 0.6:
                last_timeout_print = now
                emit(f"[{ts_ms(t0):7d}ms] BURN(无响应) — 设备烧录中, 命令超时")
            # 超时也尝试拉日志 (烧录页循环间隙 USBD_POLL 可能应答)
            try: dump_logs()
            except Exception: pass
            time.sleep(0.15)

        except (OSError, Exception) as e:
            # 串口断开 / 设备重枚举 -> 重连
            msg = str(e)
            emit(f"[{ts_ms(t0):7d}ms] 连接异常: {msg}, 重连...")
            try: pr.close()
            except Exception: pass
            time.sleep(0.8)
            pr, port = open_prog(args.port)
            if not pr:
                emit("  设备丢失, 5s 后重试...")
                time.sleep(5)
                pr, port = open_prog(args.port)
                if not pr:
                    emit("  仍未找到, 退出"); break
                emit(f"  重连成功: {port}")
            else:
                emit(f"  重连成功: {port}")
            last_key = None  # 重连后强制重打一帧状态
            time.sleep(0.2)

        else:
            time.sleep(0.12)

    try: pr.close()
    except Exception: pass
    emit("-" * 78)
    emit(f"监控结束 {time.strftime('%Y-%m-%d %H:%M:%S')}  日志: {logpath}")
    logf.close()


if __name__ == "__main__":
    main()
