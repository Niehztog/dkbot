#!/usr/bin/env python3
"""Verify the Windows build's layout against include/dk/dk_boundary.h.

    tools/check-pdb.py [DIR]    DIR holds daikatana.pdb (default: vendor/dk-*-win-x64, else the install)
"""
import glob
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_INSTALL = "/mnt/c/Program Files/Daikatana"

# Header macro -> PDB fact (off, size, msize: a member's size, enum), or a list that must all hold.
FACTS = {
    "DK_EDICT_OFF_ORIGIN":        ("off", "edict_s", "s.origin"),
    "DK_EDICT_OFF_ANGLES":        ("off", "edict_s", "s.angles"),
    "DK_EDICT_OFF_CLIENT":        ("off", "edict_s", "client"),
    "DK_EDICT_OFF_INUSE":         ("off", "edict_s", "inuse"),
    "DK_EDICT_OFF_ABSMIN":        ("off", "edict_s", "absmin"),
    "DK_EDICT_OFF_ABSMAX":        ("off", "edict_s", "absmax"),
    "DK_EDICT_OFF_SOLID":         ("off", "edict_s", "solid"),
    "DK_EDICT_OFF_CLASSNAME":     ("off", "edict_s", "className"),
    "DK_ES_OFF_OLD_ORIGIN":       ("off", "entity_state_s", "old_origin"),
    "DK_ES_OFF_MODELINDEX":       ("off", "entity_state_s", "modelindex"),
    "DK_ES_OFF_MODELINDEX2":      ("off", "entity_state_s", "modelindex2"),
    "DK_ES_OFF_FRAME":            ("off", "entity_state_s", "frame"),
    "DK_ES_OFF_SKINNUM":          ("off", "entity_state_s", "skinnum"),
    "DK_ES_OFF_EFFECTS":          ("off", "entity_state_s", "effects"),
    "DK_ES_OFF_RENDERFX":         ("off", "entity_state_s", "renderfx"),
    "DK_UENT_OFF_RECORD":         ("off", "edict_s", "record"),
    "DK_UENT_OFF_INVENTORY":      ("off", "edict_s", "inventory"),
    "DK_UENT_OFF_VELOCITY":       ("off", "edict_s", "velocity"),
    "DK_UENT_OFF_ARMOR_VAL":      ("off", "edict_s", "armor_val"),
    "DK_UENT_OFF_HEALTH":         ("off", "edict_s", "health"),
    "DK_UENT_OFF_DEADFLAG":       ("off", "edict_s", "deadflag"),
    "DK_UENT_OFF_TEAM":           ("off", "edict_s", "team"),
    "DK_UENT_OFF_WATERLEVEL":     ("off", "edict_s", "waterlevel"),
    "DK_UENT_OFF_CURWEAPON":      ("off", "edict_s", "curWeapon"),
    "DK_REC_OFF_FRAGS":           ("off", "player_record_s", "frags"),
    "DK_INVLIST_OFF_HEAD":        ("off", "invenList_t", "head"),
    "DK_INVITEM_OFF_DATA":        ("off", "invenItem_s", "data"),
    "DK_INVITEM_OFF_NEXT":        ("off", "invenItem_s", "next"),
    "DK_INVDEF_OFF_NAME":         ("off", "userInventory_s", "name"),
    "DK_INVDEF_OFF_FLAGS":        ("off", "userInventory_s", "flags"),
    "DK_INVDEF_OFF_SIZE":         ("off", "userInventory_s", "size"),
    "DK_INVDEF_OFF_AMMO_COUNT":   ("off", "ammo_t", "count"),
    "DK_INVDEF_OFF_WINFO":        [("off", "ammo_t", "winfo"),
                                   ("off", "weapon_t", "winfo")],
    "DK_INVDEF_AMMO_SIZE":        ("size", "ammo_t"),
    "DK_WINFO_OFF_SELECT_FUNC":   ("off", "weaponInfo_s", "select_func"),
    "DK_COM_OFF_FIND_REGISTERED_WEAPON": ("off", "common_export_s", "FindRegisteredWeapon"),
    "DK_HOOK_OFF_INVULN_TIME":    ("off", "playerHook_s", "invulnerability_time"),
    "DK_HOOK_OFF_ENVIRO_TIME":    ("off", "playerHook_s", "envirosuit_time"),
    "DK_HOOK_OFF_OXYLUNG_TIME":   ("off", "playerHook_s", "oxylung_time"),
    "DK_HOOK_OFF_POWER_BOOST":    ("off", "playerHook_s", "power_boost"),
    "DK_HOOK_OFF_ATTACK_BOOST":   ("off", "playerHook_s", "attack_boost"),
    "DK_HOOK_OFF_SPEED_BOOST":    ("off", "playerHook_s", "speed_boost"),
    "DK_HOOK_OFF_ACRO_BOOST":     ("off", "playerHook_s", "acro_boost"),
    "DK_HOOK_OFF_VITA_BOOST":     ("off", "playerHook_s", "vita_boost"),
    "DK_HOOK_OFF_FX_FRAME_FUNC":  ("off", "playerHook_s", "fxFrameFunc"),
    "DK_CLIENT_OFF_PS":           ("off", "gclient_s", "ps"),
    "DK_CLIENT_OFF_VERIFIED_BOT": ("off", "gclient_s", "bVerifiedBot"),
    "DK_PS_OFF_PMOVE":            ("off", "player_state_t", "pmove"),
    "DK_PS_OFF_VIEWANGLES":       ("off", "player_state_t", "viewangles"),
    "DK_PS_OFF_VIEWOFFSET":       ("off", "player_state_t", "viewoffset"),
    "DK_PS_OFF_KICKANGLES":       ("off", "player_state_t", "kick_angles"),
    "DK_PS_OFF_FOV":              ("off", "player_state_t", "fov"),
    "DK_PS_OFF_RDFLAGS":          ("off", "player_state_t", "rdflags"),
    "DK_PM_OFF_TYPE":             ("off", "pmove_state_t", "pm_type"),
    "DK_PM_OFF_FLAGS":            ("off", "pmove_state_t", "pm_flags"),
    "DK_PM_OFF_TIME":             ("off", "pmove_state_t", "pm_time"),
    "DK_PM_OFF_GRAVITY":          ("off", "pmove_state_t", "gravity"),
    "DK_PM_OFF_DELTA_ANGLES":     ("off", "pmove_state_t", "delta_angles"),
    "DK_SV_OFF_CONFIGSTRINGS":    ("off", "server_t", "configstrings"),
    "DK_SVS_OFF_REALTIME":        ("off", "server_static_t", "realtime"),
    "DK_SVS_OFF_CLIENTS":         ("off", "server_static_t", "clients"),
    "DK_CLIENT_SIZE":             ("size", "client_s"),
    "DK_CLIENT_OFF_STATE":        ("off", "client_s", "state"),
    "DK_CLIENT_OFF_PING":         ("off", "client_s", "ping"),
    "DK_CLIENT_OFF_EDICT":        ("off", "client_s", "edict"),
    "DK_CLIENT_OFF_NAME":         ("off", "client_s", "name"),
    "DK_CLIENT_NAME_SIZE":        ("msize", "client_s", "name"),
    "DK_CLIENT_OFF_DATAGRAM":     ("off", "client_s", "datagram"),
    "DK_CLIENT_OFF_DATAGRAM_BUF": ("off", "client_s", "datagram_buf"),
    "DK_CLIENT_DATAGRAM_BUF_SIZE": ("msize", "client_s", "datagram_buf"),
    "DK_CLIENT_OFF_LASTMESSAGE":  ("off", "client_s", "lastmessage"),
    "DK_CLIENT_OFF_IDLETIME":     ("off", "client_s", "idletime"),
    "DK_CLIENT_OFF_NETCHAN":      ("off", "client_s", "netchan"),
    "DK_NETCHAN_OFF_REMOTEADDR":  ("off", "netchan_t", "remote_address"),
    "DK_NETCHAN_OFF_QPORT":       ("off", "netchan_t", "qport"),
    "DK_NETCHAN_OFF_MESSAGE":     ("off", "netchan_t", "message"),
    "DK_NETCHAN_OFF_MESSAGE_BUF": ("off", "netchan_t", "message_buf"),
    "DK_NETCHAN_MESSAGE_BUF_SIZE": ("msize", "netchan_t", "message_buf"),
    "DK_NETADR_OFF_TYPE":         ("off", "netadr_t", "type"),
    "DK_NETADR_OFF_IP":           ("off", "netadr_t", "ip"),
    "DK_NETADR_OFF_PORT":         ("off", "netadr_t", "port"),
    "DK_NA_LOOPBACK":             ("enum", "netadrtype_t", "NA_LOOPBACK"),
    "DK_NA_IP":                   ("enum", "netadrtype_t", "NA_IP"),
    "DK_CS_FREE":                 ("enum", "cl_state_t", "cs_free"),
    "DK_CS_SPAWNED":              ("enum", "cl_state_t", "cs_spawned"),
    "DK_SIZEBUF_OFF_ALLOWOVERFLOW": ("off", "sizebuf_s", "allowoverflow"),
    "DK_SIZEBUF_OFF_OVERFLOWED":  ("off", "sizebuf_s", "overflowed"),
    "DK_SIZEBUF_OFF_DATA":        ("off", "sizebuf_s", "data"),
    "DK_SIZEBUF_OFF_MAXSIZE":     ("off", "sizebuf_s", "maxsize"),
    "DK_SIZEBUF_OFF_CURSIZE":     ("off", "sizebuf_s", "cursize"),
    "DK_SIZEBUF_OFF_READCOUNT":   ("off", "sizebuf_s", "readcount"),
    "DK_GI_OFF_CON_PRINTF":       ("off", "game_import_t", "Con_Printf"),
    "DK_GI_OFF_TRACEBOX":         ("off", "game_import_t", "TraceBox"),
    "DK_GI_OFF_POINTCONTENTS":    ("off", "game_import_t", "pointcontents"),
    "DK_GI_OFF_GETARGC":          ("off", "game_import_t", "GetArgc"),
    "DK_GI_OFF_GETARGV":          ("off", "game_import_t", "GetArgv"),
    "DK_GI_OFF_GETARGS":          ("off", "game_import_t", "GetArgs"),
    "DK_GI_OFF_ADDCOMMAND":       ("off", "game_import_t", "AddCommand"),
    "DK_SS_OFF_TIME":             ("off", "serverState_s", "time"),
    "DK_SS_OFF_MAPNAME":          ("off", "serverState_s", "mapName"),
    "DK_SS_OFF_ADDCOMMAND":       ("off", "serverState_s", "AddCommand"),
    "DK_SS_OFF_INVFINDITEM":      ("off", "serverState_s", "InventoryFindItem"),
    "DK_SS_OFF_GETARGV":          ("off", "serverState_s", "GetArgv"),
    "DK_SS_OFF_GETARGC":          ("off", "serverState_s", "GetArgc"),
    "DK_SS_OFF_FS_LOADFILE":      ("off", "serverState_s", "FS_LoadFile"),
    "DK_SS_OFF_FS_FREEFILE":      ("off", "serverState_s", "FS_FreeFile"),
}

