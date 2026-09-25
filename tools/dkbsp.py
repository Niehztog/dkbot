#!/usr/bin/env python3
"""Convert a Daikatana BSP into a Quake II BSP that BSPC can compile to .aas.

    tools/dkbsp.py convert <pak-dir> <mapname> <out.bsp>   # as botlib_patch/dk_maps.c does in the engine
    tools/dkbsp.py bspc <pak-dir> <mapname> <out.bsp>      # plus what BSPC's AAS pass needs
"""
import math
import os
import re
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dkpak import Pak

DK_VERSION = 41
Q2_VERSION = 38
DK_LUMPS = 21
Q2_LUMPS = 19
LUMP_ENTITIES = 0
LUMP_LIGHTING = 7
LUMP_LEAFS = 8
LUMP_BRUSHES = 14
BRUSH_SIZE = 12          # firstside, numsides, contents
# 0x800 on water kills on contact (inferred: only e4dm2's water has it).
DK_CONTENTS_HARMFUL_WATER = 0x800
Q2_CONTENTS_LAVA = 0x8
Q2_CONTENTS_WATER = 0x20
# Daikatana's player trace stops at these; BSPC drops them as unknown. Ladders it already expands.
DK_CONTENTS_PLAYERSOLID = 0x80 | 0x200
Q2_CONTENTS_SOLID = 0x1
Q2_CONTENTS_LADDER = 0x20000000
DK_LEAF_SIZE = 32
Q2_LEAF_SIZE = 28
# BSPC aborts on a larger lighting lump; drop it whole, as botlib misreads a truncated one.
BSPC_MAX_LIGHTING = 3276800

def read_bsp(pakdir, mapname):
    """The file the engine loads."""
    want = f"maps/{mapname}.bsp"
    loose = os.path.join(pakdir, "maps", mapname + ".bsp")
    if os.path.isfile(loose):
        return open(loose, "rb").read(), want
    paks = [p for p in os.listdir(pakdir) if re.fullmatch(r"pak\d+\.pak", p, re.I)]
    for p in sorted(paks, key=lambda p: int(re.sub(r"\D", "", p)), reverse=True):
        data = Pak(os.path.join(pakdir, p)).read(want)
        if data:
            return data, want
    sys.exit(f"{want} not found under {pakdir}")


def read_lumps(data):
    ident, version = struct.unpack_from("<4si", data, 0)
    if ident != b"IBSP":
        sys.exit(f"not an IBSP file: {ident!r}")
    lumps = [struct.unpack_from("<ii", data, 8 + 8 * i) for i in range(DK_LUMPS)]
    return version, lumps


def fix_entities(blob):
    """Drop named-model keys: BSPC aborts on a model without a leading *."""
    text = blob.split(b"\0")[0].decode("latin1")
    kept, dropped = [], 0
    for line in text.splitlines():
        m = re.match(r'\s*"model"\s+"([^"]*)"', line)
        if m and not m.group(1).startswith("*"):
            dropped += 1
            continue
        kept.append(line)
    if dropped:
        print(f"  entities: dropped {dropped} named-model key(s) BSPC cannot parse")
    return ("\n".join(kept) + "\n\0").encode("latin1")


def fix_contents(blob, size, offset, what):
    n, hit, solid = len(blob) // size, 0, 0
    out = bytearray(blob)
    for i in range(n):
        base = i * size + offset
        c, = struct.unpack_from("<i", out, base)
        if c & Q2_CONTENTS_WATER and c & DK_CONTENTS_HARMFUL_WATER:
            c |= Q2_CONTENTS_LAVA
            hit += 1
        if c & DK_CONTENTS_PLAYERSOLID and not c & (Q2_CONTENTS_SOLID | Q2_CONTENTS_LADDER):
            c |= Q2_CONTENTS_SOLID
            solid += 1
        struct.pack_into("<i", out, base, c)
    if hit:
        print(f"  {what}: {hit} harmful-water entr{'y' if hit == 1 else 'ies'} "
              f"marked CONTENTS_LAVA")
    if solid:
        print(f"  {what}: {solid} see-through solid entr{'y' if solid == 1 else 'ies'} "
              f"marked CONTENTS_SOLID")
    return bytes(out)


