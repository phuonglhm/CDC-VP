#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Resolve the complete frozen FlooNoC RTL compile flow with Bender and emit
# ordered tool file lists. This is what the leaf-level `fetch_rtl_deps.sh`
# cannot do: it pins single repositories but resolves no transitive tree and
# emits no source order.
#
# Guarantees enforced here:
#   * Bender is the pinned version.
#   * The FlooNoC working tree is at the frozen revision and is clean.
#   * `Bender.lock` matches the frozen hash before and after the run, so a
#     dependency set can never drift silently. Note that FlooNoC's own
#     `.gitignore` excludes `Bender.lock`, so git alone would not notice.
#   * Only `bender checkout` is run, never `bender update`; the former consumes
#     the existing lock, the latter would re-resolve it.
#   * The resolved `common_cells` revision agrees with the independent pin in
#     `fetch_rtl_deps.sh`.
#
# Outputs, under $BUILD_ROOT (default /tmp/floo_noc_rtl_filelist):
#   floo_verilator.f    Verilator -f file list
#   floo_vcs.sh         VCS compile script
#   floo_flist_plus.f   general file list with include dirs and defines
#   resolved_deps.txt   package/version/revision table actually checked out

set -euo pipefail

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/home/duyptt_HW/.local/bin:/usr/bin:/bin:$PATH

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
floo_rtl_root="${FLOONOC_RTL_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC}"
build_root="${BUILD_ROOT:-/tmp/floo_noc_rtl_filelist}"

frozen_floo_revision="9a6972a5f9b8117506d1df8a6505ce1da2bc9084"
frozen_lock_sha256="73eb4c72134b7fa51639e254e66ea211e0101496f64161add4f61b2aaa7d86bc"
required_bender_version="0.32.1"
# `common_cells` is resolved independently by fetch_rtl_deps.sh; the two paths
# must agree or one of them is stale.
expected_common_cells_revision="9ca8a7655f741e7dd5736669a20a301325194c28"

if ! command -v bender >/dev/null 2>&1; then
  cat >&2 <<'EOF'
bender not found in PATH.

Install the pinned version without touching the shell profile:

  ver=0.32.1
  url=https://github.com/pulp-platform/bender/releases/download/v${ver}/bender-x86_64-unknown-linux-gnu.tar.xz
  curl -sSL "$url" -o /tmp/bender.tar.xz
  tar -xf /tmp/bender.tar.xz -C /tmp
  install -m 0755 /tmp/bender-x86_64-unknown-linux-gnu/bender ~/.local/bin/bender

The upstream `https://pulp-platform.github.io/bender/init` installer is a
cargo-dist wrapper that installs into $CARGO_HOME/bin and edits shell profile
files; the direct artifact above avoids both.
EOF
  exit 2
fi

actual_bender_version="$(bender --version | awk '{print $2}')"
if [[ "$actual_bender_version" != "$required_bender_version" ]]; then
  echo "bender $actual_bender_version found, frozen flow uses $required_bender_version" >&2
  echo "Update this script deliberately, then re-run every RTL cross-check." >&2
  exit 2
fi

if [[ ! -d "$floo_rtl_root/.git" ]]; then
  echo "FlooNoC repository not found: $floo_rtl_root" >&2
  exit 2
fi

actual_floo_revision="$(git -C "$floo_rtl_root" rev-parse HEAD)"
if [[ "$actual_floo_revision" != "$frozen_floo_revision" ]]; then
  echo "FlooNoC is at $actual_floo_revision, frozen revision is $frozen_floo_revision" >&2
  exit 2
fi

if [[ -n "$(git -C "$floo_rtl_root" status --porcelain)" ]]; then
  echo "FlooNoC working tree is dirty; refusing to resolve against it." >&2
  git -C "$floo_rtl_root" status --short >&2
  exit 2
fi

lock_file="$floo_rtl_root/Bender.lock"
if [[ ! -f "$lock_file" ]]; then
  echo "Bender.lock not found: $lock_file" >&2
  exit 2
fi

lock_sha_before="$(sha256sum "$lock_file" | cut -d ' ' -f 1)"
if [[ "$lock_sha_before" != "$frozen_lock_sha256" ]]; then
  echo "Bender.lock does not match the frozen dependency set." >&2
  echo "actual SHA-256:   $lock_sha_before" >&2
  echo "expected SHA-256: $frozen_lock_sha256" >&2
  exit 2
fi

mkdir -p "$build_root"

# `checkout` consumes the lock file. `update` would re-resolve it and is never
# run here.
( cd "$floo_rtl_root" && bender --no-progress checkout )

lock_sha_after="$(sha256sum "$lock_file" | cut -d ' ' -f 1)"
if [[ "$lock_sha_after" != "$frozen_lock_sha256" ]]; then
  echo "Bender.lock changed during checkout; the dependency set is no longer frozen." >&2
  echo "before: $lock_sha_before" >&2
  echo "after:  $lock_sha_after" >&2
  exit 2
fi

common_cells_path="$(cd "$floo_rtl_root" && bender path common_cells)"
common_cells_revision="$(git -C "$common_cells_path" rev-parse HEAD)"
if [[ "$common_cells_revision" != "$expected_common_cells_revision" ]]; then
  echo "Bender resolved common_cells $common_cells_revision," >&2
  echo "but fetch_rtl_deps.sh pins $expected_common_cells_revision." >&2
  exit 2
fi

( cd "$floo_rtl_root" && bender script verilator ) > "$build_root/floo_verilator.f"
( cd "$floo_rtl_root" && bender script vcs ) > "$build_root/floo_vcs.sh"
( cd "$floo_rtl_root" && bender script flist-plus ) > "$build_root/floo_flist_plus.f"

python3 - "$lock_file" > "$build_root/resolved_deps.txt" <<'PY'
import re
import sys

lock = open(sys.argv[1], encoding="utf-8").read()
current = None
rows = []
for line in lock.splitlines():
    match = re.match(r"^  ([A-Za-z_]\w*):\s*$", line)
    if match:
        current = {"name": match.group(1)}
        rows.append(current)
        continue
    if current is None:
        continue
    match = re.match(r"^    (revision|version):\s*(\S+)", line)
    if match:
        current[match.group(1)] = match.group(2)

for row in rows:
    print(f"{row['name']:<22} {row.get('version', '-'):<10} {row.get('revision', '-')}")
PY

echo "bender $actual_bender_version"
echo "FlooNoC @ ${frozen_floo_revision:0:7} clean, Bender.lock unchanged"
echo "common_cells @ ${common_cells_revision:0:7} agrees with fetch_rtl_deps.sh"
echo "verilator file list: $build_root/floo_verilator.f"
echo "vcs script:          $build_root/floo_vcs.sh"
echo "flist-plus:          $build_root/floo_flist_plus.f"
echo "resolved deps:       $build_root/resolved_deps.txt"

# Prove the list is usable: elaborate the block the router cross-check needs.
if command -v verilator >/dev/null 2>&1; then
  if verilator --lint-only -Wno-fatal --top-module floo_router \
      -f "$build_root/floo_verilator.f" 2>"$build_root/floo_router_lint.log"; then
    errors="$(grep -c '^%Error' "$build_root/floo_router_lint.log" || true)"
    echo "floo_router elaborates from the generated list ($errors errors)"
  else
    echo "floo_router failed to elaborate; see $build_root/floo_router_lint.log" >&2
    grep '^%Error' "$build_root/floo_router_lint.log" | head -10 >&2
    exit 1
  fi
else
  echo "verilator not found; skipped the floo_router elaboration check" >&2
fi