# Not checkable against the PDB: include guards, and constants read out of code.
NOT_LAYOUT = {
    "DK_BOUNDARY_H", "DK_SYMNAMES_H",
    "DK_BUTTON_ATTACK", "DK_BUTTON_USE",
    "DK_WORLD_API_VERSION",
    "DK_INVDEF_FLAG_AMMO",
    "DK_CS_MODELS", "DK_MAX_MODELS", "DK_CONFIGSTRING_SIZE",
}

# Structs the header mirrors whole: (our type, PDB type, members), our own padding left out.
MIRRORS = [
    ("usercmd_t", "usercmd_s",
     "msec buttons angles forwardmove sidemove upmove impulse lightlevel"),
    ("dk_game_export_t", "game_export_s",
     "apiversion SetServerTime SpawnEntities WriteGame ReadGame WriteHeader "
     "WriteLevel ReadLevel ClientConnect ClientBegin ClientUserinfoChanged "
     "ClientDisconnect ClientCommand ClientThink RunFrame RegisterFunc "
     "ServerCommand edicts edict_size num_edicts max_edicts CanSave "
     "endIntermission LevelLoad LevelExit InitDLLs UnloadDLLs InitChangelevel "
     "LoadNodes RegisterWorldFuncs EntityLoadCleanup"),
    ("dk_cvar_t", "cvar_s",
     "name string latched_string flags modified value intValue defaultValue "
     "description defaultFlags next"),
    ("dk_trace_t", "trace_t",
     "allsolid startsolid fraction endpos plane surface contents ent"),
    ("dk_eng_cplane_t", "cplane_s", "normal dist type signbits pad planeIndex"),
    ("dk_eng_csurface_t", "csurface_s", "name flags value index color"),
]

