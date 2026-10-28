#!/bin/bash
# Assemble the FU3 VP release package (plan T23) into an fx1-style vp/ tree:
#   vp/bin/<NAME>/      vp_fu3_soc + SystemC runtime (RPATH $ORIGIN)
#   vp/configs/<NAME>/  prebuilt example images, stimulus, run_package_tests.sh
#   vp/doc/<NAME>/      guide, FU3_VP_ABI_1, maps, ISA, clock/reset, R1 gaps,
#                       test results summary, MANIFEST.txt (sha256 of every file)
#   vp/licenses/README.md  gains a section for <NAME> (once)
# and the firmware side in the fx1 sw/bootloader layout (SW_DIR):
#   sw/bootloader/sources/<NAME>/  example firmware, Makefile, README (built with
#                                  make), include/ (fu3_vp_abi.h, csr/ IP headers)
#   sw/bootloader/build/<NAME>/    make output and demo logs (git-ignored in fx1)
#   sw/bootloader/test/<NAME>/     run_vp.sh (demos, smoke check)
#
#   make_package.sh BUILD_DIR VP_DIR [NAME]
#     BUILD_DIR  a CDC-VP build configured with -DFU3_SOC_PACKAGE_NAME=NAME
#     VP_DIR     the vp/ directory of the fx1 repository
#   env: SW_DIR=dir      the sw/bootloader directory of the fx1 repository (required)
#        TEST_LOGS=dir   regression logs of BUILD_DIR, summarised in test_results.md
#        SPEC_DIRS=a:b   directories of spec documents to fingerprint in the manifest
#        GDB=path        riscv-none-elf-gdb for the package's GDB check
#        FORCE=1         replace an existing package of the same name (an empty
#                        directory is used without FORCE)
set -euo pipefail

BUILD=$(cd "$1" && pwd)
VPDIR=$(cd "$2" && pwd)
NAME=${3:-VP_FU3_V1.0}
SRC=$(cd "$(dirname "$0")/../../.." && pwd)   # CDC-VP root
PLAT=$SRC/platforms/fu3_soc
COMP=$SRC/components/FU3_Components
BINSRC=$BUILD/platforms/fu3_soc
BIN=$VPDIR/bin/$NAME CFG=$VPDIR/configs/$NAME DOC=$VPDIR/doc/$NAME
SW=$(cd "${SW_DIR:?make_package: SW_DIR (fx1 sw/bootloader) is required}" && pwd)
SWSRC=$SW/sources/$NAME SWBUILD=$SW/build/$NAME SWTEST=$SW/test/$NAME

version=$("$BINSRC/vp_fu3_soc" --version 2>/dev/null)
grep -q "vp_fu3_soc $NAME " <<< "$version" ||
    { echo "make_package: $BINSRC/vp_fu3_soc is not built as $NAME (-DFU3_SOC_PACKAGE_NAME)"; exit 1; }
for d in "$BIN" "$CFG" "$DOC" "$SWSRC" "$SWBUILD" "$SWTEST"; do
    if [ -e "$d" ] && [ -n "$(ls -A "$d")" ]; then
        [ "${FORCE:-0}" = 1 ] || { echo "make_package: $d exists and is not empty (FORCE=1 replaces it)"; exit 1; }
    fi
    rm -rf "$d"
done
mkdir -p "$BIN" "$CFG/fw" "$CFG/data" "$DOC" "$SWSRC/common" "$SWSRC/include/csr" "$SWSRC/examples" \
         "$SWBUILD" "$SWTEST"
LOGS=$(mktemp -d)   # regression and package-check logs: summarised, not shipped
trap 'rm -rf "$LOGS"' EXIT

# Binary and runtime.
cp -p "$BINSRC/vp_fu3_soc" "$BINSRC"/libsystemc.so* "$BIN/"

# Example images, sources, stimulus, checks.
for f in fu3_t20_console.elf fu3_t19_ibex.elf fu3_t19_cva6.elf fu3_t21_ibex.elf fu3_t21_cva6.elf vp_debug_blob.bin; do
    cp -p "$BINSRC/$f" "$CFG/fw/"
done
cp -p "$PLAT/tests/data/t20_rx.txt" "$PLAT/tests/data/t20_rx_pass_fail.txt" "$PLAT/tests/data/t20_flash.bin" "$CFG/data/"
cp -p "$PLAT/tools/package/run_package_tests.sh" "$CFG/"
chmod +x "$CFG/run_package_tests.sh"
cp -p "$PLAT/tools/package/configs_README.md" "$CFG/README.md"

