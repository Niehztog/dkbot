#!/usr/bin/env python3
"""Generate Daikatana botlib config files from the game's own data."""
import argparse
import json
import os
import re
import shutil
import sys

def per_episode(classname, suffix):
    return [(classname, "models/e%d/a%d_%s" % (e, e, suffix)) for e in (1, 2, 3, 4)]


def pickup(name, model):
    return [(name, model)]


# inventory name: (kind, respawn seconds, [(map classname, model)]), models as the live game spawns them.
ITEMS = {
    "item_health_25":           ("health", 20, per_episode("item_health_25", "hlth.dkm")),
    "item_health_50":           ("health", 20, per_episode("item_health_50", "hlth2.dkm")),
    "item_plasteel_armor":      ("armor", 20, pickup("item_plasteel_armor", "models/e1/a1_ar1.dkm")),
    "item_chromatic_armor":     ("armor", 20, pickup("item_chromatic_armor", "models/e1/a1_ar2.dkm")),
    "item_silver_armor":        ("armor", 20, pickup("item_silver_armor", "models/e2/a2_ar1.dkm")),
    "item_gold_armor":          ("armor", 20, pickup("item_gold_armor", "models/e2/a2_ar2.dkm")),
    "item_chainmail_armor":     ("armor", 20, pickup("item_chainmail_armor", "models/e3/a3_ar1.dkm")),
    "item_black_adamant_armor": ("armor", 20, pickup("item_black_adamant_armor", "models/e3/a3_ar2.dkm")),
    "item_kevlar_armor":        ("armor", 20, pickup("item_kevlar_armor", "models/e4/a4_ar1.dkm")),
    "item_ebonite_armor":       ("armor", 20, pickup("item_ebonite_armor", "models/e4/a4_ar2.dkm")),

    "weapon_daikatana":   ("weapon", 30, pickup("weapon_daikatana", "models/global/a_daikatana.dkm")),
    "weapon_disruptor":   ("weapon", 30, pickup("weapon_disruptor", "models/e1/a_tazer.dkm")),
    "weapon_ionblaster":  ("weapon", 30, pickup("weapon_ionblaster", "models/e1/a_ion.dkm")),
    "weapon_c4":          ("weapon", 30, pickup("weapon_c4viz", "models/e1/a_c4.dkm")),
    "weapon_shotcycler":  ("weapon", 30, pickup("weapon_shotcycler", "models/e1/a_shot.dkm")),
    "weapon_sidewinder":  ("weapon", 30, pickup("weapon_sidewinder", "models/e1/a_swindr.dkm")),
    "weapon_shockwave":   ("weapon", 30, pickup("weapon_shockwave", "models/e1/a_shokwv.dkm")),
    "weapon_gashands":    ("weapon", 30, pickup("weapon_gashands", "models/e1/a_gashand.dkm")),
    "weapon_discus":      ("weapon", 30, pickup("weapon_discus", "models/e2/a_discus.dkm")),
    "weapon_venomous":    ("weapon", 30, pickup("weapon_venomous", "models/e2/a_venom.dkm")),
    "weapon_sunflare":    ("weapon", 30, pickup("weapon_sunflare", "models/e2/a_sflare.dkm")),
    "weapon_hammer":      ("weapon", 30, pickup("weapon_hammer", "models/e2/a_hammer.dkm")),
    "weapon_trident":     ("weapon", 30, pickup("weapon_trident", "models/e2/a_tri.dkm")),
    "weapon_zeus":        ("weapon", 30, pickup("weapon_zeus", "models/e2/a_zeus.dkm")),
    "weapon_silverclaw":  ("weapon", 30, pickup("weapon_silverclaw", "models/e3/a_claw.dkm")),
    "weapon_bolter":      ("weapon", 30, pickup("weapon_bolter", "models/e3/a_bolter.dkm")),
    "weapon_stavros":     ("weapon", 30, pickup("weapon_stavros", "models/e3/a_stav.dkm")),
    "weapon_ballista":    ("weapon", 30, pickup("weapon_ballista", "models/e3/a_bal.dkm")),
    "weapon_wyndrax":     ("weapon", 30, pickup("weapon_wyndrax", "models/e3/a_wyndrx.dkm")),
    "weapon_nightmare":   ("weapon", 30, pickup("weapon_nightmare", "models/e3/a_nmare.dkm")),
    "weapon_glock":       ("weapon", 30, pickup("weapon_glock", "models/e4/a_glock.dkm")),
    "weapon_slugger":     ("weapon", 30, pickup("weapon_slugger", "models/e4/a_slug.dkm")),
    "weapon_kineticore":  ("weapon", 30, pickup("weapon_kineticore", "models/e4/a_kcore.dkm")),
    "weapon_ripgun":      ("weapon", 30, pickup("weapon_ripgun", "models/e4/a_ripgun.dkm")),
    "weapon_novabeam":    ("weapon", 30, pickup("weapon_novabeam", "models/e4/a_nova.dkm")),
    "weapon_metamaser":   ("weapon", 30, pickup("weapon_metamaser", "models/e4/a_mmaser.dkm")),

    "ammo_ionpack":       ("ammo", 30, pickup("ammo_ionpack", "models/e1/wa_ion.dkm")),
    "ammo_c4":            ("ammo", 30, pickup("ammo_c4", "models/e1/wa_c4.dkm")),
    "ammo_shells":        ("ammo", 30, pickup("ammo_shells", "models/e1/wa_shot6.dkm")),
    "ammo_rockets":       ("ammo", 30, pickup("ammo_rockets", "models/e1/wa_swindr.dkm")),
    "ammo_shocksphere":   ("ammo", 30, pickup("ammo_shocksphere", "models/e1/wa_shokwv.dkm")),
    "ammo_discus":        ("ammo", 30, []),
    "ammo_venomous":      ("ammo", 30, pickup("ammo_venomous", "models/e2/wa_venom.dkm")),
    "ammo_sunflare":      ("ammo", 30, []),
    "ammo_tritips":       ("ammo", 30, pickup("ammo_tritips", "models/e2/wa_trident.dkm")),
    "ammo_zeus":          ("ammo", 30, pickup("ammo_zeus", "models/e2/wa_zeus.dkm")),
    "ammo_bolts":         ("ammo", 30, pickup("ammo_bolts", "models/e3/wa_bolt.dkm")),
    "ammo_stavros":       ("ammo", 30, pickup("ammo_stavros", "models/e3/wa_stav.dkm")),
    "ammo_ballista":      ("ammo", 30, pickup("ammo_ballista", "models/e3/wa_bal.dkm")),
    "ammo_wisp":          ("ammo", 30, pickup("ammo_wisp", "models/e3/we_wisp.dkm")),
    "ammo_gibs":          ("ammo", 30, []),
    "ammo_bullets":       ("ammo", 30, pickup("ammo_bullets", "models/e4/wa_glock.dkm")),
    "ammo_slugger":       ("ammo", 30, pickup("ammo_slugger", "models/e4/wa_rip.dkm")),
    "ammo_kineticore":    ("ammo", 30, pickup("ammo_kineticore", "models/e4/wa_kcore.dkm")),
    "ammo_ripgun":        ("ammo", 30, pickup("ammo_ripgun", "models/e4/wa_slug.dkm")),
    "ammo_novabeam":      ("ammo", 30, pickup("ammo_novabeam", "models/e4/wa_nova.dkm")),
    "ammo_metamaser":     ("ammo", 30, []),

    "item_power_boost":   ("skill", 60, pickup("item_power_boost", "models/global/a_pwrb.dkm")),
    "item_attack_boost":  ("skill", 60, pickup("item_attack_boost", "models/global/a_atkb.dkm")),
    "item_speed_boost":   ("skill", 60, pickup("item_speed_boost", "models/global/a_spdb.dkm")),
    "item_acro_boost":    ("skill", 60, pickup("item_acro_boost", "models/global/a_acrb.dkm")),
    "item_vita_boost":    ("skill", 60, pickup("item_vita_boost", "models/global/a_vtlb.dkm")),
    "item_megashield":    ("powerup", 120, pickup("item_megashield", "models/global/a_mshield.dkm")),
    "item_goldensoul":    ("powerup", 120, pickup("item_goldensoul", "models/global/a_gsoul.dkm")),
    "item_invincibility": ("powerup", 120, pickup("item_invincibility", "models/global/a_invincibility.dkm")),
    "item_wraithorb":     ("powerup", 120, pickup("item_wraithorb", "models/global/a_wraithorb.dkm")),
    "item_flag_team1":    ("powerup", 200, pickup("item_flag_team1", "models/global/a_ctf_flagl.dkm")
                                           + pickup("item_flag_team1", "models/global/dt_bpack.dkm")),
    "item_flag_team2":    ("powerup", 200, pickup("item_flag_team2", "models/global/a_ctf_flagl.dkm")
                                           + pickup("item_flag_team2", "models/global/dt_bpack.dkm")),
}