# Names dk_plat_resolve takes from the exe's export table.
EXPORTS = ["dll_Entry", "dll_ClientConnect", "dll_ClientDisconnect",
           "dll_ClientBeginServerFrame"]

PRIM_SIZE = {
    "char": 1, "unsigned char": 1, "signed char": 1, "bool": 1, "__int8": 1,
    "short": 2, "unsigned short": 2, "wchar_t": 2,
    "int": 4, "unsigned": 4, "long": 4, "unsigned long": 4, "float": 4,
    "HRESULT": 4,
    "__int64": 8, "unsigned __int64": 8, "double": 8, "long double": 8,
}


def tool(*names):
    for n in names:
        if shutil.which(n):
            return n
    return None


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace", **kw)
    if r.returncode != 0:
        sys.exit("%s failed:\n%s" % (" ".join(cmd), r.stderr.strip()))
    return r.stdout


REC = re.compile(r"^\s+0x([0-9A-F]+) \| (LF_\w+) \[size = \d+\](?: `(.*)`)?")
MEMBER = re.compile(r"- LF_MEMBER \[name = `([^`]*)`, Type = 0x([0-9A-F]+)"
                    r"(?: \(([^)]*)\))?, offset = (\d+)")
ENUMERATE = re.compile(r"- LF_ENUMERATE \[(\w+) = (-?\d+)\]")


