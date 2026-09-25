# Debugging and measuring

## Engine console

The engine's own commands are in the Patch v1.3 `ReadMe.pdf` that ships with the game. Useful with bots (cheats need `cheats 1`): `status`, `spawn list`, `give_inventory list`, `beam <x> <y> <z>`, and `weapon_give_1` to `weapon_give_6`, four weapons with ammo each:

1. Disruptor Glove, Discus, Silverclaw, Glock
2. Ion Blaster, Venomous, Bolter, Slugger
3. C4, Sunflare, Stavro's Stave, Kineticore
4. Shotcycler, Hammer of Hades, Ballista, Ripgun
5. Sidewinder, Poseidon's Trident, Wyndrax's Wisp, Novabeam
6. Shockwave, Eye of Zeus, Nharre's Nightmare, Metamaser

## Environment

| Variable | Read by | Effect |
|---|---|---|
| `DK_MOD=<path>` | shim, launcher | the module to put in slot 0; without it the shim does nothing |
| `DK_MOD_GLOBAL=1` | shim | load the module `RTLD_GLOBAL`, so it can add names the engine looks up ([LOADER.md](LOADER.md)) |
| `DK_SHIM_VERBOSE=1` | shim | trace the shim on stderr |
| `DK_SEED=<n>` | shim | make a run a function of `n`: `time()` becomes a base plus the game clock |
| `DK_BOT_VERBOSE=1` | module | the module's log on stderr, status line included; `2` adds the reachability pass's progress |
| `DK_BOT_LOG_EVERY=<n>` | module | frames between status lines (default 100, ten seconds) |
| `DK_BOTLIB=<path>` | module | the AI library (default `gladiator_x64.so` beside the module); `none` or `0` runs bots without AI |
| `DK_BOTLIB_BASEDIR`, `DK_BOTLIB_GAMEDIR` | module | where the library reads its configs: `<basedir>/<gamedir>/` |
| `DK_BOT_CHARFILE`, `DK_BOT_CHARNAME` | module | the bot character (default `bots/dkbot_c.c`, `dkbot`; Gladiator's `bots/hunk_c.c`, `hunk` outside a `daikatana*` gamedir) |
| `DK_BOT_SPAWN=<n>` | module | bots to add when the first level loads |
| `DK_BOT_GIVE=<command>` | module | a client command run for each bot as it spawns, e.g. `weapon_give_4` |
| `DK_BOT_DUMP_ITEMS=1` | module | log classname, modelindex and model of every pickup on the map |
| `DK_BOT_LOG=<file>`, `DK_BOT_CONSOLE=1` | module, Windows | the log to a file, or to a console window of its own |
| `DK_GAME=<path>` | launcher | the `daikatana.exe` to start (default: the one beside the launcher) |

`tools/run-dkded.sh` sets `DK_MOD`, the library and config variables for this checkout and both verbose switches, and reads:

| Variable | Effect |
|---|---|
| `DK_INSTALL=<dir>` | run an existing installation in place instead of the vendored build |
| `DK_RUNDIR=<dir>` | where the vendored build is staged (default `/tmp/dkrun`) |
| `DK_RUNNER=box64\|none` | force the launcher; by default only a host that is not x86-64 uses box64 |
| `DK_LOG=<path>` | the server log (default `<rundir>/server.log`) |
| `DK_PORT=<n>` | the port (default 27992); the engine also opens `port - 10` |
| `DK_SHIM=<path>` | preload another shim |
| `DK_ARGS="..."` | extra engine arguments |
| `DK_ROTATE=1` | keep the engine's map rotation instead of pinning the map |

## Status line

```
[dkbot] frame N: bots=N | <name>@(x y z) <frags>/<deaths> hp=N mv=N area=<n>/<reachabilities> w=<weapon>, ...
```

Per bot: position; frags and deaths (counted by the module); health; `mv`, the distance covered since the previous line; `area`, the AAS area it stands in and how many reachabilities leave it (0 or 1 is a dead end, `area=0` is outside the navigation data); `w`, the weapon the game says it holds. A low `mv` alone usually means a fight: check `area` first.

## Measuring a change

```sh
tools/bench.py -n 10 --maps e4dm2,e2dm2,e1dm2a new old=/path/to/old-checkout
tools/bench.py -n 10 --maps e1dm2a on 'off=,DK_BOTLIB_GAMEDIR=daikatana_variant'
```

An arm is `name[=tree][,VAR=value,...]`: the checkout whose `build/` and `botdata/` it runs (default: this one) and extra environment. Each run is a dedicated server with four bots for `--frames` server frames (default 3000) under `+set fixedtime 100`, `DK_SEED` set to the seed and `+set gib_enable 0` ([ENGINE-BUGS.md](ENGINE-BUGS.md)), so seed n plays the same game in every arm until the arms differ. Per arm and map it prints mean and spread of frags, kills, deaths to the map, self-kills, C4 deaths and idle, the status lines at which a live bot had moved under 32 units; it names any run that stalled or ended in an engine error, whose log is in `/tmp/dkbench`.

Use ten seeds or more per arm, on a map with the hazard in question and one without. A change to how bots fight also needs mixed games, the change on half the bots, then the halves swapped. `make botlib AUTOINIT=pattern` checks the library for an uninitialized read: it plays different games from the default build if something reads one.

## Heap corruption

A fault inside `calloc` or `Z_TagMalloc`, or a glibc heap abort, is an earlier write past a heap block. `make gmalloc` builds `build/gmalloc.so`, a malloc that ends every block against an inaccessible page and never reuses memory, so the write faults where it happens. Preload it ahead of the shim:

```sh
make gmalloc
DK_SHIM=$PWD/build/gmalloc.so:$PWD/build/dk_preload.so BOX64_MALLOC_HACK=1 BOX64_SHOWSEGV=1 \
DK_PORT=28500 DK_SEED=1 DK_BOT_SPAWN=4 DK_ARGS="+set fixedtime 100 +set gib_enable 0" \
  tools/run-dkded.sh e1m3b 600
```

Under box64 the preload needs `BOX64_MALLOC_HACK=1`, and `BOX64_SHOWSEGV=1` prints the faulting instruction as `x64pc` for `addr2line -f -C -e vendor/<build>/dbgsyms/dkded.dbg`.

| Variable | Effect |
|---|---|
| `GMALLOC_EVERY=<n>`, `GMALLOC_PHASE=<0..n-1>` | guard only every nth allocation where the blocks would exceed `vm.max_map_count`; phases 0 to n-1 cover them all |
| `GMALLOC_WRITEONLY=1` | fault on writes only, past an engine that reads beyond a block |
| `GMALLOC_ALIGN=1` | end blocks exactly at the guard page; only box64 tolerates it |
