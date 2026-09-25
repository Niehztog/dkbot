# Engine bugs

Defects in the Daikatana 1.3 engine that show up in bot games.

## Kills scored as suicides

`Client_DeathMessage` reads the global `client_deathtype` before the attacker. Hazards (`trigger_hurt_touch`, `door_blocked`, `train_blocked` and others) set it, and only each client's prethink resets it, so a kill later in the same frame as a hazard touch is announced as a world death and scored as a suicide.

## Heap corruption after a switch away from the Daikatana

A switch made mid-swing leaves the sword's frame function (`daikatana_think` or `daikatana_followThrough`) in `playerHook_t.fxFrameFunc`, and the next weapon's animation runs it. It then clears two fields 0x20 bytes past the new weapon's `weapon_t`, inside the next heap chunk, and glibc fails much later. dkbot never switches a bot away from the sword while that function is pending; a player can still trigger it.

## A Nightmare pentagram outliving its controller

As it fades, `doPentagram` zeroes its own slot in its controller's `nmControllerHook_t`. Nothing clears its pointer to the controller when that goes, so a pentagram that outlives it scans the hook of whatever entity reuses the edict, reading past a smaller hook and possibly zeroing a word in another heap block. dkbot does not guard against it.

## Gibs read uninitialized stack

`ai_throw_gib` computes gib trajectories from uninitialized stack, so the same seed can play two different games. `+set gib_enable 0` avoids it, and `tools/bench.py` sets it.
