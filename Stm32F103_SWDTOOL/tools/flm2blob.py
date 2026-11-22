#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
flm2blob.py - 把 Keil FLM 烧录算法 (ELF32) 转成可在目标 RAM 执行的 C blob

用法:
    python flm2blob.py <file.FLM> <符号名> [入口基址=0x20000000]

输出:
    在当前目录生成 <符号名>.c, 内含 flash_code[] 和 const flash_algo_t <符号名>

布局 (与 DAPLink/pyOCD 一致):
    entry+0x00 : 32 字节 blob 头 (含 BKPT, LR 返回这里使 CPU 停机)
    entry+0x20 : PrgCode(RO) + PrgData(RW) + ZI(运行时清零)
    其后       : 页缓冲区 (page_size)
    再后 1KB   : 栈顶

自包含, 仅依赖标准库 (ELF32 LE 解析)。
"""
import os
import struct
import sys

SHT_PROGBITS = 1
SHT_SYMTAB = 2
SHT_NOBITS = 8

BLOB_HEADER = [0xE00ABE00, 0x062D780D, 0x24084068, 0xD3000040,
               0x1E644058, 0x1C49D1FA, 0x2A001E52, 0x4770D1F2]
HEADER_SIZE = 0x20

REQUIRED_SYMBOLS = ("Init", "UnInit", "EraseSector", "ProgramPage")
EXTRA_SYMBOLS = ("BlankCheck", "EraseChip", "Verify", "Read")

FLASH_DEVICE_STRUCT = "<H128sHLLLLBxxxLL"
FLASH_SECTOR_STRUCT = "<LL"
SECTOR_END = (0xFFFFFFFF, 0xFFFFFFFF)


class Elf32(object):
    """最小 ELF32 (little-endian) 解析: 节区表 + 符号表"""

    def __init__(self, data):
        if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
            raise ValueError("不是 32 位小端 ELF 文件")
        (self.e_shoff,) = struct.unpack_from("<I", data, 0x20)
        (self.e_shentsize,) = struct.unpack_from("<H", data, 0x2E)
        (self.e_shnum,) = struct.unpack_from("<H", data, 0x30)
        (self.e_shstrndx,) = struct.unpack_from("<H", data, 0x32)
        self.data = data
        self.sections = [self._parse_shdr(i) for i in range(self.e_shnum)]
        self.symbols = {}
        for s in self.sections:
            if s["type"] == SHT_SYMTAB:
                self._parse_symtab(s)

    def _parse_shdr(self, idx):
        off = self.e_shoff + idx * self.e_shentsize
        (name, typ, flags, addr, offset, size, link, _info,
         _align, entsize) = struct.unpack_from("<IIIIIIIIII", self.data, off)
        return {"name_off": name, "type": typ, "flags": flags, "addr": addr,
                "offset": offset, "size": size, "link": link, "entsize": entsize}

    def _strtab(self, idx):
        s = self.sections[idx]
        return self.data[s["offset"]:s["offset"] + s["size"]]

    def section_name(self, s):
        tab = self._strtab(self.e_shstrndx)
        end = tab.index(b"\x00", s["name_off"])
        return tab[s["name_off"]:end].decode("ascii", "replace")

    def _parse_symtab(self, s):
        strtab = self._strtab(s["link"])
        count = s["size"] // s["entsize"]
        for i in range(count):
            off = s["offset"] + i * s["entsize"]
            (st_name, st_value, st_size, _info, _other,
             _shndx) = struct.unpack_from("<IIIBBH", self.data, off)
            end = strtab.index(b"\x00", st_name)
            name = strtab[st_name:end].decode("ascii", "replace")
            self.symbols[name] = st_value

    def section_data(self, s):
        return self.data[s["offset"]:s["offset"] + s["size"]]

    def read_at(self, addr, size):
        """按虚拟地址从 PROGBITS 节区读取"""
        for s in self.sections:
            if s["type"] != SHT_PROGBITS:
                continue
            if s["addr"] <= addr and addr + size <= s["addr"] + s["size"]:
                off = s["offset"] + (addr - s["addr"])
                return self.data[off:off + size]
        raise ValueError("地址 0x%08X 不在任何 PROGBITS 节区内" % addr)


def build(flm_path, sym_name, entry):
    elf = Elf32(open(flm_path, "rb").read())

    # --- FlashDevice 信息 ---
    dev_addr = elf.symbols.get("FlashDevice")
    if dev_addr is None:
        raise ValueError("找不到 FlashDevice 符号")
    dev_size = struct.calcsize(FLASH_DEVICE_STRUCT)
    v = struct.unpack(FLASH_DEVICE_STRUCT, elf.read_at(dev_addr, dev_size))
    info = {
        "version": v[0], "name": v[1].split(b"\x00")[0].decode("ascii", "replace"),
        "type": v[2], "start": v[3], "size": v[4], "page_size": v[5],
        "value_empty": v[7], "prog_timeout_ms": v[8], "erase_timeout_ms": v[9],
    }
    sectors = []
    sec_size = struct.calcsize(FLASH_SECTOR_STRUCT)
    for i in range(512):
        s, a = struct.unpack(FLASH_SECTOR_STRUCT,
                             elf.read_at(dev_addr + dev_size + sec_size * i, sec_size))
        if (s, a) == SECTOR_END:
            break
        sectors.append((a, s))
    info["sectors"] = sectors

    # --- 符号地址 ---
    syms = {}
    for name in REQUIRED_SYMBOLS:
        if name not in elf.symbols:
            raise ValueError("缺少必需符号 %s" % name)
        syms[name] = elf.symbols[name]
    for name in EXTRA_SYMBOLS:
        syms[name] = elf.symbols.get(name, 0xFFFFFFFF)

    # --- 收集 PrgCode / PrgData(RW) / PrgData(ZI) ---
    s_ro = s_rw = s_zi = None
    for s in elf.sections:
        name = elf.section_name(s)
        if name == "PrgCode" and s["type"] == SHT_PROGBITS:
            s_ro = s
        elif name == "PrgData" and s["type"] == SHT_PROGBITS:
            s_rw = s
        elif name == "PrgData" and s["type"] == SHT_NOBITS:
            s_zi = s
    if s_ro is None:
        raise ValueError("缺少 PrgCode 节区")
    if s_rw is None:
        raise ValueError("缺少 PrgData(RW) 节区")
    if s_zi is None:  # 允许无 ZI, 造个空的
        s_zi = {"addr": s_rw["addr"] + s_rw["size"], "size": 0, "offset": 0,
                "type": SHT_NOBITS, "flags": 0, "link": 0, "entsize": 0,
                "name_off": 0}
    if s_ro["addr"] != 0 or \
            s_ro["addr"] + s_ro["size"] != s_rw["addr"] or \
            s_rw["addr"] + s_rw["size"] != s_zi["addr"]:
        raise ValueError("节区布局不是 RO+RW+ZI 连续")

    ro_size, rw_size, zi_size = s_ro["size"], s_rw["size"], s_zi["size"]
    algo = bytearray(ro_size + rw_size + zi_size)
    algo[0:ro_size] = elf.section_data(s_ro)
    algo[ro_size:ro_size + rw_size] = elf.section_data(s_rw)

    # --- 拼装 blob: 头 + 算法 ---
    blob = bytearray()
    for w in BLOB_HEADER:
        blob += struct.pack("<I", w)
    blob += algo
    while len(blob) % 4:
        blob.append(0)
    words = struct.unpack("<%dL" % (len(blob) // 4), blob)

    page_size = info["page_size"]
    buf_addr = entry + HEADER_SIZE + ro_size + rw_size + zi_size
    stack_top = buf_addr + page_size + 1024

    lines = []
    lines.append("/* 由 tools/flm2blob.py 自动生成, 请勿手改")
    lines.append(" * 来源: %s" % os.path.basename(flm_path))
    lines.append(" * 设备: %s  flash=0x%08X+0x%X  page=%uB" %
                 (info["name"], info["start"], info["size"], page_size))
    lines.append(" * sectors: %s" %
                 ", ".join("0x%X+0x%X" % (a, s) for a, s in sectors))
    lines.append(" */")
    lines.append('#include "flash_blob.h"')
    lines.append("")
    lines.append("static const uint32_t flash_code[] = {")
    for i in range(0, len(words), 8):
        lines.append("    " + ", ".join("0x%08X" % w for w in words[i:i + 8]) + ",")
    lines.append("};")
    lines.append("")
    lines.append("const flash_algo_t %s = {" % sym_name)
    lines.append("    0x%08Xu,  /* Init */" % (entry + HEADER_SIZE + syms["Init"]))
    lines.append("    0x%08Xu,  /* UnInit */" % (entry + HEADER_SIZE + syms["UnInit"]))
    ec = syms["EraseChip"]
    lines.append("    0x%08Xu,  /* EraseChip (0xFFFFFFFF=不支持) */" %
                 (0xFFFFFFFF if ec == 0xFFFFFFFF else entry + HEADER_SIZE + ec))
    lines.append("    0x%08Xu,  /* EraseSector */" % (entry + HEADER_SIZE + syms["EraseSector"]))
    lines.append("    0x%08Xu,  /* ProgramPage */" % (entry + HEADER_SIZE + syms["ProgramPage"]))
    lines.append("    { 0x%08Xu, 0x%08Xu, 0x%08Xu },  /* BKPT / static_base(RW) / SP */" %
                 (entry + 1, entry + HEADER_SIZE + ro_size, stack_top))
    lines.append("    0x%08Xu,  /* program_buffer */" % buf_addr)
    lines.append("    0x%08Xu,  /* algo_start */" % entry)
    lines.append("    sizeof(flash_code),")
    lines.append("    flash_code,")
    lines.append("    0x%08Xu,  /* program_buffer_size = page_size */" % page_size)
    lines.append("    0x%08Xu,  /* flash_start */" % info["start"])
    lines.append("    0x%08Xu,  /* flash_size */" % info["size"])
    lines.append("    0x%08Xu,  /* sector_size (首扇区) */" %
                 (sectors[0][1] if sectors else 0x400))
    lines.append("    %uu,        /* prog_timeout_ms */" % info["prog_timeout_ms"])
    lines.append("    %uu,        /* erase_timeout_ms */" % info["erase_timeout_ms"])
    lines.append('    "%s",' % info["name"])
    lines.append("};")
    lines.append("")

    out = sym_name.lower() + ".c"
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("%s -> %s" % (flm_path, out))
    print("  device=%s flash=0x%08X size=0x%X page=%uB algo=%uB buf@0x%08X SP=0x%08X"
          % (info["name"], info["start"], info["size"], page_size,
             len(blob), buf_addr, stack_top))
    print("  sectors: %s" % ", ".join("0x%X+0x%X" % (a, s) for a, s in sectors))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    entry = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x20000000
    build(sys.argv[1], sys.argv[2], entry)
