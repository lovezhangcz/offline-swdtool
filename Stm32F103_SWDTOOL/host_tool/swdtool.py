#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SWD 离线编程器 上位机工具
=========================
通过 USB CDC 虚拟串口把目标固件(.bin)下载到编程器内部 flash,
之后编程器检测到目标芯片(SWD)接入即自动烧录。

依赖: pip install pyserial

用法:
  python swdtool.py ping  -p COM5                 测试连接
  python swdtool.py info  -p COM5                 查看编程器状态/镜像信息
  python swdtool.py flash -p COM5 firmware.bin    下载固件到编程器
  python swdtool.py prog  -p COM5                 手动触发一次烧录
  python swdtool.py status -p COM5                查询烧录状态

不带 -p 时自动扫描所有串口寻找应答为 SWDPROG1 的设备。

协议帧: [0xAA 0x55][CMD][LEN_L LEN_H][PAYLOAD][CRC16_L CRC16_H]
"""

import argparse
import os
import struct
import sys
import time
import zlib

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("缺少 pyserial, 请先执行: pip install pyserial")

HDR = b"\xAA\x55"
CMD_PING       = 0x01
CMD_GET_INFO   = 0x02
CMD_IMG_BEGIN  = 0x10
CMD_IMG_DATA   = 0x11
CMD_IMG_END    = 0x12
CMD_PROG_NOW   = 0x20
CMD_GET_STATUS = 0x21
CMD_TGT_PROBE  = 0x22

CHUNK = 508          # 每帧最大数据负载
RSP_TIMEOUT = 5.0    # 单帧应答超时(秒); 擦除/写 flash 较慢

STATE_NAMES = {
    0: "无镜像",
    1: "待命(等待目标接入)",
    2: "烧录中",
    3: "成功(等待目标拔出)",
    4: "失败(等待目标拔出)",
}
RESULT_NAMES = {0: "-", 1: "成功", 2: "失败"}
ERR_NAMES = {
    0: "无", 1: "SWD连接失败", 2: "目标型号与配置不符", 3: "停机失败",
    4: "FPEC解锁失败", 5: "目标有读写保护", 6: "擦除失败",
    7: "写入失败", 8: "校验失败", 9: "无有效镜像",
}
SWD_ERR_NAMES = {
    0: "OK",
    1: "WAIT(目标忙, 可能已在深度睡眠/SWD被占用)",
    2: "FAULT(目标总线错误)",
    3: "PARITY(信号质量差, 线太长/干扰)",
    4: "无应答(检查接线SWCLK/SWDIO是否接反、目标是否供电、是否共地)",
    5: "DPIDR不匹配(非Cortex-M目标或信号异常)",
    6: "调试电源域未就绪",
}


def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def build_frame(cmd: int, payload: bytes = b"") -> bytes:
    body = bytes([cmd]) + struct.pack("<H", len(payload)) + payload
    return HDR + body + struct.pack("<H", crc16(body))


def read_response(ser, cmd: int, timeout: float = RSP_TIMEOUT):
    """读取一帧应答, 返回 (status, payload)。超时返回 None。"""
    buf = bytearray()
    deadline = time.time() + timeout
    while time.time() < deadline:
        b = ser.read(1)
        if b:
            buf += b
            # 找帧头
            while len(buf) >= 2 and bytes(buf[:2]) != HDR:
                del buf[0]
            if len(buf) >= 5:
                ln = buf[3] | (buf[4] << 8)
                total = 5 + ln + 2
                if len(buf) >= total:
                    frame = bytes(buf[:total])
                    if crc16(frame[2:5 + ln]) != struct.unpack("<H", frame[5 + ln:total])[0]:
                        del buf[0]
                        continue
                    rcmd = frame[2]
                    if rcmd != (cmd | 0x80):
                        del buf[:total]
                        continue
                    payload = frame[5:5 + ln]
                    return payload[0], payload[1:]
        # else: 串口读超时(0.1s)后继续
    return None


class Prog:
    def __init__(self, port: str):
        self.ser = serial.Serial(port, 115200, timeout=0.1,
                                 dsrdtr=False, rtscts=False, xonxoff=False)
        self.ser.setDTR(True)
        self.ser.reset_input_buffer()

    def close(self):
        self.ser.close()

    def cmd(self, cmd: int, payload: bytes = b"", timeout: float = RSP_TIMEOUT):
        self.ser.write(build_frame(cmd, payload))
        r = read_response(self.ser, cmd, timeout)
        if r is None:
            raise RuntimeError(f"命令 0x{cmd:02X} 应答超时")
        return r

    def ping(self, timeout: float = RSP_TIMEOUT) -> bool:
        st, pl = self.cmd(CMD_PING, timeout=timeout)
        return st == 0 and pl == b"SWDPROG1"

    def info(self):
        st, pl = self.cmd(CMD_GET_INFO)
        assert st == 0 and len(pl) >= 19, f"GET_INFO 失败 status={st}"
        valid = pl[0]
        length, crc32, load, cap = struct.unpack("<IIII", pl[1:17])
        return dict(valid=valid, length=length, crc32=crc32,
                    load=load, capacity=cap, state=pl[17], result=pl[18])

    def status(self):
        st, pl = self.cmd(CMD_GET_STATUS)
        assert st == 0 and len(pl) >= 18, f"GET_STATUS 失败 status={st}"
        state, result = pl[0], pl[1]
        err, dpidr, tid, prog = struct.unpack("<IIII", pl[2:18])
        swd_err = struct.unpack("<I", pl[18:22])[0] if len(pl) >= 22 else 0
        return dict(state=state, result=result, err=err,
                    dpidr=dpidr, tid=tid, progress=prog, swd_err=swd_err)

    def probe(self):
        st, pl = self.cmd(CMD_TGT_PROBE)
        assert st == 0 and len(pl) >= 9, f"PROBE 失败 status={st}"
        dpidr, idcode = struct.unpack("<II", pl[1:9])
        return dict(swd_err=pl[0], dpidr=dpidr, idcode=idcode)

    def flash(self, data: bytes, load_addr: int = 0x08000000):
        crc = zlib.crc32(data) & 0xFFFFFFFF
        st, _ = self.cmd(CMD_IMG_BEGIN, struct.pack("<III", len(data), crc, load_addr))
        if st == 1:
            raise RuntimeError("参数被拒(镜像过大? 设备容量不足)")
        if st != 0:
            raise RuntimeError(f"IMG_BEGIN 失败 status={st} (擦除错误)")
        t0 = time.time()
        off = 0
        while off < len(data):
            chunk = data[off:off + CHUNK]
            st, pl = self.cmd(CMD_IMG_DATA, struct.pack("<I", off) + chunk)
            if st != 0:
                raise RuntimeError(f"IMG_DATA @{off} 失败 status={st}")
            off += len(chunk)
            done = struct.unpack("<I", pl)[0] if len(pl) >= 4 else off
            pct = done * 100 // len(data)
            print(f"\r下载中 {done}/{len(data)} 字节 ({pct}%)", end="", flush=True)
        print()
        st, _ = self.cmd(CMD_IMG_END)
        if st == 3:
            raise RuntimeError("设备侧 CRC 校验失败")
        if st != 0:
            raise RuntimeError(f"IMG_END 失败 status={st}")
        dt = time.time() - t0
        print(f"固件已写入编程器: {len(data)} 字节, CRC32=0x{crc:08X}, 用时 {dt:.1f}s")


def find_device(verbose: bool = True):
    # USB 串口优先 (我们的设备是 USB CDC), 其余后排
    ports = sorted(list_ports.comports(),
                   key=lambda p: 0 if "USB" in (p.description or "").upper() else 1)
    for p in ports:
        if verbose:
            print(f"正在尝试 {p.device} ({p.description}) ...", flush=True)
        try:
            pr = Prog(p.device)
            try:
                if pr.ping(timeout=1.5):
                    if verbose:
                        print(f"  -> 找到编程器: {p.device}")
                    return p.device
            finally:
                pr.close()
        except Exception:
            pass
    return None


def print_status(port):
    pr = Prog(port)
    try:
        s = pr.status()
        i = pr.info()
    finally:
        pr.close()
    print(f"状态:       {STATE_NAMES.get(s['state'], s['state'])}")
    print(f"上次结果:   {RESULT_NAMES.get(s['result'], s['result'])}")
    print(f"错误码:     {s['err'] & 0xFF} ({ERR_NAMES.get(s['err'] & 0xFF, '未知')})")
    if s['err'] & 0xFFFFFF00:
        print(f"出错地址:   0x{s['err'] & 0xFFFFFF00:08X}")
    print(f"烧录进度:   {s['progress']} 字节")
    print(f"SWD子错误:  {s['swd_err']} ({SWD_ERR_NAMES.get(s['swd_err'], '未知')})")
    print(f"目标 DPIDR: 0x{s['dpidr']:08X}  DBGMCU_ID: 0x{s['tid']:08X}")
    if i["valid"]:
        print(f"镜像:       {i['length']} 字节 @ 0x{i['load']:08X}, CRC32=0x{i['crc32']:08X}")
    else:
        print(f"镜像:       无 (容量 {i['capacity']} 字节)")


def interactive():
    """双击运行时的交互菜单"""
    print("=" * 46)
    print("  SWD 离线编程器 上位机")
    print("=" * 46)
    port = find_device()
    if not port:
        input("未找到编程器, 请检查 USB 连接后回车退出...")
        return
    print(f"已连接: {port}\n")
    while True:
        print("  [1] 下载固件到编程器 (flash .bin)")
        print("  [2] 查看编程器状态 / 镜像信息")
        print("  [3] 手动触发一次烧录")
        print("  [4] SWD 目标探测 (诊断接线/目标状态)")
        print("  [5] 退出")
        choice = input("请选择: ").strip()
        try:
            if choice == "1":
                path = input("拖入或输入 .bin 文件路径: ").strip().strip('"')
                if not os.path.isfile(path):
                    print("文件不存在!\n")
                    continue
                with open(path, "rb") as f:
                    data = f.read()
                pr = Prog(port)
                try:
                    pr.flash(data)
                finally:
                    pr.close()
                print()
            elif choice == "2":
                print()
                print_status(port)
                print()
            elif choice == "3":
                pr = Prog(port)
                try:
                    pr.cmd(CMD_PROG_NOW)
                    print("已触发, 观察 LED: 快闪=烧录中, 常亮=成功, 三短闪=失败\n")
                finally:
                    pr.close()
            elif choice == "4":
                pr = Prog(port)
                try:
                    r = pr.probe()
                    print(f"SWD 探测: {r['swd_err']} ({SWD_ERR_NAMES.get(r['swd_err'], '未知')})")
                    print(f"DPIDR:     0x{r['dpidr']:08X}  (Cortex-M3 应为 0x1BA01477)")
                    print(f"DBGMCU_ID: 0x{r['idcode']:08X}  (GD32F1x0 应为 0x13030410)\n")
                finally:
                    pr.close()
            elif choice == "5":
                break
        except Exception as e:
            print(f"出错: {e}\n")


def main():
    if len(sys.argv) == 1:
        interactive()
        return

    ap = argparse.ArgumentParser(description="SWD 离线编程器上位机")
    ap.add_argument("action", choices=["ping", "info", "flash", "prog", "status", "probe"])
    ap.add_argument("file", nargs="?", help="flash 时的 .bin 固件")
    ap.add_argument("-p", "--port", help="串口号, 如 COM5 / /dev/ttyUSB0")
    ap.add_argument("-a", "--addr", default="0x08000000",
                    help="目标烧录起始地址 (默认 0x08000000)")
    ap.add_argument("-n", "--count", type=int, default=1,
                    help="probe 连续探测次数 (配合目标板重新上电抓连接窗口)")
    args = ap.parse_args()

    port = args.port or find_device()
    if not port:
        sys.exit("未找到编程器 (请检查 USB 连接)")
    print(f"使用串口: {port}")

    if args.action == "ping":
        pr = Prog(port)
        ok = False
        try:
            ok = pr.ping()
        finally:
            pr.close()
        print("连接正常" if ok else "设备无正确应答")
        sys.exit(0 if ok else 1)

    if args.action in ("info", "status"):
        print_status(port)
        return

    if args.action == "probe":
        pr = Prog(port)
        try:
            for i in range(args.count):
                r = pr.probe()
                line = (f"SWD={r['swd_err']}({SWD_ERR_NAMES.get(r['swd_err'], '?')[:12]}) "
                        f"DPIDR=0x{r['dpidr']:08X} ID=0x{r['idcode']:08X}")
                if args.count > 1:
                    print(f"[{i+1:3d}] {line}", flush=True)
                    if i + 1 < args.count:
                        time.sleep(0.3)
                else:
                    print(f"SWD 探测: {r['swd_err']} ({SWD_ERR_NAMES.get(r['swd_err'], '未知')})")
                    print(f"DPIDR:    0x{r['dpidr']:08X}  (Cortex-M3 应为 0x1BA01477)")
                    print(f"DBGMCU_ID:0x{r['idcode']:08X}  (GD32F1x0 应为 0x13030410)")
        finally:
            pr.close()
        return

    if args.action == "prog":
        pr = Prog(port)
        try:
            pr.cmd(CMD_PROG_NOW)
            print("已触发烧录, 观察编程器 LED: 快闪=烧录中, 常亮=成功, 三短闪=失败")
        finally:
            pr.close()
        return

    if args.action == "flash":
        if not args.file:
            sys.exit("flash 需要指定 .bin 文件")
        with open(args.file, "rb") as f:
            data = f.read()
        if not data:
            sys.exit("固件文件为空")
        load = int(args.addr, 0)
        pr = Prog(port)
        try:
            pr.flash(data, load)
        finally:
            pr.close()
        return


if __name__ == "__main__":
    main()
