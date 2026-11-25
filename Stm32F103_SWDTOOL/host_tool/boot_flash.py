#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USB 调试闭环工具: 通过 boot 升级应用 + 抓诊断日志

用法 (在 host_tool 目录下):
    python boot_flash.py info                  # 查看当前是 boot 还是 app, 版本/镜像状态
    python boot_flash.py build  [target]       # 编译 APP 并生成 .bin (默认 app_cdc)
    python boot_flash.py flash  <bin>          # 升级指定 .bin (走 boot 的 CDC 协议)
    python boot_flash.py msc    <bin>          # 升级指定 .bin (写 U 盘 + 回读 CRC 自证)
    python boot_flash.py upgrade [target]      # 一键: 编译 -> 升级 -> 校验版本 -> 打印日志
    python boot_flash.py boot                  # 让应用重启进入 boot
    python boot_flash.py run                   # 让 boot 启动应用
    python boot_flash.py log                   # 拉取应用诊断日志
    python boot_flash.py probe [秒数]         # SWD 握手: 读目标 DPIDR/IDCODE; 给秒数则连续重试
    python boot_flash.py tgt   <bin>          # 上传镜像并用 SWD 烧进目标芯片, 完成后复位运行
    python boot_flash.py clk [0]              # 示波器测试: SWCLK/SWDIO 持续方波 (0=停止)
    python boot_flash.py ports                 # 列出可用串口

target:
    app_cdc : 单功能虚拟串口 (0483:5740)
    app_msc : 复合设备, 同时出 U 盘 + 调试串口 (0483:5742), 两路都能用

协议:
    boot  : ASCII 行命令 INFO / ERASE len / W addr len crc16 + 裸数据 / F len crc32 / RUN
    app   : 二进制帧 AA 55 CMD LEN_L LEN_H PAYLOAD CRC16