class Types:
    def __init__(self, dump):
        self.recs = {}
        cur = None
        for line in dump.split("\n"):
            m = REC.match(line)
            if m:
                cur = [m.group(2), m.group(3), []]
                self.recs[int(m.group(1), 16)] = cur
            elif cur is not None:
                cur[2].append(line.strip())
        self.byname = {}
        for ti, (kind, name, lines) in self.recs.items():
            if kind in ("LF_STRUCTURE", "LF_CLASS", "LF_UNION") and name:
                body = " ".join(lines)
                fl = re.search(r"field list: 0x([0-9A-F]+)", body)
                if fl and "forward ref" not in body:
                    self.byname.setdefault(name, []).append(ti)

    def body(self, ti):
        return " ".join(self.recs[ti][2])

    def struct_size(self, ti):
        return int(re.search(r"sizeof (\d+)", self.body(ti)).group(1))

    def members(self, ti):
        fl = int(re.search(r"field list: 0x([0-9A-F]+)", self.body(ti)).group(1), 16)
        out = {}
        for line in self.recs[fl][2]:
            m = MEMBER.search(line)
            if m:
                out[m.group(1)] = (int(m.group(4)), int(m.group(2), 16), m.group(3))
        return out

    def complete(self, ti):
        kind, name, _ = self.recs[ti]
        if kind in ("LF_STRUCTURE", "LF_CLASS", "LF_UNION") and "forward ref" in self.body(ti):
            defs = self.byname.get(name, [])
            return defs[-1] if defs else None
        return ti

    def size(self, ti, prim):
        if ti < 0x1000:
            if prim and prim.endswith("*"):
                return 8
            return PRIM_SIZE.get(prim)
        kind = self.recs[ti][0]
        body = self.body(ti)
        if kind == "LF_POINTER":
            return 8
        if kind == "LF_ENUM":
            return 4
        if kind == "LF_ARRAY":
            return int(re.search(r"size: (\d+)", body).group(1))
        if kind == "LF_MODIFIER":
            m = re.search(r"referent = 0x([0-9A-F]+)(?: \(([^)]*)\))?", body)
            return self.size(int(m.group(1), 16), m.group(2))
        if kind in ("LF_STRUCTURE", "LF_CLASS", "LF_UNION"):
            full = self.complete(ti)
            return self.struct_size(full) if full else None
        return None

    def strip(self, ti):
        while ti >= 0x1000 and self.recs[ti][0] == "LF_MODIFIER":
            ti = int(re.search(r"referent = 0x([0-9A-F]+)", self.body(ti)).group(1), 16)
        return self.complete(ti) if ti >= 0x1000 else None

    def resolve(self, struct, path):
        found = []
        for ti in self.byname.get(struct, []):
            off, cur, size = 0, ti, None
            for i, part in enumerate(path.split(".")):
                mem = self.members(cur).get(part) if cur else None
                if mem is None:
                    break
                off += mem[0]
                size = self.size(mem[1], mem[2])
                if i < len(path.split(".")) - 1:
                    cur = self.strip(mem[1])
            else:
                found.append((off, size))
        return found

    def sizeof(self, struct):
        return sorted({self.struct_size(ti) for ti in self.byname.get(struct, [])})

    def enum(self, name, enumerator):
        for ti, (kind, n, lines) in self.recs.items():
            if kind != "LF_ENUM" or n != name:
                continue
            fl = re.search(r"field list: 0x([0-9A-F]+)", self.body(ti))
            if not fl:
                continue
            for line in self.recs[int(fl.group(1), 16)][2]:
                m = ENUMERATE.search(line)
                if m and m.group(1) == enumerator:
                    return int(m.group(2))
        return None


