#!/usr/bin/env bash
set -euo pipefail

if (( $# != 0 )); then
    printf 'Usage: %s (Debug build only; no arguments)\n' "$0" >&2
    exit 2
fi

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "$project_dir"

missing_tools=()
for tool in cmake ninja arm-none-eabi-gcc arm-none-eabi-g++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        missing_tools+=("$tool")
    fi
done
if (( ${#missing_tools[@]} != 0 )); then
    printf 'Missing build tool on PATH: %s\n' "${missing_tools[@]}" >&2
    printf 'Requires CMake >= 3.22, Ninja, and GNU Arm Embedded tools.\n' >&2
    exit 1
fi

cmake --preset Debug
cmake --build --preset Debug --parallel
