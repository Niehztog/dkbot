# Building

Both platforms build on Linux or WSL; the Windows binaries are cross-compiled with MinGW.

```sh
sudo apt install build-essential python3 git gcc-mingw-w64-x86-64   # Debian/Ubuntu
sudo apt install gcc-x86-64-linux-gnu                                # only on a host that is not x86-64
```

## Packages

```sh
git clone --recurse-submodules https://github.com/Niehztog/dkbot.git
cd dkbot
tools/fetch-dk.sh
.github/package.sh dev vendor/dk-2025-12-21-x64/game/data/weapons.json build/dist
```

`tools/fetch-dk.sh` downloads the supported game release into `vendor/`; the bot configuration is generated from its `weapons.json`. `package.sh` builds everything and writes `build/dist/dkbot-dev-linux-x64.tar.gz` and `build/dist/dkbot-dev-windows-x64.zip`, which install as the [README](../README.md#install) describes.

The parts also build on their own: `make` (the Linux shim and module), `make botlib` (the bot library), `make windows` (the Windows module, launcher and library) and `make test`. A clone without the submodule needs `git submodule update --init`.

## Running from the checkout

A Linux dedicated server, for development:

```sh
cp -a /path/to/your/daikatana/data vendor/gamedata
make && make botlib
tools/extract-botdata.py && tools/gen-botcfg.py && tools/gen-aas.sh --all
tools/run-dkded.sh e1dm1
```

`tools/run-dkded.sh` runs the vendored `dkded` natively or, on a host that is not x86-64, under box64; [DEBUGGING.md](DEBUGGING.md) lists its switches. On Windows, `build\dkbot-launch.exe` runs from the checkout with `DK_GAME` set to the game's `daikatana.exe`, and finds `botdata\` one directory up.

## A new game release

After `tools/fetch-dk.sh <tag>`, `make check-boundary` checks the Linux engine interface and `make check-pdb DK_WIN=<folder with daikatana.exe and daikatana.pdb>` the Windows one (needs `llvm-pdbutil`). The launcher warns on every Windows build but the one `SUPPORTED_BUILD` in `launcher/dkbot-launch.c` names.
