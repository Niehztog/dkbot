# Navigation data

The bot library routes on one AAS file per map. `aas/` has them for the 21 stock multiplayer maps; any other map needs one compiled.

## Generating

```sh
tools/gen-aas.sh e1dm1 [map...]    # these maps
tools/gen-aas.sh --all             # every map of MultiplayerMaps.json the install has
```

Each map's `.aas` is installed into `botdata/<gamedir>/maps/`: copied from `aas/` where it is there, else compiled with BSPC under wine. `--all` skips maps missing from the install or already installed, and carries on past a map that fails.

| Variable | Effect |
|---|---|
| `PAKDIR=<dir>` | the game's `data/` directory (default `vendor/gamedata`) |
| `DK_BOTLIB_GAMEDIR=<name>` | install into `botdata/<name>/maps/` (default `daikatana`) |
| `DK_FORCE=1` | compile even where `aas/` has the map, and rebuild installed maps under `--all` |
| `BSPC=<path>` | the compiler (default: BSPC 1.4, `tools/vendor/bspc/bspc.exe` in the submodule) |
| `BSPC_RUN=<launcher>` | how to start it (default `wine`) |
| `GLAD=<dir>` | the `gladiator-bot-restored` checkout (default: the submodule) |

Use BSPC 1.4 unpatched, on an x86 host: under box86 it compiles different geometry.

A freshly compiled `.aas` has no reachability. The library computes it on the map's first load and writes it back into the installed file, so run the server on the map once (`tools/run-dkded.sh <map>`) before copying the file into `aas/`. CTF and deathtag maps abort a deathmatch server: load them with `DK_ARGS="+set ctf 1"`, or `DK_ARGS="+set deathtag 1"` for `e1dt1`.

## Pipeline

```
pak entry     --tools/dkpak.py-->        Daikatana BSP, IBSP v41
BSP v41       --tools/dkbsp.py bspc-->   Quake II BSP, IBSP v38, prepared for BSPC
BSP v38       --bspc -bsp2aas-->         <map>.aas
```

In the game the library reads the map through the engine's file system and converts it in memory (`botlib_patch/dk_maps.c`), exactly as `tools/dkbsp.py convert` does. The conversion:

- keeps lumps 0..18, cuts each leaf from 32 to 28 bytes and stamps version 38;
- drops `model` keys naming an external model, and empties a lighting lump too large for BSPC;
- adds `CONTENTS_LAVA` to water with 0x800 (harmful water), and `CONTENTS_SOLID` to 0x80 and 0x200 (walkways, fences and grates, which stop players) except on ladders.

`bspc` adds two changes for BSPC:

- a face for every brush side that lacks one where it bounds open space: BSPC splits space only along sides with a face, and Daikatana's maps lack many. If BSPC still fails, `gen-aas.sh` falls back to the plain conversion.
- Quake II's not-in-deathmatch spawnflag 0x800 wherever Daikatana's 0x8000 is set.

## Physics

`dkbot/botlib_glue.c` sets the physics the library computes reachability with:

| libvar | value | from |
|---|---|---|
| `sv_gravity` | 800 | `pmove_state_t.gravity` |
| `sv_step` | 18 | `PM_StepSlideMove` |
| `sv_jumpvel` | 325 | a Hiro bot's 66-unit jump (`PM_CheckJump`) |
| `sv_maxbarrier` | 48 | Quake II's barrier-to-jump ratio, applied to 66 |
