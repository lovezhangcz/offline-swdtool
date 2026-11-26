#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
联机烧录上位机 (生产版)

工作方式:
  1. 自动连接烧录器 (USB CDC 0483:5740/5742)。目标板接入瞬间的供电跌落常
     导致烧录器 USB 掉线, 这里会自动重连 (重新枚举/换 COM 口也能找回来),
     重连后自动恢复监测, 无需人工操作。
  2. 烧录镜像来源由 [启用流水烧录] 复选框决定 (默认不勾选):
     - 不勾选: 使用烧录器内已装载的离线镜像。插入目标板烧录器会自动烧录,
       也可点击 [烧 录] 让烧录器按该镜像再烧一次。
       [写入烧录器] 把当前选择的固件写入烧录器, 供离线模式使用。
     - 勾选:   每次点击 [烧 录] 把电脑上选定的固件流式写入目标板。
       由未勾选切到勾选时, 自动清除一次烧录器内的离线镜像 (避免设备自己
       抢着烧旧镜像)。
  3. 专用烧录文件: 窗口右下角的 [制作专用文件] 打开独立子窗口 (生产发送方使用),
     最多放两段固件 (bin 可手动填偏移, hex 自动识别地址并锁定), 合并后 AES
     加密成 .aesbin; 文件尾部携带目标芯片代码 (0=自动识别) + 64 字节保留区。
     加载 .aesbin 时在内存里解密还原, 界面上显示芯片代码。
     子窗口里的 [升级烧录器] 选择应用固件即可升级烧录器 (升级前校验识别码与
     版本: 识别码不符或版本相同/更低会被拒绝, 版本 0 为调试版可覆盖任何版本)。
  4. 上位机在线期间持续轮询, 烧录器据此判定"上位机在线"(3 秒内有命令),
     每次烧录结束都会报告结果 (成功/失败) 和累计成功次数, 界面同步更新。
     累计次数由烧录器保存在 Flash 流水区, 掉线重连也不会丢。
  5. 成功: 绿灯 + 提示音; 失败: 红灯 + 低音。烧录中途掉线不算失败,
     自动重连后继续。

