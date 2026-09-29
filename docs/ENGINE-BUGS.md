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

## A match that never ends

`ChooseNextMap` takes the deathmatch map after the current one in `MultiplayerMaps.json` without checking that it exists; only its other paths fall back to `e1dm1a`. Six stock maps are followed by one that 1.3 does not ship: e1dm2a (e1dm3), e1m3b (q1dm3), e2m2a (e2dm3), e3m1c (e3dm3), e4m5a (e4nizcorpses) and slicedm1 (slicedm2). `P_ExitLevel` clears the intermission before its `changelevel` fails with `Can't find map maps/e1dm3.bsp`, and the next frame's fraglimit or timelimit check ends the match again, so the intermission restarts forever. Each key press to continue fires the weapon, and once the leader leaves, play resumes in the intermission's view, which `P_ExitLevel` only partly undoes. `dm_same_map 1`, `sv_random_map 1` or a `maps_dm` list avoid it, since those picks are checked.

## Every say counts as spam

`concmd_CheckSpam` tests `last_message_frame - framenum` against `p_spamticks`, which never holds, so every say counts and the 21st since the sender last respawned kicks it (`p_spamcount 20`, `p_spamkick 1`). Chatting bots reach that when they live long, and in the intermission loop above, where none dies. The check skips a client whose `gclient_s.bVerifiedBot` is set, which nothing in the engine sets; dkbot sets it while a bot's command runs.
