# Licensing

dkbot's own code is MIT. The Gladiator Bot it builds on is free for non-commercial use only, so a working installation as a whole may be played with but not sold.

## In this repository

| Path | Terms |
|---|---|
| `shim/`, `launcher/`, `dkbot/`, `tools/`, `include/`, `tests/`, `docs/`, the build system | **MIT** ([LICENSE](LICENSE)). Written for this project, the reverse-engineered ABI headers included. |
| `botlib_patch/` | **Derived work**: modified copies of `gladiator-bot-restored/botlib/*.c`, under that project's terms (below). `dk_maps.c` is written for this project and is MIT. |
| `aas/` | **Derived from the game's maps**: one navigation file per multiplayer map, compiled from the retail BSPs. It describes only the space a player can move through and holds no art, textures, lightmaps, entities or text; it cannot reproduce a map and is useless without the game. |
| `gladiator-bot-restored/` | **A git submodule**: this repository records only which commit to fetch. |

## Not in this repository

- **Daikatana** (Ion Storm / Eidos, 2000) is a commercial game; you must own it. None of its files are distributed here; only `aas/` is computed from them. No Daikatana source code is included, used or required: `include/dk/` and `docs/` were derived from the publicly released binaries and their debug symbols.
- **The Daikatana 1.3 patch** is the work of the community patch team. `tools/fetch-dk.sh` downloads it from their release page; nothing of it is redistributed here.
- **The Gladiator Bot** (Jan Paul "Mr. Elusive" van Waveren, 1999) is compiled from the `gladiator-bot-restored` submodule, which is distributed under the original 1999 Gladiator Bot licence: free, non-commercial use only, not an OSI-approved licence.
- **BSPC** (1999), the navigation compiler, is run from the same submodule as a build tool and is not redistributed.
- **Quake III Arena** (id Software, GPL) is read as a reference only. Nothing here is taken from it, and nothing may be: GPL code can be combined with neither the Gladiator Bot's terms nor MIT.
