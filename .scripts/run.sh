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

editor="${NK_EDITOR_BINARY:-$project_root/bin/Linux-$build_type/editor}"
if [[ ! -x "$editor" ]]; then
    echo "Editor binary not found or not executable: $editor" >&2
    echo "Build it first with: nix run .#build -- $build_type" >&2
    exit 1
fi

# Engine assets are loaded relative to the process working directory.
cd -- "$(dirname -- "$editor")"
exec "$editor" "$@"
