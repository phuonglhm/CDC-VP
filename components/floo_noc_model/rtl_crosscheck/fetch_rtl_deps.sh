#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Reproducible checkout of the FlooNoC RTL dependencies that leaf cross-checks
# compile. Revisions are read from the frozen FlooNoC `Bender.lock` and then
# checked against the values recorded here, so neither a lock-file change nor a
# silent upstream force-push can pass unnoticed.
#
# This script is not a Bender replacement. It only materialises the exact
# locked revisions that the current cross-checks need, and it never rewrites
# dependency sources.

set -euo pipefail

floo_rtl_root="${FLOONOC_RTL_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC}"
deps_root="${FLOO_RTL_DEPS_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/floo_rtl_deps}"
lock_file="$floo_rtl_root/Bender.lock"

# Frozen for FlooNoC revision 9a6972a / Bender.lock.
frozen_common_cells_rev="9ca8a7655f741e7dd5736669a20a301325194c28"
frozen_common_cells_version="1.39.0"
common_cells_url="https://github.com/pulp-platform/common_cells.git"

# SHA-256 of every dependency file compiled by a cross-check.
read -r -d '' frozen_common_cells_hashes <<'EOF' || true
eb50c6f9d402cb6dd416b3fb3aa62e96bb7ada1e5adbdc9abae68235829b29c3  src/stream_fifo_optimal_wrap.sv
3285eb2e557f6377de88df91751b0bccdad07b16be9d926314a52a23e964f02c  src/spill_register_flushable.sv
b24dbfeff17323b339fcf38854e5dfadd8b8e15489936d4a029988ece240eac3  src/stream_fifo.sv
861ea44cbd3129731b77977da36fb0dac1a1350d7df664a1b4310423e7d272bc  src/fifo_v3.sv
EOF

if [[ ! -f "$lock_file" ]]; then
  echo "FlooNoC Bender.lock not found: $lock_file" >&2
  exit 2
fi

locked_rev="$(awk '
  /^  common_cells:/ { in_pkg = 1; next }
  in_pkg && /^  [a-zA-Z_]+:/ { in_pkg = 0 }
  in_pkg && $1 == "revision:" { print $2; exit }
' "$lock_file")"
locked_version="$(awk '
  /^  common_cells:/ { in_pkg = 1; next }
  in_pkg && /^  [a-zA-Z_]+:/ { in_pkg = 0 }
  in_pkg && $1 == "version:" { print $2; exit }
' "$lock_file")"

if [[ "$locked_rev" != "$frozen_common_cells_rev"
      || "$locked_version" != "$frozen_common_cells_version" ]]; then
  echo "Bender.lock no longer matches the frozen dependency set." >&2
  echo "lock:    common_cells $locked_version @ $locked_rev" >&2
  echo "frozen:  common_cells $frozen_common_cells_version @ $frozen_common_cells_rev" >&2
  echo "Update docs/P0_SCOPE.md and rerun every cross-check before changing this." >&2
  exit 2
fi

common_cells_dir="$deps_root/common_cells"
mkdir -p "$deps_root"

if [[ ! -d "$common_cells_dir/.git" ]]; then
  echo "fetching common_cells $frozen_common_cells_version"
  git clone --quiet --filter=blob:none --no-checkout \
    "$common_cells_url" "$common_cells_dir"
fi

if ! git -C "$common_cells_dir" cat-file -e "$frozen_common_cells_rev^{commit}" 2>/dev/null; then
  git -C "$common_cells_dir" fetch --quiet origin "$frozen_common_cells_rev"
fi

git -C "$common_cells_dir" checkout --quiet --detach "$frozen_common_cells_rev"

actual_rev="$(git -C "$common_cells_dir" rev-parse HEAD)"
if [[ "$actual_rev" != "$frozen_common_cells_rev" ]]; then
  echo "common_cells checkout is $actual_rev, expected $frozen_common_cells_rev" >&2
  exit 2
fi

while read -r expected_sha file; do
  [[ -n "$file" ]] || continue
  if [[ ! -f "$common_cells_dir/$file" ]]; then
    echo "missing dependency source: $common_cells_dir/$file" >&2
    exit 2
  fi
  actual_sha="$(sha256sum "$common_cells_dir/$file" | cut -d ' ' -f 1)"
  if [[ "$actual_sha" != "$expected_sha" ]]; then
    echo "common_cells/$file does not match the frozen revision." >&2
    echo "actual SHA-256:   $actual_sha" >&2
    echo "expected SHA-256: $expected_sha" >&2
    exit 2
  fi
done <<< "$frozen_common_cells_hashes"

echo "common_cells $frozen_common_cells_version @ ${frozen_common_cells_rev:0:7} verified"
echo "COMMON_CELLS_ROOT=$common_cells_dir"
