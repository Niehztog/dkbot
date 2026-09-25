# botlib forks

Copies of `gladiator-bot-restored/botlib/*.c` that the Makefile compiles instead of the originals, since the submodule is read-only. `be_ai2_dmdk.c` is the deathmatch AI, forked from `be_ai2_dmq2.c`. `dk_maps.c` is ours: it gives the library Daikatana's maps.

Each fork differs from upstream only in blocks marked `dkbot`:

    diff -u gladiator-bot-restored/botlib/be_ai_goal.c botlib_patch/be_ai_goal.c

Upstream code that reads a Quake II inventory slot we never fill is left alone only where zero is right: `BotCanAndWantsToRocketJump` and `BotBattleUseItems`.

## Moving to a new upstream pin

1. Update the submodule: `git submodule update --remote gladiator-bot-restored`, or check out a commit in it.
2. Three-way merge each fork: `git merge-file --diff3 botlib_patch/<fork>.c <old upstream file> <new upstream file>`; the upstream of `be_ai2_dmdk.c` is `be_ai2_dmq2.c`. Where upstream rewrote a function, take the new one and re-apply our block. Check what each upstream change does to our blocks, even where it merges cleanly.
3. Keep upstream's `#if GLAD_SERVERFIX` blocks, or `GLAD_SERVERFIX=1` stops reaching them. Where upstream now carries a fix a fork made, take upstream's.
4. `make botlib`; `nm -u build/gladiator_x64.so` must list only libc and libm symbols. The library is compiled with `-w`, so a call to a renamed function fails only at load time.
5. Compiled with `-Wimplicit-function-declaration -Wincompatible-pointer-types -Wint-conversion`, the forks must warn no more than before.
6. A few seeds of `tools/bench.py` must play the same games before and after.
7. Commit the new pin together with the merged forks.

## Deviations

`be_ai2_dmdk.c`
- Live-player test, `EntityIsShooting`, `BotUpdateBattleInventory`, `BotUpdateInventory`: entity and inventory facts from `modelindex2` and `stats` 20..23 (`include/dk/dk_entstate.h`).
- `BotCheckAttack`: holds fire beyond the weapon's reach (`reach_<classname>` libvars), inside its own blast radius, and with the Zeus off target.
- `BotAimAtEnemy`: leads and scatters by projectile speed and reach, not by Quake II weapon names.
- `BotAggression`, `BotAttackMove`: weapon strength from the weapon config; a short-reach weapon fights within its reach.
- `BotAIBlocked`: opens doors with the use key or a shot, walks to their buttons and activators, calls plats that only a trigger moves, and avoids doors that stay shut.
- `AINode_DK_Station`: new node that uses health stations.
- `BotDeathmatchAI`: a bot trapped for 10 s without an enemy kills itself (`DK_AreaTrapped`); turns the view after the retreat node; takes water currents back out of the move.
- `BotSetupDeathmatchAI`: calls `DK_ForbidFatalJumps`.
- `BotCTFCarryingFlag`, `BotSameTeam`: Daikatana's flags and teams (never run).

`be_ai_move.c`
- Hazard tests, `BotGapDistance`, `BotMoveToGoal`: toxic liquid is lava 0x08, slime 0x10 and harmful water 0x800; a bot routes through it only to get out.
- `BotTravel_Jump`, `BotFinishTravel_Jump`, `BotTravel_WalkOffLedge`, `BotFinishTravel_WaterJump`, `BotTravel_Ladder`: travel fitted to Daikatana's pmove and speed.
- `BotCheckBlocked`: ignores grazed entities, sees obstacles at head height, and crouches where only a crouching box passes.
- `BotMoveToGoal`, `BotGetReachabilityToGoal`: avoids links the bot keeps failing, takes a jump from the area before it at speed, and routes a bot held up by a door, plat or player from the floor beside it.
- `BotReachabilityArea`: places a bot on a lip in the dry area.
- `BotWalkInDirection`: predicts from pmove's real take-off and refuses steps into toxic liquid.

`be_ai_goal.c`
- `BotChooseLTGItem`, `BotChooseNBGItem`: goals from within toxic liquid; a health station only while charged.
- `BotInitLevelItems`: item configs matched by model.

`be_aas_reach.c`
- `AAS_StoreReachability`: no links into lava or slime from outside; applies only when reachability is computed, and `aas/` carries the result.
- `AAS_Reachability_Ladder`: no links to area 0.
- `AAS_Reachability_Teleport`: Daikatana's `trigger_teleport`.
- `DK_ForbidFatalJumps`: at load, forbids links into traps, fatal jumps and falls, links through a `func_explosive` or a teleporter into a trap, and ladders up into crouch-only areas; marks areas with no way back (`DK_AreaTrapped`).

`be_aas_sample.c`
- `AAS_TraceClientBBox`: a split on the start moves 0.001 along, since players rest exactly `TRACEPLANE_EPSILON` above floors.

`be_aas_main.c`, `be_aas_bspq2.c`
- `AAS_LoadFiles`, `AAS_LoadBSPFile`: take the map the engine loaded, through `dk_maps.c`.

`dk_maps.c`
- Reads `maps/<map>.bsp` through the engine's `FS_LoadFile` and converts it as `tools/dkbsp.py convert` does; change both together.
