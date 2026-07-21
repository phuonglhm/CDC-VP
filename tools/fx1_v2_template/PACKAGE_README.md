# VP_FX1_V2.0 package

This directory set is the internal handover of the CDC-VP FX1 virtual platform,
its NPU-enabled FreeRTOS/TFLM firmware, and the sources directly changed for
the NPU integration.

## Package layout

| Path | Contents |
|---|---|
| `sw/bootloader/sources/VP_FX1_V2.0` | Rebuildable FreeRTOS/NPU/TFLM firmware source, selected pinned FreeRTOS source, TFLM application headers plus its pinned prebuilt archive, and VP NPU integration reference source |
| `sw/bootloader/build/VP_FX1_V2.0` | Generated RV32 ELF, raw BIN, disassembly, linker map, and section/symbol report |
| `sw/bootloader/test/VP_FX1_V2.0` | Relocatable VP smoke/interactive runner and deterministic CLI input |
| `vp/bin/VP_FX1_V2.0` | NPU-enabled x86-64 VP, SystemC runtime, and default configuration |
| `vp/doc/VP_FX1_V2.0` | Package metadata, source notes, integration documentation, and SHA-256 manifest |

## Test the package

```bash
cd sw/bootloader/test/VP_FX1_V2.0
./run_vp.sh                 # automated NPU/TFLM/scan/register smoke
./run_vp.sh interactive     # then: nc 127.0.0.1 5000
```

The automated test writes the complete UART transcript to
`sw/bootloader/build/VP_FX1_V2.0/last_run.log`.

Validate the delivered files before rebuilding:

```bash
sha256sum -c vp/doc/VP_FX1_V2.0/SHA256SUMS
```

After a local rebuild, the ELF/BIN/disassembly checksums are expected to differ
from the delivery manifest (debug information also records the local path).

## Rebuild the firmware

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:/opt/toolchains/riscv-none-elf/bin:$PATH
$CC -dumpfullversion
$CXX --version | head -n 1

make -C sw/bootloader/sources/VP_FX1_V2.0 clean all NPU=1 TFLM=1
```

The exported firmware Makefile always writes generated files to
`sw/bootloader/build/VP_FX1_V2.0`; it does not dirty the source directory.
The TFLM archive is stored outside the output directory at
`sources/VP_FX1_V2.0/third_party/tflite-micro/prebuilt/`. Therefore the entire
`build/VP_FX1_V2.0` directory can be deleted and recreated offline. The archive
was built from the pinned upstream commit and exact RV32 toolchain recorded in
`BUILD_INFO.txt`. Only headers used by the application are copied, because the
full upstream TFLM clone is not project-owned source.

## Source ownership and rebuild boundary

The FreeRTOS firmware and CDC NPU adapter/platform integration snapshot are
copied from the CDC-VP commit recorded in `BUILD_INFO.txt`. The canonical VP
source remains CDC-VP. `vp_integration_reference/` is provided for review and
traceability; rebuilding the host VP still requires the complete CDC-VP tree
and the separately maintained private `SAURIA_NPU_ROOT` implementation.

Do not independently edit both the package snapshot and CDC-VP. Apply changes
to CDC-VP, run its regression, and regenerate this package so the source,
binary, documentation, and checksums stay aligned.

## Known build warnings

- The upstream FreeRTOS RV32 port emits an integer-to-pointer-size warning in
  `vPortSetupTimerInterrupt` on this toolchain.
- The bare-metal firmware linker reports one RWX load segment.

Both warnings are present in the validated CDC-VP build and are not introduced
by packaging.
