#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Phase 4.5 distribution gate.
#
# The packaging gate says the bundle must run "from an otherwise empty temporary
# directory with no source tree, build tree, LD_LIBRARY_PATH, externally
# installed VP++ executable or host SystemC fallback". Checking that against the
# developer's own build proves less than it appears to: those artifacts sit next
# to the source, and the developer's shell already has SystemC on its library
# path.
#
# So this configures a *fresh* Release build with TPU_V3 and every other
# platform switched off — which is itself part of the gate, since Phase 4.5
# requires the handoff to build without `CDC_BUILD_TPU_V3_SOC` — then verifies:
#
#   * the package contains the binary, the SystemC runtime, configs, both
#     demonstrations, the SDK that built them, the map documentation, licences
#     and a manifest;
#   * the packaged executable has exactly RPATH=$ORIGIN and no source-tree path;
#   * libsystemc resolves from beside the binary with LD_LIBRARY_PATH unset;
#   * both demonstrations print their exact PASS marker and exit zero from a
#     directory with no access to the source or build tree;
#   * FlooNoC, Sauria, the NEO DMA, Spike, Qt/VNC and upstream VP++ platform
#     code are absent from the shipped binary and from the file listing;
#   * the licence of everything statically linked is present *with its text* --
#     the binary links the RISC-V VP++ ISS (MIT) and Berkeley SoftFloat
#     (BSD-3-Clause), and both require the notice to accompany a binary
#     distribution, so a missing one is a distribution blocker rather than a
#     documentation gap;
#   * the manifest is well-formed JSON, agrees with `--version`, and does not
#     claim a dependency the binary does not contain;
#   * an ELF rebuilt from the shipped SDK, by the documented commands, outside
#     the source tree, can be substituted for a bundled example and produces the
#     same result.
#
# Exit codes: 0 pass, 1 fail. **There is no skip.** Every other packaging
# regression in this repository skips when a host prerequisite is missing, and
# for a component test that is right. This one gates a release package: if the
# bundle cannot be built, the answer a CI run needs is "there is no shippable
# package", not a green tick and a skip nobody reads. A missing SystemC or cross
# toolchain is therefore a failure here, with a message saying what to install.
#
# Set PACKAGING_KEEP_ARTIFACTS=1 to retain a successful temporary tree, or
# PACKAGING_SYSTEMC_HOME / PACKAGING_TOOLDIR to select non-default installs.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${platform_dir}/../.." && pwd)"

# Reproducibility beats the ambient login shell: the default PATH here puts a
# Synopsys g++ wrapper ahead of the system compiler.
export CC="${PACKAGING_CC:-/usr/bin/gcc}"
export CXX="${PACKAGING_CXX:-/usr/bin/g++}"
export PATH="/usr/bin:/bin:${PATH}"

systemc_home="${PACKAGING_SYSTEMC_HOME:-/opt/systemc-2.3.4}"
tooldir="${PACKAGING_TOOLDIR:-/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin}"

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/compiler_vp_packaging.XXXXXX")"
build_dir="${work_dir}/build"
package_root="${work_dir}/package"
package_dir="${package_root}/riscv_vpp_compiler_vp"
run_dir="${work_dir}/run"
evidence_dir="${work_dir}/evidence"
mkdir -p "${evidence_dir}" "${run_dir}"

cleanup()
{
    local status=$?
    trap - EXIT
    if [[ "${PACKAGING_KEEP_ARTIFACTS:-0}" != "1" && ${status} -eq 0 ]]; then
        rm -rf "${work_dir}"
    else
        echo "packaging evidence kept under ${work_dir}" >&2
    fi
    exit "${status}"
}
trap cleanup EXIT

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

