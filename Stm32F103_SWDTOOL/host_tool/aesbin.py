#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
专用烧录文件 (.aesbin) 编解码

文件结构 (小端):
    偏移  长度  内容
    0     4    魔术字 "AESB"
    4     1    版本 = 1
    5     1    算法 = 1 (AES-128-CBC + PKCS#7)
    6     2    文件头长度 = 48
    8     4    明文长度 plain_len (= 镜像长度 + 68)
    12    4    镜像烧录起始地址 load_addr
    16    16   IV (每次制作随机)
    32    4    明文 CRC32 (解密后校验完整性)
    36    12   保留 (0)
    48    ...  密文

明文结构 (加密前):
    [固件镜像 image]
    [目标芯片代码 chip_code  u32 小端]   0x00000000 = CHIP_AUTO 自动识别
    [保留 64 字节 reserved]              全 0, 留给以后扩展

AES 密钥固化在本文件内, 最终随上位机一起编译成 EXE 发布。
注意: 密钥只存在于上位机, 烧录器本身不参与解密。
"""
import os
import struct
import zlib

# ---- 固定密钥 (16 字节, AES-128) ----
AES_KEY = b"SWDPROG-AES128-K"

MAGIC = b"AESB"
VERSION = 1
ALG_AES128_CBC = 1
HDR_LEN = 48

CHIP_TRAILER_LEN = 68          # chip_code(4) + reserved(64)
RESERVED_LEN = 64

# 保护配置标志 (存放在 reserved 第 0 字节, 随 trailer 一起进入烧录器)
FLAG_UNLOCK = 0x01             # bit0: 烧录前自动解除读保护 (会整片擦除)
FLAG_LOCK   = 0x02             # bit1: 烧录并校验通过后上锁 RDP Level 1

# 目标芯片代码 (本项目内部统一编制)
CHIP_AUTO = 0x00000000
CHIP_STM32F1 = 0x00000001
CHIP_GD32F1X0 = 0x00000002
CHIP_GD32F303 = 0x00000003
CHIP_STM32F4 = 0x00000004
CHIP_APM32M4 = 0x00000005
CHIP_STM32F030 = 0x00000006
CHIP_NAMES = {
    CHIP_AUTO:    "自动识别",
    CHIP_STM32F1: "STM32F1 系列",
    CHIP_GD32F1X0: "GD32F103/F1x0",
    CHIP_GD32F303: "GD32F303",
    CHIP_STM32F4: "STM32F4",
    CHIP_APM32M4: "APM32-M4",
    CHIP_STM32F030: "STM32F030",
}
# 制作文件时下拉框顺序
CHIP_CHOICES = [CHIP_AUTO, CHIP_STM32F1, CHIP_GD32F1X0,
                CHIP_GD32F303, CHIP_STM32F4, CHIP_APM32M4, CHIP_STM32F030]


def chip_name(code):
    return CHIP_NAMES.get(code, "未知(0x%08X)" % code)


# ======================================================================
#                     极简 AES-128 实现 (无第三方依赖)
# ======================================================================
def _gmul(a, b):
    p = 0
    for _ in range(8):
        if b & 1:
            p ^= a
        hi = a & 0x80
        a = (a << 1) & 0xFF
        if hi:
            a ^= 0x1B
        b >>= 1
    return p & 0xFF


def _rotl8(x, n):
    return ((x << n) | (x >> (8 - n))) & 0xFF


def _build_sbox():
    inv = [0] * 256
    for x in range(1, 256):
        for y in range(1, 256):
            if _gmul(x, y) == 1:
                inv[x] = y
                break
    box = [0] * 256
    for x in range(256):
        i = inv[x]
        box[x] = (i ^ _rotl8(i, 1) ^ _rotl8(i, 2) ^ _rotl8(i, 3) ^
                  _rotl8(i, 4) ^ 0x63) & 0xFF
    ibox = [0] * 256
    for i, v in enumerate(box):
        ibox[v] = i
    return box, ibox


SBOX, INV_SBOX = _build_sbox()


def _expand_key(key):
    if len(key) != 16:
        raise ValueError("AES-128 需要 16 字节密钥")
    w = [list(key[4 * i:4 * i + 4]) for i in range(4)]
    rcon = 1
    for i in range(4, 44):
        t = list(w[i - 1])
        if i % 4 == 0:
            t = t[1:] + t[:1]
            t = [SBOX[b] for b in t]
            t[0] ^= rcon
            rcon = _gmul(rcon, 2)
        w.append([w[i - 4][j] ^ t[j] for j in range(4)])
    return w


def _add_round_key(s, w, rnd):
    for i in range(16):
        s[i] ^= w[4 * rnd + (i >> 2)][i & 3]


def _shift_rows(s):
    for r in range(1, 4):
        row = [s[r + 4 * c] for c in range(4)]
        row = row[r:] + row[:r]
        for c in range(4):
            s[r + 4 * c] = row[c]


def _inv_shift_rows(s):
    for r in range(1, 4):
        row = [s[r + 4 * c] for c in range(4)]
        row = row[-r:] + row[:-r]
        for c in range(4):
            s[r + 4 * c] = row[c]


def _mix_columns(s):
    for c in range(4):
        i = 4 * c
        a0, a1, a2, a3 = s[i], s[i + 1], s[i + 2], s[i + 3]
        s[i]     = _gmul(a0, 2) ^ _gmul(a1, 3) ^ a2 ^ a3
        s[i + 1] = a0 ^ _gmul(a1, 2) ^ _gmul(a2, 3) ^ a3
        s[i + 2] = a0 ^ a1 ^ _gmul(a2, 2) ^ _gmul(a3, 3)
        s[i + 3] = _gmul(a0, 3) ^ a1 ^ a2 ^ _gmul(a3, 2)


def _inv_mix_columns(s):
    for c in range(4):
        i = 4 * c
        a0, a1, a2, a3 = s[i], s[i + 1], s[i + 2], s[i + 3]
        s[i]     = _gmul(a0, 14) ^ _gmul(a1, 11) ^ _gmul(a2, 13) ^ _gmul(a3, 9)
        s[i + 1] = _gmul(a0, 9) ^ _gmul(a1, 14) ^ _gmul(a2, 11) ^ _gmul(a3, 13)
        s[i + 2] = _gmul(a0, 13) ^ _gmul(a1, 9) ^ _gmul(a2, 14) ^ _gmul(a3, 11)
        s[i + 3] = _gmul(a0, 11) ^ _gmul(a1, 13) ^ _gmul(a2, 9) ^ _gmul(a3, 14)


def _encrypt_block(w, block):
    s = list(block)
    _add_round_key(s, w, 0)
    for rnd in range(1, 10):
        s[:] = [SBOX[b] for b in s]
        _shift_rows(s)
        _mix_columns(s)
        _add_round_key(s, w, rnd)
    s[:] = [SBOX[b] for b in s]
    _shift_rows(s)
    _add_round_key(s, w, 10)
    return bytes(s)


def _decrypt_block(w, block):
    s = list(block)
    _add_round_key(s, w, 10)
    for rnd in range(9, 0, -1):
        _inv_shift_rows(s)
        s[:] = [INV_SBOX[b] for b in s]
        _add_round_key(s, w, rnd)
        _inv_mix_columns(s)
    _inv_shift_rows(s)
    s[:] = [INV_SBOX[b] for b in s]
    _add_round_key(s, w, 0)
    return bytes(s)


def _pkcs7_pad(data):
    n = 16 - (len(data) % 16)
    return data + bytes([n]) * n


def _pkcs7_unpad(data):
    if not data or len(data) % 16:
        raise ValueError("密文长度不是 16 的整数倍")
    n = data[-1]
    if n < 1 or n > 16 or data[-n:] != bytes([n]) * n:
        raise ValueError("填充校验失败 (密钥不对或文件损坏)")
    return data[:-n]


def encrypt_cbc(key, iv, data):
    w = _expand_key(key)
    data = _pkcs7_pad(data)
    out = bytearray()
    prev = iv
    for i in range(0, len(data), 16):
        blk = bytes(x ^ y for x, y in zip(data[i:i + 16], prev))
        enc = _encrypt_block(w, blk)
        out += enc
        prev = enc
    return bytes(out)


def decrypt_cbc(key, iv, data):
    w = _expand_key(key)
    out = bytearray()
    prev = iv
    for i in range(0, len(data), 16):
        blk = data[i:i + 16]
        dec = _decrypt_block(w, blk)
        out += bytes(x ^ y for x, y in zip(dec, prev))
        prev = blk
    return _pkcs7_unpad(bytes(out))


# ======================================================================
#                           .aesbin 打包 / 解包
# ======================================================================
def merge_images(items):
    """把多段 (起始地址, 数据) 合成一整块连续镜像, 空隙填 0xFF。
    返回 (基地址, 合并后的数据); 地址重叠时报错。"""
    segs = []
    for addr, data in items:
        if data is None or len(data) == 0:
            continue
        if (addr & 0xFF000000) != 0x08000000:
            raise ValueError("起始地址 0x%08X 不在 0x08xxxxxx" % addr)
        segs.append((addr, bytes(data)))
    if not segs:
        raise ValueError("没有可打包的镜像")
    segs.sort(key=lambda s: s[0])
    for i in range(1, len(segs)):
        prev_addr, prev_data = segs[i - 1]
        if segs[i][0] < prev_addr + len(prev_data):
            raise ValueError("两段镜像地址重叠: 0x%08X 与 0x%08X"
                             % (prev_addr, segs[i][0]))
    base = segs[0][0]
    end = max(a + len(d) for a, d in segs)
    out = bytearray(b"\xff" * (end - base))
    for addr, data in segs:
        out[addr - base:addr - base + len(data)] = data
    return base, bytes(out)


def make_bytes(image, chip_code=CHIP_AUTO, load_addr=0x08000000, flags=0):
    """把原始镜像打包成 .aesbin 内容
    flags: 保护配置 (FLAG_UNLOCK/FLAG_LOCK), 写入 reserved 第 0 字节"""
    if not image:
        raise ValueError("镜像为空")
    head = image[:8]
    if head == MAGIC:
        raise ValueError("输入看起来已经是 .aesbin 文件")
    reserved = bytes([flags & 0xFF]) + b"\x00" * (RESERVED_LEN - 1)
    plain = (bytes(image) + struct.pack("<I", chip_code & 0xFFFFFFFF) +
             reserved)
    iv = os.urandom(16)
    body = encrypt_cbc(AES_KEY, iv, plain)
    hdr = (MAGIC + bytes([VERSION, ALG_AES128_CBC]) +
           struct.pack("<H", HDR_LEN) +
           struct.pack("<II", len(plain), load_addr & 0xFFFFFFFF) +
           iv + struct.pack("<I", zlib.crc32(plain) & 0xFFFFFFFF) +
           b"\x00" * 12)
    assert len(hdr) == HDR_LEN
    return hdr + body


def make_file(path, image, chip_code=CHIP_AUTO, load_addr=0x08000000, flags=0):
    data = make_bytes(image, chip_code, load_addr, flags)
    with open(path, "wb") as f:
        f.write(data)
    return dict(path=path, size=len(data), image_len=len(image),
                chip_code=chip_code, load_addr=load_addr)


def parse_bytes(data):
    """解析 .aesbin 内容, 返回镜像与随文件携带的信息"""
    if len(data) < HDR_LEN or data[:4] != MAGIC:
        raise ValueError("不是有效的 .aesbin 文件")
    ver, alg = data[4], data[5]
    hdr_len = struct.unpack("<H", data[6:8])[0]
    plain_len, load_addr = struct.unpack("<II", data[8:16])
    iv = data[16:32]
    crc = struct.unpack("<I", data[32:36])[0]
    if ver != VERSION:
        raise ValueError("不支持的 .aesbin 版本 %d" % ver)
    if alg != ALG_AES128_CBC:
        raise ValueError("不支持的加密算法 %d" % alg)
    if hdr_len != HDR_LEN or len(data) < hdr_len:
        raise ValueError("文件头长度异常")
    body = data[hdr_len:]
    if not body or len(body) % 16:
        raise ValueError("密文长度异常")
    plain = decrypt_cbc(AES_KEY, iv, body)
    if len(plain) != plain_len:
        raise ValueError("明文长度不符 (文件被截断?)")
    if (zlib.crc32(plain) & 0xFFFFFFFF) != crc:
        raise ValueError("明文校验失败 (文件损坏)")
    if plain_len <= CHIP_TRAILER_LEN:
        raise ValueError("镜像为空")
    image = plain[:-CHIP_TRAILER_LEN]
    chip_code = struct.unpack("<I", plain[-CHIP_TRAILER_LEN:-RESERVED_LEN])[0]
    reserved = plain[-RESERVED_LEN:]
    return dict(image=image, chip_code=chip_code, reserved=reserved,
                flags=reserved[0], load_addr=load_addr,
                trailer=plain[-CHIP_TRAILER_LEN:], packed_len=len(data))


def load_file(path):
    with open(path, "rb") as f:
        return parse_bytes(f.read())


def is_aesbin(path):
    return os.path.splitext(path)[1].lower() == ".aesbin"
