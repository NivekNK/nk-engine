#!/usr/bin/env bash

set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/.." && pwd)"
build_type="${1:-${NK_BUILD_TYPE:-Debug}}"

case "$build_type" in
    Debug|RelWithDebInfo|Release) ;;
    *)
        echo "Invalid build type '$build_type'. Use Debug, RelWithDebInfo, or Release." >&2
        exit 2
        ;;
esac

build_dir="${NK_BUILD_DIR:-$project_root/out/build/Linux-$build_type}"
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    cmake_options=(
        "-DCMAKE_BUILD_TYPE=$build_type"
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
    )
    [[ -z "${SLANGC:-}" ]] || cmake_options+=("-DSLANGC=$SLANGC")
    cmake -S "$project_root" -B "$build_dir" -G Ninja "${cmake_options[@]}"
fi

cmake --build "$build_dir" --target verify_shader_assets