# Documents.
P=$PLAT/doc/package
cp -p "$P/README.md" "$P/irq_dma_map.md" "$P/isa.md" "$P/clock_reset.md" "$P/r1_gaps.md" "$DOC/"
cp -p "$PLAT/doc/FU3_VP_ABI_1.md" "$DOC/"
{
    echo "# $NAME — bus memory map"
    echo
    echo "Output of \`vp_fu3_soc --list-instances\` of this package (FU3 Common Spec v2.1 map, CRM"
    echo "clock-gate/reset bits per target). DISABLED windows are the IP not received for R1."
    echo
    echo '```text'
    "$BINSRC/vp_fu3_soc" --list-instances 2>/dev/null | grep -v -e "SystemC 2" -e "Copyright" -e "ALL RIGHTS" | sed '/^$/d'
    echo '```'
} > "$DOC/memory_map.md"

# Firmware headers: FU3_VP_ABI_1 and the register definitions of every IP.
cp -p "$PLAT/include/fu3_vp_abi.h" "$SWSRC/include/"
CSR=$SWSRC/include/csr
cp -p "$P/csr_README.md" "$CSR/README.md"
cp -p "$P/fu3_adc_registers.md" "$CSR/"
cp -p "$COMP/ADC/docs/spec-decisions.md" "$CSR/fu3_adc_spec_decisions.md"
cp -p "$COMP/DMA0/AP_DMA/docs/registers.md" "$CSR/fu3_ap_dma_registers.md"
for h in crm/include/fu3_crm syscsr/include/fu3_syscsr plic/include/fu3_plic timer/include/fu3_timer \
         wdt/include/fu3_wdt uart/include/fu3_uart spi/include/fu3_spi i2c/include/fu3_i2c \
         GPIO/include/fu3/gpio QSPI/include/fu3/qspi hyperram/include/fu3_hyperram AES-256/include/fu3/aes \
         rng/include/fu3_rng mailbox/include/fu3_mailbox dbg/include/fu3_dbg; do
    rel=${h#*/include/}
    mkdir -p "$CSR/$rel"
    cp -p "$COMP/$h/registers.h" "$CSR/$rel/"
done

# Example firmware in the fx1 sw/bootloader layout: sources, then make and the demos
# (their logs stay in the git-ignored build directory).
for f in ibex_rom.S ibex_start.S cva6_start.S fw_trap.S fw_runtime.c fw_common.h ibex.ld cva6.ld; do
    cp -p "$PLAT/tests/fw/$f" "$SWSRC/common/"
done
for f in t20_console.c t19_ibex.c t19_cva6.c t21_ibex.c t21_cva6.c vp_debug_blob.S; do
    cp -p "$PLAT/tests/fw/$f" "$SWSRC/examples/"
done
cp -p "$PLAT/tools/package/sw/Makefile" "$PLAT/tools/package/sw/README.md" "$SWSRC/"
cp -p "$PLAT/tools/package/sw/run_vp.sh" "$SWTEST/"
chmod +x "$SWTEST/run_vp.sh"
make -C "$SWSRC" OUTPUT_DIR="$SWBUILD" > "$SWBUILD/sw_build.log" 2>&1 ||
    { cat "$SWBUILD/sw_build.log"; echo "make_package: example firmware build failed"; exit 1; }
echo "sw build: $(ls "$SWBUILD"/*.elf | wc -l) images and vp_debug_blob.bin" >> "$SWBUILD/sw_build.log"
"$SWTEST/run_vp.sh" smoke > "$SWBUILD/sw_demos.log" 2>&1 ||
    { cat "$SWBUILD/sw_demos.log"; echo "make_package: example firmware demos failed"; exit 1; }
echo "sw demos: $(grep -c '^ok ' "$SWBUILD/sw_demos.log") passed" >> "$SWBUILD/sw_demos.log"

# Test results: the regression of BUILD_DIR, the package's own checks, the firmware.
if [ -n "${TEST_LOGS:-}" ]; then
    cp -p "$TEST_LOGS"/*.log "$LOGS/"
fi
"$CFG/run_package_tests.sh" > "$LOGS/package_tests.log" 2>&1 ||
    { cat "$LOGS/package_tests.log"; echo "make_package: package checks failed"; exit 1; }
{
    echo "# $NAME — test results"
    echo
    echo "| Run | Result |"
    echo "| --- | --- |"
    for log in "$LOGS"/*.log "$SWBUILD/sw_build.log" "$SWBUILD/sw_demos.log"; do
        result=$(grep -hE "tests passed|package checks:|sw demos:|sw build:" "$log" | tail -1 || true)
        echo "| $(basename "$log" .log) | ${result:-no summary line} |"
    done
    echo
    echo "\`ctest_fu3\` is \`ctest -L fu3\` (the platform tests and every FU3 component test) of the"
    echo "CDC-VP build this package comes from; \`package_tests\` is \`configs/$NAME/run_package_tests.sh\`"
    echo "run on this package; \`sw_build\`/\`sw_demos\` are \`make\` in \`sw/bootloader/sources/$NAME\`"
    echo "and \`sw/bootloader/test/$NAME/run_vp.sh smoke\` (logs in \`sw/bootloader/build/$NAME/\`)."
} > "$DOC/test_results.md"

# License notices (shared fx1 file: add the section once).
if ! grep -q "^## $NAME" "$VPDIR/licenses/README.md"; then
    cat >> "$VPDIR/licenses/README.md" <<EOF

## $NAME

| Component                  | License      | Shipped as                            | Notice file        |
|----------------------------|--------------|---------------------------------------|--------------------|
| Accellera SystemC 2.3.4    | Apache-2.0   | \`vp/bin/$NAME/libsystemc.so*\`   | SYSTEMC.LICENSE, SYSTEMC.NOTICE |
| Bremen riscv-vp (RV32 ISS, CDC-VP fork \`cpu_models/fu3_iss\`) | MIT | statically linked into \`vp_fu3_soc\` | RISCV-VP.LICENSE |
| Berkeley SoftFloat-3       | BSD-3-Clause | statically linked into \`vp_fu3_soc\` | SOFTFLOAT.LICENSE  |
| CDC-VP                     | Apache-2.0   | \`vp_fu3_soc\`, example firmware and documents | CDC-VP.LICENSE, CDC-VP.NOTICE |
EOF
fi

# Manifest: revisions, source and spec fingerprints, sha256 of every package file.
cd "$SRC"
fingerprint=$(git ls-files -co --exclude-standard -- platforms/fu3_soc components/FU3_Components cpu_models/cva6_vp \
                  cpu_models/fu3_iss cpu_models/CMakeLists.txt components/CMakeLists.txt CMakeLists.txt cmake |
              sort | while read -r f; do [ -f "$f" ] && sha256sum "$f"; done | sha256sum | cut -d' ' -f1)
{
    echo "Package        : $NAME"
    echo "Assembled (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "CDC-VP HEAD    : $(git rev-parse HEAD)"
    echo "Working tree   : $(git status --porcelain --untracked-files=normal | wc -l) changed/untracked paths at assembly"
    echo "Source sha256  : $fingerprint (sorted sha256 of the FU3 sources: platforms/fu3_soc,"
    echo "                 components/FU3_Components, cpu_models/cva6_vp, cpu_models/fu3_iss, build files)"
    echo "Build          : $BUILD"
    echo
    echo "vp_fu3_soc --version:"
    sed 's/^/  /' <<< "$version"
    if [ -n "${SPEC_DIRS:-}" ]; then
        echo
        echo "Specifications (sha256):"
        IFS=: read -ra spec_dirs <<< "$SPEC_DIRS"
        for d in "${spec_dirs[@]}"; do
            find "$d" -type f \( -name "*.xlsx" -o -name "*.docx" -o -name "*.md" -o -name "*.pdf" \) | sort |
                while read -r f; do echo "  $(sha256sum "$f" | cut -d' ' -f1)  $f"; done
        done
    fi
    echo
    echo "Files (sha256, paths relative to vp/):"
    (cd "$VPDIR" && find "bin/$NAME" "configs/$NAME" "doc/$NAME" -type f ! -name MANIFEST.txt | sort |
        while read -r f; do echo "  $(sha256sum "$f" | cut -d' ' -f1)  $f"; done)
    echo
    echo "Firmware (sha256, paths relative to sw/bootloader/; build/ is make output, not listed):"
    (cd "$SW" && find "sources/$NAME" "test/$NAME" -type f | sort |
        while read -r f; do echo "  $(sha256sum "$f" | cut -d' ' -f1)  $f"; done)
} > "$DOC/MANIFEST.txt"
echo "make_package: $NAME assembled in $VPDIR ($(grep -c '^  [0-9a-f]\{64\}  [bcd]' "$DOC/MANIFEST.txt") files)"
