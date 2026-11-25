#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
联机流式烧录上位机 (纯上位机实现, 不依赖任何主板固件改动)

工作方式 (手动触发, 适合插板瞬间有供电浪涌的场合, 如电机控制板):
    1. 自动连接烧录器 (USB CDC 0483:5740/5742); 空闲时每 3 秒巡检并清除机内
       脱机镜像, 防止主板自动烧录机制与上位机抢 SWD
    2. 后台每秒 4 次 SWD 握手 (读目标 IDCODE), 仅用于在面板上显示
       "目标板是否就位/芯片型号", 不会自动烧录
    3. 操作员插好目标板, 点击 [烧 录] 按钮 -> 流式烧录 -> 回读 CRC 校验
    4. 成功: 绿灯 + "嘟"提示音 + 成功计数; 失败: 红灯 + 低音 + 失败计数;
       烧录失败自动重试 1 次 (目标上电首次会话偶发擦除超时)
    5. 烧录中途 USB 掉线 (供电跌落): 不计失败, 自动重连, 重新点击即可

运行: 双击 online_prog.py 或  python online_prog.py
依赖: Python3 + pyserial + tkinter(自带), 复用同目录 boot_flash.py 的协议实现
"""
import os
import queue
import threading
import time
import tkinter as tk
from tkinter import filedialog, ttk
import zlib

import boot_flash as bf

try:
    import winsound
except ImportError:
    winsound = None

DETECT_ADDR = 0xE0042000          # 目标 DBGMCU_IDCODE, 用读它当 SWD 握手
POLL_S = 0.25                     # 握手轮询间隔 (每秒 4 次)
PATROL_S = 3.0                    # 空闲巡检镜像间隔
RECONNECT_S = 1.5                 # 掉线重连间隔
SETTLE_S = 0.5                    # 重连后等供电稳定再握手

# 状态 -> LED颜色
S_NO_DEV, S_READY, S_PROG, S_OK, S_FAIL = range(5)
LED = {S_NO_DEV: "#808080", S_READY: "#18a0c8",
       S_PROG: "#e6b800", S_OK: "#28c840", S_FAIL: "#e03030"}
STATE_TXT = {S_NO_DEV: "设备未连接",
             S_READY:  "就绪 - 点击 [烧 录] 开始",
             S_PROG:   "烧录中, 请勿拔线!",
             S_OK:     "烧录成功 ✓",
             S_FAIL:   "烧录失败 ✗"}


def beep_ok():
    if winsound:
        winsound.Beep(1568, 120)
        winsound.Beep(2093, 180)


def beep_fail():
    if winsound:
        winsound.Beep(392, 350)


class Worker(threading.Thread):
    """串口状态机线程: 独占串口; GUI 经 queue 收事件, 经共享属性发指令"""

    def __init__(self, q):
        super().__init__(daemon=True)
        self.q = q
        self.stop_evt = threading.Event()
        self.lock = threading.Lock()
        self.fw = None                # (load_addr, data, crc32, 文件名)
        self.burn_req = False

    # ---- GUI 下发的指令 ----
    def set_firmware(self, fw):
        with self.lock:
            self.fw = fw

    def request_burn(self):
        with self.lock:
            self.burn_req = True

    def snapshot(self):
        with self.lock:
            fw, burn = self.fw, self.burn_req
            self.burn_req = False
            return fw, burn

    # ---- 往 GUI 发事件 ----
    def emit(self, kind, **kw):
        self.q.put((kind, kw))

    # ---- 目标探测: 读 IDCODE 当握手, 返回 idcode 或 None ----
    def detect(self, app):
        try:
            d = app.tgt_read(DETECT_ADDR, 4, timeout=2.0)
            return int.from_bytes(d, "little")
        except Exception:
            return None

    # ---- 烧录一整块目标 (失败自动重试 1 次: 目标刚上电的首次会话偶发
    #      擦除超时, 实测第二次即成功; 重试对产线更友好) ----
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

    def run(self):
        app = None
        state = S_NO_DEV
        last_patrol = 0.0
        presence = None               # 最近一次握手结果 (None=无目标)
        self.emit("state", state=state)

        while not self.stop_evt.is_set():
            fw, burn = self.snapshot()

            # ---------- 设备连接 ----------
            if app is None:
                dev = bf.find_port(verbose=False)
                if dev:
                    app = bf.try_app(dev, 1.0)
                if app is None:
                    time.sleep(RECONNECT_S)
                    continue
                try:
                    ver = app.version()
                except Exception:
                    ver = "?"
                self.emit("device", port=dev, version=ver)
                state = S_READY
                presence = None
                self.emit("state", state=state)
                time.sleep(SETTLE_S)    # 等供电稳定再恢复握手
                continue

            try:
                # ---------- 巡检: 清掉脱机镜像, 防止主板自动烧录抢 SWD ----
                if state != S_PROG and time.time() - last_patrol > PATROL_S:
                    last_patrol = time.time()
                    try:
                        m = app.info()
                        if m["valid"]:
                            app.img_clear()
                            self.emit("log", text="已清除机内脱机镜像 (%d 字节), "
                                                  "主板自动烧录保持静默" % m["length"])
                    except Exception:
                        pass

                # ---------- 点击触发烧录 ----------
                if burn and state != S_PROG:
                    if fw is None:
                        self.emit("log", text="尚未选择固件, 无法烧录")
                    elif self.detect(app) is None:
                        self.emit("log", text="未检测到目标板, 请接好后再点击")
                    else:
                        state = S_PROG
                        self.emit("state", state=state)
                        self.emit("progress", done=0, total=len(fw[1]))
                        t0 = time.time()
                        code, done = self.program(app, fw[0], fw[1], fw[2])
                        dt = time.time() - t0
                        if code == 0:
                            state = S_OK
                            self.emit("state", state=state)
                            self.emit("result", ok=True, seconds=dt)
                            beep_ok()
                        else:
                            state = S_FAIL
                            self.emit("state", state=state)
                            self.emit("result", ok=False, code=code,
                                      done=done, seconds=dt)
                            beep_fail()
                    continue

                # ---------- 空闲: 轮询目标是否就位 (仅显示, 不自动烧) ----
                if state in (S_READY, S_OK, S_FAIL):
                    idcode = self.detect(app)
                    if idcode != presence:
                        presence = idcode
                        self.emit("presence", idcode=idcode)
                time.sleep(POLL_S)

            except Exception as e:
                # 串口断开/设备掉线 (如目标板接入瞬间拉垮供电)。
                # 烧录中途掉线不算目标板失败: 不计数不报警, 重连后重新点击即可
                if state == S_PROG:
                    self.emit("log", text="烧录中途 USB 掉线 (供电跌落?), "
                                          "已自动重连, 请重新点击 [烧 录]")
                    self.emit("state", state=S_FAIL)
                    self.emit("result", ok=False, code=1, done=0,
                              seconds=0, count=False)
                else:
                    self.emit("log", text="设备连接中断: %s" % e)
                try:
                    app.close()
                except Exception:
                    pass
                app = None
                state = S_NO_DEV
                presence = None
                self.emit("state", state=state)
                self.emit("device", port=None, version=None)
                self.emit("presence", idcode=None)
                time.sleep(RECONNECT_S)

        if app is not None:
            try:
                app.close()
            except Exception:
                pass


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("联机流式烧录")
        self.resizable(False, False)
        self.q = queue.Queue()
        self.worker = Worker(self.q)
        self.ok_cnt = 0
        self.fail_cnt = 0
        self.dev_online = False
        self.burning = False
        self._build()
        self.worker.start()
        self.after(100, self._poll)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ---------------- UI ----------------
    def _build(self):
        pad = dict(padx=8, pady=4)

        top = tk.Frame(self)
        top.pack(fill="x", **pad)
        self.lb_dev = tk.Label(top, text="设备: 未连接 (等待插入 USB)", anchor="w",
                               fg="#a00000")
        self.lb_dev.pack(side="left", fill="x", expand=True)

        fwf = tk.Frame(self)
        fwf.pack(fill="x", **pad)
        self.btn_fw = tk.Button(fwf, text="选择固件...", width=12,
                                command=self._choose_fw)
        self.btn_fw.pack(side="left")
        self.lb_fw = tk.Label(fwf, text="未选择 (支持 .bin / .hex)", anchor="w",
                              fg="#606060")
        self.lb_fw.pack(side="left", padx=8)

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
        tk.Button(cnt, text="计数清零", command=self._clear_cnt).pack(side="right")

        self.log = tk.Text(self, width=64, height=10, state="disabled",
                           font=("Consolas", 9))
        self.log.pack(padx=8, pady=(4, 8))

    # ---------------- 事件 ----------------
    def _choose_fw(self):
        p = filedialog.askopenfilename(
            title="选择要烧录的固件",
            filetypes=[("固件", "*.bin *.hex *.ihx"), ("所有文件", "*.*")])
        if not p:
            return
        try:
            load, data = bf.load_image(p)
            if not data:
                raise ValueError("固件为空")
            if (load & 0xFF000000) != 0x08000000:
                raise ValueError("起始地址 0x%08X 不在 0x08xxxxxx" % load)
        except Exception as e:
            self._log("固件无效: %s" % e)
            self.lb_fw.config(text="固件无效: %s" % e, fg="#a00000")
            return
        crc = zlib.crc32(data) & 0xFFFFFFFF
        self.worker.set_firmware((load, data, crc, os.path.basename(p)))
        self.lb_fw.config(text="%s  %d 字节  CRC32=%08X  @0x%08X"
                               % (os.path.basename(p), len(data), crc, load),
                          fg="#000000")
        self._log("已载入固件 %s (%d 字节)" % (os.path.basename(p), len(data)))
        self._update_burn_btn()

    def _burn(self):
        self.worker.request_burn()

    def _update_burn_btn(self):
        has_fw = self.worker.fw is not None
        ok = has_fw and self.dev_online and not self.burning
        self.btn_burn.config(state="normal" if ok else "disabled")

    def _clear_cnt(self):
        self.ok_cnt = self.fail_cnt = 0
        self.lb_ok.config(text="成功: 0")
        self.lb_fail.config(text="失败: 0")

    def _log(self, text):
        self.log.config(state="normal")
        self.log.insert("end", "[%s] %s\n" % (time.strftime("%H:%M:%S"), text))
        self.log.see("end")
        if int(self.log.index("end-1c").split(".")[0]) > 200:
            self.log.delete("1.0", "100.0")
        self.log.config(state="disabled")

    def _set_led(self, state):
        self.cv.itemconfig(self.led, fill=LED[state])

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
                        self.lb_dev.config(text="设备: 未连接 (等待插入 USB)",
                                           fg="#a00000")
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
                        self.ok_cnt += 1
                        self.lb_ok.config(text="成功: %d" % self.ok_cnt)
                        self._log("烧录成功, 耗时 %.1fs, 累计成功 %d 块"
                                  % (kw["seconds"], self.ok_cnt))
                    elif kw.get("count", True):
                        self.fail_cnt += 1
                        self.lb_fail.config(text="失败: %d" % self.fail_cnt)
                        self._log("烧录失败: code=%d(%s) 已写入 %d 字节"
                                  % (kw["code"],
                                     bf.TP_NAMES.get(kw["code"], "?"),
                                     kw["done"]))
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
