#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_flag.py — 为 boot 生成“应用有效”升级标志页 (0x08001800)

boot 的 Boot_AppValid() 判定条件:
    flash[0x08001800] = { magic=0x424F4F54, len, crc32, flag=0xA55A5AA5 }
    crc32 = CRC32-IEEE(app 从 0x08002000 起的 len 字节)   (与 zlib.crc32 相同)

用 J-Link 直接烧录 app 时, 这一页不会被 Keil/上位机自动写入。
本脚本根据 app 的 .bin 自动算出 len/crc32, 输出 16 字节 flag.bin,
由 J-Link 的 loadbin 写到 0x08001800。

用法:
    python gen_flag.py <app.bin> <flag.bin>
"""
import os
import struct
import sys
import zlib

FLAG_ADDR = 0x08001800
BOOT_FLAG_MAGIC = 0x424F4F54   # "BOOT"
BOOT_FLAG_VALID = 0xA55A5AA5
APP_MAX = 22 * 1024


def main():
    if len(sys.argv) != 3:
        print("usage: gen_flag.py <app.bin> <flag.bin>")
        return 2

    bin_path, flag_path = sys.argv[1], sys.argv[2]
    with open(bin_path, "rb") as f:
        image = f.read()

    if not image:
        print("[flag] ERROR: app bin is empty")
        return 1
    if len(image) > APP_MAX:
        print("[flag] ERROR: app %d bytes > %d (22KB 应用区放不下)"
              % (len(image), APP_MAX))
        return 1

    crc = zlib.crc32(image) & 0xFFFFFFFF
    payload = struct.pack("<IIII", BOOT_FLAG_MAGIC, len(image), crc,
                          BOOT_FLAG_VALID)

    with open(flag_path, "wb") as f:
        f.write(payload)

    print("[flag] app   : %s" % os.path.basename(bin_path))
    print("[flag] len   : %d bytes (0x%X)" % (len(image), len(image)))
    print("[flag] crc32 : 0x%08X" % crc)
    print("[flag] -> %s (16 bytes @ 0x%08X)"
          % (flag_path, FLAG_ADDR))
    return 0


if __name__ == "__main__":
    sys.exit(main())
