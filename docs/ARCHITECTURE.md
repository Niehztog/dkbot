# Architecture

Every engine fact here comes from the official 1.3 `-dbgsyms` packages and disassembly. Offsets live in `include/dk/dk_boundary.h`; `make check-boundary` (Linux) and `make check-pdb` (Windows) re-verify them against a new build.

## Layers

```
Daikatana engine (closed, unmodified)
      |  dlopen/dlsym boundary; slot 0 taken by the shim or the launcher (LOADER.md)
dkbot/   the slot-0 module, the counterpart of Gladiator's game/bl_*.c glue
      |  bot_import_t / bot_export_t (GetBotAPI)
botlib   gladiator-bot-restored/botlib plus the botlib_patch/ forks, built as gladiator_x64.so
```

Everything above `dkbot/dk_platform.h` is shared by Linux (`dk_platform_posix.c`) and Windows (`dk_win.c`).

## The module

- `entry.c` overrides `dll_Entry`, chaining to the engine's, for the server and level messages, `dll_ClientConnect`, where a real client claiming a bot's edict takes it back, and `dll_ClientDisconnect`, where a bot the engine drops, by a kick, is forgotten; the engine frees its slot.
- `frame.c` wraps `globals.RunFrame` at server init and every level load, but only while it holds the exported `P_RunFrame`.
- `bots.c` runs each bot as a fake client: a free client edict, taken from `maxclients` down, goes through `ClientConnect`, `ClientUserinfoChanged` and `ClientBegin`, then gets a synthesized `usercmd_t` through `ClientThink` every frame. Bots spawn one per second, since spawn-point selection puts a batch on one point.
- `clientbuf.c` re-arms each bot's engine slot every frame, since `SV_InitGame` reallocates `svs.clients`: `cs_spawned`, message buffers (a per-client send to a slot without them aborts the server), `lastmessage` and `idletime` against the timeout and idle kicks, qport -1, and on a listen server an address that keeps bot packets out of the host's loopback. Never swallow a per-client send instead: senders stage into a shared buffer that only the real call empties.
- The usercmd is Gladiator's `game/bl_main.c` conversion: `sidemove` along Quake II's right vector, moves, jump and crouch as +-400, `msec` = 1000 * thinktime, angles relative to `delta_angles`, and `ClientThink` called twice with half the `msec`, or pmove at the server's 10 Hz fails steps, stairs and water jumps. A dead bot respawns on an attack-button transition, and only because the module runs `dll_ClientBeginServerFrame` for it.
- `botlib_glue.c` loads `gladiator_x64.so` from beside the module (or `DK_BOTLIB`), supplies `bot_import_t`, sets the libvars and hands the library the engine's `FS_LoadFile`, through which it reads the map. Each frame it calls `BotStartFrame`, `BotUpdateEntity` for every live edict, and `BotUpdateClient` and `BotAI` per bot. The library needs:
  - health in `stats[1]`, or the AI thinks it is dead and does not move;
  - the full client state (`snapshot.c`), with `delta_angles` passed only as their change since the last update, as `bl_main.c` does by clearing them;
  - every live edict, or it sees an empty world;
  - the engine's own model table, `sv.configstrings` from `CS_MODELS`: it recognises pickups by model and finds a door's button by its `*n` model;
  - `ent = 0` from a trace that hits nothing, and Daikatana's see-through solid bits 0x80 and 0x200 in a player-clip mask, as the engine's player trace has them.
- `snapshot.c` states what Daikatana's entities do not show in Quake II's conventions: player, alive, shooting, invulnerable and team go into `entity_state_t.modelindex2`, the timed effects and armour into `stats` 20..23 (`include/dk/dk_entstate.h`). Game-specific behaviour belongs in the AI forks (`botlib_patch/`), not in values forged for Quake II logic.
- `cmd.c` records the game's client-command handlers as the game registers them through `AddCommand`, from `dll_ServerLoad`, so the hook goes in before that call; a bot's commands run through them, with their arguments served by redirected `GetArgc`/`GetArgv`/`GetArgs`. `GetGameAPI` copies `gi` and `serverState` afresh at every server start, so the hooks and the redirect go in again at every message 12. The game registers `say` only on a listen server, so bots chat only there. The library's `use <weapon>` becomes a weapon switch. `bot` is registered with `Cmd_AddCommand`, so only the console and rcon reach it.
- Without the library, its configs or the map's navigation data, the console says what is missing, `bot add` refuses and pending bots wait; a bot whose `BotSetupClient` fails is dropped.