def c_int(text):
    """An integer constant made of C literals, parentheses and |, or None."""
    val = None
    for tok in re.findall(r"0[xX][0-9a-fA-F]+|\d+|\S", text.replace(" ", "")):
        if tok in "()":
            continue
        if tok == "|":
            if val is None:
                return None
            continue
        if not re.fullmatch(r"0[xX][0-9a-fA-F]+|\d+", tok):
            return None
        n = int(tok, 16) if tok[:2] in ("0x", "0X") else int(tok, 8) if tok[0] == "0" else int(tok)
        val = n if val is None else val | n
    return val


def header_macros(cc, inc):
    out = run([cc, "-E", "-dM", "-I" + inc, "-x", "c", "-"],
              input='#include "dk/dk_boundary.h"\n')
    macros = {}
    for line in out.split("\n"):
        m = re.match(r"#define (DK_\w+) (.+)$", line)
        if m and c_int(m.group(2)) is not None:
            macros[m.group(1)] = c_int(m.group(2))
    return macros


def mirror_layout(cc, inc):
    """Every mirrored member's offset and size, as the compiler prints them into assembly."""
    src = ['#include "dk/dk_boundary.h"',
           '#define P(tag, v) __asm__ volatile ("\\n->%c0 " tag :: "i" ((long long)(v)))',
           "void dk_probe(void) {"]
    for ctype, _pdb, names in MIRRORS:
        src.append('P("%s", sizeof(%s));' % (ctype, ctype))
        for n in names.split():
            src.append('P("%s.%s", __builtin_offsetof(%s, %s));' % (ctype, n, ctype, n))
            src.append('P("%s.%s#", sizeof(((%s *)0)->%s));' % (ctype, n, ctype, n))
    src.append("}")
    asm = run([cc, "-S", "-o", "-", "-I" + inc, "-x", "c", "-"], input="\n".join(src))
    return {m.group(2): int(m.group(1)) for m in re.finditer(r"->(-?\d+) (\S+)", asm)}


