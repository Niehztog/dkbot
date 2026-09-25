#!/usr/bin/env python3
"""Read Daikatana PAK files.

    tools/dkpak.py find <pak-dir> <name>      # search every pak in a directory
"""
import os
import struct
import sys


def dk_decompress(data, out_len=0):
    """compresstype 1 -- not the engine's DK_decompress, which is LZF for network packets."""
    out = bytearray()
    ip, n = 0, len(data)
    while ip < n:
        ctrl = data[ip]
        if ctrl == 0xFF:
            break
        if ctrl <= 0x3F:
            cnt = ctrl + 1
            out += data[ip + 1:ip + 1 + cnt]
            ip += 1 + cnt
        elif ctrl <= 0x7F:
            out += b"\0" * (ctrl - 62)
            ip += 1
        elif ctrl <= 0xBF:
            out += bytes([data[ip + 1]]) * (ctrl - 126)
            ip += 2
        elif ctrl <= 0xFD:
            cnt = ctrl - 190
            ref = len(out) - data[ip + 1] - 2
            if ref < 0:
                raise ValueError(f"back reference before output start at ip={ip}")
            for i in range(cnt):
                out.append(out[ref + i])
            ip += 2
        else:
            ip += 1
    if out_len and len(out) != out_len:
        raise ValueError(f"decompress: got {len(out)}, expected {out_len}")
    return bytes(out)

DK_ENTRY = 72


class Pak:
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as f:
            ident, dirofs, dirlen = struct.unpack("<4sii", f.read(12))
            if ident != b"PACK" or dirlen % DK_ENTRY:
                raise ValueError(f"{path}: not a Daikatana PACK file")
            f.seek(dirofs)
            raw = f.read(dirlen)
        self.entries = []
        for off in range(0, dirlen, DK_ENTRY):
            name = raw[off:off + 56].split(b"\0")[0].decode("latin1")
            filepos, filelen, clen, ctype = struct.unpack_from("<iiii", raw, off + 56)
            self.entries.append(dict(name=name, pos=filepos, len=filelen,
                                     clen=clen, ctype=ctype))

    def read(self, name):
        e = next((x for x in self.entries if x["name"].lower() == name.lower()), None)
        if not e:
            return None
        with open(self.path, "rb") as f:
            f.seek(e["pos"])
            if e["ctype"] == 0:
                return f.read(e["len"])
            if e["ctype"] != 1:
                raise ValueError(f"{name}: unknown compresstype {e['ctype']}")
            return dk_decompress(f.read(e["clen"]), e["len"])


def main():
    if len(sys.argv) != 4 or sys.argv[1] != "find":
        sys.exit(__doc__)
    d, want = sys.argv[2], sys.argv[3]
    for p in sorted(os.listdir(d)):
        if not p.lower().endswith(".pak"):
            continue
        pak = Pak(os.path.join(d, p))
        hit = [e for e in pak.entries if e["name"].lower() == want.lower()]
        if hit:
            e = hit[0]
            print(f"{p}: {e['name']} len={e['len']} clen={e['clen']} ctype={e['ctype']}")


if __name__ == "__main__":
    main()
