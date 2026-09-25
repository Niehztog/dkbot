#!/bin/sh
# Build the release packages: .github/package.sh <version> <weapons.json of the supported release> <outdir>
set -eu

VER=$1 JSON=$2
mkdir -p "$3"
OUT=$(cd "$3" && pwd)
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
BOTDATA="$WORK/botdata/daikatana"

tools/extract-botdata.py --out "$WORK/gladiator" >/dev/null
tools/gen-botcfg.py --weapons-json "$JSON" --from "$WORK/gladiator" --out "$BOTDATA" >/dev/null
git diff --quiet -- include/dk/dk_inventory.h ||
	{ echo "include/dk/dk_inventory.h does not match $JSON" >&2; exit 1; }
cp aas/*.aas "$BOTDATA/maps/"
python3 - "$BOTDATA/maps" <<'EOF'
import glob, os, struct, sys
maps = sorted(glob.glob(os.path.join(sys.argv[1], "*.aas")))
bare = [m for m in maps if not struct.unpack_from("<ii", open(m, "rb").read(), 80)[1]]
if not maps or bare:
    sys.exit("no reachability in: %s" % (", ".join(map(os.path.basename, bare)) or "(no .aas)"))
EOF

make -j"$(nproc)" all botlib windows test > "$WORK/build.log" 2>&1 ||
	{ cat "$WORK/build.log" >&2; exit 1; }

readme() {
	cat <<EOF
dkbot $VER -- multiplayer bots for Daikatana 1.3, $1

dkbot is an independent project, not affiliated with or endorsed by the
Daikatana 1.3 project or its maintainers; report problems at the address below,
not to them. It is experimental and provided "as is", without warranty of any
kind; see dkbot-LICENSE.txt and dkbot-NOTICE.md.

Requires the 64-bit Daikatana 1.3, release v12-21-2025.

Install: $2
Start:   $3
Bots:    "bot add 4" on the console adds four, "bot remove all" removes them.
Remove:  delete the files this archive added; no game file is changed.

https://github.com/Niehztog/dkbot
EOF
}

L="$WORK/linux"
mkdir -p "$L"
cp build/dk_preload.so build/dkbot.so build/gladiator_x64.so "$L/"
cp -r "$WORK/botdata" "$L/"
cp LICENSE "$L/dkbot-LICENSE.txt"
cp NOTICE.md "$L/dkbot-NOTICE.md"
cat > "$L/dkbot-server.sh" <<'EOF'
#!/bin/sh
# Start the dedicated server with dkbot: ./dkbot-server.sh [engine arguments]
set -eu
cd "$(dirname "$0")"
[ -x ./dkded ] || { echo "no dkded here: extract dkbot into your Daikatana 1.3 directory" >&2; exit 1; }
export LD_PRELOAD="$PWD/dk_preload.so${LD_PRELOAD:+:$LD_PRELOAD}"
export DK_MOD="$PWD/dkbot.so"
export DK_BOTLIB_BASEDIR="${DK_BOTLIB_BASEDIR:-$PWD/botdata}"
export DK_BOTLIB_GAMEDIR="${DK_BOTLIB_GAMEDIR:-daikatana}"
[ $# -gt 0 ] || set -- +map e1dm1
exec ./dkded +set dedicated 1 +set deathmatch 1 +set dm_spawn_farthest 1 "$@"
EOF
chmod +x "$L/dkbot-server.sh"
readme "Linux x86-64 dedicated server" \
	"extract this archive into the directory that holds dkded and data/." \
	"./dkbot-server.sh, with engine arguments if you like (default +map e1dm1)." \
	> "$L/dkbot-README.txt"
tar -C "$L" --owner=0 --group=0 --numeric-owner -czf "$OUT/dkbot-$VER-linux-x64.tar.gz" \
	dkbot-server.sh dk_preload.so dkbot.so gladiator_x64.so botdata \
	dkbot-README.txt dkbot-LICENSE.txt dkbot-NOTICE.md

W="$WORK/windows"
mkdir -p "$W"
cp build/dkbot-launch.exe build/dkbot.dll build/gladiator_x64.dll "$W/"
cp -r "$WORK/botdata" "$W/"
cp LICENSE "$W/dkbot-LICENSE.txt"
cp NOTICE.md "$W/dkbot-NOTICE.md"
readme "Windows x64" \
	"extract this archive into the folder that holds daikatana.exe; daikatana.pdb must be there too." \
	"dkbot-launch.exe, with game arguments if you like (e.g. +set deathmatch 1 +map e1dm1)." \
	> "$W/dkbot-README.txt"
python3 - "$W" "$OUT/dkbot-$VER-windows-x64.zip" <<'EOF'
import os, sys, zipfile
src, dst = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as z:
    for d, dirs, files in os.walk(src):
        dirs.sort()
        for f in sorted(files):
            z.write(os.path.join(d, f), os.path.relpath(os.path.join(d, f), src))
EOF

ls -l "$OUT"