运行: 双击 online_prog.py 或  python online_prog.py
依赖: Python3 + pyserial + tkinter(自带), 复用同目录 boot_flash.py 的协议实现
"""
import contextlib
import io
import math
import os
import queue
import struct
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
import zlib

import boot_flash as bf
import aesbin

try:
    import winsound
except ImportError:
    winsound = None

DETECT_ADDR = 0xE0042000          # 目标 DBGMCU_IDCODE, 用读它当 SWD 握手
POLL_S = 0.25                     # 目标握手轮询间隔
STATUS_S = 0.5                    # 烧录器状态/结果轮询间隔 (同时充当心跳)
RECONNECT_S = 1.0                 # 掉线重连间隔
SETTLE_S = 0.5                    # 重连后等供电稳定再握手
BURN_WAIT_S = 90.0                # 单次烧录最长等待
INSERT_SETTLE_S = 0.8             # 目标插入后等待供电稳定再触发 (电机板浪涌)
AUTO_RETRY_MAX = 2                # 离线自动烧录失败后自动再触发次数

ST_PROG = bf.ST_PROGRAMMING
ST_DONE = bf.ST_DONE_WAIT
ST_FAILW = bf.ST_FAIL_WAIT

# 状态 -> LED颜色
S_NO_DEV, S_READY, S_PROG, S_OK, S_FAIL = range(5)
LED = {S_NO_DEV: "#808080", S_READY: "#18a0c8",
       S_PROG: "#e6b800", S_OK: "#28c840", S_FAIL: "#e03030"}
STATE_TXT = {S_NO_DEV: "设备未连接 (自动重连中…)",
             S_READY:  "就绪 - 点击 [烧 录] 开始",
             S_PROG:   "烧录中, 请勿拔线!",
             S_OK:     "烧录成功 ✓",
             S_FAIL:   "烧录失败 ✗"}


def _wav_bytes(segments, rate=16000, vol=0.4):
    """把 [(频率Hz, 毫秒), ...] 合成一段 16bit 单声道 WAV。
    相位跨段连续 -> 音调切换处无间隙/咔哒 (Beep 两连音"不连续"正源于相位跳变)。"""
    pcm = bytearray()
    phase = 0.0
    for freq, ms in segments:
        step = 2.0 * math.pi * freq / rate
        for _ in range(int(rate * ms / 1000)):
            pcm += struct.pack("<h", int(vol * math.sin(phase) * 32767))
            phase += step
    data = bytes(pcm)
    return (b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " +
            struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) +
            b"data" + struct.pack("<I", len(data)) + data)


def _play(segments):
    """后台线程播放提示音, 不阻塞 GUI 主线程 (winsound.Beep 同步阻塞会卡住界面)。
    注意: winsound 从内存播放 (SND_MEMORY) 不支持 SND_ASYNC, 必须在后台线程里
    同步播放; 波形相位跨段连续, 双音无咔哒。"""
    if not winsound:
        return

    def _run():
        try:
            winsound.PlaySound(_wav_bytes(segments), winsound.SND_MEMORY)
        except Exception:
            for f, ms in segments:       # 个别系统不支持内存 wav 时回退 Beep
                try:
                    winsound.Beep(f, ms)
                except Exception:
                    pass

    threading.Thread(target=_run, daemon=True).start()


def beep_ok():
    _play([(1568, 120), (2093, 180)])    # 上行双音: 成功 (连贯无咔哒)


def beep_fail():
    _play([(392, 350)])                  # 单低音: 失败


class Worker(threading.Thread):
    """串口状态机线程: 独占串口; GUI 经 queue 收事件, 经共享属性发指令"""

    def __init__(self, q):
        super().__init__(daemon=True)
        self.q = q
        self.stop_evt = threading.Event()
        self.lock = threading.Lock()
        self.state = S_NO_DEV
        self.fw = None                # dict: load/data/crc/name/trailer
        self.burn_req = False
        self.upload_req = False       # 把镜像写入烧录器 (离线模式用)
        self.upgrade_req = None       # 待执行的"升级烧录器"固件路径
        self.port = None              # 当前设备串口
        self.stream_mode = False      # 启用流水烧录
        self.clear_req = False        # 需要清除一次烧录器镜像
        self.last_seq = None          # 设备侧烧录尝试序号 (发现新结果用)
        self.host_on = None           # 设备回报的"上位机在线"标志
        self.dev = {}                 # 最近一次 GET_INFO
        self.dev_key = None           # 设备镜像信息变化去重
        self.t_status = 0.0
        self.t_reconn = 0.0
        self.reconn_tries = 0

    # ---- GUI 下发的指令 ----
    def set_firmware(self, fw):
        with self.lock:
            self.fw = fw

    def request_burn(self):
        with self.lock:
            self.burn_req = True

    def request_upload(self):
        with self.lock:
            self.upload_req = True

    def request_upgrade(self, path):
        with self.lock:
            self.upgrade_req = path

    def take_upgrade(self):
        with self.lock:
            r = self.upgrade_req
            self.upgrade_req = None
            return r

    def take_upload(self):
        with self.lock:
            r = self.upload_req
            self.upload_req = False
            return r

    def set_stream_mode(self, on):
        with self.lock:
            on = bool(on)
            if on and not self.stream_mode:
                # 由未选中 -> 选中: 清一次烧录器镜像, 防止设备自己烧旧固件
                self.clear_req = True
            self.stream_mode = on

    def snapshot(self):
        with self.lock:
            return self.fw, self.burn_req, self.stream_mode

    def take_burn(self):
        with self.lock:
            r = self.burn_req
            self.burn_req = False
            return r

    def take_clear(self):
        with self.lock:
            return self.clear_req

    def clear_done(self):
        with self.lock:
            self.clear_req = False

    # ---- 往 GUI 发事件 ----
    def emit(self, kind, **kw):
        self.q.put((kind, kw))

    def set_state(self, s):
        self.state = s
        self.emit("state", state=s)

    def ping_ok(self, app):
        try:
            return app.ping(1.0)
        except Exception:
            return False

    # ---- 目标探测: 读 IDCODE 当握手, 返回 idcode 或 None ----
    def detect(self, app):
        try:
            d = app.tgt_read(DETECT_ADDR, 4, timeout=1.5)
            return int.from_bytes(d, "little")
        except Exception:
            return None

    # ---- 轮询烧录器: 心跳 / 设备镜像信息 / 烧录结果回报 ----
    def poll_device(self, app, force=False):
        now = time.time()
        # 烧录中加快轮询让进度条更顺滑; 空闲时 STATUS_S 轮询兼作心跳
        interval = 0.15 if self.state == S_PROG else STATUS_S
        if not force and (now - self.t_status) < interval:
            return None
        self.t_status = now
        try:
            st = app.status(timeout=8.0)
        except Exception:
            if self.ping_ok(app):
                return None           # 烧录器忙 (正在烧录), 下一轮再查
            raise

        try:
            inf = app.info()
        except Exception:
            inf = None
        if inf is not None:
            self.dev = inf
            chip = inf.get("chip_code", 0xFFFFFFFF)
            key = (inf["valid"], inf["length"], inf["burn_count"], chip)
            if key != self.dev_key:
                self.dev_key = key
                self.emit("devinfo", valid=inf["valid"], length=inf["length"],
                          burn=inf["burn_count"], chip_code=chip)

        if st.get("host_online") is not None and st["host_online"] != self.host_on:
            self.host_on = st["host_online"]
            self.emit("host", on=bool(st["host_online"]))

        # 设备侧自动烧录 (插入目标板/烧录针触发): 把设备进度同步到界面进度条。
        # 上位机主动烧录 (run_offline/run_stream/run_upload) 各自已报进度,
        # 且它们执行期间不会调用到这里, 不会重复。
        if st["state"] == ST_PROG:
            if self.state != S_PROG:
                self.set_state(S_PROG)         # 黄灯 + "烧录中" + 禁用烧录按钮
            total = int((self.dev or {}).get("length") or 0)
            if total:
                self.emit("progress", done=st["progress"], total=total)

        seq = st.get("burn_seq")
        if seq is None:
            return st                  # 老固件没有尝试序号, 只能靠本地回报
        if self.last_seq is None:
            self.last_seq = seq        # 首次只做基线
        elif seq != self.last_seq and st["state"] in (ST_DONE, ST_FAILW):
            self.last_seq = seq
            ok = (st["result"] == 1)
            self.emit("result", ok=ok, code=(st["err"] & 0xFF),
                      done=st["progress"], seconds=0.0, device=True,
                      count=(inf or {}).get("burn_count"))
            self.set_state(S_OK if ok else S_FAIL)
        return st

    # ---- 流式烧录: 每次把电脑上的固件写进目标板 ----
    def program(self, app, load, data, crc):
        last = (12, 0)
        chip_emitted = False
        for attempt in (1, 2):
            if attempt > 1:
                self.emit("log", text="首次烧录未成功, 自动重试 1 次 ...")
                time.sleep(0.3)
            r = app.strm_begin(len(data), crc, load)
            if r["status"] != 0:
                last = (r["status"], 0)
                continue
            if not chip_emitted:
                chip_emitted = True
                chip = ""
                try:
                    for ln in app.logs().splitlines():
                        if ln.startswith("CHIP "):
                            chip = ln[5:].split(" PID=")[0]
                except Exception:
                    pass
                self.emit("chip", name=chip)
            off = 0
            while off < len(data):
                chunk = data[off:off + bf.TGT_CHUNK]
                got = app.strm_data(off, chunk)
                if got["status"] != 0:
                    break
                off += len(chunk)
                self.emit("progress", done=off, total=len(data))
                if self.stop_evt.is_set():
                    return 12, off
            else:
                r = app.strm_end()
                last = (r["status"], len(data))
                if last[0] == 0:
                    return last
                continue
            last = (got["status"], off)      # strm_data 失败
        return last

    def run_stream(self, app, fw):
        """流水烧录: 下载电脑上的固件到目标板 (尾部信息不烧录)"""
        load, data, crc = fw["load"], fw["data"], fw["crc"]
        self.set_state(S_PROG)
        self.emit("progress", done=0, total=len(data))
        seq0 = None
        try:
            seq0 = app.status(timeout=5.0).get("burn_seq")
        except Exception:
            pass
        t0 = time.time()
        code, done = self.program(app, load, data, crc)
        st = self.poll_device(app, force=True)
        seq1 = st.get("burn_seq") if st else None
        if seq0 is None or seq1 is None or seq0 == seq1:
            # 设备没有记录这次尝试 (老固件/参数被拒): 由上位机直接回报
            ok = (code == 0)
            self.emit("result", ok=ok, code=code, done=done,
                      seconds=time.time() - t0)
            self.set_state(S_OK if ok else S_FAIL)

    def run_upload(self, app, fw):
        """把镜像 (含尾部携带信息) 原样写入烧录器, 供离线模式使用"""
        img, trailer = fw["data"], fw["trailer"]
        payload = img + trailer
        cap = int((self.dev or {}).get("capacity") or 0)
        if cap and len(payload) > cap:
            self.emit("log", text="镜像 %d 字节超出烧录器镜像区 (%d 字节)"
                                  % (len(payload), cap))
            return
        self.set_state(S_PROG)
        self.emit("progress", done=0, total=len(img))
        t0 = time.time()
        name = fw["name"].encode("ascii", "replace").replace(b"?", b"_")
        app.img_begin(len(payload), fw["crc"], fw["load"], name, len(trailer))
        off = 0
        while off < len(payload):
            chunk = payload[off:off + bf.TGT_CHUNK]
            got = app.img_data(off, chunk)
            off += len(chunk)
            if got != off:
                raise RuntimeError("设备已接收 %d != 期望 %d" % (got, off))
            self.emit("progress", done=min(off, len(img)), total=len(img))
        app.img_end()
        self.emit("log", text="已写入烧录器: 镜像 %d 字节 + 尾部信息 %d 字节 (%.1fs)"
                              % (len(img), len(trailer), time.time() - t0))
        self.set_state(S_READY)

    def run_offline(self, app):
        """离线镜像烧录: 让烧录器用已装载的镜像烧一次"""
        inf = self.dev or {}
        if not inf.get("valid"):
            self.emit("log", text="烧录器内没有离线镜像, 请先装载固件 (U盘或工具写入)")
            return
        total = int(inf.get("length") or 1)
        self.set_state(S_PROG)
        self.emit("progress", done=0, total=total)
        app.prog_now()

        end = time.time() + BURN_WAIT_S
        st = None
        while time.time() < end and not self.stop_evt.is_set():
            try:
                st = app.status(timeout=20.0)
            except Exception:
                if not self.ping_ok(app):
                    raise             # 真的掉线了
                time.sleep(0.2)
                continue
            if st["state"] in (ST_DONE, ST_FAILW):
                break
            self.emit("progress", done=st["progress"], total=total)
            time.sleep(0.15)
        if st is None:
            self.emit("log", text="等待烧录结果超时")
            self.set_state(S_READY)
            return

        self.poll_device(app, force=True)
        if st.get("burn_seq") is None:   # 老固件: 本地回报
            ok = (st["result"] == 1)
            self.emit("result", ok=ok, code=(st["err"] & 0xFF),
                      done=st["progress"], seconds=0.0)
            self.set_state(S_OK if ok else S_FAIL)

    def run_upgrade(self, path):
        """升级烧录器应用 (走 boot 的 CDC 流程): 识别码/版本由 boot_flash 校验,
        do_flash 会自己让应用重启进 boot、写入、CRC 校验并启动新应用。"""
        self.emit("log", text="开始升级烧录器: %s" % os.path.basename(path))
        self.set_state(S_PROG)
        self.emit("progress", done=0, total=1)
        buf = io.StringIO()
        try:
            with contextlib.redirect_stdout(buf):
                ver = bf.do_flash(self.port, path)
        finally:
            for ln in buf.getvalue().replace("\r", "").splitlines():
                if ln.strip():
                    self.emit("log", text=ln.strip())
        self.emit("log", text="升级完成, 烧录器已重启 (版本 %s)" % (ver or "?"))

    def run(self):
        app = None
        presence = None
        self.set_state(S_NO_DEV)

        while not self.stop_evt.is_set():
            fw, burn, mode = self.snapshot()

            # ---------- 设备连接 / 自动重连 ----------
            if app is None:
                dev = bf.find_port(verbose=False)
                app = bf.ensure_app(dev, 3.0) if dev else None
                if app is None:
                    self.reconn_tries += 1
                    now = time.time()
                    if now - self.t_reconn >= 2.0:
                        self.t_reconn = now
                        self.emit("reconnect", attempt=self.reconn_tries)
                    time.sleep(RECONNECT_S)
                    continue
                self.reconn_tries = 0
                dev = app.dev
                self.port = dev
                try:
                    ver = app.version()
                except Exception:
                    ver = "?"
                self.last_seq = None
                self.host_on = None
                self.dev_key = None
                self.t_status = 0.0
                presence = None
                self.emit("device", port=dev, version=ver)
                self.emit("presence", idcode=None)
                self.set_state(S_READY)
                time.sleep(SETTLE_S)      # 等供电稳定再握手
                continue

            try:
                # ---------- 升级烧录器 (子窗口 [升级烧录器] 触发) ----------
                up = self.take_upgrade()
                if up:
                    try:
                        app.close()
                    except Exception:
                        pass
                    try:
                        self.run_upgrade(up)
                    except Exception as e:
                        self.emit("log", text="升级失败: %s" % e)
                    self.take_burn()
                    self.last_seq = None
                    self.dev_key = None
                    self.host_on = None
                    self.t_status = 0.0
                    app = None
                    self.port = None
                    presence = None
                    self.emit("device", port=None, version=None)
                    self.emit("presence", idcode=None)
                    self.emit("host", on=False)
                    self.set_state(S_NO_DEV)
                    time.sleep(SETTLE_S)
                    continue

                # ---------- 勾选流水烧录时, 清一次烧录器镜像 ----------
                if self.take_clear() and self.state != S_PROG:
                    try:
                        app.img_clear()
                        self.clear_done()
                        self.emit("log", text="已清除烧录器离线镜像 (流水烧录就绪)")
                        self.emit("devinfo", valid=0, length=0, burn=0)
                        self.last_seq = None
                        time.sleep(0.1)
                    except Exception as e:
                        if not self.ping_ok(app):
                            raise         # 掉线, 走重连 (clear_req 保留)
                        self.emit("log", text="清除镜像暂未成功 (%s), 稍后重试" % e)
                        time.sleep(1.0)
                        continue

                # ---------- 轮询: 心跳 / 设备信息 / 烧录结果 ----------
                self.poll_device(app)

                # ---------- 写入烧录器 (离线模式用) ----------
                if self.take_upload() and self.state != S_PROG:
                    if fw is None:
                        self.emit("log", text="请先选择固件文件")
                    else:
                        try:
                            self.run_upload(app, fw)
                        except Exception as e:
                            if not self.ping_ok(app):
                                raise         # 掉线, 走重连
                            self.emit("log", text="写入烧录器失败: %s" % e)
                            self.set_state(S_READY)
                    continue

                # ---------- 点击触发烧录 ----------
                if burn and self.state != S_PROG:
                    self.take_burn()
                    if mode:
                        if fw is None:
                            self.emit("log", text="流水烧录需要先选择固件文件")
                        else:
                            self.run_stream(app, fw)
                    else:
                        self.run_offline(app)
                    continue

                # ---------- 空闲: 轮询目标是否就位 (仅显示) ----------
                if self.state in (S_READY, S_OK, S_FAIL):
                    idcode = self.detect(app)
                    if idcode != presence:
                        presence = idcode
                        self.emit("presence", idcode=idcode)
                time.sleep(POLL_S)

            except Exception as e:
                # 串口断开/设备掉线 (目标板接入瞬间拉垮供电很常见)。
                # 烧录中途掉线不计数, 自动重连后继续即可。
                if self.state == S_PROG:
                    self.emit("log", text="烧录中途 USB 掉线 (供电跌落?), "
                                          "已自动重连, 请重新点击 [烧 录]")
                else:
                    self.emit("log", text="设备连接中断: %s" % e)
                try:
                    app.close()
                except Exception:
                    pass
                app = None
                self.port = None
                presence = None
                self.take_burn()          # 掉线时丢弃未执行的烧录请求
                self.emit("device", port=None, version=None)
                self.emit("presence", idcode=None)
                self.emit("host", on=False)
                self.set_state(S_NO_DEV)
                time.sleep(RECONNECT_S)

        if app is not None:
            try:
                app.close()
            except Exception:
                pass


class MakeAesbinDialog(tk.Toplevel):
    """生产固件发送方用的制作窗口: 可放两段固件, 合并后 AES 加密打包"""

    def __init__(self, master, on_upgrade=None):
        super().__init__(master)
        self.title("制作专用烧录文件 (.aesbin)")
        self.transient(master)
        self.grab_set()
        self.resizable(False, False)
        self.on_upgrade = on_upgrade     # 升级烧录器回调 (传入选定的固件路径)
        self.slots = [dict(path=None, data=None, is_hex=False),
                      dict(path=None, data=None, is_hex=False)]
        self._rows = []
        self._build()

    def _build(self):
        tk.Label(self, text="把固件打包成 AES 加密的 .aesbin (尾部附带目标芯片代码)",
                 fg="#204080").pack(anchor="w", padx=12, pady=(10, 6))

        for i in range(2):
            fr = tk.Frame(self)
            fr.pack(fill="x", padx=12, pady=3)
            tk.Label(fr, text="文件 %d:" % i, width=7, anchor="w").pack(side="left")
            tk.Button(fr, text="打开...", width=8,
                      command=lambda i=i: self._open(i)).pack(side="left")
            lb = tk.Label(fr, text="未选择", width=34, anchor="w", fg="#606060")
            lb.pack(side="left", padx=6)
            tk.Label(fr, text="偏移(hex)").pack(side="left")
            var = tk.StringVar(value="0x08000000")
            ent = tk.Entry(fr, textvariable=var, width=12)
            ent.pack(side="left", padx=(4, 0))
            self._rows.append(dict(lb=lb, var=var, ent=ent))

        cf = tk.Frame(self)
        cf.pack(fill="x", padx=12, pady=(8, 2))
        tk.Label(cf, text="目标芯片:").pack(side="left")
        vals = ["0x%08X  %s" % (c, aesbin.chip_name(c))
                for c in aesbin.CHIP_CHOICES]
        self.chip_var = tk.StringVar(value=vals[0])
        ttk.Combobox(cf, values=vals, textvariable=self.chip_var,
                     state="readonly", width=30).pack(side="left", padx=6)

        tk.Label(self, text="说明: hex 文件按文件内地址自动识别偏移 (不可改); "
                            "bin 文件可手动填写偏移; 两段地址不能重叠, 空隙填 0xFF。",
                 fg="#606060", justify="left",
                 wraplength=520).pack(anchor="w", padx=12, pady=(6, 4))

        bar = tk.Frame(self)
        bar.pack(fill="x", padx=12, pady=(4, 12))
        tk.Button(bar, text="升级烧录器", width=12,
                  command=self._upgrade).pack(side="left")
        tk.Button(bar, text="生成 .aesbin", width=14,
                  command=self._generate).pack(side="right")
        tk.Button(bar, text="关闭", width=8,
                  command=self.destroy).pack(side="right", padx=8)

    def _upgrade(self):
        """选择应用固件并交给主窗口的 Worker 执行升级 (识别码/版本由升级流程校验)"""
        if self.on_upgrade is None:
            return
        p = filedialog.askopenfilename(
            title="选择烧录器应用固件 (升级用)",
            filetypes=[("应用固件", "*.bin *.hex *.ihx"), ("所有文件", "*.*")])
        if not p:
            return
        if not messagebox.askyesno(
                "确认升级",
                "将把\n%s\n升级到烧录器的 APP 区。\n\n"
                "升级前会校验固件识别码与版本: 识别码不符、或版本相同/更低会被拒绝"
                " (版本 0 为调试版, 可覆盖任何版本)。\n"
                "升级期间请勿断开 USB, 完成后烧录器会自动重启。\n\n继续?"
                % os.path.basename(p)):
            return
        self.destroy()                   # 关掉子窗口, 便于查看主窗口日志
        self.on_upgrade(p)

    def _open(self, i):
        p = filedialog.askopenfilename(
            title="选择固件 %d" % i,
            filetypes=[("固件", "*.bin *.hex *.ihx"), ("所有文件", "*.*")])
        if not p:
            return
        if aesbin.is_aesbin(p):
            messagebox.showwarning("提示", "请选择原始固件 (.bin/.hex)")
            return
        try:
            base, data = bf.load_image(p)
            if not data:
                raise ValueError("固件为空")
        except Exception as e:
            messagebox.showerror("固件无效", str(e))
            return
        is_hex = p.lower().endswith((".hex", ".ihx"))
        self.slots[i] = dict(path=p, data=data, is_hex=is_hex)
        self._rows[i]["var"].set("0x%08X" % base)
        self._rows[i]["ent"].config(state="disabled" if is_hex else "normal")
        self._rows[i]["lb"].config(
            text="%s  %d 字节%s" % (os.path.basename(p), len(data),
                                    "  [hex 自动识别地址]" if is_hex
                                    else "  [bin 可改偏移]"),
            fg="#000000")

    def _offset(self, i):
        txt = self._rows[i]["var"].get().strip()
        try:
            v = int(txt, 16)
        except ValueError:
            messagebox.showerror("偏移无效",
                                 "文件 %d 的偏移请用十六进制, 例如 0x08000000" % i)
            return None
        if (v & 0xFF000000) != 0x08000000:
            messagebox.showerror("偏移无效", "偏移需在 0x08000000~0x08FFFFFF 之间")
            return None
        return v

    def _generate(self):
        items = []
        for i in range(2):
            if self.slots[i]["data"] is None:
                continue
            off = self._offset(i)
            if off is None:
                return
            items.append((off, self.slots[i]["data"]))
        if not items:
            messagebox.showwarning("提示", "至少打开一个固件文件")
            return
        try:
            base, merged = aesbin.merge_images(items)
        except Exception as e:
            messagebox.showerror("镜像无效", str(e))
            return
        code = int(self.chip_var.get().split()[0], 16)
        name = os.path.splitext(
            os.path.basename(self.slots[0]["path"] or "firmware"))[0]
        out = filedialog.asksaveasfilename(
            title="保存专用烧录文件", defaultextension=".aesbin",
            initialfile=name + ".aesbin",
            filetypes=[("专用烧录文件", "*.aesbin")])
        if not out:
            return
        try:
            r = aesbin.make_file(out, merged, code, base)
        except Exception as e:
            messagebox.showerror("打包失败", str(e))
            return
        messagebox.showinfo(
            "完成",
            "已生成: %s\n\n镜像范围: 0x%08X ~ 0x%08X (%d 字节)\n"
            "目标芯片: %s\n文件大小: %d 字节"
            % (out, base, base + len(merged), len(merged),
               aesbin.chip_name(code), r["size"]))


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("联机烧录 (生产版)")
        self.resizable(False, False)
        self.q = queue.Queue()
        self.worker = Worker(self.q)
        self.ok_cnt = 0
        self.fail_cnt = 0
        self.dev_online = False
        self.burning = False
        self.dev = {}
        self.dev_burn = None          # 设备累计次数 (基线/最新)
        self.ok_base = 0              # 成功计数基线 (设备计数差值)
        self._build()
        self.worker.start()
        self.after(100, self._poll)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ---------------- UI ----------------
    def _build(self):
        pad = dict(padx=8, pady=4)

        top = tk.Frame(self)
        top.pack(fill="x", **pad)
        self.lb_dev = tk.Label(top, text="设备: 未连接 (自动重连中…)", anchor="w",
                               fg="#a00000")
        self.lb_dev.pack(side="left", fill="x", expand=True)
        self.lb_host = tk.Label(top, text="", anchor="e", fg="#606060")
        self.lb_host.pack(side="right")

        fwf = tk.Frame(self)
        fwf.pack(fill="x", **pad)
        self.btn_fw = tk.Button(fwf, text="选择固件...", width=11,
                                command=self._choose_fw)
        self.btn_fw.pack(side="left")
        self.btn_upload = tk.Button(fwf, text="写入烧录器", width=11,
                                    state="disabled", command=self._upload)
        self.btn_upload.pack(side="left", padx=(6, 0))
        self.stream_var = tk.BooleanVar(value=False)
        self.cb_stream = tk.Checkbutton(fwf, text="启用流水烧录",
                                        variable=self.stream_var,
                                        command=self._on_stream_toggle)
        self.cb_stream.pack(side="right")

        self.lb_fw = tk.Label(self, text="未选择 (支持 .bin / .hex / .aesbin)",
                              anchor="w", fg="#606060")
        self.lb_fw.pack(fill="x", padx=8)

        self.lb_mode = tk.Label(self, text="", anchor="w", fg="#204080")
        self.lb_mode.pack(fill="x", padx=8)

        mid = tk.Frame(self)
        mid.pack(fill="x", **pad)
        self.cv = tk.Canvas(mid, width=110, height=110, highlightthickness=0)
        self.cv.pack(side="left", padx=6)
        self.led = self.cv.create_oval(15, 15, 95, 95, fill=LED[S_NO_DEV],
                                       outline="#404040", width=2)
        right = tk.Frame(mid)
        right.pack(side="left", fill="both", expand=True)
        self.lb_state = tk.Label(right, text="未连接", anchor="w",
                                 font=("Microsoft YaHei UI", 13, "bold"))
        self.lb_state.pack(fill="x", pady=(4, 2))
        self.lb_chip = tk.Label(right, text="目标板: 未检测到", anchor="w",
                                fg="#404040")
        self.lb_chip.pack(fill="x")
        self.pb = ttk.Progressbar(right, length=340, mode="determinate")
        self.pb.pack(fill="x", pady=(10, 2))
        self.lb_prog = tk.Label(right, text="", anchor="w", fg="#404040")
        self.lb_prog.pack(fill="x")
        self.btn_burn = tk.Button(right, text="烧  录", width=14,
                                  state="disabled", command=self._burn,
                                  font=("Microsoft YaHei UI", 12, "bold"),
                                  bg="#2050a0", fg="white",
                                  activebackground="#184080")
        self.btn_burn.pack(anchor="e", pady=(8, 0))

        cnt = tk.Frame(self)
        cnt.pack(fill="x", **pad)
        self.lb_ok = tk.Label(cnt, text="成功: 0", fg="#108018",
                              font=("Microsoft YaHei UI", 16, "bold"))
        self.lb_ok.pack(side="left", padx=10)
        self.lb_fail = tk.Label(cnt, text="失败: 0", fg="#b01010",
                                font=("Microsoft YaHei UI", 16, "bold"))
        self.lb_fail.pack(side="left", padx=30)
        self.lb_devimg = tk.Label(cnt, text="设备累计: -", fg="#404040")
        self.lb_devimg.pack(side="left", padx=15)
        tk.Button(cnt, text="计数清零", command=self._clear_cnt).pack(side="right")

        self.log = tk.Text(self, width=64, height=10, state="disabled",
                           font=("Consolas", 9))
        self.log.pack(padx=8, pady=(4, 8))

        # 角落: 生产方使用的专用烧录文件制作入口
        bot = tk.Frame(self)
        bot.pack(fill="x", padx=8, pady=(0, 6))
        tk.Button(bot, text="制作专用文件", width=13,
                  command=self._open_maker).pack(side="right")

        self._refresh_mode()

    # ---------------- 事件 ----------------
    def _choose_fw(self):
        p = filedialog.askopenfilename(
            title="选择要烧录的固件",
            filetypes=[("固件 / 专用文件", "*.bin *.hex *.ihx *.aesbin"),
                       ("专用烧录文件", "*.aesbin"), ("所有文件", "*.*")])
        if not p:
            return
        try:
            load, data, meta = bf.load_image_ex(p)
            if not data:
                raise ValueError("固件为空")
            if (load & 0xFF000000) != 0x08000000:
                raise ValueError("起始地址 0x%08X 不在 0x08xxxxxx" % load)
        except Exception as e:
            self._log("固件无效: %s" % e)
            self.lb_fw.config(text="固件无效: %s" % e, fg="#a00000")
            return
        crc = zlib.crc32(data) & 0xFFFFFFFF
        self.worker.set_firmware(dict(load=load, data=data, crc=crc,
                                      name=os.path.basename(p),
                                      trailer=meta["trailer"]))
        txt = ("%s  %d 字节  CRC32=%08X  @0x%08X"
               % (os.path.basename(p), len(data), crc, load))
        notes = []
        if meta["fmt"] == "aesbin":
            notes.append("aesbin 已解密")
        if meta["chip_code"] is not None:
            notes.append("芯片: %s" % aesbin.chip_name(meta["chip_code"]))
        if notes:
            txt += "  [" + ", ".join(notes) + "]"
        self.lb_fw.config(text=txt, fg="#000000")
        self._log("已载入%s %s (%d 字节)"
                  % ("专用烧录文件" if meta["fmt"] == "aesbin" else "固件",
                     os.path.basename(p), len(data)))
        self._update_burn_btn()

    def _open_maker(self):
        MakeAesbinDialog(self, self._request_upgrade)

    def _request_upgrade(self, path):
        if not self.dev_online:
            messagebox.showwarning("未连接", "烧录器未连接, 无法升级")
            return
        self._log("请求升级烧录器: %s" % os.path.basename(path))
        self.worker.request_upgrade(path)

    def _upload(self):
        self.worker.request_upload()

    def _on_stream_toggle(self):
        on = bool(self.stream_var.get())
        self.worker.set_stream_mode(on)
        if on:
            self._log("启用流水烧录: 将清除烧录器离线镜像, 之后每次从电脑固件流式烧录")
        else:
            self._log("关闭流水烧录: 烧录时使用烧录器内已装载的离线镜像")
        self._refresh_mode()
        self._update_burn_btn()

    def _refresh_mode(self):
        if self.stream_var.get():
            self.lb_mode.config(
                text="模式: 流水烧录 — 点击 [烧 录] 把电脑固件写入目标板")
        else:
            self.lb_mode.config(
                text="模式: 烧录器离线镜像 — 插入目标板自动烧录, 也可点击 [烧 录] 重烧")

    def _burn(self):
        self.worker.request_burn()

    def _update_burn_btn(self):
        if self.burning or not self.dev_online:
            ok = up = False
        elif self.stream_var.get():
            ok = self.worker.fw is not None
            up = False                  # 流水烧录不需要预先写入烧录器
        else:
            ok = bool(self.dev.get("valid"))
            up = self.worker.fw is not None
        self.btn_burn.config(state="normal" if ok else "disabled")
        self.btn_upload.config(state="normal" if up else "disabled")

    def _clear_cnt(self):
        self.fail_cnt = 0
        self.ok_base = self.dev_burn if self.dev_burn is not None else 0
        self.ok_cnt = 0
        self.lb_ok.config(text="成功: 0")
        self.lb_fail.config(text="失败: 0")
        self._log("计数已清零 (设备累计 %d 次保持不变)"
                  % (self.dev_burn if self.dev_burn is not None else 0))

    def _log(self, text):
        self.log.config(state="normal")
        self.log.insert("end", "[%s] %s\n" % (time.strftime("%H:%M:%S"), text))
        self.log.see("end")
        if int(self.log.index("end-1c").split(".")[0]) > 200:
            self.log.delete("1.0", "100.0")
        self.log.config(state="disabled")

    def _set_led(self, state):
        self.cv.itemconfig(self.led, fill=LED[state])

    def _update_devimg(self):
        burn = self.dev_burn if self.dev_burn is not None else 0
        if self.dev.get("valid"):
            txt = "设备累计: %d 次 (离线镜像 %d 字节" % (
                burn, self.dev.get("length", 0))
            chip = self.dev.get("chip_code", 0xFFFFFFFF)
            if chip != 0xFFFFFFFF:
                txt += ", 芯片 %s" % aesbin.chip_name(chip)
            self.lb_devimg.config(text=txt + ")")
        else:
            self.lb_devimg.config(text="设备累计: %d 次 (无离线镜像)" % burn)

    # ---------------- 队列轮询 ----------------
    def _poll(self):
        try:
            while True:
                kind, kw = self.q.get_nowait()
                if kind == "state":
                    s = kw["state"]
                    self._set_led(s)
                    self.lb_state.config(text=STATE_TXT[s])
                    self.burning = (s == S_PROG)
                    if s == S_PROG:
                        self.lb_prog.config(text="")
                    self._update_burn_btn()
                elif kind == "device":
                    self.dev_online = bool(kw["port"])
                    if kw["port"]:
                        self.lb_dev.config(text="设备: %s 已连接  固件 %s"
                                           % (kw["port"], kw["version"]),
                                           fg="#006000")
                        self._log("烧录器已连接: %s (%s)"
                                  % (kw["port"], kw["version"]))
                    else:
                        self.lb_dev.config(text="设备: 未连接 (自动重连中…)",
                                           fg="#a00000")
                        self.lb_host.config(text="")
                        self.dev = {}
                        self._update_devimg()
                    self._update_burn_btn()
                elif kind == "reconnect":
                    self.lb_dev.config(
                        text="设备: 未连接 (自动重连中… 第 %d 次)" % kw["attempt"],
                        fg="#a00000")
                elif kind == "host":
                    if kw["on"]:
                        self.lb_host.config(text="烧录器已确认联机", fg="#006000")
                    else:
                        self.lb_host.config(text="烧录器未联机", fg="#a06000")
                elif kind == "devinfo":
                    cnt = kw.get("burn") or 0
                    self.dev = dict(valid=kw.get("valid"),
                                    length=kw.get("length") or 0,
                                    chip_code=kw.get("chip_code", 0xFFFFFFFF))
                    if self.dev_burn is None:
                        self.dev_burn = cnt
                        self.ok_base = cnt        # 首次连接: 从现在开始计
                    elif cnt < self.dev_burn:     # 设备计数被重置 (换镜像/清除)
                        self._log("设备计数已重置为 %d" % cnt)
                        self.dev_burn = cnt
                        self.ok_base = cnt
                    else:
                        self.dev_burn = cnt
                    self.ok_cnt = self.dev_burn - self.ok_base
                    self.lb_ok.config(text="成功: %d" % self.ok_cnt)
                    self._update_devimg()
                    self._update_burn_btn()
                elif kind == "presence":
                    if kw["idcode"] is not None:
                        self.lb_chip.config(text="目标板: 已就位  IDCODE=0x%08X"
                                            % kw["idcode"], fg="#006000")
                    else:
                        self.lb_chip.config(text="目标板: 未检测到",
                                            fg="#a06000")
                elif kind == "chip":
                    if kw["name"]:
                        self.lb_chip.config(
                            text=self.lb_chip.cget("text") + "  (%s)" % kw["name"])
                        self._log("芯片识别: %s" % kw["name"])
                elif kind == "progress":
                    done, total = kw["done"], kw["total"]
                    pct = (done * 100 // total) if total else 0
                    self.pb["value"] = pct
                    self.lb_prog.config(text="%d / %d 字节  (%d%%)"
                                        % (done, total, pct))
                elif kind == "result":
                    if kw["ok"]:
                        beep_ok()
                        self._log("烧录成功, 耗时 %.1fs" % kw.get("seconds", 0))
                    else:
                        self.fail_cnt += 1
                        self.lb_fail.config(text="失败: %d" % self.fail_cnt)
                        self._log("烧录失败: code=%d(%s) 已写入 %d 字节"
                                  % (kw["code"],
                                     bf.TP_NAMES.get(kw["code"], "?"),
                                     kw.get("done", 0)))
                        beep_fail()
                elif kind == "log":
                    self._log(kw["text"])
        except queue.Empty:
            pass
        self.after(100, self._poll)

    def _on_close(self):
        self.worker.stop_evt.set()
        self.after(200, self.destroy)


if __name__ == "__main__":
    App().mainloop()