"""
import os
import struct
import subprocess
import sys
import time
import zlib

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("缺少 pyserial, 请先执行: pip install pyserial")

# ------------------------------------------------------------------ 常量 ----
VID = 0x0483
PID_CDC = 0x5740        # 单功能 CDC (boot 也是这个)
PID_MSC = 0x5741        # 单功能 MSC (无串口, 不会出现在 COM 列表)
PID_COMP = 0x5742       # 复合设备 MSC+CDC (APP_MSC 调试版)
SERIAL_PIDS = (PID_CDC, PID_COMP)
APP_BASE = 0x08004000
CHUNK = 512
BOOT_WAIT = 12.0
MSC_MAX_PAYLOAD = 16 * 1024 - 1024   # 镜像区 16K, 第 0 页留头/目录, 剩 30 簇 = 15360

HDR = b"\xAA\x55"
CMD_PING = 0x01
CMD_GET_INFO = 0x02
CMD_GET_VERSION = 0x03
CMD_IMG_BEGIN = 0x10
CMD_IMG_DATA = 0x11
CMD_IMG_END = 0x12
CMD_PROG_NOW = 0x20
CMD_GET_STATUS = 0x21
CMD_TGT_PROBE = 0x22
CMD_REBOOT_BOOT = 0x23
CMD_LOG_POLL = 0x24
CMD_CLK_TEST = 0x25

TGT_LOAD_ADDR = 0x08000000       # 目标芯片烧录起始地址
TGT_CHUNK = 508                  # IMG_DATA 单包最大数据量

# 目标侧状态机 (与 main.c 的 ST_xxx 一致)
ST_NO_IMAGE, ST_ARMED, ST_PROGRAMMING, ST_DONE_WAIT, ST_FAIL_WAIT = 0, 1, 2, 3, 4
ST_NAMES = {ST_NO_IMAGE: "无镜像", ST_ARMED: "待命", ST_PROGRAMMING: "烧录中",
            ST_DONE_WAIT: "成功(等拔出)", ST_FAIL_WAIT: "失败(等拔出)"}

# TargetProg 错误码 (target_prog.h)
TP_NAMES = {0: "OK", 1: "SWD连接失败", 2: "目标ID不符", 3: "停机失败",
            4: "FPEC解锁失败", 5: "读/写保护", 6: "擦除失败", 7: "写入失败",
            8: "CRC校验不一致", 9: "无镜像", 10: "烧录算法失败"}

ROOT = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.join(ROOT, "..", "app_offline_prog", "project")
UV4 = r"C:\Keil_v5\UV4\UV4.exe"
FROMELF = r"C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe"

TARGETS = {
    "app_cdc": ("offline_prog_APP_CDC", "obj_app_cdc", "offline_prog_app_cdc"),
    "app_msc": ("offline_prog_APP_MSC", "obj_app_msc", "offline_prog_app_msc"),
    "boot": ("offline_prog_BOOT", "obj_boot", "offline_prog_boot"),
}


# ------------------------------------------------------------------ 基础 ----
def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def build_frame(cmd: int, payload: bytes = b"") -> bytes:
    body = bytes([cmd]) + struct.pack("<H", len(payload)) + payload
    return HDR + body + struct.pack("<H", crc16(body))


def find_port(verbose=True):
    cands = [p for p in list_ports.comports()
             if p.vid == VID and p.pid in SERIAL_PIDS]
    if not cands:
        if verbose:
            print("未找到 0483:5740(CDC) / 0483:5742(复合) 串口, 可用串口:")
            for p in list_ports.comports():
                print("   ", p.device, p.description, hex(p.vid or 0), hex(p.pid or 0))
        return None
    return cands[0].device


def open_port(dev, timeout=0.05):
    ser = serial.Serial(dev, 115200, timeout=timeout,
                        dsrdtr=False, rtscts=False, xonxoff=False)
    ser.setDTR(True)
    ser.reset_input_buffer()
    return ser


def reopen(dev, seconds=15.0):
    end = time.time() + seconds
    last = None
    while time.time() < end:
        try:
            return open_port(dev)
        except Exception as e:          # 设备重新枚举期间会短暂打不开
            last = e
            time.sleep(0.25)
    raise RuntimeError("串口 %s 无法打开: %s" % (dev, last))


# ------------------------------------------------------------------ 应用侧 --
class AppLink:
    def __init__(self, dev):
        self.dev = dev
        self.ser = open_port(dev)

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass

    def _read_frame(self, cmd, timeout):
        buf = bytearray()
        end = time.time() + timeout
        while time.time() < end:
            b = self.ser.read(1)
            if not b:
                continue
            buf += b
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
                    if frame[2] != (cmd | 0x80):
                        del buf[:total]
                        continue
                    pl = frame[5:5 + ln]
                    return pl[0], pl[1:]
        return None

    def cmd(self, cmd, payload=b"", timeout=2.0):
        self.ser.reset_input_buffer()
        self.ser.write(build_frame(cmd, payload))
        r = self._read_frame(cmd, timeout)
        if r is None:
            raise RuntimeError("命令 0x%02X 无应答" % cmd)
        return r

    def ping(self, timeout=1.0):
        try:
            st, pl = self.cmd(CMD_PING, timeout=timeout)
            return st == 0 and pl == b"SWDPROG1"
        except Exception:
            return False

    def version(self):
        st, pl = self.cmd(CMD_GET_VERSION)
        if st != 0:
            raise RuntimeError("GET_VERSION 失败")
        return pl.decode("ascii", "replace")

    def logs(self):
        out = []
        for _ in range(64):
            st, pl = self.cmd(CMD_LOG_POLL)
            if st != 0 or not pl:
                break
            out.append(pl.decode("ascii", "replace"))
        return "".join(out)

    def info(self):
        """返回镜像/状态: valid, length, crc32, load, capacity, state, result"""
        st, pl = self.cmd(CMD_GET_INFO)
        if st != 0 or len(pl) < 19:
            raise RuntimeError("GET_INFO 失败 status=%d len=%d" % (st, len(pl)))
        length, crc, load, cap = struct.unpack("<IIII", pl[1:17])
        return dict(valid=pl[0], length=length, crc32=crc, load=load,
                    capacity=cap, state=pl[17], result=pl[18])

    def reboot_to_boot(self):
        try:
            self.cmd(CMD_REBOOT_BOOT, timeout=1.0)
        except Exception:
            pass

    def probe(self, timeout=8.0):
        """SWD 握手: status=0 成功, dpidr 期望 0x1BA01477 (F1) / 0x0BB11477"""
        st, pl = self.cmd(CMD_TGT_PROBE, timeout=timeout)
        if st != 0 or len(pl) < 9:
            raise RuntimeError("TGT_PROBE 失败 status=%d len=%d" % (st, len(pl)))
        dpidr, idcode = struct.unpack("<II", pl[1:9])
        return dict(status=pl[0], dpidr=dpidr, idcode=idcode)

    # ---------------- 目标芯片烧录 (镜像上传 + 触发) ----------------
    def img_begin(self, length, crc, load=TGT_LOAD_ADDR):
        st, _ = self.cmd(CMD_IMG_BEGIN, struct.pack("<III", length, crc, load),
                         timeout=6.0)
        if st != 0:
            raise RuntimeError("IMG_BEGIN 失败 status=%d" % st)

    def img_data(self, off, chunk):
        """返回设备侧已接收总字节数 (应与 off+len(chunk) 相等)"""
        st, pl = self.cmd(CMD_IMG_DATA, struct.pack("<I", off) + chunk,
                          timeout=6.0)
        if st != 0 or len(pl) < 4:
            raise RuntimeError("IMG_DATA off=%d 失败 status=%d" % (off, st))
        return struct.unpack("<I", pl[:4])[0]

    def img_end(self):
        st, _ = self.cmd(CMD_IMG_END, timeout=8.0)
        if st != 0:
            raise RuntimeError("IMG_END 失败 status=%d (3=CRC不符/状态错)" % st)

    def prog_now(self):
        st, _ = self.cmd(CMD_PROG_NOW, timeout=3.0)
        if st != 0:
            raise RuntimeError("PROG_NOW 失败 status=%d" % st)

    def status(self, timeout=3.0):
        """state, result, err, dpidr, idcode, progress, swd_err"""
        st, pl = self.cmd(CMD_GET_STATUS, timeout=timeout)
        if st != 0 or len(pl) < 22:
            raise RuntimeError("GET_STATUS 失败 status=%d len=%d" % (st, len(pl)))
        err, dpidr, idcode, prog, swd_err = struct.unpack("<IIIII", pl[2:22])
        return dict(state=pl[0], result=pl[1], err=err, dpidr=dpidr,
                    idcode=idcode, progress=prog, swd_err=swd_err)


# ------------------------------------------------------------------ boot 侧 --
class BootLink:
    def __init__(self, dev, ser):
        self.dev = dev
        self.ser = ser

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass

    def read_line(self, timeout=2.0):
        buf = bytearray()
        end = time.time() + timeout
        while time.time() < end:
            b = self.ser.read(1)
            if not b:
                continue
            if b == b"\n":
                return buf.decode("ascii", "replace").strip()
            if b != b"\r":
                buf += b
        return None

    def drain(self):
        time.sleep(0.05)
        try:
            self.ser.reset_input_buffer()
        except Exception:
            pass

    def cmd(self, line, timeout=5.0):
        self.ser.write((line + "\n").encode())
        return self.read_line(timeout)

    def info(self):
        self.drain()
        self.ser.write(b"\n")          # 清掉可能残留的半行数据
        time.sleep(0.02)
        self.ser.reset_input_buffer()
        r = self.cmd("INFO", timeout=2.0)
        if r is None or not r.startswith("INFO"):
            raise RuntimeError("boot INFO 无应答: %r" % r)
        return r

    def write_chunk(self, addr, data, timeout=3.0):
        # CRC 必须带 0x 前缀: boot 的 parse_num 只有看到 0x 才按十六进制解析
        self.ser.write(("W %d %d 0x%04X\n" % (addr, len(data), crc16(data))).encode())
        self.ser.write(data)
        r = self.read_line(timeout)
        if r != "OK":
            raise RuntimeError("写入 0x%08X 失败: %r" % (addr, r))


def try_app(dev, timeout=1.0):
    """设备当前如果是应用, 返回 AppLink, 否则 None"""
    try:
        a = AppLink(dev)
    except Exception:
        return None
    if a.ping(timeout):
        return a
    a.close()
    return None


def wait_boot(dev, seconds=BOOT_WAIT):
    """等 boot 的串口可用。注意 app_msc(0483:5742) 进 boot(0483:5740) 会换一个
    COM 口号, 所以每轮都重新扫描端口, 不能死守进入前拿到的 dev。"""
    end = time.time() + seconds
    last = None
    while time.time() < end:
        cur = find_port(verbose=False) or dev
        try:
            ser = open_port(cur)
            bl = BootLink(cur, ser)
            info = bl.info()
            return bl, info
        except Exception as e:
            last = e
            time.sleep(0.3)
    raise RuntimeError("等待 boot 超时: %s" % last)


def enter_boot(dev, force=False):
    """必要时让应用重启进 boot, 返回 (BootLink, info 行)"""
    app = try_app(dev)
    if app is not None:
        if not force:
            print("[app] 在线, 请求重启进入 boot ...")
        app.reboot_to_boot()
        app.close()
        time.sleep(0.4)
    else:
        print("[boot] 设备已在 boot 模式")
    return wait_boot(dev)


# ---------------------------------------------------------------- U 盘 (MSC) --
def find_msc_drive():
    """按盘内 README.TXT 指纹找本设备的虚拟 U 盘 (避免误选其它盘)"""
    for d in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
        root = "%s:\\" % d
        try:
            if os.path.isfile(os.path.join(root, "README.TXT")):
                return root
        except OSError:
            continue
    return None


def msc_write(drv, bin_path):
    """把固件写到 U 盘根目录的 FIRMWARE.BIN, 并强制刷盘"""
    data = open(bin_path, "rb").read()
    dst = os.path.join(drv, "FIRMWARE.BIN")
    f = open(dst, "wb")
    try:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    finally:
        f.close()
    # 再刷一次: Windows 可能把目录/FAT 更新留在卷缓存里
    try:
        f = open(dst, "r+b")
        try:
            f.flush()
            os.fsync(f.fileno())
        finally:
            f.close()
    except OSError:
        pass
    return data


def do_msc(bin_path):
    data = open(bin_path, "rb").read()
    # U 盘的可用空间 = IMG_STORE_SIZE(16K) - FAT/目录占用的扇区, 超出会被 Windows 报"磁盘已满"
    if len(data) == 0 or len(data) > MSC_MAX_PAYLOAD:
        raise RuntimeError("镜像大小 %d 超出 U 盘可写上限 %d" % (len(data), MSC_MAX_PAYLOAD))

    drv = find_msc_drive()
    if drv is None:
        raise RuntimeError("未找到本设备的 U 盘 (盘内应有 README.TXT); "
                           "若设备停在 boot, 先 python boot_flash.py run")
    print("[msc] U 盘:", drv)
    msc_write(drv, bin_path)
    crc = zlib.crc32(data) & 0xFFFFFFFF
    print("[msc] 已写入 FIRMWARE.BIN: %d 字节, CRC32=%08X" % (len(data), crc))

    dev = find_port(verbose=False)
    if dev is None:
        print("[msc] 无调试串口(复合版才有), 无法自动核对; 看设备 LED 即可")
        return
    end = time.time() + 10.0
    last = None
    while time.time() < end:
        app = try_app(dev, 1.0)
        if app is not None:
            try:
                last = app.info()
                if last["valid"] and last["length"] == len(data) and last["crc32"] == crc:
                    print("[msc] 设备已确认: len=%d crc=%08X state=%d"
                          % (last["length"], last["crc32"], last["state"]))
                    logs = app.logs()
                    if logs:
                        print("[msc] 日志:")
                        for ln in logs.splitlines():
                            print("   ", ln)
                    return True
            except Exception:
                pass
            finally:
                app.close()
        time.sleep(0.4)
    raise RuntimeError("设备未确认镜像: %r" % (last,))


# ------------------------------------------------------------------ 升级 ----
def do_flash(dev, bin_path):
    data = open(bin_path, "rb").read()
    if len(data) == 0 or len(data) > 32 * 1024:
        raise RuntimeError("镜像大小异常: %d" % len(data))
    bl, info = enter_boot(dev, force=True)
    print("[boot]", info)
    print("[boot] 擦除 %d 字节 ..." % len(data))
    r = bl.cmd("ERASE %d" % len(data), timeout=15.0)
    if r != "OK":
        raise RuntimeError("擦除失败: %r" % r)

    t0 = time.time()
    for off in range(0, len(data), CHUNK):
        chunk = data[off:off + CHUNK]
        bl.write_chunk(APP_BASE + off, chunk)
        if (off // CHUNK) % 8 == 0:
            sys.stdout.write("\r[boot] 写入 %d/%d" % (min(off + CHUNK, len(data)), len(data)))
            sys.stdout.flush()
    print("\r[boot] 写入完成 %.1fs" % (time.time() - t0))

    crc = zlib.crc32(data) & 0xFFFFFFFF
    print("[boot] 整片校验 CRC32=%08X ..." % crc)
    r = bl.cmd("F %d 0x%08X" % (len(data), crc), timeout=10.0)
    if r != "OK":
        raise RuntimeError("CRC 校验失败: %r" % r)

    print("[boot] 启动应用 ...")
    bl.ser.write(b"RUN\n")
    bl.read_line(2.0)
    bl.close()
    time.sleep(0.4)

    # 等应用枚举, 确认版本 (MSC 模式无 CDC 应答, 升级本身已由 CRC 保证)
    # boot(5740) -> app_msc(5742) 会换 COM 口, 每轮重新扫描
    app = None
    end = time.time() + 10.0
    while time.time() < end:
        adev = find_port(verbose=False)
        if adev is not None:
            app = try_app(adev, 1.0)
            if app is not None:
                break
        time.sleep(0.3)
    if app is None:
        print("[app] 未收到 CDC 应答 (可能烧的是 APP_MSC 单功能版), 升级已完成且 CRC 已校验")
        return None
    ver = app.version()
    print("[app] 版本:", ver)
    logs = app.logs()
    if logs:
        print("[app] 启动日志:")
        for ln in logs.splitlines():
            print("   ", ln)
    app.close()
    return ver


def do_tgt(dev, bin_path):
    """把 .bin 上传到本机镜像区, 然后通过 SWD 烧进目标芯片并复位运行"""
    data = open(bin_path, "rb").read()
    if len(data) == 0:
        raise RuntimeError("固件为空: %s" % bin_path)
    if len(data) > 15 * 1024:
        raise RuntimeError("固件 %d 字节超出镜像区可用空间 (~15KB)" % len(data))

    crc = zlib.crc32(data) & 0xFFFFFFFF
    print("[tgt] %s: %d 字节 CRC32=%08X -> 目标 0x%08X"
          % (os.path.basename(bin_path), len(data), crc, TGT_LOAD_ADDR))

    app = try_app(dev, 1.5)
    if app is None:
        print("设备不在应用模式, 先执行: python boot_flash.py run")
        return 3

    t0 = time.time()
    app.img_begin(len(data), crc)

    off = 0
    while off < len(data):
        chunk = data[off:off + TGT_CHUNK]
        got = app.img_data(off, chunk)
        off += len(chunk)
        if got != off:
            raise RuntimeError("设备已接收 %d != 期望 %d" % (got, off))
    print("\r[tgt] 镜像上传完成 %d/%d 字节 (%.1fs)" % (off, len(data), time.time() - t0))

    app.img_end()
    print("[tgt] 镜像生效 (CRC32 已由设备校验)")

    # 镜像生效后设备会自动探测并烧录; 这里等一小会儿避免重复触发
    time.sleep(0.15)
    try:
        st = app.status()
    except Exception:
        st = None
    if st is None or st["state"] not in (ST_PROGRAMMING, ST_DONE_WAIT, ST_FAIL_WAIT):
        print("[tgt] 手动触发烧录 ...")
        app.prog_now()

    # 烧录在主循环里同步执行, 期间不响应命令: 用长超时轮询
    last = None
    end = time.time() + 90.0
    while time.time() < end:
        try:
            last = app.status(timeout=30.0)
        except Exception:
            time.sleep(0.3)
            continue
        if last["state"] in (ST_DONE_WAIT, ST_FAIL_WAIT):
            break
        print("\r[tgt] %s progress=%d/%d   "
              % (ST_NAMES.get(last["state"], "?"), last["progress"], len(data)), end="")
        time.sleep(0.2)
    print()

    if last is None:
        app.close()
        raise RuntimeError("等待烧录结果超时")

    err = last["err"]
    code = err & 0xFF
    err_addr = err & 0xFFFFFF00
    ok = (last["state"] == ST_DONE_WAIT and last["result"] == 1)
    print("[tgt] DPIDR=0x%08X IDCODE=0x%08X progress=%d"
          % (last["dpidr"], last["idcode"], last["progress"]))
    if ok:
        print("[tgt] 烧录成功, 目标已复位运行 (SYSRESETREQ)")
    else:
        print("[tgt] 烧录失败: code=%d(%s) addr=0x%08X swd_err=%d"
              % (code, TP_NAMES.get(code, "?"), err_addr, last["swd_err"]))

    logs = app.logs()
    if logs:
        print("[app] 日志:")
        for ln in logs.splitlines():
            print("   ", ln)
    app.close()
    return 0 if ok else 4


def do_build(target="app_cdc"):
    if target not in TARGETS:
        raise RuntimeError("未知 target: %s (可选: %s)" % (target, ", ".join(TARGETS)))
    tname, objdir, outname = TARGETS[target]
    uvproj = os.path.join(PROJ, "offline_prog.uvproj")
    log = os.path.join(ROOT, "build_%s.log" % target)
    print("[build]", tname)
    pr = subprocess.run([UV4, "-b", uvproj, "-t", tname, "-j0", "-o", log],
                        capture_output=True)
    txt = open(log, "r", errors="replace").read() if os.path.exists(log) else ""
    if pr.returncode != 0 or "0 Error(s)" not in txt:
        print(txt[-3000:])
        raise RuntimeError("编译失败 (exit=%d)" % pr.returncode)
    for ln in txt.splitlines():
        if "Program Size" in ln:
            print("   ", ln.strip())

    axf = os.path.join(PROJ, objdir, outname + ".axf")
    outdir = os.path.join(ROOT, "out")
    os.makedirs(outdir, exist_ok=True)
    bin_path = os.path.join(outdir, target + ".bin")
    subprocess.run([FROMELF, "--bin", "--output", bin_path, axf], check=True)
    print("[bin]", bin_path, os.path.getsize(bin_path), "bytes")
    return bin_path


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1

    cmd = args[0].lower()
    if cmd == "ports":
        for p in list_ports.comports():
            print(p.device, p.description,
                  "%04X:%04X" % (p.vid or 0, p.pid or 0))
        return 0

    if cmd == "build":
        do_build(args[1] if len(args) > 1 else "app_cdc")
        return 0

    if cmd == "msc":
        # 只依赖 U 盘, 不需要先找到串口
        if len(args) < 2:
            print("用法: python boot_flash.py msc <bin>")
            return 1
        do_msc(args[1])
        return 0

    dev = find_port()
    if dev is None:
        return 2

    if cmd in ("upgrade", "flash"):
        if cmd == "upgrade":
            target = args[1] if len(args) > 1 else "app_cdc"
            bin_path = do_build(target)
        else:
            if len(args) < 2:
                print("用法: python boot_flash.py flash <bin>")
                return 1
            bin_path = args[1]
        do_flash(dev, bin_path)
        return 0

    if cmd == "info":
        app = try_app(dev, 1.0)
        if app is not None:
            print("[app]", dev, "版本:", app.version())
            try:
                m = app.info()
                print("[app] 镜像: valid=%d len=%d crc=%08X load=0x%08X cap=%d "
                      "state=%d result=%d"
                      % (m["valid"], m["length"], m["crc32"], m["load"],
                         m["capacity"], m["state"], m["result"]))
            except Exception as e:
                print("[app] 镜像信息读取失败:", e)
            logs = app.logs()
            if logs:
                print("[app] 日志:")
                for ln in logs.splitlines():
                    print("   ", ln)
            app.close()
        else:
            bl, info = wait_boot(dev, 3.0)
            print("[boot]", dev, info)
            bl.close()
        return 0

    if cmd == "boot":
        app = try_app(dev, 1.0)
        if app is None:
            print("设备已经在 boot 模式")
            return 0
        app.reboot_to_boot()
        app.close()
        bl, info = wait_boot(dev)
        print("[boot]", info)
        bl.close()
        return 0

    if cmd == "run":
        bl, info = enter_boot(dev, force=True)
        print("[boot]", info)
        r = bl.cmd("RUN", timeout=3.0)
        bl.close()
        print("[boot] RUN ->", r)
        time.sleep(0.5)
        app = None
        end = time.time() + 10.0
        while time.time() < end:
            adev = find_port(verbose=False)
            if adev is not None:
                app = try_app(adev, 1.0)
                if app is not None:
                    break
            time.sleep(0.3)
        if app is None:
            try:
                bl2, info2 = wait_boot(dev, 1.5)
                bl2.close()
                print("[boot]", info2)
                print("应用未启动, 仍在 boot")
                return 3
            except Exception:
                print("未收到 CDC 应答 (APP_MSC 无串口), 已跳转应用")
                return 0
        print("[app] 版本:", app.version())
        app.close()
        return 0

    if cmd == "probe":
        app = try_app(dev, 1.5)
        if app is None:
            print("设备不在应用模式, 先执行: python boot_flash.py run")
            return 3
        # 可选参数: 持续探测秒数 (抓目标上电后 SWD 还开着的窗口)
        try:
            secs = float(args[1]) if len(args) > 1 else 0.0
        except ValueError:
            secs = 0.0
        end = time.time() + secs
        names = {0: "OK", 1: "WAIT", 2: "FAULT", 3: "PARITY",
                 4: "NORESP(无应答)", 5: "IDCODE", 6: "POWER"}
        n = 0
        while True:
            r = app.probe()
            n += 1
            if r["status"] == 0 or time.time() >= end:
                break
            if secs:
                print("\r[probe] 第 %d 次: %s" % (n, names.get(r["status"], r["status"])), end="")
            time.sleep(0.02)
        print("\n[probe] 尝试 %d 次, 结果: %s" % (n, names.get(r["status"], r["status"])))
        print("[probe] DPIDR  = 0x%08X" % r["dpidr"])
        print("[probe] IDCODE = 0x%08X" % r["idcode"])
        logs = app.logs()
        if logs:
            print("[app] 日志:")
            for ln in logs.splitlines():
                print("   ", ln)
        app.close()
        return 0 if r["status"] == 0 else 4

    if cmd == "tgt":
        if len(args) < 2:
            print("用法: python boot_flash.py tgt <bin>")
            return 1
        return do_tgt(dev, args[1])

    if cmd == "clk":
        # 示波器测试: 开启/关闭 SWCLK(PA3)/SWDIO(PA4) 持续方波
        app = try_app(dev, 1.5)
        if app is None:
            print("设备不在应用模式, 先执行: python boot_flash.py run")
            return 3
        val = 1
        if len(args) > 1 and args[1].lower() in ("0", "off", "stop"):
            val = 0
        try:
            app.cmd(CMD_CLK_TEST, bytes([val]), timeout=1.5)
        except Exception:
            pass
        print("[clk] 已%s输出: SWCLK=PA3(高频方波) SWDIO=PA4(低频方波)"
              % ("开启" if val else "关闭"))
        time.sleep(0.2)
        app.close()
        return 0

    if cmd == "log":
        app = try_app(dev, 1.0)
        if app is None:
            print("设备不在应用模式, 先执行: python boot_flash.py run")
            return 3
        logs = app.logs()
        sys.stdout.write(logs if logs else "(无新日志)\n")
        app.close()
        return 0

    print(__doc__)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print("FAILED:", e)
        sys.exit(1)