# weapon -> weaponInfo_s.ammoName; a weapon missing here is melee.
WEAPON_AMMO = {
    "weapon_ionblaster": "ammo_ionpack",
    "weapon_c4":         "ammo_c4",
    "weapon_shotcycler": "ammo_shells",
    "weapon_sidewinder": "ammo_rockets",
    "weapon_shockwave":  "ammo_shocksphere",
    "weapon_discus":     "ammo_discus",
    "weapon_venomous":   "ammo_venomous",
    "weapon_sunflare":   "ammo_sunflare",
    "weapon_trident":    "ammo_tritips",
    "weapon_zeus":       "ammo_zeus",
    "weapon_bolter":     "ammo_bolts",
    "weapon_stavros":    "ammo_stavros",
    "weapon_ballista":   "ammo_ballista",
    "weapon_wyndrax":    "ammo_wisp",
    "weapon_nightmare":  "ammo_gibs",
    "weapon_glock":      "ammo_bullets",
    "weapon_slugger":    "ammo_slugger",
    "weapon_kineticore": "ammo_kineticore",
    "weapon_ripgun":     "ammo_ripgun",
    "weapon_novabeam":   "ammo_novabeam",
    "weapon_metamaser":  "ammo_metamaser",
}

# entity_state_t.mins/maxs of each pickup as spawned; the engine rests it at floor - mins[2].
KIND_BOX = {"ammo":   ((-8, -8, 0), (8, 8, 24)),
            "weapon": ((-16, -16, -16), (16, 16, 12)),
            "skill":  ((-8, -8, -16), (8, 8, 16))}
