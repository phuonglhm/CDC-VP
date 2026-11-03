#!/usr/bin/env bash
# =============================================================================
# Integrated Circuits and Systems laboratory
# Author: Hung Nguyen <ngthung@hcmut.edu.vn>
# Module: virtual_platform.cdc_vp.run_in_docker
# Description: Build the VP with the RV32/CDC-VP platform on Linux, and test it.
# Changelog:
#   2026-09-29: Initial version.
# =============================================================================
#
#   examples/cdc_vp/run_in_docker.sh [ctest label regex]    default: "unit|scenario|platform|riscv"
#
# Builds the image (cdc_vp/Dockerfile) if needed, fetches CDC-VP and the
# Bremen riscv-vp ISS at pinned commits into a cache volume, configures the
# VP with ICSL_ETH_VP_CDC=ON, builds everything including the RV32 firmware,
# and runs ctest.  The repository is mounted read-only; the build lives in
# the cache volume.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE=icsl-eth-vp
LABELS="${1:-unit|scenario|platform|riscv}"
CDC_VP_URL=https://github.com/phuonglhm/CDC-VP.git
CDC_VP_COMMIT=50e570a8f17ac794e3705e69651bcf0d182471e6
RISCV_VP_URL=https://github.com/agra-uni-bremen/riscv-vp.git
RISCV_VP_COMMIT=48b2f5877b2368cc466fb0da155db349e676c0b0

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    docker build -t "$IMAGE" -f "$ROOT/examples/cdc_vp/Dockerfile" "$ROOT/scripts"
fi

HOST_ROOT="$ROOT"
if command -v cygpath >/dev/null 2>&1; then HOST_ROOT="$(cygpath -w "$ROOT")"; fi
export MSYS_NO_PATHCONV=1               # Git Bash: keep container paths as written

docker run --rm -v "$HOST_ROOT:/work/eth:ro" -v ftel-eth-vp-v1-0-6-cache:/cache "$IMAGE" bash -euo pipefail -c "
fetch() {   # fetch <url> <commit> <dir>: a single pinned commit
    if [ \"\$(git -C \"\$3\" rev-parse HEAD 2>/dev/null)\" != \"\$2\" ]; then
        rm -rf \"\$3\"; git init -q \"\$3\"
        git -C \"\$3\" fetch -q --depth 1 \"\$1\" \"\$2\"
        git -C \"\$3\" checkout -q FETCH_HEAD
    fi
}
fetch $CDC_VP_URL $CDC_VP_COMMIT /cache/CDC-VP
fetch $RISCV_VP_URL $RISCV_VP_COMMIT /cache/CDC-VP/third_party/riscv-vp
cmake -S /work/eth -B /cache/build-fx1-ethernet -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DICSL_ETH_VP_CDC=ON -DCDC_VP_ROOT=/cache/CDC-VP >/dev/null
cmake --build /cache/build-fx1-ethernet
ctest --test-dir /cache/build-fx1-ethernet -L '$LABELS' --output-on-failure --timeout 3600
"
