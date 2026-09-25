# dkbot -- multiplayer bots for Daikatana 1.3

> **Disclaimer:** dkbot is an independent project, not affiliated with or endorsed by the Daikatana 1.3 project or its maintainers. Please report problems here, not to them.
>
> dkbot is provided "as is", without warranty of any kind; use it at your own risk. See [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).

**Status: experimental.** Expect bugs.

dkbot adds bots to Daikatana 1.3 deathmatch without modifying the game. Their AI is Mr. Elusive's Gladiator Bot for Quake II (1999), built from the [gladiator-bot-restored](https://github.com/Niehztog/gladiator-bot-restored) submodule. Bots run on the server, so clients on any platform can play against them.

## Requirements

The 64-bit build of [Daikatana 1.3 v12-21-2025](https://github.com/maraakate/daikatana/releases/tag/v12-21-2025):

- **Linux x86-64:** the dedicated server, `dkded`, with the game's `data/`.
- **Windows x64:** the game as the 1.3 x64 installer sets it up, with `daikatana.pdb` beside `daikatana.exe`. Bots run on a listen or a dedicated server.

Not supported: the 32-bit builds, a listen server in the Linux game, and Windows' `dk_ded.exe`.

## Install

Download the package for your platform from the [releases](https://github.com/Niehztog/dkbot/releases).

**Linux:** extract it into the directory that holds `dkded` and `data/`, and start the server with its script; engine arguments pass through (default `+map e1dm1`):

```sh
tar xzf dkbot-<version>-linux-x64.tar.gz -C /path/to/daikatana
cd /path/to/daikatana && ./dkbot-server.sh
```

**Windows:** extract it into the folder that holds `daikatana.exe`, and start `dkbot-launch.exe` in place of `daikatana.exe`. Game arguments pass through, e.g. `dkbot-launch.exe +set deathmatch 1 +map e1dm1`; `+set dedicated 1` makes it a dedicated server. Windows SmartScreen and some antivirus software warn about the launcher, because it is unsigned and injects the bot module into the game.

To uninstall, delete the files the package added; dkbot changes no game file.

## Playing

On the server console, or over rcon:

```
bot add 4           # four bots, one per second
bot list
bot remove <name>   # "bot remove" drops the last bot, "bot remove all" every bot
```

Players join with `connect <server>:27992`. Bots stay through map changes and count against `maxclients`. Adding `bot` to `sv_rcon_banned_commands` limits it to `rcon_master_password`.

- Deathmatch only.
- Bots never use the Slugger's cordite grenades or inventory items.
- Bots play only on maps with navigation data. The package has it for every stock multiplayer map; [docs/AAS.md](docs/AAS.md) makes it for others.
- Bots sometimes get stuck in a dead end for a while.

If `bot` is an unknown command, the game was not started through `dkbot-server.sh` or `dkbot-launch.exe`. If the console says `dkbot: bots unavailable: ...`, it names what is missing.

## Documentation

- [docs/BUILD.md](docs/BUILD.md) -- building from source
- [docs/AAS.md](docs/AAS.md) -- navigation data
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) -- how the module works
- [docs/LOADER.md](docs/LOADER.md) -- how the module gets into the engine
- [docs/DEBUGGING.md](docs/DEBUGGING.md) -- switches, status line, measuring
- [docs/ENGINE-BUGS.md](docs/ENGINE-BUGS.md) -- engine defects seen in bot games
- [botlib_patch/README.md](botlib_patch/README.md) -- changes to the bot library
- [CLAUDE.md](CLAUDE.md) -- instructions for coding agents

## License

dkbot's own code is MIT ([LICENSE](LICENSE)). `botlib_patch/` and the bot library are under the 1999 Gladiator Bot licence, free for non-commercial use only; see [NOTICE.md](NOTICE.md).

## Reporting problems

Open an issue at https://github.com/Niehztog/dkbot/issues with your platform, the map, the dkbot version and a log of a run with `DK_BOT_VERBOSE=1` ([docs/DEBUGGING.md](docs/DEBUGGING.md)).