TALL_WEAPON = ((-16, -16, -16), (16, 16, 64))
BOX = {"item_health_25":     ((-10, -10, -24), (10, 10, 5)),
       "item_health_50":     ((-13, -13, -24), (13, 13, -5)),
       "ammo_tritips":       ((-16, -16, 0), (16, 16, 68)),
       "ammo_wisp":          ((-8, -8, -8), (8, 8, 8)),
       "weapon_nightmare":   TALL_WEAPON,
       "weapon_stavros":     TALL_WEAPON,
       "weapon_trident":     TALL_WEAPON,
       "weapon_venomous":    TALL_WEAPON,
       "weapon_wyndrax":     TALL_WEAPON,
       "weapon_zeus":        TALL_WEAPON,
       "item_megashield":    ((-16, -16, 0), (16, 16, 48)),
       "item_goldensoul":    ((-8, -8, -24), (8, 8, 0)),
       "item_invincibility": ((-10, -10, 0), (10, 10, 24)),
       "item_wraithorb":     ((-12, -12, -24), (12, 12, 16)),
       "item_flag_team1":    ((-16, -16, -1), (16, 16, 64)),
       "item_flag_team2":    ((-16, -16, -1), (16, 16, 64))}

# Used, not picked up, so no inventory slot; misc_hosportal's style picks its model and box.
STATIONS = [("misc_hosportal", "models/e1/hosportal1.dkm", ((-16, -16, -24), (16, 16, 36))),
            ("misc_hosportal", "models/e1/hosportal2.dkm", ((-16, -16, -24), (16, 16, 24))),
            ("misc_hosportal", "models/e1/hosportal3.dkm", ((-16, -16, -24), (16, 16, 24))),
            ("misc_fountain",  "models/e2/a2_hlthfnt.dkm", ((-16, -16, -24), (16, 16, 8)))]
# The item type be_ai_goal.c and be_ai2_dmdk.c know a station by; seconds a used one is avoided.
ITEM_STATION = 8
STATION_AVOID = 10
W_STATION = 70
W_STATION_TOPUP = 25

KIND_TO_ITEMTYPE = {"health": "ITEM_HEALTH", "armor": "ITEM_ARMOR",
                    "weapon": "ITEM_WEAPON", "ammo": "ITEM_AMMO",
                    "powerup": "ITEM_POWERUP",
                    "skill": "ITEM_POWERUP"}

# Not fw_items.c/fw_weap.c: those use Quake II's INVENTORY_* names; ours are generated.
COPY_FROM_GLADIATOR = ["syn.c", "syn.h", "rnd.c", "match.c",
                       "match.h", "rchat.c", "ichat.h", "chars.h",
                       "teamplay.h", "game.h", "fw_aggr.c"]

