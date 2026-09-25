#!/usr/bin/env python3
"""Extract Gladiator's botlib config files into this project's botdata/."""
import argparse
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_PAK = os.path.join(os.environ.get("GLAD", os.path.join(ROOT, "gladiator-bot-restored")),
                           "assets", "pak7.pak")


def read_pak(path):
    with open(path, "rb") as f:
        sig, off, ln = struct.unpack("<4sii", f.read(12))
        if sig != b"PACK":
            sys.exit(f"{path}: not a Quake PAK ({sig!r})")
        f.seek(off)
        entries = []
        for _ in range(ln // 64):
            e = f.read(64)
            name = e[:56].split(b"\0")[0].decode("latin1")
            fofs, flen = struct.unpack("<ii", e[56:64])
            entries.append((name, fofs, flen))
        return entries


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pak", default=DEFAULT_PAK)
    ap.add_argument("--out", default="botdata/gladiator")
    args = ap.parse_args()

    pak = args.pak if os.path.isabs(args.pak) else os.path.join(ROOT, args.pak)
    out = args.out if os.path.isabs(args.out) else os.path.join(ROOT, args.out)

    if not os.path.exists(pak):
        sys.exit(f"{pak} not found -- point --pak at Gladiator's pak7.pak")

    entries = read_pak(pak)
    wanted = [e for e in entries
              if e[0].lower().endswith(".c") or e[0].lower().endswith(".h")]
    with open(pak, "rb") as f:
        for name, fofs, flen in wanted:
            dest = os.path.join(out, name)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            f.seek(fofs)
            with open(dest, "wb") as o:
                o.write(f.read(flen))
    print(f"extracted {len(wanted)} config files into {out}")
    top = sorted({n.split('/')[0] for n, _, _ in wanted})
    print("  " + ", ".join(top))


if __name__ == "__main__":
    main()
