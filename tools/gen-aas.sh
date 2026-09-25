#!/bin/sh
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GLAD="${GLAD:-$ROOT/gladiator-bot-restored}"
# BSPC 1.4 is the Win32 bspc.exe; the Linux binary beside it is 1.2.
BSPC="${BSPC:-$GLAD/tools/vendor/bspc/bspc.exe}"
BSPC_RUN="${BSPC_RUN:-wine}"
PAKDIR="${PAKDIR:-$ROOT/vendor/gamedata}"
GAMEDIR="${DK_BOTLIB_GAMEDIR:-daikatana}"
DEST="$ROOT/botdata/$GAMEDIR/maps"

[ $# -ge 1 ] || { echo "usage: $0 <map> [map...] | --all" >&2; exit 1; }

need_bspc() {
	[ -f "$BSPC" ] || {
		echo "missing bspc: $BSPC" >&2
		echo "  it ships in the gladiator-bot-restored submodule (git submodule" >&2
		echo "  update --init), under tools/vendor/bspc/; or set GLAD or BSPC" >&2
		return 1; }
	command -v "$BSPC_RUN" >/dev/null || {
		echo "$BSPC_RUN not installed -- needed to run $BSPC" >&2
		return 1; }
	return 0
}

mkdir -p "$ROOT/build/maps" "$ROOT/build/aas" "$DEST"
WORK=$(mktemp -d)                 # bspc drops bspc.log into the CWD
trap 'rm -rf "$WORK"' EXIT

run_bspc() {
	( cd "$WORK" && "$BSPC_RUN" "$BSPC" -bsp2aas "$ROOT/build/maps/$MAP.bsp" \
		-output "$ROOT/build/aas" >bspc.out 2>&1 ) && [ -f "$ROOT/build/aas/$MAP.aas" ]
}

build_one() {
	MAP="$1"
	echo "=== $MAP ==="
	if [ -f "$ROOT/aas/$MAP.aas" ] && [ "${DK_FORCE:-0}" = 0 ]; then
		cp "$ROOT/aas/$MAP.aas" "$DEST/$MAP.aas"
		echo "  installed aas/$MAP.aas ($(stat -c%s "$DEST/$MAP.aas") bytes, shipped)"
		return 0
	fi
	[ -d "$PAKDIR" ] || { echo "  no game data: $PAKDIR (see docs/BUILD.md)" >&2; return 1; }
	need_bspc || return 1
	"$ROOT/tools/dkbsp.py" bspc "$PAKDIR" "$MAP" "$ROOT/build/maps/$MAP.bsp" || {
		echo "  BSP conversion failed" >&2; return 1; }
	rm -f "$ROOT/build/aas/$MAP.aas"
	if ! run_bspc; then
		# BSPC 1.4 can crash on the extra splits; the plain conversion compiles with holes.
		echo "  bspc failed on the prepared map; compiling the plain conversion instead" >&2
		"$ROOT/tools/dkbsp.py" convert "$PAKDIR" "$MAP" "$ROOT/build/maps/$MAP.bsp" >/dev/null &&
			run_bspc || {
			echo "  bspc failed; tail of its output:" >&2
			tail -5 "$WORK/bspc.out" >&2
			return 1
		}
	fi
	cp "$ROOT/build/aas/$MAP.aas" "$DEST/$MAP.aas"
	echo "  installed $DEST/$MAP.aas ($(stat -c%s "$DEST/$MAP.aas") bytes, compiled;"
	echo "  the engine calculates its reachability at first load)"
}

if [ "$1" = --all ]; then
	LIST="$PAKDIR/MultiplayerMaps.json"
	[ -f "$LIST" ] || { echo "no $LIST -- is $PAKDIR a Daikatana data/ directory?" >&2; exit 1; }
	MAPS=$(python3 -c 'import json,sys
print("\n".join(dict.fromkeys(e["map"] for e in json.load(open(sys.argv[1])) if e.get("map"))))' "$LIST")
	OK=0; SKIP=0; ABSENT=0; FAIL=0; FAILED=
	for MAP in $MAPS; do
		if [ -f "$DEST/$MAP.aas" ] && [ "${DK_FORCE:-0}" = 0 ] &&
		   { [ ! -f "$ROOT/aas/$MAP.aas" ] || cmp -s "$ROOT/aas/$MAP.aas" "$DEST/$MAP.aas"; }; then
			SKIP=$((SKIP + 1)); continue
		fi
		if [ ! -f "$PAKDIR/maps/$MAP.bsp" ] && [ -z "$("$ROOT/tools/dkpak.py" find "$PAKDIR" "maps/$MAP.bsp")" ]; then
			ABSENT=$((ABSENT + 1)); continue
		fi
		if build_one "$MAP"; then OK=$((OK + 1)); else FAIL=$((FAIL + 1)); FAILED="$FAILED $MAP"; fi
	done
	echo "built $OK, already present $SKIP, not in this install $ABSENT, failed $FAIL${FAILED:+:$FAILED}"
	exit 0
fi

for MAP in "$@"; do
	build_one "$MAP" || exit 1
done
