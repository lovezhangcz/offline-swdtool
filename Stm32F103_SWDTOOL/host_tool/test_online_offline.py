# -*- coding: utf-8 -*-
"""真机回归测试: 大容量CDC + online_prog 离线烧录模式
流程: 连接 -> 载入 ZZSW 固件 -> 写入烧录器 -> 点击烧录 x5 -> 汇总 + 截图
"""
import os
import time
import zlib

import boot_flash as bf
import online_prog as op

PATH = r"D:\keilwork\swdtool\ZZSW_motorSDFL_7cell_20261125_all_all.bin"
load, data, meta = bf.load_image_ex(PATH)
FW = dict(load=load, data=data, crc=zlib.crc32(data) & 0xFFFFFFFF,
          name=os.path.basename(PATH), trailer=meta["trailer"])
print("firmware: %d bytes crc=%08X trailer=%dB" % (len(data), FW["crc"], len(FW["trailer"])))

app = op.App()
S = {"i": 0, "t0": time.time(), "n": 0, "last": 0}
results = []


def tick():
    w = app.worker
    now = time.time()
    i = S["i"]

    if i == 0:                                  # 等连接
        if app.dev_online:
            print("[T] connected:", app.lb_dev.cget("text"))
            app.worker.set_firmware(FW)
            app._upload()                       # [写入烧录器]
            S["i"], S["t0"] = 1, now
    elif i == 1:                                # 等写入完成
        inf = w.dev or {}
        if inf.get("valid") and inf.get("length") == len(data) + len(FW["trailer"]):
            print("[T] uploaded, device image len=%d crc=%08X"
                  % (inf["length"], inf.get("crc32", 0)))
            S["i"], S["t0"] = 2, now
        elif now - S["t0"] > 90:
            raise RuntimeError("upload timeout")
    elif i == 2:                                # 稳定后点击 [烧 录]
        if w.state in (op.S_READY, op.S_OK, op.S_FAIL) and now - S["t0"] > 4:
            S["last"] = app.ok_cnt + app.fail_cnt
            app._burn()
            S["i"], S["t0"] = 3, now
    elif i == 3:                                # 等本轮结果
        if app.ok_cnt + app.fail_cnt > S["last"]:
            S["n"] += 1
            results.append((S["n"], app.ok_cnt, app.fail_cnt, now - S["t0"]))
            print("[T] burn #%d done: ok=%d fail=%d (%.1fs) state=%s"
                  % (S["n"], app.ok_cnt, app.fail_cnt, now - S["t0"],
                     app.lb_state.cget("text")))
            if S["n"] >= 5:
                S["i"] = 9
            else:
                S["i"], S["t0"] = 2, now
        elif now - S["t0"] > 120:
            raise RuntimeError("burn timeout")
    elif i == 9:
        print("[T] ALL DONE:", results)
        print("[T] counters:", app.lb_ok.cget("text"), "/", app.lb_fail.cget("text"))
        print("[T] device line:", app.lb_dev.cget("text"))
        try:
            from PIL import ImageGrab
            app.update()
            x, y = app.winfo_rootx(), app.winfo_rooty()
            ImageGrab.grab((x, y, x + app.winfo_width(),
                            y + app.winfo_height())).save("out/gui_offline_test.png")
            print("[T] screenshot -> out/gui_offline_test.png")
        except Exception as e:
            print("[T] shot skip:", e)
        app._on_close()
        app.after(500, app.destroy)
        return
    app.after(300, tick)


app.after(500, tick)
app.mainloop()
print("[T] EXIT")