class Report:
    def __init__(self):
        self.bad = 0

    def line(self, ok, what, detail=""):
        if not ok:
            self.bad += 1
        print("     %s %s%s" % ("ok      " if ok else "MISMATCH", what,
                                 ("  " + detail) if detail else ""))


def check_macros(rep, types, macros):
    print("   header macros (MinGW view of dk_boundary.h): %d" % len(macros))
    for name in sorted(macros):
        if name in NOT_LAYOUT:
            continue
        want = macros[name]
        facts = FACTS.get(name)
        if facts is None:
            rep.line(False, name, "= 0x%x has no PDB fact in tools/check-pdb.py" % want)
            continue
        for fact in facts if isinstance(facts, list) else [facts]:
            kind, what = fact[0], ".".join(fact[1:])
            if kind == "off":
                got = sorted({o for o, _s in types.resolve(fact[1], fact[2])})
            elif kind == "msize":
                got = sorted({s for _o, s in types.resolve(fact[1], fact[2])})
            elif kind == "size":
                got = types.sizeof(fact[1])
            else:
                v = types.enum(fact[1], fact[2])
                got = [] if v is None else [v]
            label = "%-30s 0x%-6x %s %s" % (name, want, kind, what)
            if not got:
                rep.line(False, label, "-- not in the PDB")
            elif got != [want]:
                rep.line(False, label, "-- PDB says " + ", ".join("0x%x" % g for g in got))
            else:
                rep.line(True, label)
    for name in sorted(set(FACTS) - set(macros)):
        rep.line(False, name, "is in tools/check-pdb.py but not in the header")


def check_mirrors(rep, types, layout):
    print("   mirrored structs")
    for ctype, pdb, names in MIRRORS:
        sizes = types.sizeof(pdb)
        ours = layout.get(ctype)
        rep.line(sizes == [ours], "sizeof %-17s 0x%-4x %s" % (ctype, ours or 0, pdb),
                 "" if sizes == [ours] else "-- PDB says %s" % sizes)
        for n in names.split():
            got = types.resolve(pdb, n)
            off, size = layout.get("%s.%s" % (ctype, n)), layout.get("%s.%s#" % (ctype, n))
            ok = bool(got) and all(g == (off, size) for g in got)
            rep.line(ok, "  %-28s +0x%-4x %3d bytes" % (n, off or 0, size or 0),
                     "" if ok else "-- PDB says %s" % (
                         ", ".join("+0x%x %s bytes" % g for g in got) or "no such member"))


def check_publics(rep, pdbutil, pdb):
    names = re.findall(r'"(\?[^"]+)"', open(os.path.join(ROOT, "dkbot", "dk_win.c")).read())
    dump = run([pdbutil, "dump", "-publics", pdb])
    pubs = re.findall(r"S_PUB32 \[size = \d+\] `([^`]*)`", dump)
    print("   dbghelp names in dkbot/dk_win.c: %d (of %d publics)" % (len(names), len(pubs)))
    for n in names:
        count = pubs.count(n)
        rep.line(count >= 1, n, "" if count == 1 else
                 "-- not a public" if not count else "-- %d publics" % count)


