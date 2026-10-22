#!/usr/bin/env bash
# Builds "Multiverse Toons.app" for macOS as a universal binary
# (Apple Silicon arm64 for macOS 11+, Intel x86_64 for macOS 10.15+).
# Run on a Mac with the Xcode command line tools (xcode-select --install).
#
#   Prototype/tools/build_macos.sh [out_dir]
#
# SOFTWARE_GL_TEST=1 builds an arm64-only test copy whose GLFW doesn't insist
# on a hardware-accelerated renderer, so the smoke test can run on GPU-less CI
# Macs through Apple's software OpenGL renderer. Never ship that build.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/Prototype/build-portable/macos}"
VERSION="${VERSION:-0.2}"
RAYLIB_TAG="5.0"
RAYLIB_SRC="${RAYLIB_SRC:-$OUT/raylib}"
APP="$OUT/Multiverse Toons.app"

mkdir -p "$OUT"
if [[ ! -f "$RAYLIB_SRC/src/raylib.h" ]]; then
	git clone --quiet --depth 1 --branch "$RAYLIB_TAG" https://github.com/raysan5/raylib "$RAYLIB_SRC"
fi
if [[ "${SOFTWARE_GL_TEST:-0}" == 1 ]]; then
	rm -rf "$OUT/raylib-swgl"
	cp -R "$RAYLIB_SRC" "$OUT/raylib-swgl"
	RAYLIB_SRC="$OUT/raylib-swgl"
	sed -i '' 's/ADD_ATTRIB(NSOpenGLPFAAccelerated);//' "$RAYLIB_SRC/src/external/glfw/src/nsgl_context.m"
fi

RL_DEFS=(-DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 -DGL_SILENCE_DEPRECATION -DSUPPORT_MODULE_RAUDIO=0)
FRAMEWORKS=(-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreFoundation)

build_arch() {
	local arch="$1" minos="$2"
	local obj="$OUT/obj-$arch"
	mkdir -p "$obj"
	local flags=(-arch "$arch" -mmacosx-version-min="$minos" -O2)
	for f in rcore rshapes rtextures rtext utils rmodels; do
		clang "${flags[@]}" "${RL_DEFS[@]}" -std=gnu99 -w -I "$RAYLIB_SRC/src" -I "$RAYLIB_SRC/src/external/glfw/include" \
			-c "$RAYLIB_SRC/src/$f.c" -o "$obj/rl_$f.o" &
	done
	# GLFW's Cocoa backend is Objective-C.
	clang "${flags[@]}" "${RL_DEFS[@]}" -x objective-c -w -I "$RAYLIB_SRC/src" -I "$RAYLIB_SRC/src/external/glfw/include" \
		-c "$RAYLIB_SRC/src/rglfw.c" -o "$obj/rl_rglfw.o" &
	for f in main Game GameRender GameHud Render World; do
		clang++ "${flags[@]}" -std=c++17 -Wall -Wextra -Wno-missing-field-initializers \
			-I "$RAYLIB_SRC/src" -I "$RAYLIB_SRC/src/external/glfw/include" -I "$ROOT/Source/MultiverseToons/Public" \
			-c "$ROOT/Prototype/src/$f.cpp" -o "$obj/$f.o" &
	done
	wait
	clang++ "${flags[@]}" "$obj"/*.o -o "$OUT/MultiverseToons-$arch" "${FRAMEWORKS[@]}"
}

build_arch arm64 11.0
if [[ "${SOFTWARE_GL_TEST:-0}" != 1 ]]; then
	build_arch x86_64 10.15
fi

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
if [[ -f "$OUT/MultiverseToons-x86_64" && "${SOFTWARE_GL_TEST:-0}" != 1 ]]; then
	lipo -create "$OUT/MultiverseToons-arm64" "$OUT/MultiverseToons-x86_64" -output "$APP/Contents/MacOS/MultiverseToons"
else
	cp "$OUT/MultiverseToons-arm64" "$APP/Contents/MacOS/MultiverseToons"
fi
strip -x "$APP/Contents/MacOS/MultiverseToons"
python3 -I "$ROOT/Prototype/tools/export_levels.py" "$APP/Contents/Resources/data/levels" >/dev/null
cp -R "$ROOT/Prototype/assets/portraits" "$APP/Contents/Resources/data/portraits"
cp -R "$ROOT/Prototype/assets/sprites" "$APP/Contents/Resources/data/sprites"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>Multiverse Toons</string>
	<key>CFBundleDisplayName</key><string>Multiverse Toons</string>
	<key>CFBundleIdentifier</key><string>com.multiversetoons.prototype</string>
	<key>CFBundleExecutable</key><string>MultiverseToons</string>
	<key>CFBundlePackageType</key><string>APPL</string>
	<key>CFBundleShortVersionString</key><string>$VERSION</string>
	<key>CFBundleVersion</key><string>$VERSION</string>
	<key>LSMinimumSystemVersion</key><string>10.15</string>
	<key>LSApplicationCategoryType</key><string>public.app-category.action-games</string>
	<key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST

# Ad-hoc signature: required for Apple Silicon. (Not notarized: first launch
# needs right-click > Open, see README.txt.)
codesign --force --deep --sign - "$APP"
lipo -info "$APP/Contents/MacOS/MultiverseToons"
echo "Built $APP"
