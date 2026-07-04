#!/usr/bin/env bash
# pack_fx1_sdk.sh - assemble the versioned VP_FX1 SDK handover for the firmware team.
#
# Produces a self-contained SDK tree (VP runtime + BSP + sample driver) and a
# versioned tarball, then optionally stages it into an FX1 project directory.
# The tree is namespaced per-SoC (VP_FX1_SOC) so more SoC targets can be added
# later; see tools/fx1_sdk_template/README.md for the layout.
#
# Usage:
#   tools/pack_fx1_sdk.sh [--fx1 <dir>] [--build]
#
#   --build       (re)build the VP package target before packing
#   --fx1 <dir>   also sync the SDK into <dir> (e.g. ../fx1)
#
# Output: out/vp_fx1_sdk_<sha>/  and  out/vp_fx1_sdk_<sha>.tar.gz
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo"

do_build=0
fx1_dir=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build) do_build=1; shift ;;
        --fx1)   fx1_dir="$2"; shift 2 ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

soc="VP_FX1_SOC"
pkg="$repo/out/vp_fx1_full_soc"
tmpl="$repo/tools/fx1_sdk_template"
common="$repo/fw/common"
# SystemC prefix the VP was actually linked against (from the build cache, not
# the environment - $SYSTEMC_HOME may point at an unrelated install, e.g. the
# Arm Fast Models copy).
systemc_home="$(sed -n 's/^SYSTEMC_HOME:PATH=//p' build-soc/CMakeCache.txt 2>/dev/null)"
systemc_home="${systemc_home:-/opt/systemc-2.3.4}"
riscv_vp_dir="$repo/third_party/riscv-vp"

if [[ "$do_build" == 1 || ! -x "$pkg/vp_fx1_full_soc" ]]; then
    echo ">> building VP package target"
    cmake --build build-soc -j"$(nproc)" --target vp_fx1_full_soc_package
fi

[[ -x "$pkg/vp_fx1_full_soc" ]] || { echo "missing VP package at $pkg" >&2; exit 1; }

echo ">> checking register headers against the TLM models"
"$repo/tools/check_regs_drift.sh"

sha="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
date_utc="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
sdk="$repo/out/vp_fx1_sdk_${sha}"

echo ">> assembling $sdk"
rm -rf "$sdk"
bsp="$sdk/sw/bsp/$soc"
mkdir -p "$sdk/vp/bin/$soc" "$sdk/vp/configs/$soc" "$sdk/vp/doc/$soc" \
         "$sdk/vp/licenses" "$sdk/vp/src/$soc" \
         "$bsp/include/soc/regs" "$bsp/include/hal" \
         "$bsp/src" "$bsp/startup" "$bsp/link" "$bsp/regref" \
         "$sdk/sw/drivers/sources/$soc" "$sdk/sw/drivers/build/$soc" \
         "$sdk/sw/bootloader/sources/$soc" "$sdk/sw/bootloader/build/$soc" \
         "$sdk/sw/bootloader/scripts/$soc" "$sdk/sw/bootloader/test/$soc"

# --- VP runtime (the "chip") -----------------------------------------------
cp "$pkg/vp_fx1_full_soc"   "$sdk/vp/bin/$soc/"
cp "$pkg"/libsystemc.so*    "$sdk/vp/bin/$soc/"
cp "$pkg"/configs/*.yaml    "$sdk/vp/configs/$soc/"
cp "$tmpl/vp/run_vp.sh"     "$sdk/sw/bootloader/test/$soc/"
chmod +x "$sdk/sw/bootloader/test/$soc/run_vp.sh"

# --- SoC-level docs ---------------------------------------------------------
cp "$repo/docs/peripheral_memory_map.md"      "$sdk/vp/doc/$soc/"
cp "$repo/docs/interrupt_modeling_policy.md"  "$sdk/vp/doc/$soc/"

# --- BSP (the ABI the firmware compiles against) ---------------------------
cp "$common"/include/soc/*.h       "$bsp/include/soc/"
cp "$common"/include/soc/regs/*.h  "$bsp/include/soc/regs/"   # clean, model-verified
cp "$common"/drivers/mmio.h        "$bsp/include/hal/"
cp "$common"/drivers/uart.h        "$bsp/include/hal/"
cp "$common"/drivers/uart.c        "$bsp/src/"
cp "$common"/startup/startup_riscv.S "$bsp/startup/"
cp "$common"/linker/riscv.ld       "$bsp/link/"
cp "$tmpl/sw/bsp/bsp.mk"           "$bsp/"
cp "$tmpl/sw/bsp/toolchain.md"     "$bsp/"

# --- Register reference: per-IP behaviour docs ------------------------------
# Read-only register/bit-field + behaviour docs for every IP. Documentation only
# (the per-IP README); the compiled ABI lives in include/soc + include/soc/regs.
for ipdir in "$repo"/components/*/; do
    ip="$(basename "$ipdir")"
    case "$ip" in common|bus_router|memory_tlm) continue ;; esac
    [[ -f "$ipdir/README.md" ]] || continue
    mkdir -p "$bsp/regref/$ip"
    cp "$ipdir/README.md" "$bsp/regref/$ip/"
