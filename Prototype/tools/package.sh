#!/usr/bin/env bash
# Builds and packs a ready-to-play archive into Builds/:
#   PLATFORM=linux   -> MultiverseToons-Prototype-<v>-linux-x86_64.tar.gz
#   PLATFORM=windows -> MultiverseToons-Prototype-<v>-windows-x64.zip
#   PLATFORM=macos   -> MultiverseToons-Prototype-<v>-macos-universal.zip (run on a Mac)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PLATFORM="${PLATFORM:-linux}"
export VERSION="${VERSION:-0.2}"
BUILD="${BUILD_DIR:-$ROOT/Prototype/build-portable/$PLATFORM}"
case "$PLATFORM" in
	linux) NAME="MultiverseToons-Prototype-$VERSION-linux-x86_64" ;;
	windows) NAME="MultiverseToons-Prototype-$VERSION-windows-x64" ;;
	macos) NAME="MultiverseToons-Prototype-$VERSION-macos-universal" ;;
	*) echo "unknown PLATFORM=$PLATFORM" >&2; exit 2 ;;
esac
STAGE="$BUILD/stage/$NAME"

if [[ "$PLATFORM" == macos ]]; then
	"$ROOT/Prototype/tools/build_macos.sh" "$BUILD"
else
	PLATFORM="$PLATFORM" "$ROOT/Prototype/tools/build_portable.sh" "$BUILD"
fi

rm -rf "$BUILD/stage"
mkdir -p "$STAGE" "$ROOT/Builds"
sed "s/@VERSION@/$VERSION/g" "$ROOT/Prototype/package/README.txt" > "$STAGE/README.txt"

case "$PLATFORM" in
	linux)
		cp "$BUILD/MultiverseToons" "$ROOT/Prototype/package/play.sh" "$STAGE/"
		cp -r "$BUILD/data" "$STAGE/"
		chmod +x "$STAGE/play.sh" "$STAGE/MultiverseToons"
		tar -C "$BUILD/stage" -czf "$ROOT/Builds/$NAME.tar.gz" "$NAME"
		echo "Packaged $ROOT/Builds/$NAME.tar.gz"
		;;
	windows)
		cp "$BUILD/MultiverseToons.exe" "$STAGE/"
		cp -r "$BUILD/data" "$STAGE/"
		sed -i.bak 's/$/\r/' "$STAGE/README.txt" && rm -f "$STAGE/README.txt.bak"
		(cd "$BUILD/stage" && python3 -I -c 'import shutil,sys; shutil.make_archive(sys.argv[1], "zip", ".", sys.argv[2])' "$ROOT/Builds/$NAME" "$NAME")
		echo "Packaged $ROOT/Builds/$NAME.zip"
		;;
	macos)
		cp -R "$BUILD/Multiverse Toons.app" "$STAGE/"
		# ditto keeps the code signature and bundle metadata intact.
		ditto -c -k --sequesterRsrc --keepParent "$STAGE" "$ROOT/Builds/$NAME.zip"
		echo "Packaged $ROOT/Builds/$NAME.zip"
		;;
esac
