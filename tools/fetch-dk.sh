#!/bin/sh
# Fetch a Daikatana 1.3 release and its debug symbols into vendor/: tools/fetch-dk.sh [tag]
set -eu

TAG="${1:-v12-21-2025}"
BASE="https://github.com/maraakate/daikatana/releases/download/$TAG"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/vendor"

# Tags are vMM-DD-YYYY, asset names YYYY-MM-DD.
DATE="$(printf '%s' "$TAG" | sed 's/^v//' | awk -F- '{print $3"-"$1"-"$2}')"
GAME="Daikatana-Linux-$DATE-x64.tar.bz2"
SYMS="Daikatana-Linux-$DATE-x64-dbgsyms.tar.bz2"
DEST="$OUT/dk-$DATE-x64"

mkdir -p "$OUT" "$DEST"
for f in "$GAME" "$SYMS"; do
	if [ ! -f "$OUT/$f" ]; then
		echo "fetching $f"
		curl -sSL -o "$OUT/$f" "$BASE/$f"
	fi
done

rm -rf "$DEST/game" "$DEST/dbgsyms"
mkdir -p "$DEST/game" "$DEST/dbgsyms"
tar xjf "$OUT/$GAME"  -C "$DEST/game"    --strip-components=1
tar xjf "$OUT/$SYMS" -C "$DEST/dbgsyms" --strip-components=1
echo "unpacked into $DEST"
echo "next: make check-boundary (or tools/dump-boundary.py $DEST)"
