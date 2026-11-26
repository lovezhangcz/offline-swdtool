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
    python boot_flash.py tgt   <bin|hex|aesbin>  # 上传镜像并用 SWD 烧进目标芯片, 完成后复位运行
    python boot_flash.py strm  <bin|hex|aesbin>  # 流式烧录: 分包直达目标 Flash, 不受本机镜像区大小限制
    python boot_flash.py makeaes <bin|hex> [芯片代码] [输出.aesbin]  # 制作 AES 加密专用烧录文件
    python boot_flash.py erase                # 整片擦除目标芯片, 并校验首末字为空白
    python boot_flash.py fsize                # 本机 Flash 实际容量自检 (64KB/128KB 之上实测)
    python boot_flash.py clear                # 删除烧录器内的目标固件 (之后不再自动烧录)
    python boot_flash.py clk [0]              # 示波器测试: SWCLK/SWDIO 持续方波 (0=停止)
    python boot_flash.py ports                 # 列出可用串口

target:
    app_cdc : 单功能虚拟串口 (0483:5740)
    app_msc : 复合设备, 同时出 U 盘 + 调试串口 (0483:5742), 两路都能用
    app_cdc_256k : 同上 CDC, 给大容量主板用 (应用 0x08002000, 镜像区 112KB)
    app_msc_256k : 同上复合设备, 给大容量主板用 (应用 0x08002000, 镜像区 112KB)
    boot : 唯一的 boot (boot 6KB + 标志区 2KB + 应用 20KB), 两种容量通用

协议:
    boot  : ASCII 行命令 INFO / ERASE len / W addr len crc16 + 裸数据 / F len crc32 / RUN
    app   : 二进制帧 AA 55 CMD LEN_L LEN_H PAYLOAD CRC16
