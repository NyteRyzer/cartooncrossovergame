#!/usr/bin/env bash
# Builds and runs the engine-independent logic tests (no Unreal install needed).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/Tools/tests/bin"
CXX="${CXX:-c++}"
mkdir -p "$OUT"
for test in omnitrix_test hover_math_test; do
	"$CXX" -std=c++17 -Wall -Wextra -Werror -I "$ROOT/Source/MultiverseToons/Public" \
		"$ROOT/Tools/tests/$test.cpp" -o "$OUT/$test"
	"$OUT/$test"
done
python3 -I "$ROOT/Tools/tests/test_layout_schema.py"
python3 -I "$ROOT/Content/Python/greybox/layout_schema.py" "$ROOT/Content/Python/greybox/layouts"
