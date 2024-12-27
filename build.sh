#!/usr/bin/env bash

set -e
set -o pipefail

run() {
    echo "$@"
    "$@" || exit 1
}

script_dir="$(cd $(dirname "$0") && pwd)"

cd "$script_dir" || exit 1

for build_type in Debug Release; do
    run cmake -B build/$build_type -S . -DCMAKE_BUILD_TYPE=$build_type
    run cmake --build build/$build_type
done

run cp build/Debug/compile_commands.json ./
