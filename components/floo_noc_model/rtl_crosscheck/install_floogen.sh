#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Installs FlooGen from the **frozen** FlooNoC tree and generates the reference
# topology used to check the model's mesh wiring.
#
# Install from the tree, never from PyPI: `pip install floogen` would fetch
# whatever version is current, and the generated netlist has to match revision
# `9a6972a` for its port ordering to be evidence about this model.
#
# Two environment facts this script works around:
#
#   * FlooGen 0.8.4 requires Python >= 3.10 and the default `python3` on this
#     host is 3.9. It uses `python3.11` explicitly.
#   * Output goes to a build directory, never into the FlooNoC tree, so the
#     frozen checkout stays clean and `git status` there stays empty.
#
# `verible-verilog-format` is not installed, so FlooGen warns that it skipped
# formatting. That is cosmetic; the netlist is complete.

set -euo pipefail

export PATH=/usr/bin:/bin:$PATH

floo_rtl_root="${FLOONOC_RTL_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC}"
build_root="${BUILD_ROOT:-/tmp/floo_noc_floogen}"
venv="$build_root/venv"
generated="$build_root/generated"
expected_version="0.8.4"

if [[ ! -f "$floo_rtl_root/pyproject.toml" ]]; then
  echo "FlooNoC tree not found: $floo_rtl_root" >&2
  exit 2
fi

declared_version="$(sed -n 's/^version = "\(.*\)"/\1/p' "$floo_rtl_root/pyproject.toml" | head -n 1)"
if [[ "$declared_version" != "$expected_version" ]]; then
  echo "FlooGen version in the frozen tree is $declared_version, expected $expected_version." >&2
  exit 2
fi

if ! command -v python3.11 > /dev/null; then
  echo "python3.11 not found; FlooGen $expected_version needs Python >= 3.10." >&2
  exit 2
fi

mkdir -p "$build_root"
if [[ ! -x "$venv/bin/floogen" ]]; then
  python3.11 -m venv "$venv"
  "$venv/bin/pip" install --quiet --upgrade pip
  "$venv/bin/pip" install --quiet "$floo_rtl_root"
fi

installed_version="$("$venv/bin/floogen" --version | awk '{print $NF}')"
if [[ "$installed_version" != "$expected_version" ]]; then
  echo "installed FlooGen is $installed_version, expected $expected_version." >&2
  exit 2
fi

mkdir -p "$generated"
"$venv/bin/floogen" rtl \
  -c "$floo_rtl_root/floogen/examples/axi_mesh_xy.yml" \
  -o "$generated"

# The frozen tree must be untouched by all of the above.
if [[ -n "$(git -C "$floo_rtl_root" status --porcelain)" ]]; then
  echo "the frozen FlooNoC tree was modified; refusing to report success." >&2
  git -C "$floo_rtl_root" status --short >&2
  exit 1
fi

echo "FLOOGEN=$venv/bin/floogen"
echo "GENERATED_ROOT=$generated"
ls -1 "$generated"