def convert(pakdir, mapname):
    data, want = read_bsp(pakdir, mapname)
    version, lumps = read_lumps(data)
    if version != DK_VERSION:
        print(f"warning: expected version {DK_VERSION}, got {version}")

    pieces = []
    for i in range(Q2_LUMPS):
        ofs, ln = lumps[i]
        blob = data[ofs:ofs + ln]
        if i == LUMP_ENTITIES:
            blob = fix_entities(blob)
        if i == LUMP_LIGHTING and ln > BSPC_MAX_LIGHTING:
            print(f"  lighting: dropped {ln} bytes, over BSPC's "
                  f"{BSPC_MAX_LIGHTING} limit")
            blob = b""
        if i == LUMP_LEAFS:
            if ln % DK_LEAF_SIZE:
                sys.exit(f"leaf lump {ln} is not a multiple of {DK_LEAF_SIZE}")
            n = ln // DK_LEAF_SIZE
            blob = b"".join(blob[k * DK_LEAF_SIZE:k * DK_LEAF_SIZE + Q2_LEAF_SIZE]
                            for k in range(n))
            print(f"  leafs: {n} entries, {DK_LEAF_SIZE} -> {Q2_LEAF_SIZE} bytes "
                  f"(dropped brushnum)")
            blob = fix_contents(blob, Q2_LEAF_SIZE, 0, "leafs")
        if i == LUMP_BRUSHES:
            blob = fix_contents(blob, BRUSH_SIZE, 8, "brushes")
        pieces.append(blob)
    return pieces


def write_bsp(out, pieces, note=""):
    header_size = 8 + 8 * Q2_LUMPS
    body, dir_entries, pos = bytearray(), [], header_size
    for blob in pieces:
        pad = (-len(blob)) % 4
        dir_entries.append((pos, len(blob)))
        body += blob + b"\0" * pad
        pos += len(blob) + pad

    hdr = bytearray(struct.pack("<4si", b"IBSP", Q2_VERSION))
    for ofs, ln in dir_entries:
        hdr += struct.pack("<ii", ofs, ln)

    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "wb") as f:
        f.write(bytes(hdr) + bytes(body))
    print(f"wrote {out}: {len(hdr) + len(body)} bytes, IBSP v{Q2_VERSION}, "
          f"{Q2_LUMPS} lumps (dropped {DK_LUMPS - Q2_LUMPS} Daikatana lumps){note}")


# BSPC splits space only along brush sides it finds a face for; Daikatana's maps lack many.
LUMP_PLANES = 1
LUMP_VERTEXES = 2
LUMP_NODES = 4
LUMP_TEXINFO = 5
LUMP_FACES = 6
LUMP_LEAFBRUSHES = 10
LUMP_EDGES = 11
LUMP_SURFEDGES = 12
LUMP_MODELS = 13
LUMP_BRUSHSIDES = 15
Q2_SURF_HINT_SKIP = 0x100 | 0x200
BSPC_SOLID = 0x1 | 0x2 | 0x10000      # solid, window, playerclip
BSPC_BOXES = (((-16, -16, -24), (16, 16, 32)), ((-16, -16, -24), (16, 16, 4)))
BSPC_BOGUS_RANGE = 65535 + 128
BSPC_CLIP_EPSILON = -0.1
BSPC_TINY_EDGE = 0.2
BSPC_TEXTURED_AREA = 20
BSPC_SLIVER = 0.1
Q2_MAX = {LUMP_FACES: 65536, LUMP_VERTEXES: 65536, LUMP_EDGES: 128000, LUMP_SURFEDGES: 256000}
DK_SPAWNFLAG_NOT_DM = 0x8000
Q2_SPAWNFLAG_NOT_DM = 0x800
SAMPLE_STEP = 8.0
SAMPLE_INSET = 0.01


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def base_winding(n, d):
    """BSPC's BaseWindingForPlane."""
    x = max(range(3), key=lambda i: abs(n[i]))
    up = (0.0, 0.0, 1.0) if x != 2 else (1.0, 0.0, 0.0)
    v = dot(up, n)
    up = [up[i] - v * n[i] for i in range(3)]
    ln = math.sqrt(dot(up, up))
    up = [c / ln for c in up]
    rt = [c * BSPC_BOGUS_RANGE for c in cross(up, n)]
    up = [c * BSPC_BOGUS_RANGE for c in up]
    o = [c * d for c in n]
    return [tuple(o[i] - rt[i] + up[i] for i in range(3)), tuple(o[i] + rt[i] + up[i] for i in range(3)),
            tuple(o[i] + rt[i] - up[i] for i in range(3)), tuple(o[i] - rt[i] - up[i] for i in range(3))]