W_SPARE = 5

# Far above other items: goals rank by weight / travel time.
RANGED_FLOOR = 250

# A splash weapon's worth with the enemy inside its blast.
CLOSE_FLOOR = 75

# Above Gladiator's items (to 74) and the Quake II slots the library hardcodes.
DK_ITEM_BASE = 80

FIXED_INVENTORY_SLOTS = """//enemy stuff
#define ENEMY_HORIZONTAL_DIST			200
#define ENEMY_HEIGHT						201
#define NUM_VISIBLE_ENEMIES			202
#define NUM_VISIBLE_TEAMMATES			203
//using powerups
#define USING_QUAD						204
#define USING_INVULNERABILITY			205
#define USING_SILENCER					206
#define USING_REBREATHER				207
#define USING_ENVIRONMENTSUIT			208
#define USING_ANCIENTHEAD				209
#define USING_POWERSCREEN				210
#define USING_POWERSHIELD				211
//using weapons
#define USING_BLASTER					215
#define USING_SHOTGUN					216
#define USING_SUPERSHOTGUN				217
#define USING_MACHINEGUN				218
#define USING_CHAINGUN					219
#define USING_GRENADELAUNCHER			220
#define USING_ROCKETLAUNCHER			221
#define USING_HYPERBLASTER				222
#define USING_RAILGUN					223
#define USING_BFG10K						224
#define USING_GRENADES					225
#define USING_GRAPPLE					226
//enemy using weapons
#define ENEMY_BLASTER					230
#define ENEMY_SHOTGUN					231
#define ENEMY_SUPERSHOTGUN				232
#define ENEMY_MACHINEGUN				233
#define ENEMY_CHAINGUN					234
#define ENEMY_GRENADELAUNCHER			235
#define ENEMY_ROCKETLAUNCHER			236
#define ENEMY_HYPERBLASTER				237
#define ENEMY_RAILGUN					238
#define ENEMY_BFG10K						239
#define ENEMY_GRENADES					240
#define ENEMY_GRAPPLE					241
//enemy using powerups
#define ENEMY_QUAD						245
#define ENEMY_INVULNERABILITY			246
#define ENEMY_POWERSCREEN				247
#define ENEMY_POWERSHIELD				248

//Daikatana's own derived slot, filled by botlib_patch/be_ai2_dmdk.c.
#define INVENTORY_ARMOR					190	// armour points, not pickups held"""

WEIGHT_BY_KIND = {"health": 60, "armor": 50, "ammo": 30, "powerup": 90,
                  "skill": 85}
COPY_DIRS = ["bots", "default"]

HEADER = """//===========================================================================
// {name}
//
// GENERATED by tools/gen-botcfg.py -- edit the generator, not this file.
// Daikatana equivalents of Gladiator's Quake II configuration.
//===========================================================================
"""


def ident(classname):
    return "INVENTORY_" + classname.upper().replace("ITEM_", "").replace("WEAPON_", "")


def gen_inv_h(items):
    out = [HEADER.format(name="inv.h -- Daikatana inventory indices"), ""]
    out.append("#define INVENTORY_NONE\t\t\t0")
    for name, idx in slots(items):
        out.append(f"#define {ident(name):<34}{idx}\t// {name}")
    out += ["",
            "// Fixed by botlib/be_ai_def.h, not ours to move:",
            "#define INVENTORY_HEALTH\t\t\t41",
            "",
            "// The library's own slots, copied verbatim from Gladiator's",
            "// inv.h. It fills these itself (enemy distance, what either side",
            "// is using), and the weight files may switch on them -- its own",
            "// Grenades weight keys on ENEMY_HORIZONTAL_DIST. Leaving them out",
            "// is not harmless: the precompiler stops at the first unknown",
            "// name with \"expected a number\", BotSetupClient then fails, and",
            "// the bots run with no character at all -- no weights, no goals,",
            "// no weapon switching.",
            FIXED_INVENTORY_SLOTS,
            ""]
    return "\n".join(out)


def slots(items):
    out = [(name, DK_ITEM_BASE + k) for k, name in enumerate(items)]
    # 190 is INVENTORY_ARMOR; 200+ is the library's
    if out[-1][1] >= 190:
        sys.exit("ran out of config-owned inventory indices")
    return out


