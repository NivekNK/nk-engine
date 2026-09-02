#!/usr/bin/env bash

set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/.." && pwd)"

build_type="${NK_BUILD_TYPE:-Debug}"
if (($# > 0)); then
    case "$1" in
        Debug|RelWithDebInfo|Release)
            build_type="$1"
            shift
            ;;
    esac
fi

case "$build_type" in
    Debug|RelWithDebInfo|Release) ;;
    *)
        echo "Invalid build type '$build_type'. Use Debug, RelWithDebInfo, or Release." >&2
        exit 2
        ;;
esac

build_dir="${NK_BUILD_DIR:-$project_root/out/build/Linux-$build_type}"
cmake_options=(
    "-DCMAKE_BUILD_TYPE=$build_type"
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
)

if [[ -n "${SLANGC:-}" ]]; then
    cmake_options+=("-DSLANGC=$SLANGC")
fi
if [[ -n "${Vulkan_INCLUDE_DIR:-}" ]]; then
    cmake_options+=("-DVulkan_INCLUDE_DIR=$Vulkan_INCLUDE_DIR")
fi
if [[ -n "${Vulkan_LIBRARY:-}" ]]; then
    cmake_options+=("-DVulkan_LIBRARY=$Vulkan_LIBRARY")
fi

echo "Configuring NK Engine ($build_type) in $build_dir"
cmake \
    -S "$project_root" \
    -B "$build_dir" \
    -G Ninja \
    "${cmake_options[@]}"

echo "Building NK Engine ($build_type)"
cmake --build "$build_dir" "$@"