def chop(w, n, d, eps):
    """BSPC's ChopWindingInPlace; None if nothing is in front."""
    dists = [dot(p, n) - d for p in w]
    sides = [1 if x > eps else -1 if x < -eps else 0 for x in dists]
    if 1 not in sides:
        return None
    if -1 not in sides:
        return w
    out = []
    for i, p in enumerate(w):
        if sides[i] >= 0:
            out.append(p)
            if sides[i] == 0:
                continue
        j = (i + 1) % len(w)
        if sides[j] == 0 or sides[j] == sides[i]:
            continue
        t = dists[i] / (dists[i] - dists[j])
        q = w[j]
        out.append(tuple(d if n[c] == 1 else -d if n[c] == -1 else p[c] + t * (q[c] - p[c])
                         for c in range(3)))
    return out


def winding_tiny(w):
    long_edges = 0
    for i, p in enumerate(w):
        if math.dist(p, w[(i + 1) % len(w)]) > BSPC_TINY_EDGE:
            long_edges += 1
            if long_edges == 3:
                return False
    return True


def winding_area(w):
    total = 0.0
    for i in range(2, len(w)):
        c = cross([w[i - 1][k] - w[0][k] for k in range(3)], [w[i][k] - w[0][k] for k in range(3)])
        total += 0.5 * math.sqrt(dot(c, c))
    return total


class Lumps:
    def __init__(self, pieces):
        def items(i, fmt):
            return list(struct.iter_unpack(fmt, pieces[i]))
        self.planes = [((a, b, c), d) for a, b, c, d, _ in items(LUMP_PLANES, "<4fi")]
        self.verts = items(LUMP_VERTEXES, "<3f")
        self.nodes = items(LUMP_NODES, "<3i6h2H")
        self.texflags = [t[8] for t in items(LUMP_TEXINFO, "<8f2i32si")]
        self.faces = items(LUMP_FACES, "<HhihhBBBBi")
        self.leafs = items(LUMP_LEAFS, "<ihh6h4H")
        self.leafbrushes = [x for x, in items(LUMP_LEAFBRUSHES, "<H")]
        self.edges = items(LUMP_EDGES, "<HH")
        self.surfedges = [x for x, in items(LUMP_SURFEDGES, "<i")]
        self.headnodes = [m[9] for m in items(LUMP_MODELS, "<9f3i")]
        self.brushes = items(LUMP_BRUSHES, "<3i")
        self.sides = items(LUMP_BRUSHSIDES, "<Hh")

    def brush_sides(self, b):
        first, num, _ = self.brushes[b]
        return [(first + k, pn, ti) for k, (pn, ti) in enumerate(self.sides[first:first + num])]

    def model_brushes(self, m):
        out, stack = set(), [self.headnodes[m]]
        while stack:
            n = stack.pop()
            if n < 0:
                leaf = self.leafs[-n - 1]
                out.update(self.leafbrushes[leaf[11]:leaf[11] + leaf[12]])
            else:
                stack += self.nodes[n][1:3]
        return out