def gen_inventory_h(items, weapons):
    """include/dk/dk_inventory.h: the same numbers for the C side."""
    out = ["/* GENERATED by tools/gen-botcfg.py -- edit the generator, not this file. */",
           "#ifndef DK_INVENTORY_H", "#define DK_INVENTORY_H", "",
           "#define DK_INVENTORY_ITEMS(X) \\"]
    out += [f'\tX("{name}", {idx}) \\' for name, idx in slots(items)]
    out += ["", ""]
    out += [f"#define DK_{ident(name)} {idx}" for name, idx in slots(items)
            if name.startswith("item_flag_")]
    out += ["",
            "#define DK_WEAPON_REACH(X) \\"]
    out += [f'\tX("{cn}", {reach(cn, w)}) \\'
            for cn, w in sorted(weapons.items())]
    out += ["", "", "#endif", ""]
    return "\n".join(out)


def reach(classname, w):
    """A projectile reaches speed * lifetime, not weapons.json's range; C4's lifetime is a fuse."""
    rng, speed, life = (int(w.get("range") or 0), float(w.get("speed") or 0),
                        float(w.get("lifetime") or 0))
    if speed > 0 and life > 0 and classname != "weapon_c4":
        return max(rng, int(speed * life))
    return rng



# weaponinfo_t.flags bits for be_ai2_dmdk.c, which must use the same values.
DK_WFL_ENGINE_TARGET = 0x100
WEAPON_FLAGS = {"weapon_zeus": DK_WFL_ENGINE_TARGET}


def gen_items_c(items):
    out = [HEADER.format(name="items.c -- Daikatana item configuration"),
           '#include "inv.h"', '#include "game.h"', "",
           "#define ITEM_NONE\t\t0", "#define ITEM_AMMO\t\t1",
           "#define ITEM_WEAPON\t\t2", "#define ITEM_HEALTH\t\t3",
           "#define ITEM_ARMOR\t\t4", "#define ITEM_POWERUP\t\t5",
           "#define ITEM_KEY\t\t6", "#define ITEM_FLAG\t\t7",
           f"#define ITEM_STATION\t\t{ITEM_STATION}", ""]
    for name, (kind, respawn, pickups) in items.items():
        for cn, model in pickups:
            mins, maxs = BOX.get(cn) or KIND_BOX.get(kind, ((-16, -16, -16), (16, 16, 16)))
            out += [f'iteminfo "{cn}"', "{",
                    f'\tname\t\t\t"{name}"',
                    f'\tmodel\t\t\t"{model}"',
                    f"\ttype\t\t\t{KIND_TO_ITEMTYPE[kind]}",
                    f"\tindex\t\t\t{ident(name)}",
                    f"\trespawntime\t\t{respawn}",
                    "\tmins\t\t\t{%d,%d,%d}" % mins,
                    "\tmaxs\t\t\t{%d,%d,%d}" % maxs,
                    "} //end iteminfo", ""]
    for cn, model, (mins, maxs) in STATIONS:
        out += [f'iteminfo "{cn}"', "{",
                f'\tname\t\t\t"{cn}"',
                f'\tmodel\t\t\t"{model}"',
                "\ttype\t\t\tITEM_STATION",
                "\tindex\t\t\tINVENTORY_NONE",
                f"\trespawntime\t\t{STATION_AVOID}",
                "\tmins\t\t\t{%d,%d,%d}" % mins,
                "\tmaxs\t\t\t{%d,%d,%d}" % maxs,
                "} //end iteminfo", ""]
    return "\n".join(out)


# Must equal DK_SPLASH_SLACK in botlib_patch/be_ai2_dmdk.c.
SPLASH_SLACK = 64

# weapon: (splash radius, the engine function it is from); none turns the self-damage guard off.
SPLASH = {
    "weapon_c4":         (300,     "c4Explode"),
    "weapon_sidewinder": (128,     "sidewinder_waitforclientload"),
    "weapon_ionblaster": (64,      "ionblaster_think"),
    "weapon_zeus":       (64,      "weapon_zeus_strike"),
    "weapon_ballista":   (128,     "removeBallista"),
    "weapon_shockwave":  (300,     "shockwaveTouch"),
    "weapon_trident":    (128,     "tipThink; 100 on a direct hit, tipTouch"),
}


