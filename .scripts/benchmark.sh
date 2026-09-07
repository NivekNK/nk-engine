#!/usr/bin/env bash
set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_type="${1:-Release}"
frames="${2:-660}"
if [[ ! "$frames" =~ ^[0-9]+$ ]] || ((frames <= 60)); then
    echo "Benchmark needs more than 60 frames (the first 60 warm up the GPU)." >&2
    exit 2
fi

# Fixed camera/scene, no input during the run; keep the window size identical
# across comparisons. GPU timestamps are read only after resource retirement.
export NK_BENCHMARK=1
export NK_SMOKE_TEST_FRAMES="$frames"
export NK_SMOKE_TEST_CYCLE_RENDER_MODES=0
exec "$script_dir/run.sh" "$build_type"