def side_winding(planes, sides, pn, grow=None):
    """BSPC's Q2_BrushSideWinding; with grow, the side of the brush grown by grow(n, d)."""
    n, d = planes[pn]
    w = base_winding(n, grow(n, d) if grow else d)
    for _, pn2, _ in sides:
        n2, d2 = planes[pn2]
        if pn2 == pn or (dot(n, n2) > 0.999 and abs(d - d2) < 0.01):
            continue
        if grow:
            w = chop(w, [-c for c in n2], -grow(n2, d2), 0)
        else:
            fn, fd = planes[pn2 ^ 1]
            w = chop(w, fn, fd, BSPC_CLIP_EPSILON)
        if w is None:
            return None
    return w


def face_on_winding(lumps, face, w):
    """BSPC's Q2_FaceOnWinding: the area of w the face covers."""
    pn, side, firstedge, numedges = face[:4]
    n = lumps.planes[pn][0]
    if side:
        n = [-c for c in n]
    for e in lumps.surfedges[firstedge:firstedge + numedges]:
        v = lumps.edges[abs(e)]
        v1, v2 = lumps.verts[v[e > 0]], lumps.verts[v[e <= 0]]
        en = cross([v1[k] - v2[k] for k in range(3)], n)
        ln = math.sqrt(dot(en, en))
        if ln:
            en = [c / ln for c in en]
            w = chop(w, en, dot(en, v1), BSPC_CLIP_EPSILON)
            if w is None:
                return 0.0
    return winding_area(w)


def unmatched_sides(lumps):
    """The brush sides Q2_FixTextureReferences finds no face for."""
    by_pair = defaultdict(list)
    for f in lumps.faces:
        by_pair[f[0] & ~1].append(f)
    out = set()
    for b in range(len(lumps.brushes)):
        sides = lumps.brush_sides(b)
        if any(ti > 0 and lumps.texflags[ti] & Q2_SURF_HINT_SKIP for _, _, ti in sides):
            continue
        for s, pn, _ in sides:
            w = side_winding(lumps.planes, sides, pn)
            if (w is None or winding_tiny(w) or winding_area(w) < BSPC_TEXTURED_AREA
                    or any(face_on_winding(lumps, f, w) > BSPC_SLIVER for f in by_pair[pn & ~1])):
                continue
            out.add(s)
    return out


def exact_winding(planes, sides, pn):
    w = side_winding(planes, sides, pn, lambda n, d: d)
    return [p for i, p in enumerate(w) if math.dist(p, w[i - 1]) > 0.01] if w else []


def outshone(planes, sides, s, pn):
    """A side within a unit of a larger side's plane: BSPC 1.4 can crash on the sliver between them."""
    n = planes[pn][0]
    w = exact_winding(planes, sides, pn)
    area = winding_area(w)
    for s2, pn2, _ in sides:
        n2, d2 = planes[pn2]
        if s2 == s or dot(n, n2) < 0.999 or any(abs(dot(n2, p) - d2) > 1 for p in w):
            continue
        area2 = winding_area(exact_winding(planes, sides, pn2))
        if area2 > area or (area2 == area and s2 < s):
            return True
    return False


def entities(blob):
    return [dict(re.findall(r'"([^"]*)"\s+"([^"]*)"', b))
            for b in re.findall(r"\{([^{}]*)\}", blob.split(b"\0")[0].decode("latin1"))]


def spawnflags(ent):
    try:
        return int(float(ent.get("spawnflags", "0")))
    except ValueError:
        return 0


def bspc_solids(lumps, ents):
    """(brush, origin) for each brush the AAS pass makes solid."""
    models = [(0, (0.0, 0.0, 0.0))]
    for e in ents:
        m = e.get("model", "")
        if e.get("classname") == "func_wall" and m.startswith("*") and not spawnflags(e) & DK_SPAWNFLAG_NOT_DM:
            try:
                org = tuple(float(x) for x in e.get("origin", "0 0 0").split())
            except ValueError:
                org = ()
            models.append((int(m[1:]), org if len(org) == 3 else (0.0, 0.0, 0.0)))
    return [(b, org) for m, org in models for b in sorted(lumps.model_brushes(m))
            if lumps.brushes[b][2] & BSPC_SOLID]