def gen_weapons_c(weapons, items):
    out = [HEADER.format(name="weapons.c -- Daikatana weapon configuration"),
           '#include "inv.h"', '#include "game.h"', "",
           "#define VEC_ORIGIN\t\t\t{0, 0, 0}",
           "#define DAMAGETYPE_IMPACT\t\t1",
           "#define DAMAGETYPE_RADIAL\t\t2", ""]
    level = 0
    for cn, w in weapons.items():
        if cn not in items:
            continue
        proj = cn.replace("weapon_", "") + "_projectile"
        speed = w.get("speed") or 0
        dmg = w.get("damage") or 0
        splash = SPLASH.get(cn)
        if splash:
            radius, src = splash
            radline = f"\tradius\t\t\t{radius}\t\t//{src}"
            # 3 is IMPACT|RADIAL: the field parser takes a number, not an expression.
            typeline = "\tdamagetype\t\t3\t\t//IMPACT|RADIAL"
        else:
            radline = "\tradius\t\t\t0"
            typeline = "\tdamagetype\t\tDAMAGETYPE_IMPACT"
        out += ["projectileinfo", "{",
                f'\tname\t\t\t"{proj}"',
                f'\tmodel\t\t\t"{items[cn][2][0][1]}"',
                "\tflags\t\t\t0",
                "\tgravity\t\t\t0",
                f"\tdamage\t\t\t{dmg}",
                radline,
                typeline,
                "} //end projectileinfo", ""]
        ammo = WEAPON_AMMO.get(cn)
        out += ["weaponinfo", "{",
                f'\tname\t\t\t"{cn}"',
                # unique per weapon: BotChooseBestFightWeapon switches only on a different model
                f'\tmodel\t\t\t"{items[cn][2][0][1]}"',
                f"\tlevel\t\t\t{level}",
                f"\tweaponindex\t\t{ident(cn)}",
                f"\tflags\t\t\t{WEAPON_FLAGS.get(cn, 0)}",
                f'\tprojectile\t\t"{proj}"',
                "\tnumprojectiles\t\t1", "\thspread\t\t\t0", "\tvspread\t\t\t0",
                f"\tspeed\t\t\t{speed}",
                "\tacceleration\t\t0", "\trecoil\t\t\tVEC_ORIGIN",
                "\toffset\t\t\t{24, 8, -8}", "\tangleoffset\t\tVEC_ORIGIN",
                f"\tammoamount\t\t{w.get('ammo_per_use') or 0}",
                f"\tammoindex\t\t{ident(ammo) if ammo else 'INVENTORY_NONE'}",
                "\tactivate\t\t0.5", "\treload\t\t\t0.5",
                "\tspinup\t\t\t0", "\tspindown\t\t0",
                "} //end weaponinfo", ""]
    return "\n".join(out)


def wconst(classname):
    return "W_" + classname.upper().replace("ITEM_", "").replace("WEAPON_", "")


def item_weight(classname, kind, weapons):
    if kind == "weapon":
        w = weapons.get(classname) or {}
        rating = w.get("rating") or 100
        weight = max(40, min(100, int(40 + (rating - 100) / 300.0 * 60)))
        if classname in WEAPON_AMMO:
            weight = max(weight, RANGED_FLOOR)
        return weight
    return WEIGHT_BY_KIND[kind]


def fight_weight(classname, weapons):
    """Worth in a fight: the raw rating, since RANGED_FLOOR would tie every ranged weapon."""
    return int((weapons.get(classname) or {}).get("rating") or 100)


# BotSetupLibrary wants the file, though the AI never reads sounds.
SOUNDS_C = HEADER.format(name="sounds.c -- intentionally empty") + """
// Gladiator's AI never reads the sound list, so there is nothing to describe.
"""


