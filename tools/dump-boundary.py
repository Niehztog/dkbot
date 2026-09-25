#!/usr/bin/env python3
"""Verify Daikatana 1.3's game-module loader contract against a build.

    tools/dump-boundary.py vendor/dk-2025-12-21-x64            # build dir
    tools/dump-boundary.py path/to/dkded.orig                  # single ELF
"""
import os
import re
import shutil
import struct
import subprocess
import sys

LOADER = "_Z12DLL_LoadDLLsPc"
FINDER = "_Z16DLL_FindFunctionPKc"
NEEDED_SYMS = [LOADER, FINDER, "dll_Entry", "gi", "globals", "maxclients",
               "sv", "svs", "com"]


def objdump():
    for cand in ("x86_64-linux-gnu-objdump", "objdump", "llvm-objdump-18"):
        if shutil.which(cand):
            return cand
    sys.exit("no objdump found (need binutils, ideally x86_64-linux-gnu-objdump)")


class Elf:
    def __init__(self, path):
        self.path = path
        self.data = open(path, "rb").read()
        phoff = struct.unpack_from("<Q", self.data, 0x20)[0]
        phentsize = struct.unpack_from("<H", self.data, 0x36)[0]
        phnum = struct.unpack_from("<H", self.data, 0x38)[0]
        self.segs = []
        for i in range(phnum):
            off = phoff + i * phentsize
            if struct.unpack_from("<I", self.data, off)[0] != 1:
                continue
            p_off, p_va, _, p_filesz, _ = struct.unpack_from("<QQQQQ", self.data, off + 8)
            self.segs.append((p_va, p_va + p_filesz, p_off))

    def off(self, va):
        for start, end, off in self.segs:
            if start <= va < end:
                return off + (va - start)
        return None

    def cstr(self, va, limit=200):
        off = self.off(va)
        if off is None:
            return None
        end = self.data.find(b"\0", off)
        return self.data[off:min(end, off + limit)].decode("latin1")

    def qword(self, va):
        off = self.off(va)
        return struct.unpack_from("<Q", self.data, off)[0] if off is not None else None


def dynsyms(path):
    out = subprocess.run(["nm", "-D", "--defined-only", path],
                         capture_output=True, text=True).stdout
    return {line.split()[-1] for line in out.splitlines() if line.strip()}


def disas(od, path, symbol):
    out = subprocess.run([od, "-d", "--no-show-raw-insn", f"--disassemble={symbol}", path],
                         capture_output=True, text=True)
    if "can't disassemble" in out.stderr:
        sys.exit(f"{od} cannot disassemble x86-64 on this host; "
                 f"install binutils-x86-64-linux-gnu")
    return out.stdout


def report(path):
    print(f"== {path}")
    elf = Elf(path)
    od = objdump()

    syms = dynsyms(path)
    missing = [s for s in NEEDED_SYMS if s not in syms]
    print(f"   exported dynamic symbols : {len(syms)}")
    for s in NEEDED_SYMS:
        print(f"     {'ok ' if s in syms else 'MISSING'} {s}")

    text = disas(od, path, LOADER)
    dlopens = [m for m in re.finditer(r"([0-9a-f]+):\s+call\s+[0-9a-f]+ <dlopen@plt>", text)]
    self_dlopens = text.count("xor    %edi,%edi")
    print(f"   dlopen sites in DLL_LoadDLLs : {len(dlopens)} "
          f"({self_dlopens} with a NULL filename)")

    entries = [elf.cstr(int(m.group(1), 16))
               for m in re.finditer(r"mov\s+\$0x([0-9a-f]{5,}),%esi", text)]
    entries = [e for e in entries if e and e.endswith("_Entry")]
    print(f"   module entry symbols         : {', '.join(entries) or '(none found)'}")

    names = []
    for m in re.finditer(r"mov\s+\$0x([0-9a-f]{5,}),%edi", text):
        s = elf.cstr(int(m.group(1), 16))
        if s and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", s) and not s.startswith("Unable"):
            names.append(s)
    seen, boundary = set(), []
    for n in names:
        if n not in seen and n != "dedicated":
            seen.add(n)
            boundary.append(n)
    print(f"   DLL_FindFunction names       : {len(boundary)}")
    for n in boundary:
        print(f"     {n}")

    etext = disas(od, path, "dll_Entry")
    jt = re.search(r"jmp\s+\*0x([0-9a-f]+)\(,%rsi,8\)", etext)
    mx = re.search(r"cmp\s+\$0x([0-9a-f]+),%rsi", etext)
    if jt and mx:
        table, hi = int(jt.group(1), 16), int(mx.group(1), 16)
        default = elf.qword(table)
        msgs = []
        for i in range(hi + 1):
            tgt = elf.qword(table + 8 * i)
            if tgt != default:
                msgs.append(i)
        print(f"   dll_Entry messages handled   : {msgs} (0..{hi} dispatched)")
    ver = re.search(r"cmpl\s+\$0x([0-9a-f]+),\(%rdx\)", etext)
    if ver:
        print(f"   world API version            : 0x{int(ver.group(1), 16):X}")

    return not missing and self_dlopens >= 2


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    target = sys.argv[1]
    paths = []
    if os.path.isdir(target):
        for rel in ("dbgsyms/dkded.orig", "dbgsyms/daikatana.orig",
                    "game/dkded", "game/daikatana"):
            p = os.path.join(target, rel)
            if os.path.exists(p):
                paths.append(p)
        paths = paths[:2] or sys.exit(f"no Daikatana binaries under {target}")
    else:
        paths = [target]

    ok = all(report(p) for p in paths)
    print("\nRESULT:", "loader contract intact" if ok else "CONTRACT BROKEN - read docs/LOADER.md")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