def face_samples(w, n):
    """Grid points on the inside of w, or its centre."""
    w = [p for i, p in enumerate(w) if math.dist(p, w[i - 1]) > 0.01]
    if len(w) < 3:
        return []
    x = max(range(3), key=lambda i: abs(n[i]))
    u = (0.0, 0.0, 1.0) if x != 2 else (1.0, 0.0, 0.0)
    k = dot(u, n)
    u = [u[i] - k * n[i] for i in range(3)]
    ln = math.sqrt(dot(u, u))
    u = [c / ln for c in u]
    v = cross(n, u)
    flat = [(dot(p, u), dot(p, v)) for p in w]
    turn = 1 if sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(flat, flat[1:] + flat[:1])) > 0 else -1
    edges = [(a, b, math.dist(a, b)) for a, b in zip(flat, flat[1:] + flat[:1])]

    def inside(p):
        return all(turn * ((b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]))
                   >= SAMPLE_INSET * el for a, b, el in edges if el > 0)
    base = [n[i] * dot(w[0], n) for i in range(3)]
    lo = [min(p[i] for p in flat) for i in range(2)]
    hi = [max(p[i] for p in flat) for i in range(2)]
    pts = [(a, b) for a in frange(lo[0], hi[0]) for b in frange(lo[1], hi[1]) if inside((a, b))]
    centre = (sum(p[0] for p in flat) / len(flat), sum(p[1] for p in flat) / len(flat))
    if not pts and inside(centre):
        pts = [centre]
    return [tuple(base[i] + a * u[i] + b * v[i] for i in range(3)) for a, b in pts]


def frange(lo, hi):
    x = lo + SAMPLE_STEP / 2
    while x < hi:
        yield x
        x += SAMPLE_STEP


