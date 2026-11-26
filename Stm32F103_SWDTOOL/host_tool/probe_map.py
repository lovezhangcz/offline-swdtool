# -*- coding: utf-8 -*-
"""Probe target memory map: find which addresses respond."""
import sys
import boot_flash as bf

ADDRS = [
    # STM32F4 style map
    0x08000000, 0x20000000,
    0x1FFF0000, 0x1FFF7A10, 0x1FFF7A22, 0x1FFF7A2C,
    0x1FFFF7E0,  # GD32 flash size
    0x40020000, 0x40020400, 0x40020800, 0x40020C00,
    0x40021000, 0x40021400, 0x40021800, 0x40021C00,
    0x40022000, 0x40022400, 0x40022800, 0x40022C00,
    0x40023000, 0x40023400, 0x40023800, 0x40023C00,
    0x40024000, 0x40024400, 0x40024800,
    0x40026000, 0x40026400,
    0x40000000, 0x40000400, 0x40003000,  # TIM2 TIM3 IWDG
    0x40010000, 0x40010400, 0x40011000, 0x40013800, 0x40013C00, 0x40014000, 0x40014400,
    0xE000E010, 0xE000ED00, 0xE0042000,
]

def main():
    dev = bf.find_port(verbose=True)
    if not dev:
        print("no port")
        return
    app = bf.try_app(dev, 1.5)
    if not app:
        print("no app")
        return
    for a in ADDRS:
        try:
            d = app.tgt_read(a, 4)
            print("0x%08X: %s" % (a, d.hex()))
        except Exception as e:
            print("0x%08X: FAULT (%s)" % (a, str(e)[:40]))
    app.close()

main()
