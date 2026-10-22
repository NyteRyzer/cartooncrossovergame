#!/usr/bin/env sh
# Launches the prototype from wherever it was extracted.
cd "$(dirname "$0")" || exit 1
exec ./MultiverseToons "$@"
