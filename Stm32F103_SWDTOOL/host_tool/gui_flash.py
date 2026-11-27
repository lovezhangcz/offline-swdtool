#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
离线烧录器 图形上位机 (带日志)

运行:
    python gui_flash.py

功能:
    串口选择/连接、设备信息、SWD 探测、读取设备日志
    固件支持 .bin 与 .hex (HEX 由上位机自动转成 BIN 后下发)
    离线烧录 (tgt, 固件 <=36KB, 先存本机镜像区再烧目标)
    流式烧录 (strm, 大小只受目标 Flash 容量限制)
    整片擦除目标芯片 (带确认, 擦完校验首末字为空白)
    清除烧录器固件 (删除板内镜像, 之后不再自动烧录)
    U 盘模式烧录 (需主板运行 APP_MSC)
    升级主板应用 (走 boot 的 CDC 升级通道, 校验固件识别码与版本)

所有操作输出与设备日志都带时间戳显示在窗口里, 并同步写入 logs/ 目录。
"""
import datetime
import os
import queue
import sys
import threading
import traceback
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

try:
    import boot_flash as bf
except Exception as _e:                      # pyserial 缺失等
    tk.Tk().withdraw()
    from tkinter import messagebox
    messagebox.showerror("依赖缺失", "加载 boot_flash.py 失败:\n%s\n\n请先: pip install pyserial" % _e)
    raise SystemExit(1)

try:                                        # 复用 online 上位机的制作窗口
    from online_prog import MakeAesbinDialog
except Exception:
    MakeAesbinDialog = None

import aesbin

ROOT = os.path.dirname(os.path.abspath(__file__))
LOG_DIR = os.path.join(ROOT, "logs")
DEF_BIN_DIR = os.path.join(ROOT, "..", "testprj", "project", "obj")
IMG_MAX = 31 * 1024                          # 镜像区可用上限 (32KB - 1KB 头页)


def ts():
    return datetime.datetime.now().strftime("%H:%M:%S")


class _Redirect:
    """把 print 输出按行推到界面队列 (带时间戳)"""
    def __init__(self, q):
        self.q = q

    def write(self, s):
        if not s:
            return
        for ln in s.replace("\r", "\n").split("\n"):
            ln = ln.strip()
            if ln:
                self.q.put("%s %s\n" % (ts(), ln))

    def flush(self):
        pass


class FlashGui:
    def __init__(self, root):
        self.root = root
        self.q = queue.Queue()
        self.busy = False
        self.log_file = None

        root.title("离线烧录器 上位机")
        root.geometry("1000x680")
        os.makedirs(LOG_DIR, exist_ok=True)
        self.log_path = os.path.join(
            LOG_DIR, datetime.datetime.now().strftime("gui_%Y%m%d_%H%M%S.log"))
        self.log_file = open(self.log_path, "a", encoding="utf-8")

        self._build()
        self.refresh_ports()
        self.root.after(90, self._drain)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self.log("日志文件: %s" % self.log_path)

    # ------------------------------------------------------------ 界面 ----
    def _build(self):
        pad = dict(padx=6, pady=4)

        top = ttk.Frame(self.root)
        top.pack(fill="x", **pad)

        ttk.Label(top, text="串口:").pack(side="left")
        self.cmb_port = ttk.Combobox(top, width=46, state="readonly")
        self.cmb_port.pack(side="left", padx=4)
        self.btn_refresh = ttk.Button(top, text="刷新", width=6, command=self.refresh_ports)
        self.btn_refresh.pack(side="left")
        self.btn_conn = ttk.Button(top, text="连接/查看", command=self.on_connect)
        self.btn_conn.pack(side="left", padx=4)
        self.lbl_state = ttk.Label(top, text="未连接", foreground="#888")
        self.lbl_state.pack(side="left", padx=8)

        ops = ttk.Frame(self.root)
        ops.pack(fill="x", **pad)
        self.btn_probe = ttk.Button(ops, text="SWD 探测", command=self.on_probe)
        self.btn_probe.pack(side="left")
        self.btn_dlog = ttk.Button(ops, text="读设备日志", command=self.on_dev_log)
        self.btn_clear = ttk.Button(ops, text="清空窗口", command=lambda: self.txt.delete("1.0", "end"))
        self.btn_clear.pack(side="left", padx=4)
        self.btn_openlog = ttk.Button(ops, text="打开日志目录", command=self._open_logdir)
        self.btn_openlog.pack(side="left", padx=4)

        frm = ttk.Frame(self.root)
        frm.pack(fill="x", **pad)
        ttk.Label(frm, text="固件:").pack(side="left")
        self.var_bin = tk.StringVar()
        self.ent_bin = ttk.Entry(frm, textvariable=self.var_bin)
        self.ent_bin.pack(side="left", fill="x", expand=True, padx=4)
        self.ent_bin.bind("<FocusOut>", lambda e: self._show_bin_info())
        ttk.Button(frm, text="浏览...", command=self._pick_bin).pack(side="left")
        self.lbl_bin = ttk.Label(self.root, text="", foreground="#666")
        self.lbl_bin.pack(fill="x", padx=12)

        act = ttk.Frame(self.root)
        act.pack(fill="x", **pad)
        self.btn_tgt = ttk.Button(act, text="离线烧录 (烧录器内镜像)", command=self.on_tgt)
        self.btn_tgt.pack(side="left")
        self.btn_strm = ttk.Button(act, text="流式烧录 (不限大小)", command=self.on_strm)
        self.btn_strm.pack(side="left", padx=6)
        self.btn_erase = ttk.Button(act, text="擦除目标芯片", command=self.on_erase)
        self.btn_erase.pack(side="left", padx=6)
        self.btn_imgclr = ttk.Button(act, text="清除烧录器固件", command=self.on_img_clear)
        self.btn_imgclr.pack(side="left", padx=6)
        self.btn_msc = ttk.Button(act, text="U盘模式烧录 (APP_MSC)", command=self.on_msc)
        self.btn_msc.pack(side="left", padx=6)
        self.btn_app = ttk.Button(act, text="升级主板应用", command=self.on_flash_app)
        self.btn_app.pack(side="left", padx=6)

        act2 = ttk.Frame(self.root)
        act2.pack(fill="x", **pad)
        self.btn_upload = ttk.Button(act2, text="写入烧录器 (离线镜像)",
                                     command=self.on_upload)
        self.btn_upload.pack(side="left")
        self.btn_make = ttk.Button(act2, text="制作专用文件 (.aesbin)",
                                   command=self.on_make_aesbin)
        self.btn_make.pack(side="left", padx=6)
        self.btn_lock = ttk.Button(act2, text="芯片上锁 (RDP L1)",
                                   command=self.on_rdp_lock)
        self.btn_lock.pack(side="left", padx=6)
        self.btn_unlock = ttk.Button(act2, text="芯片解锁 (解保护+擦除)",
                                     command=self.on_rdp_unlock)
        self.btn_unlock.pack(side="left", padx=6)

        self.pb = ttk.Progressbar(self.root, mode="indeterminate")
        self.pb.pack(fill="x", padx=8, pady=(2, 0))

        box = ttk.Frame(self.root)
        box.pack(fill="both", expand=True, **pad)
        self.txt = tk.Text(box, wrap="none", bg="#111", fg="#d0ffd0",
                           insertbackground="#d0ffd0", font=("Consolas", 10))
        sb_y = ttk.Scrollbar(box, orient="vertical", command=self.txt.yview)
        sb_x = ttk.Scrollbar(box, orient="horizontal", command=self.txt.xview)
        self.txt.configure(yscrollcommand=sb_y.set, xscrollcommand=sb_x.set)
        self.txt.grid(row=0, column=0, sticky="nsew")
        sb_y.grid(row=0, column=1, sticky="ns")
        sb_x.grid(row=1, column=0, sticky="ew")
        box.rowconfigure(0, weight=1)
        box.columnconfigure(0, weight=1)

        self.var_bin.set(self._default_bin())
        self._show_bin_info()

    def _default_bin(self):
        for n in ("blink_10k.bin", "blink_test.bin"):
            p = os.path.join(DEF_BIN_DIR, n)
            if os.path.isfile(p):
                return os.path.abspath(p)
        return ""

    def _pick_bin(self):
        p = filedialog.askopenfilename(
            title="选择固件", initialdir=os.path.abspath(DEF_BIN_DIR),
            filetypes=[("固件 (bin/hex/aesbin)", "*.bin;*.hex;*.ihx;*.aesbin"),
                       ("AES 专用文件", "*.aesbin"),
                       ("BIN", "*.bin"), ("HEX", "*.hex"), ("所有文件", "*.*")])
        if p:
            self.var_bin.set(p)
            self._show_bin_info()

    def _show_bin_info(self):
        p = self.var_bin.get().strip()
        if not p or not os.path.isfile(p):
            self.lbl_bin.config(text="(未选择有效文件)")
            return
        try:
            addr, data, meta = bf.load_image_ex(p)
        except Exception as e:
            self.lbl_bin.config(text="文件解析失败: %s" % e, foreground="#c62828")
            return
        kind = {"aesbin": "AESBIN", "hex": "HEX->BIN", "bin": "BIN"}[meta["fmt"]]
        extra = ""
        if meta["fmt"] == "aesbin":
            extra = "  %s" % aesbin.chip_name(meta["chip_code"])
            fl = meta["reserved"][0] if meta["reserved"] else 0
            if fl & aesbin.FLAG_UNLOCK:
                extra += " 烧前解锁"
            if fl & aesbin.FLAG_LOCK:
                extra += " 烧后上锁"
        payload = len(data) + len(meta["trailer"])
        if payload <= IMG_MAX:
            self.lbl_bin.config(
                text="%s  %d 字节%s  起始 0x%08X — 离线烧录/流式烧录均可"
                     % (kind, len(data), extra, addr), foreground="#2e7d32")
        else:
            self.lbl_bin.config(
                text="%s  %d 字节%s  起始 0x%08X — 超出 31KB, 只能用流式烧录"
                     % (kind, len(data), extra, addr), foreground="#c62828")

    # ------------------------------------------------------------ 日志 ----
    def log(self, s):
        for ln in str(s).replace("\r", "\n").split("\n"):
            if ln.strip():
                self.q.put("%s %s\n" % (ts(), ln.strip()))

    def _drain(self):
        try:
            while True:
                item = self.q.get_nowait()
                if isinstance(item, tuple):          # ("DONE", rc)
                    self._task_finished(item[1])
                    continue
                self.txt.insert("end", item)
                self.txt.see("end")
                if self.log_file:
                    self.log_file.write(item)
                    self.log_file.flush()
        except queue.Empty:
            pass
        self.root.after(90, self._drain)

    def _open_logdir(self):
        try:
            os.startfile(LOG_DIR)
        except Exception as e:
            self.log("打开目录失败: %s" % e)

    def _on_close(self):
        if self.busy:
            return
        if self.log_file:
            self.log_file.close()
        self.root.destroy()

    # ------------------------------------------------------------ 任务 ----
    def _set_buttons(self, on):
        st = "normal" if on else "disabled"
        for b in (self.btn_refresh, self.btn_conn, self.btn_probe, self.btn_dlog,
                  self.btn_tgt, self.btn_strm, self.btn_erase, self.btn_imgclr,
                  self.btn_msc, self.btn_app, self.btn_upload, self.btn_make,
                  self.btn_lock, self.btn_unlock):
            b.config(state=st)

    def _task_finished(self, rc):
        self.busy = False
        self.pb.stop()
        self._set_buttons(True)
        if rc == 0:
            self.lbl_state.config(text="就绪", foreground="#2e7d32")
            self.log("==== 完成 ====")
        else:
            self.lbl_state.config(text="出错 (rc=%s)" % rc, foreground="#c62828")
            self.log("==== 结束, 返回码 %s ====" % rc)

    def run_task(self, title, fn):
        if self.busy:
            return
        self.busy = True
        self._set_buttons(False)
        self.pb.start(12)
        self.lbl_state.config(text="运行中: %s" % title, foreground="#1565c0")
        self.log("======== %s ========" % title)

        def worker():
            old_out, old_err = sys.stdout, sys.stderr
            sys.stdout = sys.stderr = _Redirect(self.q)
            rc = 0
            try:
                rc = fn() or 0
            except (RuntimeError, ValueError) as e:
                print("失败: %s" % e)          # 预期内的校验/通信错误, 不打堆栈
                rc = -1
            except Exception as e:
                print("异常: %s" % e)
                for ln in traceback.format_exc().splitlines():
                    print(ln)
                rc = -1
            finally:
                sys.stdout, sys.stderr = old_out, old_err
                self.q.put(("DONE", rc))

        threading.Thread(target=worker, daemon=True).start()

    # ------------------------------------------------------------ 动作 ----
    def refresh_ports(self):
        items = []
        for p in bf.list_ports.comports():
            vid = p.vid or 0
            pid = p.pid or 0
            tag = ""
            if vid == bf.VID and pid in bf.SERIAL_PIDS:
                tag = "  <烧录器>"
            items.append("%s  %s  %04X:%04X%s" % (p.device, p.description, vid, pid, tag))
        self.cmb_port["values"] = items
        if items and not self.cmb_port.get():
            pref = [i for i in items if "<烧录器>" in i]
            self.cmb_port.set(pref[0] if pref else items[0])
        self.log("串口列表: %d 个" % len(items))

    def _dev(self):
        s = self.cmb_port.get().strip()
        return s.split()[0] if s else None

    def _need_dev(self):
        d = self._dev()
        if not d:
            raise RuntimeError("请先选择串口")
        return d

    def _need_bin(self):
        p = self.var_bin.get().strip()
        if not p or not os.path.isfile(p):
            raise RuntimeError("请先选择有效的固件文件")
        return p

    def on_connect(self):
        def job():
            dev = self._need_dev()
            print("连接 %s ..." % dev)
            app = bf.ensure_app(dev, 3.0)
            if app is None:
                raise RuntimeError("设备无应答: 请确认应用已运行 (可先执行 upgrade/run)")
            try:
                print("版本: %s" % app.version())
                i = app.info()
                print("镜像: valid=%d len=%d crc=%08X load=0x%08X 容量=%d"
                      % (i["valid"], i["length"], i["crc32"], i["load"], i["capacity"]))
                if i.get("fw_version") is not None:
                    print("固件版本: %s  识别码: 0x%08X  烧录计数: %d 次"
                          % (bf.fmt_fw_version(i["fw_version"]),
                             i.get("fw_magic") or 0, i.get("burn_count") or 0))
                print("状态: %s  result=%d"
                      % (bf.ST_NAMES.get(i["state"], i["state"]), i["result"]))
                try:
                    s = app.status()
                    print("DPIDR=0x%08X IDCODE=0x%08X progress=%d swd_err=%d"
                          % (s["dpidr"], s["idcode"], s["progress"], s["swd_err"]))
                except Exception as e:
                    print("GET_STATUS: %s" % e)
            finally:
                app.close()
            return 0
        self.run_task("连接/查看设备", job)

    def on_probe(self):
        def job():
            dev = self._need_dev()
            app = bf.try_app(dev, 2.0)
            if app is None:
                raise RuntimeError("设备无应答")
            try:
                r = app.probe()
                print("SWD 握手: %s  DPIDR=0x%08X IDCODE=0x%08X"
                      % (bf.TP_NAMES.get(r["status"], r["status"]),
                         r["dpidr"], r["idcode"]))
            finally:
                app.close()
            return 0 if r["status"] == 0 else 4
        self.run_task("SWD 探测", job)

    def on_dev_log(self):
        def job():
            dev = self._need_dev()
            app = bf.try_app(dev, 2.0)
            if app is None:
                raise RuntimeError("设备无应答")
            try:
                logs = app.logs()
                if not logs:
                    print("(设备日志为空)")
                for ln in logs.splitlines():
                    print("[dev] %s" % ln)
            finally:
                app.close()
            return 0
        self.run_task("读取设备日志", job)

    def on_tgt(self):
        """用烧录器内已保存的镜像烧录 (不再下载文件)"""
        def job():
            return bf.do_burn(self._need_dev())
        self.run_task("离线烧录 (烧录器内镜像)", job)

    def on_upload(self):
        """只把镜像写入烧录器 (离线模式用), 不触发烧录"""
        def job():
            dev = self._need_dev()
            p = self._need_bin()
            _, data, meta = bf.load_image_ex(p)
            payload = len(data) + len(meta["trailer"])
            if payload > IMG_MAX:
                raise RuntimeError("固件 %d 字节(含尾部)超过 31KB, 本机镜像区放不下"
                                   % payload)
            return bf.do_upload(dev, p)
        self.run_task("写入烧录器", job)

    def on_strm(self):
        def job():
            dev = self._need_dev()
            return bf.do_strm(dev, self._need_bin())
        self.run_task("流式烧录 (strm)", job)

    def on_erase(self):
        if not messagebox.askyesno(
                "确认擦除",
                "将整片擦除目标芯片的 Flash, 原有程序会全部丢失且不可恢复。\n\n继续?"):
            return

        def job():
            return bf.do_erase(self._need_dev())
        self.run_task("整片擦除目标芯片", job)

    def on_img_clear(self):
        if not messagebox.askyesno(
                "确认清除",
                "将删除烧录器内保存的目标固件镜像。\n"
                "删除后再次插上目标板不会被烧录。\n\n继续?"):
            return

        def job():
            return bf.do_clear(self._need_dev())
        self.run_task("清除烧录器固件", job)

    def on_msc(self):
        def job():
            return bf.do_msc(self._need_bin())
        self.run_task("U盘模式烧录 (msc)", job)

    def on_flash_app(self):
        def job():
            dev = self._need_dev()
            return bf.do_flash(dev, self._need_bin())
        self.run_task("升级主板应用", job)

    def on_make_aesbin(self):
        if MakeAesbinDialog is None:
            messagebox.showerror("不可用", "无法加载制作窗口 (online_prog.py)")
            return
        MakeAesbinDialog(self.root, on_upgrade=self._upgrade_from_maker)

    def _upgrade_from_maker(self, path):
        self.run_task("升级主板应用",
                      lambda: bf.do_flash(self._need_dev(), path))

    def on_rdp_lock(self):
        if not messagebox.askyesno(
                "确认上锁",
                "将给目标芯片写入读保护 RDP Level 1。\n"
                "上锁后调试器无法再读取芯片, 重新烧录前需要先解锁。\n\n继续?"):
            return

        def job():
            dev = self._need_dev()
            app = bf.try_app(dev, 2.0)
            if app is None:
                raise RuntimeError("设备无应答")
            try:
                st = app.rdp_set(True)
            finally:
                app.close()
            if st == 16:
                print("已写入上锁值, 请给目标断电重启后生效")
                return 0
            if st != 0:
                raise RuntimeError("上锁失败: code=%d (%s)"
                                   % (st, bf.TP_NAMES.get(st, "?")))
            print("已上锁 RDP Level 1")
            return 0
        self.run_task("芯片上锁", job)

    def on_rdp_unlock(self):
        """与 online 上位机 [擦除芯片] 同一动作: 自动解锁 + 整片擦除"""
        if not messagebox.askyesno(
                "确认解锁",
                "将解除目标读保护并整片擦除芯片 (选项字节改写)。\n"
                "原有程序会全部丢失, 不可恢复。\n\n继续?"):
            return

        def job():
            return bf.do_erase(self._need_dev())
        self.run_task("芯片解锁 (解保护+擦除)", job)


def main():
    root = tk.Tk()
    try:
        root.call("tk", "scaling", 1.25)
    except Exception:
        pass
    FlashGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
