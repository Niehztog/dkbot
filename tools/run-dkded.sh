#!/bin/sh
# Run the dedicated server with the shim attached: tools/run-dkded.sh [map] [seconds]
set -eu

MAP="${1:-e1dm1}"
SECS="${2:-}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [ -n "${DK_INSTALL:-}" ]; then
	# Run in place and change nothing: it is the player's own install.
	RUNDIR="$(cd "$DK_INSTALL" && pwd)"
	STAGE=0
	[ -x "$RUNDIR/dkded" ] || { echo "no dkded in $RUNDIR" >&2; exit 1; }
	[ -d "$RUNDIR/data" ]  || { echo "no data/ in $RUNDIR" >&2; exit 1; }
else
	RUNDIR="${DK_RUNDIR:-/tmp/dkrun}"
	STAGE=1
	GAME="$(ls -d "$ROOT"/vendor/dk-*-x64/game 2>/dev/null | tail -1 || true)"
	[ -n "$GAME" ] || { echo "no vendor/dk-*-x64/game -- run tools/fetch-dk.sh, or set DK_INSTALL" >&2; exit 1; }
	[ -d "$ROOT/vendor/gamedata" ] || { echo "no vendor/gamedata -- copy a full data/ dir there (see docs/BUILD.md)" >&2; exit 1; }
fi
[ -x "$ROOT/build/dk_preload.so" ] || make -C "$ROOT" >/dev/null

RUNNER="${DK_RUNNER:-}"
if [ -z "$RUNNER" ]; then
	case "$(uname -m)" in
	x86_64|amd64) RUNNER=none ;;
	*)            RUNNER=box64 ;;
	esac
fi
if [ "$RUNNER" != none ]; then
	command -v "$RUNNER" >/dev/null || {
		echo "$RUNNER not found -- needed to run x86-64 binaries on $(uname -m)" >&2
		exit 1; }
fi

# Real copies, not symlinks: the engine looks for data/ beside /proc/self/exe.
if [ "$STAGE" = 1 ]; then
	mkdir -p "$RUNDIR"
	if [ ! -x "$RUNDIR/dkded" ] || [ "$GAME/dkded" -nt "$RUNDIR/dkded" ]; then
		echo "staging binaries into $RUNDIR"
		cp -a "$GAME"/. "$RUNDIR"/
	fi
	rm -rf "$RUNDIR/data"
	ln -s "$ROOT/vendor/gamedata" "$RUNDIR/data"
fi

LOG="${DK_LOG:-$RUNDIR/server.log}"
echo "running $MAP in $RUNDIR (log: $LOG)"
cd "$RUNDIR"
set -- +set dedicated 1 +set deathmatch 1
if [ "${DK_ROTATE:-0}" = 0 ]; then
	set -- "$@" +set dm_same_map 1 +set sv_auto_rotate_map 0 \
	           +set sv_random_map 0
fi
# Else a respawn can telefrag whoever stands on its spawn point.
set -- "$@" +set dm_spawn_farthest 1
[ -n "${DK_PORT:-}" ] && set -- "$@" +set port "$DK_PORT"
set -- "$@" ${DK_ARGS:-} +map "$MAP"
# Under box64, LD_PRELOAD would load the shim into box64 itself.
SHIM="${DK_SHIM:-$ROOT/build/dk_preload.so}"
if [ "$RUNNER" = none ]; then
	export LD_PRELOAD="$SHIM"
else
	export BOX64_LD_PRELOAD="$SHIM"
fi
export DK_MOD="$ROOT/build/dkbot.so"
if [ -f "$ROOT/build/gladiator_x64.so" ]; then
	export DK_BOTLIB="${DK_BOTLIB:-$ROOT/build/gladiator_x64.so}"
	export DK_BOTLIB_BASEDIR="${DK_BOTLIB_BASEDIR:-$ROOT/botdata}"
	export DK_BOTLIB_GAMEDIR="${DK_BOTLIB_GAMEDIR:-daikatana}"
	# Gladiator's default character fails to load from a daikatana* gamedir.
	case "$DK_BOTLIB_GAMEDIR" in
	daikatana*)
		export DK_BOT_CHARFILE="${DK_BOT_CHARFILE:-bots/dkbot_c.c}"
		export DK_BOT_CHARNAME="${DK_BOT_CHARNAME:-dkbot}"
		;;
	esac
fi
export DK_SHIM_VERBOSE="${DK_SHIM_VERBOSE:-1}"
export DK_BOT_VERBOSE="${DK_BOT_VERBOSE:-1}"

AASDIR="${DK_BOTLIB_BASEDIR:-$ROOT/botdata}/${DK_BOTLIB_GAMEDIR:-daikatana}/maps"
[ -f "$AASDIR/$MAP.aas" ] || echo "warning: no $AASDIR/$MAP.aas -- run tools/gen-aas.sh $MAP" >&2

if [ "$RUNNER" = none ]; then set -- ./dkded "$@"; else set -- "$RUNNER" ./dkded "$@"; fi
if [ -n "$SECS" ]; then
	timeout "$SECS" "$@" </dev/null 2>&1 | tee "$LOG" || true
else
	"$@" 2>&1 | tee "$LOG"
fi