def check_exe(rep, readobj, pdbutil, exe, pdb):
    print("   %s" % os.path.basename(exe))
    exports = set(re.findall(r"^\s*Name: (\S+)$", run([readobj, "--coff-exports", exe]), re.M))
    for n in EXPORTS:
        rep.line(n in exports, "export " + n)
    dll, hooked = None, False
    for line in run([readobj, "--coff-imports", exe]).split("\n"):
        m = re.match(r"\s*Name: (\S+)$", line)
        if m:
            dll = m.group(1).lower()
        if dll == "kernel32.dll" and re.match(r"\s*Symbol: GetProcAddress \(", line):
            hooked = True
    rep.line(hooked, "imports KERNEL32.dll!GetProcAddress by name (dk_win.c hooks it)")
    stamp = re.search(r"TimeDateStamp: .*\((0x[0-9A-F]+)\)", run([readobj, "--file-headers", exe]))
    want = re.search(r"#define SUPPORTED_BUILD (0x[0-9a-f]+)u",
                     open(os.path.join(ROOT, "launcher", "dkbot-launch.c")).read())
    same = bool(stamp and want) and int(stamp.group(1), 16) == int(want.group(1), 16)
    rep.line(same, "launcher/dkbot-launch.c's SUPPORTED_BUILD is this exe's %s"
             % (stamp.group(1) if stamp else "timestamp"),
             "" if same else "-- it holds %s" % (want.group(1) if want else "none"))
    dbg = run([readobj, "--coff-debug-directory", exe])
    # llvm-readobj 21 prints the GUID formatted, older ones its bytes
    b = re.search(r"PDBGUID: \(([0-9A-F ]+)\)", dbg)
    g = re.search(r"PDBGUID: \{([0-9A-F-]+)\}", dbg)
    age = re.search(r"PDBAge: (\d+)", dbg)
    summ = run([pdbutil, "dump", "-summary", pdb])
    pg = re.search(r"GUID: \{([0-9A-F-]+)\}", summ)
    pa = re.search(r"Age: (\d+)", summ)
    if (b or g) and age and pg and pa:
        if g:
            guid = g.group(1)
        else:
            x = bytes.fromhex(b.group(1).replace(" ", ""))
            guid = "%s-%s-%s-%s-%s" % (x[3::-1].hex(), x[5:3:-1].hex(), x[7:5:-1].hex(),
                                       x[8:10].hex(), x[10:].hex())
        same = guid.upper() == pg.group(1).upper() and age.group(1) == pa.group(1)
        rep.line(same, "PDB is this exe's: {%s} age %s" % (pg.group(1), pa.group(1)),
                 "" if same else "-- exe wants {%s} age %s" % (guid.upper(), age.group(1)))
    else:
        rep.line(False, "PDB is this exe's", "-- no CodeView record to compare")


def locate(arg):
    if arg:
        return arg
    vend = sorted(glob.glob(os.path.join(ROOT, "vendor", "dk-*-win-x64")))
    if vend:
        return vend[-1]
    if os.path.isfile(os.path.join(DEFAULT_INSTALL, "daikatana.pdb")):
        return DEFAULT_INSTALL
    sys.exit("no Windows build: pass the directory that holds daikatana.pdb "
             "(the release's DK_PDB_<date>_x64.7z has it, and every install does)")


def main():
    args = sys.argv[1:]
    inc = os.path.join(ROOT, "include")
    if args and args[0].startswith("-"):
        sys.exit(__doc__)
    where = locate(args[0] if args else None)
    pdb = os.path.join(where, "daikatana.pdb")
    exe = os.path.join(where, "daikatana.exe")
    if not os.path.isfile(pdb):
        sys.exit("no daikatana.pdb in %s" % where)
    pdbutil = tool("llvm-pdbutil", *("llvm-pdbutil-%d" % v for v in range(22, 13, -1)))
    readobj = tool("llvm-readobj", *("llvm-readobj-%d" % v for v in range(22, 13, -1)))
    cc = os.environ.get("WINCC") or tool("x86_64-w64-mingw32-gcc")
    if not pdbutil or not cc:
        sys.exit("needs llvm-pdbutil and a MinGW compiler (WINCC)")

    print("== %s" % where)
    types = Types(run([pdbutil, "dump", "-types", pdb]))
    rep = Report()
    check_macros(rep, types, header_macros(cc, inc))
    check_mirrors(rep, types, mirror_layout(cc, inc))
    check_publics(rep, pdbutil, pdb)
    if os.path.isfile(exe) and readobj:
        check_exe(rep, readobj, pdbutil, exe, pdb)
    else:
        print("   (no daikatana.exe or llvm-readobj: exe checks skipped)")
    print("\nRESULT:", "Windows layout intact" if not rep.bad else
          "%d MISMATCHES - fix include/dk/dk_boundary.h before trusting the "
          "Windows build (a layout of Windows' own goes under #ifdef _WIN32)"
          % rep.bad)
    return 1 if rep.bad else 0


if __name__ == "__main__":
    sys.exit(main())
