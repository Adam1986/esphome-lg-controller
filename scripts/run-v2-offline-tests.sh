#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT
passed=0
for source in esphome/tests/*.cpp; do
  exe="$workdir/$(basename "${source%.cpp}")"
  g++ -std=c++17 -Wall -Wextra -pedantic -I esphome/components/lg_controller "$source" -o "$exe"
  "$exe"
  passed=$((passed + 1))
done
echo "PASS: $passed C++ regression programs"
python3 esphome/tests/test_v2_ui_guards.py
python3 -m py_compile esphome/components/lg_controller/climate.py
echo "PASS: Python syntax and mocked UI guard test"
echo 'Note: does not replace full ESPHome compile or a physical device trial.'