def gen_fw_items(items, weapons):
    """In this fuzzy syntax `case N` covers values below N, so `case 1` means "has none"."""
    out = [HEADER.format(name="fw_items.c -- Daikatana item weights"), ""]
    cap = {a: int((weapons.get(w) or {}).get("ammo_max") or 0) for w, a in WEAPON_AMMO.items()}
    for name, (kind, _r, pickups) in items.items():
        # botlib looks a weight up by the pickup's classname
        for cn in dict.fromkeys(c for c, _m in pickups):
            if kind == "ammo" and cap.get(name):
                out += [f'weight "{cn}"', "{",
                        f"\tswitch({ident(name)})", "\t{",
                        f"\t\tcase {max(1, cap[name] // 4)}: return {wconst(name)};",
                        f"\t\tcase {cap[name]}: return {item_weight(name, kind, weapons) // 5};",
                        "\t\tdefault: return 0;",
                        "\t} //end switch", "} //end weight", ""]
                continue
            if kind != "weapon":
                out += [f'weight "{cn}"', "{",
                        f"\tswitch({ident(name)})", "\t{",
                        f"\t\tdefault: return {wconst(name)};",
                        "\t} //end switch", "} //end weight", ""]
                continue
            ammo = WEAPON_AMMO.get(name)
            if ammo:
                low = max(1, int(((weapons.get(name) or {}).get("ammo_max") or 20) / 4))
                owned = ["\t\tdefault:", "\t\t{",
                         f"\t\t\tswitch({ident(ammo)})", "\t\t\t{",
                         f"\t\t\t\tcase {low}: return {wconst(name)}_LOW;",
                         f"\t\t\t\tdefault: return {W_SPARE};",
                         "\t\t\t} //end switch", "\t\t} //end default"]
            else:
                owned = [f"\t\tdefault: return {W_SPARE};"]
            out += [f'weight "{cn}"', "{",
                    f"\tswitch({ident(name)})", "\t{",
                    f"\t\tcase 1: return {wconst(name)};",
                    *owned,
                    "\t} //end switch", "} //end weight", ""]
    for cn in dict.fromkeys(c for c, _m, _b in STATIONS):
        out += [f'weight "{cn}"', "{",
                "\tswitch(INVENTORY_HEALTH)", "\t{",
                "\t\tcase 50: return W_STATION;",
                "\t\tcase 80: return W_STATION_TOPUP;",
                "\t\tdefault: return 0;",
                "\t} //end switch", "} //end weight", ""]
    return "\n".join(out)


def gen_fw_weap(items, weapons):
    out = [HEADER.format(name="fw_weap.c -- Daikatana weapon weights"), ""]
    for cn, (kind, _r, _p) in items.items():
        if kind != "weapon":
            continue
        ammo = WEAPON_AMMO.get(cn)
        rng = int((weapons.get(cn) or {}).get("range") or 0)
        if rng and not ammo:
            out += [f'weight "{cn}"', "{", f"\tswitch({ident(cn)})", "\t{",
                    "\t\tcase 1: return 0;",
                    "\t\tdefault:", "\t\t{",
                    "\t\t\tswitch(ENEMY_HORIZONTAL_DIST)", "\t\t\t{",
                    f"\t\t\t\tcase {int(rng * 1.25)}: return {wconst(cn)};",
                    "\t\t\t\tdefault: return 0;",
                    "\t\t\t} //end switch", "\t\t} //end default",
                    "\t} //end switch", "} //end weight", ""]
            continue
        # Worth less where be_ai2_dmdk.c's self-damage guard holds fire.
        splash = SPLASH.get(cn)
        radius = splash[0] if splash else 0
        def fire_value(indent):
            t = "\t" * indent
            if not radius:
                return [f"{t}return {wconst(cn)};"]
            return [f"{t}switch(ENEMY_HORIZONTAL_DIST)", f"{t}{{",
                    f"{t}\tcase {radius + SPLASH_SLACK}: return {wconst(cn) + '_CLOSE'};",
                    f"{t}\tdefault: return {wconst(cn)};",
                    f"{t}}} //end switch"]

        out += [f'weight "{cn}"', "{", f"\tswitch({ident(cn)})", "\t{",
                "\t\tcase 1: return 0;", "\t\tdefault:", "\t\t{"]
        if ammo:
            out += [f"\t\t\tswitch({ident(ammo)})", "\t\t\t{",
                    "\t\t\t\tcase 1: return 0;",
                    "\t\t\t\tdefault:", "\t\t\t\t{"]
            out += fire_value(5)
            out += ["\t\t\t\t} //end default", "\t\t\t} //end switch"]
        else:
            out += fire_value(3)
        out += ["\t\t} //end default", "\t} //end switch", "} //end weight", ""]
    return "\n".join(out)


