#!/usr/bin/env bash
# Builds the portable binary and packs a ready-to-play tarball:
#   Builds/MultiverseToons-Prototype-<version>-linux-x86_64.tar.gz
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION="${VERSION:-0.1}"
NAME="MultiverseToons-Prototype-$VERSION-linux-x86_64"
BUILD="${BUILD_DIR:-$ROOT/Prototype/build-portable}"
STAGE="$BUILD/stage/$NAME"

"$ROOT/Prototype/tools/build_portable.sh" "$BUILD"

rm -rf "$BUILD/stage"
mkdir -p "$STAGE"
cp "$BUILD/MultiverseToons" "$STAGE/"
cp -r "$BUILD/data" "$STAGE/"
cp "$ROOT/Prototype/package/README.txt" "$STAGE/"
cp "$ROOT/Prototype/package/play.sh" "$STAGE/"
chmod +x "$STAGE/play.sh" "$STAGE/MultiverseToons"

mkdir -p "$ROOT/Builds"
tar -C "$BUILD/stage" -czf "$ROOT/Builds/$NAME.tar.gz" "$NAME"
echo "Packaged $ROOT/Builds/$NAME.tar.gz"