require_command()
{
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

match_file()
{
    local expected="$1" actual="$2"
    [[ -f "${actual}" ]] || fail "required package record missing: ${actual}"
    cmp -s "${expected}" "${actual}" \
        || fail "package record differs from source: ${actual}"
}

require_command cmake
require_command readelf
require_command nm
require_command ldd
require_command cmp
require_command python3
require_command timeout
require_command make

[[ -d "${systemc_home}" ]] \
    || fail "SystemC not found at ${systemc_home}. Set PACKAGING_SYSTEMC_HOME."
# Not a skip. Phase 4.5 makes both demonstrations mandatory package content, so
# without a cross toolchain there is no package to gate -- and reporting that as
# "skipped" is how a release goes out with nothing in it.
[[ -x "${tooldir}/riscv-none-elf-gcc" ]] \
    || fail "no RV32GCV cross toolchain at ${tooldir}. The package cannot be
       built without it, because the two demonstrations are required content.
       Set PACKAGING_TOOLDIR."

echo "packaging work directory: ${work_dir}"

# TPU_V3 off, every other platform off: the handoff must not need them.
cmake -S "${repo_root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSYSTEMC_HOME="${systemc_home}" \
    -DCDC_BUILD_RISCV_VPP_COMPILER_VP=ON \
    -DCDC_BUILD_TPU_V3_SOC=OFF \
    -DCDC_BUILD_TPU_V3_TESTS=OFF \
    -DCDC_BUILD_TESTS=OFF \
    -DCDC_BUILD_MINI_TLM=OFF \
    -DCDC_BUILD_CPU_EVAL=OFF \
    -DCDC_BUILD_CUSTOM_SOC=OFF \
    -DCDC_BUILD_NOC_SOC=OFF \
    -DCDC_PACKAGE_ROOT="${package_root}" \
    >"${evidence_dir}/configure.log" 2>&1 \
    || { cat "${evidence_dir}/configure.log" >&2; fail "configure failed"; }

cmake --build "${build_dir}" --target riscv_vpp_compiler_vp_package --parallel \
    >"${evidence_dir}/package.log" 2>&1 \
    || { cat "${evidence_dir}/package.log" >&2; fail "packaging failed"; }

# ── contents ─────────────────────────────────────────────────────────────────

package_bin="${package_dir}/riscv_vpp_compiler_vp"
[[ -x "${package_bin}" ]] \
    || fail "packaged riscv_vpp_compiler_vp is missing or not executable"

for config in default tight_watchdog; do
    [[ -f "${package_dir}/configs/${config}.yaml" ]] \
        || fail "packaged configs/${config}.yaml is missing"
done

for artefact in \
    examples/scalar_hello/scalar_hello.elf \
    examples/scalar_hello/scalar_hello.elf.dis \
    examples/rvv_vector_add/rvv_vector_add.elf \
    examples/rvv_vector_add/rvv_vector_add.elf.dis \
    sdk/Makefile \
    sdk/common/crt0.S \
    sdk/common/host_io.c \
    sdk/common/host_io.h \
    sdk/common/link.ld \
    sdk/common/link.ld.in \
    sdk/include/compiler_vp/host_io_map.h \
    sdk/examples/scalar_hello/main.c \
    sdk/examples/rvv_vector_add/main.c \
    docs/MEMORY_MAP.md \
    docs/COMPILER_QUICKSTART.md \
    docs/ISA_ABI_CONTRACT.md \
    licenses/RISCV-VP-PLUSPLUS.MIT.txt \
    licenses/BERKELEY-SOFTFLOAT-3d.BSD-3-Clause.txt \
    README.md \
    BUILD_MANIFEST.json
do
    [[ -f "${package_dir}/${artefact}" ]] || fail "packaged ${artefact} is missing"
done

# The SDK must be the tested sources, not a cleaned-up copy of them.
match_file "${repo_root}/fw/riscv_vpp_compiler_vp/Makefile" \
           "${package_dir}/sdk/Makefile"
match_file "${repo_root}/fw/riscv_vpp_compiler_vp/common/crt0.S" \
           "${package_dir}/sdk/common/crt0.S"
match_file "${platform_dir}/include/compiler_vp/host_io_map.h" \
           "${package_dir}/sdk/include/compiler_vp/host_io_map.h"

match_file "${repo_root}/LICENSE"        "${package_dir}/licenses/Apache-2.0.txt"
match_file "${repo_root}/NOTICE"         "${package_dir}/licenses/CDC-VP-NOTICE.txt"
match_file "${repo_root}/THIRD_PARTY.md" "${package_dir}/licenses/THIRD_PARTY.md"

# ── the licences of everything statically linked ─────────────────────────────
#
# Existence is not enough: an empty file left by a failed copy passes `-f` and
# fails the obligation. Each is checked for the text that makes it that licence,
# and the inventory is checked for the two components, because a package whose
# THIRD_PARTY.md does not list what is in the binary is inaccurate whichever way
# it is read.

match_file "${repo_root}/third_party/riscv-vp-plusplus/LICENSE" \
           "${package_dir}/licenses/RISCV-VP-PLUSPLUS.MIT.txt"

grep -q "Permission is hereby granted" \
     "${package_dir}/licenses/RISCV-VP-PLUSPLUS.MIT.txt" \
    || fail "the shipped RISC-V VP++ licence does not contain the MIT grant"

softfloat_notice="${package_dir}/licenses/BERKELEY-SOFTFLOAT-3d.BSD-3-Clause.txt"
grep -q "Redistribution and use" "${softfloat_notice}" \
    || fail "the shipped SoftFloat notice does not contain the BSD-3-Clause terms"
grep -q "SoftFloat" "${softfloat_notice}" \
    || fail "the shipped SoftFloat notice does not name SoftFloat"
# Matched on a fragment that fits one line. The upstream header wraps the
# copyright mid-phrase ("... The Regents of the University of\nCalifornia."),
# so grepping for the whole sentence fails on a notice that is perfectly intact.
grep -q "Regents of the University" "${softfloat_notice}" \
    || fail "the shipped SoftFloat notice has lost its copyright line"

for component in "riscv-vp-plusplus" "SoftFloat"; do
    grep -qF "${component}" "${package_dir}/licenses/THIRD_PARTY.md" \
        || fail "THIRD_PARTY.md does not list ${component}, which is statically linked"
done
echo "licence records PASS"

echo "package contents PASS"

compgen -G "${package_dir}/libsystemc.so*" >/dev/null \
    || fail "the SystemC runtime was not bundled"

# ── the executable must not point back at the build machine ──────────────────

mapfile -t dynamic_paths < <(
    readelf -d "${package_bin}" \
        | sed -n \
            -e 's/.*Library rpath: \[\(.*\)\]/RPATH:\1/p' \
            -e 's/.*Library runpath: \[\(.*\)\]/RUNPATH:\1/p'
)
if [[ ${#dynamic_paths[@]} -ne 1
      || "${dynamic_paths[0]}" != 'RPATH:$ORIGIN' ]]; then
    printf '%s\n' "${dynamic_paths[@]:-<none>}" >"${evidence_dir}/dynamic_paths.txt"
    fail "packaged executable must have exactly RPATH=\$ORIGIN, got: ${dynamic_paths[*]:-<none>}"
fi
if readelf -d "${package_bin}" | grep -qF "${repo_root}"; then
    fail "the packaged executable's dynamic section references the source tree"
fi

env -u LD_LIBRARY_PATH ldd "${package_bin}" >"${evidence_dir}/ldd.txt"
grep -q 'not found' "${evidence_dir}/ldd.txt" \
    && fail "packaged executable has an unresolved shared library"
systemc_path="$(
    awk '$1 ~ /^libsystemc\.so/ && $2 == "=>" { print $3 }' "${evidence_dir}/ldd.txt"
)"
[[ -n "${systemc_path}" ]] || fail "ldd did not resolve libsystemc"
package_real="$(cd "${package_dir}" && pwd -P)"
systemc_real="$(cd "$(dirname "${systemc_path}")" && pwd -P)"
[[ "${systemc_real}" == "${package_real}" ]] \
    || fail "libsystemc resolved outside the package: ${systemc_path}"
echo "RPATH and local SystemC resolution PASS"

# ── the absence half of the boundary, checked on the shipped bundle ──────────
#
# The build-time guard checks the sources, the link interface and the VP++
# compile list. This checks what was actually shipped, which is the artifact a
# licence or accuracy claim would attach to.

nm --defined-only -C "${package_bin}" >"${evidence_dir}/symbols.txt" 2>/dev/null || true
nm -u -C "${package_bin}" >>"${evidence_dir}/symbols.txt" 2>/dev/null || true
for needle in floo_noc floonoc noc_interconnect sauria neo_dma image_transform \
              dma_tlm pl330 riscv_isa_sim fesvr htif_t \
              QApplication QWidget vncserver rfbScreen DirectCoreRunner; do
    if grep -qi -- "${needle}" "${evidence_dir}/symbols.txt"; then
        fail "the shipped binary references a symbol containing '${needle}'"
    fi
done

find "${package_dir}" -type f | sed "s|^${package_dir}/||" \
    >"${evidence_dir}/listing.txt"
for needle in floo sauria noc_soc neo_dma image_transform spike libQt libvnc; do
    if grep -qi -- "${needle}" "${evidence_dir}/listing.txt"; then
        fail "the package ships a file matching '${needle}':
$(grep -i -- "${needle}" "${evidence_dir}/listing.txt")"
    fi
done
echo "boundary PASS (no NoC, Sauria, NEO DMA, Spike, GUI or upstream platform)"

# ── run it with no access to the source or build tree ────────────────────────
#
# The package is moved, not copied: a bundle that still works only because the
# original directory exists is not portable, and copying would not catch that.

mv "${package_dir}" "${run_dir}/bundle"
bundle="${run_dir}/bundle"

run_demo()
{
    local name="$1" marker="$2" log="${evidence_dir}/run_${1}.log"
    ( cd "${bundle}" \
      && env -u LD_LIBRARY_PATH timeout --kill-after=10 300 \
             ./riscv_vpp_compiler_vp --elf "examples/${name}/${name}.elf" ) \
        >"${log}" 2>&1 \
        || { cat "${log}" >&2; fail "packaged run of ${name} failed"; }
    grep -qF "${marker}" "${log}" \
        || { cat "${log}" >&2; fail "packaged ${name} did not print '${marker}'"; }
    grep -q 'status         : PASS' "${log}" \
        || fail "packaged ${name} did not report PASS"
}

run_demo scalar_hello   "SCALAR HELLO: PASS"
run_demo rvv_vector_add "VECTOR ADD: PASS"

# The exact lines the plan names, all of them, from the packaged run.
for line in "Hello from RISC-V VP++ RV32GCV" "XLEN=32" "hart_id=0"; do
    grep -qF "${line}" "${evidence_dir}/run_scalar_hello.log" \
        || fail "packaged scalar_hello did not print '${line}'"
done
for line in "RVV=1.0" "VLEN=512" "vlenb=64"; do
    grep -qF "${line}" "${evidence_dir}/run_rvv_vector_add.log" \
        || fail "packaged rvv_vector_add did not print '${line}'"
done
echo "packaged demonstrations PASS"

( cd "${bundle}" && env -u LD_LIBRARY_PATH ./riscv_vpp_compiler_vp --version ) \
    >"${evidence_dir}/version.log" 2>&1 || fail "packaged --version failed"
grep -q 'build type      : Release' "${evidence_dir}/version.log" \
    || fail "packaged binary does not report the Release build type"

( cd "${bundle}" && env -u LD_LIBRARY_PATH ./riscv_vpp_compiler_vp --print-config ) \
    >"${evidence_dir}/print_config.log" 2>&1 \
    || fail "packaged --print-config failed"

# ── build manifest ───────────────────────────────────────────────────────────

python3 - "${bundle}/BUILD_MANIFEST.json" "${evidence_dir}/version.log" \
    <<'PY' || fail "build manifest check failed"
import json
import re
import sys

manifest = json.load(open(sys.argv[1], encoding="utf-8"))
version_text = open(sys.argv[2], encoding="utf-8").read()


def require(condition, message):
    if not condition:
        print(f"FAIL: {message}", file=sys.stderr)
        raise SystemExit(1)


require(manifest["schema"] == "cdc-vp/build-manifest/2", "unexpected schema")
require(manifest["platform"] == "riscv_vpp_compiler_vp", "unexpected platform")
require(manifest["build"]["type"] == "Release", "build type is not Release")
require(manifest["runtime"]["bundled_systemc"] is True,
        "SystemC not declared bundled")

# Two revisions, captured at two different moments: the binary's when the
# platform is configured, the package's when the bundle is assembled. Recording
# one and calling it "the revision" would let the manifest name a commit the
# binary was never built from.
source = manifest["source"]
require(source["revision_available"] is True,
        "this build is a Git checkout, so a revision must be recorded")
require(len(source["build_revision"]) == 40, "build_revision is not a full SHA")
require(len(source["package_revision"]) == 40,
        "package_revision is not a full SHA")
require(isinstance(source["dirty_at_package_time"], bool),
        "dirty_at_package_time is not a bool")

match = re.search(r"CDC-VP revision : (\S+)", version_text)
require(match is not None, "--version does not report a revision")
require(match.group(1) == source["build_revision"],
        f"--version says {match.group(1)}, manifest says {source['build_revision']}")

# The frozen compiler contract, in machine-readable form, agreeing with what the
# binary says. A team quoting one of them must not be able to contradict the
# other.
machine = manifest["machine"]
require(machine["isa"] == "rv32gcv_zvl512b", f"unexpected ISA {machine['isa']!r}")
require(machine["abi"] == "ilp32d", f"unexpected ABI {machine['abi']!r}")
require(machine["harts"] == 1, "the manifest does not declare a single hart")
require(machine["xlen"] == 32, "XLEN is not 32")
require(machine["rvv_version"] == "1.0", "RVV version is not 1.0")
require(machine["vlen_bits"] == 512, "VLEN is not 512")
require(machine["elen_bits"] == 64, "ELEN is not 64")
require(machine["vlenb"] == 64, "vlenb is not 64")
require(machine["scalar_and_vector_share_one_hart"] is True,
        "the manifest does not state that scalar and vector share one hart")
require(machine["isa"] in version_text,
        "--version and the manifest disagree about the ISA")
require("not pipeline- or cycle-accurate" in machine["accuracy_note"].lower(),
        "the manifest does not disclaim cycle accuracy")

# The VP++ source is a base revision *plus* a patch series, so a single
# revision would describe source that was never compiled.
vpp = manifest["upstream"]["riscv_vp_plusplus"]
require(vpp["linked"] is True, "the manifest does not declare VP++ linked")
require(len(vpp["base_revision"]) == 40, "the VP++ base is not a full SHA")
require(len(vpp["patch_series"]) >= 3,
        "the approved VP++ patch series is not recorded")
for patch in vpp["patch_series"]:
    require(len(patch["patch_sha256"]) == 64,
            f"patch {patch['reference']} has no content hash")
    require(patch["kind"] in ("upstream-backport", "downstream-conformance"),
            f"patch {patch['reference']} has an unknown kind {patch['kind']!r}")
    require(patch["reference"] in version_text,
            f"--version does not name patch {patch['reference']}")
require(vpp["effective_source"].startswith(vpp["base_revision"]),
        "effective_source does not start from the base revision")

# Spike is pinned as an oracle and never linked. Saying otherwise would put a
# licence obligation and an accuracy claim on a binary that has neither.
require(manifest["upstream"]["spike"]["linked"] is False,
        "Spike is declared linked")

# Every non-goal, recorded as absent. The independence gate is what enforces it;
# this makes a package that quietly acquired one contradict its own manifest.
absent = manifest["absent"]
for component in ("floo_noc", "sauria", "neo_dma", "core_sram",
                  "image_transform", "spike", "qt_vnc_gui",
                  "upstream_vpp_platforms"):
    require(absent[component] is False,
            f"the manifest claims {component} is present")

host_io = manifest["memory_map"]["host_io"]
require(host_io["simulator_only"] is True,
        "the host-I/O window is not marked simulator-only")
require(manifest["memory_map"]["ram"]["base"] == 0x80000000,
        "the RAM base is not the TPU_V3 global-RAM base")
print("build manifest PASS")
PY

# ── an image built by the documented external commands ───────────────────────
#
# The last line of the packaging gate: "a packaged ELF built by the documented
# external cross-toolchain commands can be substituted for the bundled examples
# and produces the same result."
#
# Built from the shipped SDK, in the moved bundle, with the source tree
# unreachable. If the SDK depended on anything outside itself -- a relative path
# back to the platform's include directory, a file packaging forgot -- this is
# where it shows.

sdk_build="${run_dir}/bundle/sdk"
( cd "${sdk_build}" && make TOOLDIR="${tooldir}" clean all verify ) \
    >"${evidence_dir}/sdk_build.log" 2>&1 \
    || { cat "${evidence_dir}/sdk_build.log" >&2; fail "rebuilding from the shipped SDK failed"; }

rebuilt="${sdk_build}/examples/rvv_vector_add/rvv_vector_add.elf"
[[ -f "${rebuilt}" ]] || fail "the SDK rebuild produced no vector example"

( cd "${bundle}" \
  && env -u LD_LIBRARY_PATH timeout --kill-after=10 300 \
         ./riscv_vpp_compiler_vp --elf "${rebuilt}" ) \
    >"${evidence_dir}/run_rebuilt.log" 2>&1 \
    || { cat "${evidence_dir}/run_rebuilt.log" >&2; fail "the SDK-rebuilt image failed"; }
grep -qF "VECTOR ADD: PASS" "${evidence_dir}/run_rebuilt.log" \
    || fail "the SDK-rebuilt image did not print its PASS marker"

# Same result, not merely "also passed": the guest-visible output of the
# substituted image must match the bundled one line for line.
extract_guest_output()
{
    sed -n '/^RVV=1.0$/,/^VECTOR ADD: PASS$/p' "$1"
}
diff <(extract_guest_output "${evidence_dir}/run_rvv_vector_add.log") \
     <(extract_guest_output "${evidence_dir}/run_rebuilt.log") \
    >"${evidence_dir}/substitution.diff" \
    || { cat "${evidence_dir}/substitution.diff" >&2
         fail "the substituted image produced different output"; }
echo "external-toolchain substitution PASS"

echo "riscv_vpp_compiler_vp packaging regression PASS"