def gen_character(items, weapons, src, name="dkbot"):
    """Gladiator's hunk renamed, with our weight files: the library needs every characteristic."""
    consts = [f"#define {wconst(cn):<28}{item_weight(cn, kind, weapons)}"
              for cn, (kind, _r, _p) in items.items()]
    # _LOW: a carried weapon nearly out of ammo.
    consts += [f"#define {wconst(cn) + '_LOW':<28}"
               f"{max(10, item_weight(cn, 'weapon', weapons) // 2)}"
               for cn, (kind, _r, _p) in items.items()
               if kind == "weapon" and WEAPON_AMMO.get(cn)]
    consts += [f"#define {'W_STATION':<28}{W_STATION}",
               f"#define {'W_STATION_TOPUP':<28}{W_STATION_TOPUP}"]
    weapon_consts = [c for c, (k, _r, _p) in items.items() if k == "weapon"]

    item_file = "\n".join([HEADER.format(name=f"{name}_i.c -- item weights"),
                            '#include "inv.h"', '#include "game.h"', ""]
                           + consts + ["", '#include "fw_items.c"', ""])
    weap_file = "\n".join([HEADER.format(name=f"{name}_w.c -- weapon weights"),
                            '#include "inv.h"', '#include "game.h"', ""]
                           + [f"#define {wconst(c):<28}"
                              f"{fight_weight(c, weapons)}"
                              for c in weapon_consts]
                           + [f"#define {wconst(c) + '_CLOSE':<28}"
                              f"{max(CLOSE_FLOOR, fight_weight(c, weapons) // 8)}"
                              for c in weapon_consts if c in SPLASH]
                           + ["", '#include "fw_weap.c"', ""])

    base = open(os.path.join(src, "bots", "hunk_c.c")).read()
    char_file = base.replace('character "hunk"', f'character "{name}"')
    char_file = re.sub(r'(CHARACTERISTIC_WEAPONWEIGHTS\s+)"[^"]*"',
                       rf'\1"bots/{name}_w.c"', char_file)
    char_file = re.sub(r'(CHARACTERISTIC_ITEMWEIGHTS\s+)"[^"]*"',
                       rf'\1"bots/{name}_i.c"', char_file)
    return char_file, item_file, weap_file


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(root, "botdata/daikatana"))
    ap.add_argument("--weapons-json",
                    default=os.path.join(root, "vendor/gamedata/weapons.json"))
    ap.add_argument("--from", dest="src",
                    default=os.path.join(root, "botdata/gladiator"))
    args = ap.parse_args()

    with open(args.weapons_json) as f:
        weapons = {w["classname"]: w for w in json.load(f)}

    os.makedirs(args.out, exist_ok=True)
    open(os.path.join(args.out, "inv.h"), "w").write(gen_inv_h(ITEMS))
    # Only when it changes: the module is compiled against it.
    header = os.path.join(root, "include/dk/dk_inventory.h")
    text = gen_inventory_h(ITEMS, weapons)
    if not os.path.exists(header) or open(header).read() != text:
        open(header, "w").write(text)
        print(f"wrote {os.path.relpath(header, root)} -- run make to rebuild the module")
    open(os.path.join(args.out, "items.c"), "w").write(gen_items_c(ITEMS))
    open(os.path.join(args.out, "weapons.c"), "w").write(
        gen_weapons_c(weapons, ITEMS))
    open(os.path.join(args.out, "fw_items.c"), "w").write(
        gen_fw_items(ITEMS, weapons))
    open(os.path.join(args.out, "fw_weap.c"), "w").write(
        gen_fw_weap(ITEMS, weapons))

    open(os.path.join(args.out, "sounds.c"), "w").write(SOUNDS_C)

    copied = 0
    for name in COPY_FROM_GLADIATOR:
        src = os.path.join(args.src, name)
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(args.out, name))
            copied += 1
    for d in COPY_DIRS:
        src = os.path.join(args.src, d)
        if os.path.isdir(src):
            shutil.copytree(src, os.path.join(args.out, d), dirs_exist_ok=True)
    os.makedirs(os.path.join(args.out, "maps"), exist_ok=True)

    char_file, item_file, weap_file = gen_character(ITEMS, weapons, args.src)
    bots = os.path.join(args.out, "bots")
    os.makedirs(bots, exist_ok=True)
    open(os.path.join(bots, "dkbot_c.c"), "w").write(char_file)
    open(os.path.join(bots, "dkbot_i.c"), "w").write(item_file)
    open(os.path.join(bots, "dkbot_w.c"), "w").write(weap_file)

    named = sum(1 for cn in ITEMS if cn in weapons)
    print(f"wrote {args.out}: inv.h, items.c ({len(ITEMS)} items, "
          f"{sum(len(p) for _k, _r, p in ITEMS.values())} pickups), "
          f"weapons.c ({named} weapons from weapons.json)")
    print(f"copied {copied} shared configs + {len(COPY_DIRS)} directories "
          f"from {os.path.relpath(args.src, root)}")
    print("character: bots/dkbot_c.c (+ dkbot_i.c item weights, "
          "dkbot_w.c weapon weights)")


if __name__ == "__main__":
    main()
