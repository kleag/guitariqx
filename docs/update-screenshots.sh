#!/bin/sh
# Refresh the screenshots of the documentation: tst_ui runs guitariqx
# against the fake engine (tests/fixtures/engine.json) in each UI state.
#
#   cmake -B build && cmake --build build
#   docs/update-screenshots.sh [build]

set -eu
build=${1:-build}
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

LC_ALL=C.UTF-8 GUITARIQX_SCREENSHOT_DIR="$out" "$build/tests/tst_ui" > /dev/null
mkdir -p docs/images
for shot in main main-small tuner switcher learnmode learn midimap saveas files-remote files-remote-old-engine; do
    cp "$out/$shot.png" "docs/images/$shot.png"
done
ls -l docs/images
