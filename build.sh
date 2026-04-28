#!/usr/bin/env bash
set -euo pipefail

run() {
    echo "$@"
    "$@" || exit 1
}

script_dir="$(cd "$(dirname "$0")" && pwd)"
cd "$script_dir" || exit 1

for build_type in Debug Release; do
    run cmake -B "build/${build_type}" -S . -DCMAKE_BUILD_TYPE="${build_type}"
    run cmake --build "build/${build_type}"
done

# Symlink compile_commands.json for clangd (points to Debug by default)
ln -sf build/Debug/compile_commands.json ./