done

# --- Sample driver + docs --------------------------------------------------
cp -r "$tmpl/sw/drivers/uart_hello" "$sdk/sw/drivers/sources/$soc/"
cp "$tmpl/README.md"                "$sdk/"

# --- Third-party license notices --------------------------------------------
# The VP binary statically links the Bremen riscv-vp ISS (MIT) and Berkeley
# SoftFloat-3 (BSD-3-Clause), and ships libsystemc.so (Apache-2.0). All three
# require their notice to accompany binary redistribution.
cp "$systemc_home/share/doc/systemc/LICENSE" "$sdk/vp/licenses/SYSTEMC.LICENSE"
cp "$systemc_home/share/doc/systemc/NOTICE"  "$sdk/vp/licenses/SYSTEMC.NOTICE"
cp "$riscv_vp_dir/LICENSE"                   "$sdk/vp/licenses/RISCV-VP.LICENSE"
# SoftFloat carries its BSD-3 license only in source headers; extract the block.
sed -n '/^\/\*====/,/^====.*\*\/$/p' \
    "$riscv_vp_dir/vp/src/vendor/softfloat/include/softfloat/softfloat.h" \
    | sed -n '1,/^====.*\*\/$/p' > "$sdk/vp/licenses/SOFTFLOAT.LICENSE"
[[ -s "$sdk/vp/licenses/SOFTFLOAT.LICENSE" ]] \
    || { echo "failed to extract SoftFloat license text" >&2; exit 1; }

cat > "$sdk/vp/licenses/README.md" <<'EOF'
# Third-party license notices

The VP executable and bundled runtime redistribute the following open-source
components. Keep this directory together with `vp/bin/` whenever the SDK is
passed on.

| Component                  | License      | Shipped as                          | Notice file        |
|----------------------------|--------------|-------------------------------------|--------------------|
| Accellera SystemC 2.3.4    | Apache-2.0   | `vp/bin/VP_FX1_SOC/libsystemc.so*`  | SYSTEMC.LICENSE, SYSTEMC.NOTICE |
| Bremen riscv-vp (RV32 ISS) | MIT          | statically linked into the VP binary | RISCV-VP.LICENSE   |
| Berkeley SoftFloat-3       | BSD-3-Clause | statically linked into the VP binary | SOFTFLOAT.LICENSE  |
EOF

# --- Keep reserved/output dirs present in git checkouts ---------------------
touch "$sdk/vp/src/$soc/.gitkeep" \
      "$sdk/sw/drivers/build/$soc/.gitkeep" \
      "$sdk/sw/bootloader/sources/$soc/.gitkeep" \
      "$sdk/sw/bootloader/build/$soc/.gitkeep" \
      "$sdk/sw/bootloader/scripts/$soc/.gitkeep"

# --- Version stamp ---------------------------------------------------------
cat > "$sdk/vp/VERSION" <<EOF
VP_FX1 SoC SDK
CDC-VP source : ${sha}
Built (UTC)   : ${date_utc}
CPU backend   : riscv_vp (Bremen RV32IMAC)
ABI           : rv32imac / ilp32, link base 0x80000000
Memory/IRQ map: vp/doc/${soc}/peripheral_memory_map.md (docs @ ${sha})
EOF

# --- Tarball ---------------------------------------------------------------
tarball="$repo/out/vp_fx1_sdk_${sha}.tar.gz"
tar -C "$repo/out" -czf "$tarball" "vp_fx1_sdk_${sha}"
echo ">> tarball: $tarball"

# --- Optional stage into an FX1 project ------------------------------------
if [[ -n "$fx1_dir" ]]; then
    fx1_abs="$(cd "$fx1_dir" && pwd)"
    echo ">> syncing SDK into $fx1_abs"
    mkdir -p "$fx1_abs/vp" "$fx1_abs/sw"
    cp -r "$sdk/vp/." "$fx1_abs/vp/"
    cp -r "$sdk/sw/." "$fx1_abs/sw/"
    cp "$sdk/README.md" "$fx1_abs/README.md"
    echo ">> FX1 staged. Next:"
    echo "     make -C $fx1_abs/sw/drivers/sources/$soc/uart_hello"
    echo "     $fx1_abs/sw/bootloader/test/$soc/run_vp.sh"
fi

echo ">> done: $sdk"