"""
import os
import shutil
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

import aesbin

# ------------------------------------------------------------------ 常量 ----
VID = 0x0483
PID_CDC = 0x5740        # 单功能 CDC (boot 也是这个)
PID_MSC = 0x5741        # 单功能 MSC (无串口, 不会出现在 COM 列表)
PID_COMP = 0x5742       # 复合设备 MSC+CDC (APP_MSC 调试版)
SERIAL_PIDS = (PID_CDC, PID_COMP)
APP_BASE = 0x08002000
APP_SIZE = 20 * 1024        # APP 区 20KB (所有容量统一: boot 6KB + 标志区 2KB 之后)
APP_BASES = (APP_BASE,)
CHUNK = 512
BOOT_WAIT = 12.0

HDR = b"\xAA\x55"
CMD_PING = 0x01
CMD_GET_INFO = 0x02
CMD_GET_VERSION = 0x03
CMD_IMG_CLEAR = 0x13
CMD_IMG_BEGIN = 0x10
CMD_IMG_DATA = 0x11
CMD_IMG_END = 0x12
CMD_PROG_NOW = 0x20
CMD_GET_STATUS = 0x21
CMD_TGT_PROBE = 0x22
CMD_REBOOT_BOOT = 0x23
CMD_LOG_POLL = 0x24
CMD_CLK_TEST = 0x25
CMD_FLASH_TEST = 0x26
CMD_STRM_BEGIN = 0x30
CMD_STRM_DATA = 0x31
CMD_STRM_END = 0x32
CMD_TGT_ERASE = 0x33
CMD_TGT_READ = 0x34

TGT_LOAD_ADDR = 0x08000000       # 目标芯片烧录起始地址
TGT_CHUNK = 508                  # IMG_DATA 单包最大数据量

# 目标侧状态机 (与 main.c 的 ST_xxx 一致)
ST_NO_IMAGE, ST_ARMED, ST_PROGRAMMING, ST_DONE_WAIT, ST_FAIL_WAIT = 0, 1, 2, 3, 4
ST_NAMES = {ST_NO_IMAGE: "无镜像", ST_ARMED: "待命", ST_PROGRAMMING: "烧录中",
            ST_DONE_WAIT: "成功(等拔出)", ST_FAIL_WAIT: "失败(等拔出)"}

# TargetProg 错误码 (target_prog.h)
TP_NAMES = {0: "OK", 1: "SWD连接失败", 2: "目标ID不符", 3: "停机失败",
            4: "FPEC解锁失败", 5: "读/写保护", 6: "擦除失败", 7: "写入失败",
            8: "CRC校验不一致", 9: "无镜像", 10: "烧录算法失败",
            11: "超出目标Flash容量", 12: "参数错误",
            13: "镜像是HEX文本(不是bin)"}

ROOT = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.join(ROOT, "..", "app_offline_prog", "project")
UV4 = r"C:\Keil_v5\UV4\UV4.exe"
FROMELF = r"C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe"

TARGETS = {
    "app_cdc": ("offline_prog_APP_CDC", "obj_app_cdc", "offline_prog_app_cdc"),
    "app_msc": ("offline_prog_APP_MSC", "obj_app_msc", "offline_prog_app_msc"),
    # 256KB 大容量主板: 镜像区 192KB (0x08010000-0x0803FFFF), boot 与 64KB 版共用
    "app_cdc_256k": ("offline_prog_APP_CDC_256K", "obj_app_cdc_256k",
                     "offline_prog_app_cdc_256k"),
    "app_msc_256k": ("offline_prog_APP_MSC_256K", "obj_app_msc_256k",
                     "offline_prog_app_msc_256k"),
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
        """返回镜像/状态: valid, length, crc32, load, capacity, state, result, flash_kb"""
        st, pl = self.cmd(CMD_GET_INFO)
        if st != 0 or len(pl) < 19:
            raise RuntimeError("GET_INFO 失败 status=%d len=%d" % (st, len(pl)))
        length, crc, load, cap = struct.unpack("<IIII", pl[1:17])
        fkb = struct.unpack("<I", pl[19:23])[0] if len(pl) >= 23 else 0
        burn = struct.unpack("<I", pl[23:27])[0] if len(pl) >= 27 else 0
        chip = struct.unpack("<I", pl[27:31])[0] if len(pl) >= 32 else 0xFFFFFFFF
        trailer = pl[31] if len(pl) >= 32 else 0
        if not pl[0]:                 # 无镜像时头区是空白, 字段无意义
            length = crc = load = 0
        return dict(valid=pl[0], length=length, crc32=crc, load=load,
                    capacity=cap, state=pl[17], result=pl[18], flash_kb=fkb,
                    burn_count=burn, chip_code=chip, trailer=trailer)

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
    def img_begin(self, length, crc, load=TGT_LOAD_ADDR, name=b"", trailer_len=0):
        """name: 可选文件名, 设备存入镜像头, 使 U 盘里显示同样的名字
        trailer_len: 载荷尾部随文件携带的信息长度 (不烧进目标)"""
        pl = struct.pack("<IIII", length, crc, load, trailer_len)
        if name:
            pl += name[:63]
        st, _ = self.cmd(CMD_IMG_BEGIN, pl, timeout=6.0)
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
        """state, result, err, dpidr, idcode, progress, swd_err
        [+ burn_seq, host_online: 新固件扩展字段]"""
        st, pl = self.cmd(CMD_GET_STATUS, timeout=timeout)
        if st != 0 or len(pl) < 22:
            raise RuntimeError("GET_STATUS 失败 status=%d len=%d" % (st, len(pl)))
        err, dpidr, idcode, prog, swd_err = struct.unpack("<IIIII", pl[2:22])
        r = dict(state=pl[0], result=pl[1], err=err, dpidr=dpidr,
                 idcode=idcode, progress=prog, swd_err=swd_err,
                 burn_seq=None, host_online=None)
        if len(pl) >= 27:
            r["burn_seq"] = struct.unpack("<I", pl[22:26])[0]
            r["host_online"] = pl[26]
        return r

    # ---------------- 流式烧录 (不占本机镜像区, 支持超大固件) ----------------
    def strm_begin(self, length, crc, load=TGT_LOAD_ADDR):
        """返回 status(TP_xxx), flash_kb(目标 Flash 容量 KB), err"""
        st, pl = self.cmd(CMD_STRM_BEGIN, struct.pack("<III", length, crc, load),
                          timeout=30.0)      # 连接 + 擦除较慢
        if st != 0 or len(pl) < 9:
            raise RuntimeError("STRM_BEGIN 失败 status=%d len=%d" % (st, len(pl)))
        flash_kb, err = struct.unpack("<II", pl[1:9])
        return dict(status=pl[0], flash_kb=flash_kb, err=err)

    def strm_data(self, off, chunk, timeout=10.0):
        st, pl = self.cmd(CMD_STRM_DATA, struct.pack("<I", off) + chunk,
                          timeout=timeout)
        if st != 0 or len(pl) < 9:
            raise RuntimeError("STRM_DATA off=%d 失败 status=%d" % (off, st))
        return dict(status=pl[0],
                    received=struct.unpack("<I", pl[1:5])[0],
                    err=struct.unpack("<I", pl[5:9])[0])

    def strm_end(self, timeout=120.0):
        """尾部写页 + 回读 CRC 校验, 耗时随固件大小增长"""
        st, pl = self.cmd(CMD_STRM_END, timeout=timeout)
        if st != 0 or len(pl) < 5:
            raise RuntimeError("STRM_END 失败 status=%d" % st)
        return dict(status=pl[0], err=struct.unpack("<I", pl[1:5])[0])

    def img_clear(self, timeout=5.0):
        """删除烧录器内的目标固件镜像; 之后不会再对目标自动烧录"""
        st, _ = self.cmd(CMD_IMG_CLEAR, timeout=timeout)
        if st != 0:
            raise RuntimeError("IMG_CLEAR 失败 status=%d" % st)

    def tgt_erase(self, timeout=30.0):
        """整片擦除目标芯片, 返回 status(TP_xxx) 与目标 Flash 容量(KB)"""
        st, pl = self.cmd(CMD_TGT_ERASE, timeout=timeout)
        if st != 0 or len(pl) < 5:
            raise RuntimeError("TGT_ERASE 失败 status=%d" % st)
        return dict(status=pl[0], flash_kb=struct.unpack("<I", pl[1:5])[0])

    def tgt_read(self, addr, length, timeout=8.0):
        """诊断: 直接读目标存储器 (每次 <=48 字节, 内部循环), 返回 bytes"""
        out = bytearray()
        while length > 0:
            n = min(48, length)
            n &= ~3                      # 4 字节对齐
            st, pl = self.cmd(CMD_TGT_READ, struct.pack("<II", addr, n),
                              timeout=timeout)
            if st != 0:
                raise RuntimeError("TGT_READ @0x%08X 失败 status=%d" % (addr, st))
            out += pl
            addr += n
            length -= n
        return bytes(out)

    def flash_test(self, timeout=10.0):
        """本机 Flash 实际容量自检: 芯片自报 KB + 64/128/256KB 之上页的擦写结果"""
        st, pl = self.cmd(CMD_FLASH_TEST, timeout=timeout)
        if st != 0 or len(pl) < 3:
            raise RuntimeError("FLASH_TEST 失败 status=%d" % st)
        r = dict(reported=pl[0], at64=pl[1], at128=pl[2], at256=4,
                 idcode=0, fmc_pid=0, page_size=0)
        if len(pl) >= 4:
            r["at256"] = pl[3]
        if len(pl) >= 12:
            r["idcode"] = struct.unpack("<I", pl[4:8])[0]
            r["fmc_pid"] = struct.unpack("<I", pl[8:12])[0]
        if len(pl) >= 13:
            r["page_size"] = pl[12]
        if len(pl) >= 17:
            r["scan_bad"] = struct.unpack("<I", pl[13:17])[0]
        if len(pl) >= 21:
            r["wrp"] = struct.unpack("<I", pl[17:21])[0]
        return r


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


def msc_free(drv):
    """返回 (可用字节, 总字节); 虚拟盘只有 15KB, 很容易被旧文件占满"""
    try:
        u = shutil.disk_usage(drv)
        return u.free, u.total
    except OSError:
        return None, None


def msc_clean(drv):
    """清掉虚拟盘上的旧文件 (保留 README.TXT), 释放簇给新固件"""
    removed = []
    try:
        names = os.listdir(drv)
    except OSError as e:
        raise RuntimeError("无法读取 U 盘 %s: %s" % (drv, e))
    for n in names:
        if n.upper() == "README.TXT" or n.upper().startswith("SYSTEM VOLUME"):
            continue
        p = os.path.join(drv, n)
        try:
            if os.path.isfile(p):
                sz = os.path.getsize(p)
                os.remove(p)
                removed.append("%s(%dB)" % (n, sz))
        except OSError as e:
            print("[msc] 删除 %s 失败: %s" % (n, e))
    return removed


def msc_write(drv, data):
    """把固件数据写到 U 盘根目录的 FIRMWARE.BIN, 并强制刷盘"""
    dst = os.path.join(drv, "FIRMWARE.BIN")

    free, total = msc_free(drv)
    if free is None or free < len(data):
        removed = msc_clean(drv)          # 先腾地方: 覆盖写更大的文件会被报"磁盘已满"
        if removed:
            print("[msc] 已清理旧文件:", ", ".join(removed))
        free, total = msc_free(drv)
    if free is not None and free < len(data):
        raise RuntimeError("U 盘空间不足: 需要 %d 字节, 可用 %d 字节 (虚拟盘共 %d)。"
                           "大固件请用流式烧录" % (len(data), free, total))

    try:
        with open(dst, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
    except OSError as e:
        try:
            os.remove(dst)
        except OSError:
            pass
        raise RuntimeError("写入 U 盘失败: %s (固件 %d 字节, 可用 %s 字节)"
                           % (e, len(data), free))
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
    load_addr, data = load_image(bin_path)
    # 可用空间由虚拟盘自身决定 (64KB 版约 36KB, 256KB 版约 191KB), msc_write 会查
    if len(data) == 0:
        raise RuntimeError("固件为空: %s" % bin_path)
    if (load_addr & 0xFF000000) != 0x08000000:
        raise RuntimeError("固件起始地址 0x%08X 不在 0x08xxxxxx" % load_addr)

    drv = find_msc_drive()
    if drv is None:
        raise RuntimeError("未找到本设备的 U 盘 (盘内应有 README.TXT); "
                           "若设备停在 boot, 先 python boot_flash.py run")
    print("[msc] U 盘:", drv)
    msc_write(drv, data)
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
def load_app_image(path):
    """读取主板 APP 固件 (bin/hex), 校验向量表; 返回 (数据, 应用基址)

    应用基址取复位向量所在的 1KB 页: 两版布局分别是 0x08001800 / 0x08001C00,
    这样同一套上位机可以给两种布局烧录, 不用关心 boot 版本。"""
    addr, data = load_image(path)
    if path.lower().endswith((".hex", ".ihx")) and addr not in APP_BASES:
        raise RuntimeError("HEX 起始地址 0x%08X 不是主板 APP 基址 0x%08X"
                           % (addr, APP_BASE))
    if len(data) < 8 or len(data) > APP_SIZE:
        raise RuntimeError("APP 固件大小异常: %d 字节 (上限 %d)" % (len(data), APP_SIZE))
    sp, rv = struct.unpack("<II", data[:8])
    if not (0x20000000 <= sp < 0x20010000):
        raise RuntimeError("不是主板 APP 固件: 向量表 SP=0x%08X" % sp)
    base = (rv & ~1) & ~0x3FF
    if base not in APP_BASES:
        raise RuntimeError("不是主板 APP 固件: 复位向量 0x%08X 不在 0x%08X 起的"
                           " APP 区 (是不是选成了目标芯片固件?)"
                           % (rv, APP_BASE))
    return data, base


def info_field(info, key, default=None):
    """从 boot 的 "INFO ver=.. app=.. base=0x.." 行里取一个字段"""
    for tok in info.split():
        if tok.startswith(key + "="):
            v = tok[len(key) + 1:]
            try:
                return int(v, 0)
            except ValueError:
                try:
                    return int(v, 16)      # boot 打印的 base=08002000 不带 0x
                except ValueError:
                    return v
    return default


def do_flash(dev, bin_path):
    """升级主板应用: bin/hex 均可, 镜像必须位于 APP 区"""
    data, app_base = load_app_image(bin_path)
    bl, info = enter_boot(dev, force=True)
    print("[boot]", info)
    boot_base = info_field(info, "base")
    if boot_base is None:
        raise RuntimeError("当前 boot 未上报应用基址 (旧版 boot, 应用在 0x08001800); "
                           "请先用 ST-Link 烧入新的 out\\boot.bin")
    if app_base != boot_base:
        raise RuntimeError("镜像按 0x%08X 链接, 但当前 boot 的应用基址是 0x%08X, 布局不一致"
                           % (app_base, boot_base))
    print("[boot] 擦除 %d 字节 ..." % len(data))
    r = bl.cmd("ERASE %d" % len(data), timeout=15.0)
    if r != "OK":
        raise RuntimeError("擦除失败: %r" % r)

    t0 = time.time()
    for off in range(0, len(data), CHUNK):
        chunk = data[off:off + CHUNK]
        bl.write_chunk(app_base + off, chunk)
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


# ------------------------------------------------------------ 固件装载 ----
def hex_to_bin(path):
    """把 Intel HEX 转成 (基地址, 连续 bin 数据), 数据空隙填 0xFF"""
    mem = {}
    base = 0
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            if not line.startswith(":"):
                raise ValueError("HEX 第 %d 行格式错误 (缺少 ':')" % lineno)
            try:
                raw = bytes.fromhex(line[1:])
            except ValueError:
                raise ValueError("HEX 第 %d 行不是合法十六进制" % lineno)
            if len(raw) < 5:
                raise ValueError("HEX 第 %d 行长度不足" % lineno)
            n = raw[0]
            addr = (raw[1] << 8) | raw[2]
            typ = raw[3]
            data = raw[4:4 + n]
            if len(data) != n:
                raise ValueError("HEX 第 %d 行数据长度不符" % lineno)
            if (sum(raw[:4 + n]) + raw[4 + n]) & 0xFF:
                raise ValueError("HEX 第 %d 行校验和错误" % lineno)
            if typ == 0x00:                       # 数据
                for i, b in enumerate(data):
                    mem[base + addr + i] = b
            elif typ == 0x01:                     # 结束
                break
            elif typ == 0x02:                     # 扩展段地址
                base = ((data[0] << 8) | data[1]) << 4
            elif typ == 0x04:                     # 扩展线性地址
                base = ((data[0] << 8) | data[1]) << 16
            # 03/05 起始地址记录: 忽略
    if not mem:
        raise ValueError("HEX 文件没有数据记录: %s" % path)
    lo, hi = min(mem), max(mem)
    blob = bytearray(b"\xff" * (hi - lo + 1))
    for a, b in mem.items():
        blob[a - lo] = b
    return lo, bytes(blob)


def load_image(path):
    """读取固件文件: .hex/.ihx 自动转 bin; 返回 (加载地址, 数据)"""
    if path.lower().endswith((".hex", ".ihx")):
        return hex_to_bin(path)
    return TGT_LOAD_ADDR, open(path, "rb").read()


def load_image_ex(path):
    """读取固件: .aesbin 在内存里解密还原, 其它按 bin/hex。
    返回 (加载地址, 镜像数据, 附加信息 dict)
    附加信息: fmt / chip_code(None=未携带) / trailer(随镜像携带的尾部字节)"""
    if aesbin.is_aesbin(path):
        r = aesbin.load_file(path)
        return (r["load_addr"], r["image"],
                dict(fmt="aesbin", chip_code=r["chip_code"],
                     trailer=r["trailer"], reserved=r["reserved"]))
    load, data = load_image(path)
    fmt = "hex" if path.lower().endswith((".hex", ".ihx")) else "bin"
    return load, data, dict(fmt=fmt, chip_code=None, trailer=b"", reserved=b"")


def do_tgt(dev, bin_path):
    """把固件 (bin/hex/aesbin) 上传到本机镜像区, 再经 SWD 烧进目标并复位运行
    aesbin 会在内存里解密; 文件尾部携带的芯片代码等信息一起存入烧录器,
    但 SWD 烧录时只写镜像部分, 不会烧到目标芯片"""
    load_addr, data, meta = load_image_ex(bin_path)
    trailer = meta["trailer"]
    payload = data + trailer
    if len(data) == 0:
        raise RuntimeError("固件为空: %s" % bin_path)
    if (load_addr & 0xFF000000) != 0x08000000:
        raise RuntimeError("固件起始地址 0x%08X 不在 0x08xxxxxx" % load_addr)
    crc = zlib.crc32(data) & 0xFFFFFFFF

    app = try_app(dev, 1.5)
    if app is None:
        print("设备不在应用模式, 先执行: python boot_flash.py run")
        return 3

    # 镜像区大小随固件版本不同 (64KB 版约 36KB, 256K 版 110KB), 以设备自报为准
    cap = app.info().get("capacity", 0)
    if cap and len(payload) > cap:
        raise RuntimeError("固件 %d 字节(含尾部 %d 字节)超出本机镜像区可用空间 "
                           "(%d 字节), 更大请用流式烧录 (strm)"
                           % (len(payload), len(trailer), cap))

    tag = ""
    if meta["chip_code"] is not None:
        tag = "  芯片代码=%s" % aesbin.chip_name(meta["chip_code"])
    print("[tgt] %s: %d 字节 CRC32=%08X -> 0x%08X%s"
          % (os.path.basename(bin_path), len(data), crc, load_addr, tag))

    t0 = time.time()
    # 文件名改为 .bin, 与 U 盘写入方式一致 (U 盘里看到的就是原始文件名)
    fw_name = os.path.splitext(os.path.basename(bin_path))[0] + ".bin"
    fw_name = fw_name.encode("ascii", "replace").replace(b"?", b"_")
    app.img_begin(len(payload), crc, load_addr, fw_name, len(trailer))

    off = 0
    while off < len(payload):
        chunk = payload[off:off + TGT_CHUNK]
        got = app.img_data(off, chunk)
        off += len(chunk)
        if got != off:
            raise RuntimeError("设备已接收 %d != 期望 %d" % (got, off))
    print("\r[tgt] 镜像上传完成 %d/%d 字节 (%.1fs)"
          % (len(data), len(data), time.time() - t0))

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


def do_erase(dev):
    """整片擦除目标芯片 (SWD 连接 -> 全片擦除 -> 首末字空白校验 -> 复位运行)"""
    app = try_app(dev, 1.5)
    if app is None:
        print("设备不在应用模式, 先执行: python boot_flash.py run")
        return 3

    t0 = time.time()
    r = app.tgt_erase()
    if r["status"] != 0:
        print("[erase] 擦除失败: code=%d(%s) 目标Flash=%dKB"
              % (r["status"], TP_NAMES.get(r["status"], "?"), r["flash_kb"]))
        app.close()
        return 4
    print("[erase] 整片擦除完成: 目标Flash=%dKB, 首末字均为 0xFFFFFFFF (%.1fs)"
          % (r["flash_kb"], time.time() - t0))
    logs = app.logs()
    if logs:
        print("[app] 日志:")
        for ln in logs.splitlines():
            print("   ", ln)
    app.close()
    return 0


def do_clear(dev):
    """删除烧录器内保存的目标固件镜像 (之后不会再自动烧录)"""
    app = try_app(dev, 1.5)
    if app is None:
        print("设备不在应用模式, 先执行: python boot_flash.py run")
        return 3
    m = app.info()
    if not m["valid"]:
        print("[clear] 烧录器里本来就没有固件")
        app.close()
        return 0
    print("[clear] 当前镜像: %d 字节 crc=%08X -> 删除" % (m["length"], m["crc32"]))
    app.img_clear()
    time.sleep(0.3)
    m = app.info()
    if m["valid"]:
        raise RuntimeError("清除后镜像仍然有效")
    print("[clear] 已删除, 当前状态: valid=0 state=%d (不会再对目标烧录)" % m["state"])
    app.close()
    return 0


def do_fsize(dev):
    """查本机 Flash 实际容量: 芯片自报值 + 64KB/128KB 之上页的真实擦写结果"""
    app = try_app(dev, 1.5)
    if app is None:
        print("设备不在应用模式, 先执行: python boot_flash.py run")
        return 3
    r = app.flash_test()
    names = {0: "OK", 1: "解锁/擦除失败", 2: "编程失败", 3: "回读不一致", 4: "该页不存在"}
    print("[fsize] 芯片 IDCODE=0x%08X (DEV=0x%03X REV=0x%04X) FMC_PID=0x%08X"
          % (r["idcode"], r["idcode"] & 0xFFF, (r["idcode"] >> 16) & 0xFFFF,
             r["fmc_pid"]))
    print("[fsize] 芯片自报 F_SIZE: %d KB" % r["reported"])
    print("[fsize] 0x08010000 (64KB 之上) 擦写: %s" % names.get(r["at64"], r["at64"]))
    print("[fsize] 0x08020000 (128KB 之上) 擦写: %s" % names.get(r["at128"], r["at128"]))
    print("[fsize] 0x08040000 (256KB 之上) 擦写: %s" % names.get(r["at256"], r["at256"]))
    psz = {0: "1KB", 1: "2KB"}.get(r["page_size"], "探测失败(%d)" % r["page_size"])
    print("[fsize] 物理页大小: %s" % psz)
    if "scan_bad" in r:
        if r["scan_bad"] == 0xFFFFFFFF:
            print("[fsize] 0x08010000-0x0803FFFF 逐页擦除: 全部正常")
        else:
            print("[fsize] 0x08010000-0x0803FFFF 逐页擦除: 0x%08X 起失败" % r["scan_bad"])
    if "wrp" in r:
        print("[fsize] FLASH_WRPR=0x%08X%s" % (r["wrp"], "" if r["wrp"] == 0xFFFFFFFF
                                              else "  <- 存在写保护"))
    if r["at256"] == 0:
        print("[fsize] 结论: 实际可用 Flash >= 512KB")
    elif r["at128"] == 0:
        print("[fsize] 结论: 实际可用 Flash = 256KB (自报 %dKB 不可信)" % r["reported"])
    elif r["at64"] == 0:
        print("[fsize] 结论: 实际可用 Flash = 128KB (自报 %dKB 不可信)" % r["reported"])
    else:
        print("[fsize] 结论: 实际可用 Flash = 64KB")
    app.close()
    return 0


def do_strm(dev, bin_path):
    """流式烧录 (bin/hex/aesbin): 数据分包直达目标 Flash, 不占本机镜像区
    aesbin 先解密, 只把镜像部分写进目标, 尾部携带的信息不烧录"""
    load_addr, data, meta = load_image_ex(bin_path)
    if len(data) == 0:
        raise RuntimeError("固件为空: %s" % bin_path)
    if (load_addr & 0xFF000000) != 0x08000000:
        raise RuntimeError("固件起始地址 0x%08X 不在 0x08xxxxxx" % load_addr)
    crc = zlib.crc32(data) & 0xFFFFFFFF
    tag = ""
    if meta["chip_code"] is not None:
        tag = "  芯片代码=%s" % aesbin.chip_name(meta["chip_code"])
    print("[strm] %s: %d 字节 CRC32=%08X -> 0x%08X%s"
          % (os.path.basename(bin_path), len(data), crc, load_addr, tag))

    app = try_app(dev, 1.5)
    if app is None:
        print("设备不在应用模式, 先执行: python boot_flash.py run")
        return 3

    # 设备可能正在自动烧录镜像区里的旧固件, 等它空闲再开始流式会话
    for _ in range(120):
        try:
            st = app.status(timeout=5.0)
        except Exception:
            time.sleep(0.3)
            continue
        if st["state"] != ST_PROGRAMMING:
            break
        time.sleep(0.5)

    t0 = time.time()
    r = app.strm_begin(len(data), crc, load_addr)
    if r["status"] != 0:
        print("[strm] 开始失败: code=%d(%s) 目标Flash=%dKB"
              % (r["status"], TP_NAMES.get(r["status"], "?"), r["flash_kb"]))
        if r["status"] == 11:
            print("[strm] 固件 %d 字节 > 目标 Flash %dKB, 请换更大容量的目标芯片"
                  % (len(data), r["flash_kb"]))
        app.close()
        return 4
    print("[strm] 目标已连接/停机/擦除完成, 目标Flash=%dKB (%.1fs)"
          % (r["flash_kb"], time.time() - t0))

    off = 0
    next_pct = 10
    while off < len(data):
        chunk = data[off:off + TGT_CHUNK]
        got = app.strm_data(off, chunk)
        if got["status"] != 0:
            print("\n[strm] 传输失败 off=%d code=%d(%s)"
                  % (off, got["status"], TP_NAMES.get(got["status"], "?")))
            app.close()
            return 4
        off += len(chunk)
        if got["received"] != off:
            raise RuntimeError("设备已接收 %d != 期望 %d" % (got["received"], off))
        pct = off * 100 // len(data)
        if pct >= next_pct:
            print("\r[strm] 传输 %3d%%  %d/%d  (%.1fs)"
                  % (pct, off, len(data), time.time() - t0), end="")
            next_pct = pct + 10
    print("\r[strm] 传输 100%%  %d/%d  (%.1fs)" % (off, len(data), time.time() - t0))

    r = app.strm_end()
    err_addr = r["err"] & 0xFFFFFF00
    if r["status"] != 0:
        print("[strm] 烧录失败: code=%d(%s) addr=0x%08X"
              % (r["status"], TP_NAMES.get(r["status"], "?"), err_addr))
        app.close()
        return 4

    print("[strm] 回读 CRC32 校验通过, 目标已复位运行 (总耗时 %.1fs)" % (time.time() - t0))
    try:
        st = app.status()
        print("[strm] DPIDR=0x%08X IDCODE=0x%08X"
              % (st["dpidr"], st["idcode"]))
    except Exception:
        pass
    logs = app.logs()
    if logs:
        print("[app] 日志:")
        for ln in logs.splitlines():
            print("   ", ln)
    app.close()
    return 0


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

    if cmd == "makeaes":
        # 纯本地操作, 不需要连接烧录器
        if len(args) < 2:
            print("用法: python boot_flash.py makeaes <bin|hex> [芯片代码] [输出.aesbin]")
            return 1
        src = args[1]
        code = int(args[2], 0) if len(args) > 2 else 0
        out = args[3] if len(args) > 3 else os.path.splitext(src)[0] + ".aesbin"
        load, data = load_image(src)
        r = aesbin.make_file(out, data, code, load)
        print("[aesbin] %s: 镜像 %d 字节, 芯片 %s, 文件 %d 字节"
              % (out, len(data), aesbin.chip_name(code), r["size"]))
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
                extra = ""
                if m["chip_code"] != 0xFFFFFFFF:
                    extra = " 芯片代码=%s" % aesbin.chip_name(m["chip_code"])
                print("[app] 镜像: valid=%d len=%d crc=%08X load=0x%08X cap=%d "
                      "state=%d result=%d 本机Flash自报=%dKB 成功烧录=%d次%s"
                      % (m["valid"], m["length"], m["crc32"], m["load"],
                         m["capacity"], m["state"], m["result"], m["flash_kb"],
                         m["burn_count"], extra))
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

    if cmd == "strm":
        if len(args) < 2:
            print("用法: python boot_flash.py strm <bin>   (流式烧录, 不受 15KB 镜像区限制)")
            return 1
        return do_strm(dev, args[1])

    if cmd == "erase":
        return do_erase(dev)

    if cmd == "read":
        # read <addr> <len>: 诊断, 直接读目标存储器并 hexdump
        if len(args) < 3:
            print("用法: python boot_flash.py read <addr> <len>")
            return 1
        addr = int(args[1], 0)
        n = int(args[2], 0)
        app = try_app(dev, 1.5)
        if app is None:
            print("设备不在应用模式, 先执行: python boot_flash.py run")
            return 3
        try:
            data = app.tgt_read(addr, n)
        finally:
            app.close()
        for off in range(0, len(data), 16):
            row = data[off:off + 16]
            print("0x%08X  %-47s  %s" % (
                addr + off,
                " ".join("%02X" % b for b in row),
                "".join(chr(b) if 32 <= b < 127 else "." for b in row)))
        return 0

    if cmd == "fsize":
        return do_fsize(dev)

    if cmd == "clear":
        return do_clear(dev)

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