def exposed_sides(lumps, unmatched, solids):
    """Unmatched sides whose face, grown by a BSPC box, borders space no other grown solid covers."""
    bounds = {}
    for b, org in solids:
        sides = lumps.brush_sides(b)
        pts = [p for _, pn, _ in sides for p in (side_winding(lumps.planes, sides, pn) or ())]
        if pts:
            bounds[b] = ([min(p[i] for p in pts) + org[i] for i in range(3)],
                         [max(p[i] for p in pts) + org[i] for i in range(3)])
    wanted, skipped = set(), set()
    for mins, maxs in BSPC_BOXES:
        grown, cells = [], defaultdict(list)
        for b, org in solids:
            if b not in bounds:
                continue

            def grow(n, d, org=org):
                return d + dot(n, org) - sum(n[i] * (mins[i] if n[i] > 0 else maxs[i]) for i in range(3))
            lo = [bounds[b][0][i] - maxs[i] for i in range(3)]
            hi = [bounds[b][1][i] - mins[i] for i in range(3)]
            planes = [(lumps.planes[pn][0], grow(*lumps.planes[pn])) for _, pn, _ in lumps.brush_sides(b)]
            for cell in cells_of(lo, hi):
                cells[cell].append(len(grown))
            grown.append((b, grow, planes, lo, hi))

        def covered(p, skip):
            for k in cells.get(tuple(int(c // 128) for c in p), ()):
                b, _, planes, lo, hi = grown[k]
                if (b != skip and all(lo[i] < p[i] < hi[i] for i in range(3))
                        and all(dot(n, p) < d for n, d in planes)):
                    return True
            return False
        for b, grow, _, _, _ in grown:
            sides = lumps.brush_sides(b)
            for s, pn, _ in sides:
                if s not in unmatched or s in wanted or s in skipped:
                    continue
                if outshone(lumps.planes, sides, s, pn):
                    skipped.add(s)
                    continue
                w = side_winding(lumps.planes, sides, pn, grow)
                n = lumps.planes[pn][0]
                if w and any(not covered(tuple(p[i] + 0.5 * n[i] for i in range(3)), b)
                             for p in face_samples(w, n)):
                    wanted.add(s)
    return wanted


def cells_of(lo, hi):
    return [(x, y, z) for x in range(int(lo[0] // 128), int(hi[0] // 128) + 1)
            for y in range(int(lo[1] // 128), int(hi[1] // 128) + 1)
            for z in range(int(lo[2] // 128), int(hi[2] // 128) + 1)]


def mark_not_deathmatch(blob):
    """Set Quake II's not-in-deathmatch spawnflag where Daikatana's is, so BSPC drops those walls."""
    count = 0

    def fix(m):
        nonlocal count
        v = int(m.group(2))
        nv = v & ~Q2_SPAWNFLAG_NOT_DM | (Q2_SPAWNFLAG_NOT_DM if v & DK_SPAWNFLAG_NOT_DM else 0)
        count += bool(v & DK_SPAWNFLAG_NOT_DM)
        return m.group(1) + str(nv) + m.group(3)
    text, _, rest = blob.decode("latin1").partition("\0")
    text = re.sub(r'("spawnflags"\s+")(\d+)(")', fix, text)
    return (text + "\0" + rest).encode("latin1"), count


def prepare_for_bspc(pieces):
    lumps = Lumps(pieces)
    unmatched = unmatched_sides(lumps)
    wanted = sorted(exposed_sides(lumps, unmatched, bspc_solids(lumps, entities(pieces[LUMP_ENTITIES]))))
    owner = {s: b for b in range(len(lumps.brushes)) for s, _, _ in lumps.brush_sides(b)}
    pieces = list(pieces)
    verts, edges = bytearray(pieces[LUMP_VERTEXES]), bytearray(pieces[LUMP_EDGES])
    surfedges, faces = bytearray(pieces[LUMP_SURFEDGES]), bytearray(pieces[LUMP_FACES])
    for s in wanted:
        pn, ti = lumps.sides[s]
        w = exact_winding(lumps.planes, lumps.brush_sides(owner[s]), pn)
        v0, e0, s0 = len(verts) // 12, len(edges) // 4, len(surfedges) // 4
        for k, p in enumerate(w):
            verts += struct.pack("<3f", *p)
            edges += struct.pack("<HH", v0 + k, v0 + (k + 1) % len(w))
            surfedges += struct.pack("<i", e0 + k)
        faces += struct.pack("<HhihhBBBBi", pn, 0, s0, len(w), max(ti, 0), 0, 255, 255, 255, -1)
    pieces[LUMP_VERTEXES], pieces[LUMP_EDGES] = bytes(verts), bytes(edges)
    pieces[LUMP_SURFEDGES], pieces[LUMP_FACES] = bytes(surfedges), bytes(faces)
    for i, size in ((LUMP_FACES, 20), (LUMP_VERTEXES, 12), (LUMP_EDGES, 4), (LUMP_SURFEDGES, 4)):
        if len(pieces[i]) // size > Q2_MAX[i]:
            sys.exit(f"lump {i}: {len(pieces[i]) // size} entries, over BSPC's {Q2_MAX[i]}")
    print(f"  faces: added {len(wanted)} for brush sides BSPC finds none for and must split on "
          f"({len(unmatched)} sides have none)")
    pieces[LUMP_ENTITIES], marked = mark_not_deathmatch(pieces[LUMP_ENTITIES])
    if marked:
        print(f"  entities: {marked} not in deathmatch, flagged with Quake II's 0x800")
    return pieces


if __name__ == "__main__":
    if len(sys.argv) != 5 or sys.argv[1] not in ("convert", "bspc"):
        sys.exit(__doc__)
    pieces = convert(sys.argv[2], sys.argv[3])
    if sys.argv[1] == "bspc":
        pieces = prepare_for_bspc(pieces)
    write_bsp(sys.argv[4], pieces, ", prepared for BSPC" if sys.argv[1] == "bspc" else "")