`tools/gen-botcfg.py` writes the library's configs into `botdata/daikatana/` from the game's `weapons.json`, and the same inventory numbers into `include/dk/dk_inventory.h`. Item indices start at 80 (`DK_ITEM_BASE`), clear of the Quake II slots the library reads by number; `inv.h` must also carry the library's own 200+ slots, or bots run without a character.

The library is linked `-Bsymbolic`: the engine exports its globals, and the library's own `ctf`, `logfile` and the like would bind to them.

## Engine facts

- Daikatana is a Quake II fork: `usercmd_t` is Quake II's, `game_import_t`/`game_export_t` extend Quake II's. The world module talks to the engine through `serverState_t` (`gstate`), handed over by `dll_Entry` message 12; anything aimed at clients goes through it, not `gi`.
- `gstate->mapName` is empty during `dll_LevelLoad`.
- `GetGameAPI` runs when a server starts: the first map, and a map after `disconnect` or `killserver`. A map change on a running server sends `dll_LevelExit` and `dll_LevelLoad` only.
- `edict_s` keeps Quake II's public header. `absmin`/`absmax` are the link box, one unit larger per side: take box constants from `PM_CheckDuck`.
- `player_record_t.deaths` is never written.
- `gi.cvar()` creates the cvar it reads, and an empty `teamplay` aborts the game: use `Cvar_VariableString`.
- Paks and BSPs are not Quake II's (72-byte pak entries, compressed; IBSP v41): use `tools/dkpak.py` and `tools/dkbsp.py`.
- The use key: `Client_Use_f` runs on a fresh press of `BUTTON_USE`, traces an 8-unit box 80 units along the view from the eye and uses what it hits. Most doors and buttons need it. A `func_door` or `func_door_rotate` opens on touch only with spawnflag 0x10, is shot open with health and opened by its trigger with a targetname; a `func_door_secret` without targetname opens on use or when shot; a `func_button` fires on touch only with spawnflag 1 and is shot with health. A door with spawnflag 0x40 toggles. A `func_plat` with a targetname moves only when fired.
- Health stations (`misc_hosportal`, and Episode 2's `misc_fountain`, renamed `misc_lifewater`) start on use below maximum health and heal a point every 0.2 s while the user stays within 64 units with the station in view, up to 50. Then they show frame 0 and ignore use while recharging, and frame 1 again when full.
- pmove is Quake II's plus edge friction (quadrupled while no floor lies within 36 units below a box 32 units ahead) and water currents from the `CONTENTS_CURRENT_*` bits, which Daikatana's pools carry. A box, even a falling one, steps up 18 units only where it fits 18 units higher, so under a low ceiling a crouching player can step where a standing one cannot.
- Many solids are entities the AAS does not hold: props, health stations, trees, doors, plats, trapdoors, and `func_explosive` breakables (windows, paper walls, planks, gates), solid unless spawnflag 1 or 0x200 is set.

## Inventory

- `userEntity_t.inventory` is a list of `invenItem_t` nodes, each pointing at a `userInventory_s` whose `name` is the pickup's classname, the key `serverState_t.InventoryFindItem(list, name)` takes.
- Ammo items (flag 0x20000) keep their count and a `weaponInfo_s *` past the base struct; weapon items store their `weaponInfo_s *` at the same offset. Read a count only when the ammo flag is set and `size` covers it.
- Windows lays items out differently: `modelName` is `char[MAX_PATH]`, 4096 bytes on Linux and 260 on Windows, so every later field moves. The `DK_INV*` macros in `include/dk/dk_boundary.h` carry both layouts.
- A weapon switch is `weaponSelect(ent, weaponInfo_s *)` and lands a frame or two later. The Slugger's item holds its cordite mode's `weaponInfo_s`, so it is selected through that one's `select_func`, as a player's key does. `dkbot_select_weapon` never switches away from the Daikatana while `playerHook_t.fxFrameFunc` is pending ([ENGINE-BUGS.md](ENGINE-BUGS.md)), and `bots.c` re-applies a request while the game disagrees, since the library asks only once.
- Daikatana's five skills (power, attack, speed, acro, vitality; `*_boost` in binary names) are permanent levels in `playerHook_t`, which also holds the timed effects as absolute expiries. The AI gets each carried item as 1, ammo as its count, in its inventory slot, the skill levels in theirs, and the timed effects and armour in `stats` 20..23.
