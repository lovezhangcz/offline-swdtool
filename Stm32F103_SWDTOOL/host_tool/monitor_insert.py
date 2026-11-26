# -*- coding: utf-8 -*-
"""USB 侧监听: 目标板插拔时设备自动触发的全过程
高率轮询 在位检测 + 设备状态 + burn_seq + 日志, 只在变化时打印, 带毫秒时间戳
"""
import time
import boot_flash as bf

DUR = 90.0


def main():
    t0 = time.time()
    app = None
    last = {"pres": None, "state": None, "seq": None, "result": None,
            "err": None, "prog": None, "online": False}
    logbuf = ""

    def ts():
        return "%7.1f" % (time.time() - t0)

    while time.time() - t0 < DUR:
        if app is None:
            dev = bf.find_port(verbose=False)
            if dev:
                app = bf.try_app(dev, 1.0)
            if app is None:
                if last["online"]:
                    print("%s  USB 掉线 (设备枚举丢失)" % ts())
                    last["online"] = False
                time.sleep(0.3)
                continue
            if not last["online"]:
                print("%s  USB 已连接 (%s)" % (ts(), app.dev))
                last["online"] = True
            continue

        try:
            # 在位检测 (烧录中会返回忙, 视为在位)
            try:
                d = app.tgt_read(0xE0042000, 4, timeout=1.0)
                pres = int.from_bytes(d, "little")
            except Exception:
                pres = -1                       # 忙或失败, 不算拔出
            if pres != -1 and pres != last["pres"]:
                print("%s  目标 %s (IDCODE=0x%08X)"
                      % (ts(), "插入" if pres else "拔出", pres or 0))
                last["pres"] = pres
            elif pres == -1 and last["pres"] is None:
                pass

            st = app.status(timeout=2.0)
            key = (st["state"], st["result"], st.get("burn_seq"),
                   st["err"], st["progress"])
            if (st["state"], st.get("burn_seq")) != (last["state"], last["seq"]):
                print("%s  设备 state=%d result=%d seq=%s err=0x%08X prog=%d"
                      % (ts(), st["state"], st["result"], st.get("burn_seq"),
                         st["err"], st["progress"]))
                last["state"], last["seq"] = st["state"], st.get("burn_seq")

            logs = app.logs()
            if logs:
                for ln in logs.splitlines():
                    print("%s  [log] %s" % (ts(), ln))
            time.sleep(0.2)
        except Exception as e:
            print("%s  USB 异常: %s" % (ts(), str(e)[:60]))
            try:
                app.close()
            except Exception:
                pass
            app = None
            last["online"] = False
            last["pres"] = None
            time.sleep(0.3)

    if app:
        app.close()
    print("监听结束")


main()
