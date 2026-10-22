#!/usr/bin/env bash
# Builds a portable binary of the prototype with Zig's C/C++ toolchain.
#
#   PLATFORM=linux   (default) x86_64, glibc 2.28: Ubuntu 20.04+, Debian 10+,
#                    Fedora 29+, Arch, Mint, Pop!_OS, SteamOS...
#   PLATFORM=windows x86_64 Windows 7+ .exe, cross-compiled (no MSVC needed)
#
# macOS needs Apple's SDK frameworks, so it is built natively by
# tools/build_macos.sh on a Mac (CI does this on a macOS runner).
#
# Needs: zig (or `pip install ziglang`), git, python3. Linux builds also need
# X11/GL development headers (Debian/Ubuntu: libx11-dev libxrandr-dev
# libxi-dev libxcursor-dev libxinerama-dev libgl-dev).
#
#   PLATFORM=windows Prototype/tools/build_portable.sh [out_dir]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PLATFORM="${PLATFORM:-linux}"
OUT="${1:-$ROOT/Prototype/build-portable/$PLATFORM}"
case "$PLATFORM" in
	linux)
		TARGET="${TARGET:-x86_64-linux-gnu.2.28}"
		EXE="MultiverseToons"
		# Zig's own libc headers win; system dirs are only consulted for X11/GL headers.
		SYS_INC=(-idirafter /usr/include -idirafter /usr/include/x86_64-linux-gnu)
		LIBS=(-lm -ldl -lpthread)
		;;
	windows)
		TARGET="${TARGET:-x86_64-windows-gnu}"
		EXE="MultiverseToons.exe"
		SYS_INC=()
		LIBS=(-lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -Wl,--subsystem,windows)
		;;
	*) echo "unknown PLATFORM=$PLATFORM" >&2; exit 2 ;;
esac
RAYLIB_TAG="5.0"
RAYLIB_SRC="${RAYLIB_SRC:-$OUT/raylib}"

if [[ -n "${ZIG:-}" ]]; then
	read -r -a ZIGCMD <<< "$ZIG"
elif command -v zig >/dev/null 2>&1; then
	ZIGCMD=(zig)
else
	ZIGCMD=(python3 -m ziglang)
fi

mkdir -p "$OUT/obj"
if [[ ! -f "$RAYLIB_SRC/src/raylib.h" ]]; then
	git clone --quiet --depth 1 --branch "$RAYLIB_TAG" https://github.com/raysan5/raylib "$RAYLIB_SRC"
fi

COMMON=(-target "$TARGET" -O2 -fno-sanitize=undefined "${SYS_INC[@]}")
RL_DEFS=(-DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 -D_GNU_SOURCE -DSUPPORT_MODULE_RAUDIO=0)

pids=()
for f in rcore rshapes rtextures rtext utils rmodels rglfw; do
	"${ZIGCMD[@]}" cc "${COMMON[@]}" "${RL_DEFS[@]}" -std=gnu99 -w \
		-I "$RAYLIB_SRC/src" -I "$RAYLIB_SRC/src/external/glfw/include" \
		-c "$RAYLIB_SRC/src/$f.c" -o "$OUT/obj/rl_$f.o" &
	pids+=($!)
done
for f in main Game GameRender GameHud Render World; do
	"${ZIGCMD[@]}" c++ "${COMMON[@]}" -std=c++17 -Wall -Wextra -Wno-missing-field-initializers \
		-I "$RAYLIB_SRC/src" -I "$ROOT/Source/MultiverseToons/Public" \
		-c "$ROOT/Prototype/src/$f.cpp" -o "$OUT/obj/$f.o" &
	pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done

"${ZIGCMD[@]}" c++ -target "$TARGET" "$OUT"/obj/*.o -o "$OUT/$EXE" "${LIBS[@]}" -s
rm -f "$OUT"/*.pdb
python3 -I "$ROOT/Prototype/tools/export_levels.py" "$OUT/data/levels" >/dev/null
echo "Built $OUT/$EXE ($TARGET)"
