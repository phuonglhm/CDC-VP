# fx1_soc SDK packaging (VP_FX1_V3.0)

This directory turns a CDC-VP build of `fx1_soc` into the `VP_FX1_V3.0`
delivery for the firmware and software teams. The delivery uses the layout of
the SDK repository (`fx1`: `vp/{bin,doc}/<ver>`,
`sw/bootloader/{sources,build,test}/<ver>`), so exporting it is a copy.

| File | Role |
|---|---|
| `package_sdk.sh` | assembles the tree. Behind the CMake target `fx1_soc_package` |
| `check_package.sh` | checks a tree: contents, `RPATH=$ORIGIN` + `ldd` with an empty environment, licence texts, `SHA256SUMS`, manifest vs. files and `--version` |
| `run_packaging_regression.sh` | the distribution gate, ctest `fx1_packaging` (label `fx1_packaging`, not in `run_ci.sh`) |
| `run_docker_check.sh` | manual: runs the package in a clean `almalinux:9` container |
| `export_to_sdk.sh` | copies a checked package into the SDK repository; never commits |
| `write_manifest.py` | `BUILD_MANIFEST.json` |
| `sdk/` | the SDK firmware `Makefile` and README, shipped as is |
| `test/` | `run_vp.sh`, the `isp run` replay and the NPU self-test script, shipped as is |
| `docs/` | the delivery's README, memory map and firmware guides (camera, Ethernet, DDR3, NPU) |

## What goes in

| Item | Source |
|---|---|
| **VP** | `fx1_soc` from the build, made portable by `cdc_make_portable` (`RPATH=$ORIGIN`), with the `libsystemc.so.2.3` it needs |
| **Firmware sources** | the files the CDC-VP build compiles for `freertos_cli.elf`, copied, never rewritten, plus the FreeRTOS-Kernel V11.2.0 subset |
| **Firmware outputs** | built by the shipped `sdk/Makefile` during packaging. That Makefile is location-independent (`-ffile-prefix-map`), so a user's rebuild gives the packaged ELF byte for byte with the same compiler |
| **ISP samples** | `isp/<sample>/` holds the prepared job (`run_isp_demo.py --prepare-only --job-env`) and the golden NV12 SHA-256 from the stand-alone model (`fx1_isp_run_raw`), not from the VP |
| **NPU** | the driver, job format, `cmd_npu.c`, C oracle and fixture generator (`fw/fx1_soc/host/npu_job_gen.c`, `make npu-jobs`) as firmware sources; `demo/npu_run.py` and every fixture of `fx1_npu_jobs` (`test/npu/<name>/`, with `FIXTURE.txt`). Packaging checks that `make npu-jobs` from the shipped sources gives the build's fixtures byte for byte, then runs `run_vp.sh npu-selftest` and `run_vp.sh npu all` on the assembled tree; the outcomes go into the manifest's `npu` section with the model drop's manifest SHA-256, ABI, features and the model configuration (`env=none`) |
| **Docs and licences** | `docs/` (`NPU_REGISTER_ABI.md`, the SDK adaptation of the NPU component README, ships as `npu/register_abi.md`: `fx1_unit_npu_abi_doc_sync` keeps its tables and headings equal to the component's), `LICENSE`, `NOTICE`, `THIRD_PARTY.md`, the VP++ MIT licence, the SoftFloat notice (extracted from a compiled source), the SystemC licence, the FreeRTOS MIT licence, the SAURIA licence and provenance (`licenses/SAURIA.*`), the model drop's `THIRD_PARTY_NOTICES.md` unchanged and beside it `SAURIA-V4.7-DROP.SDK_SCOPE.md` (`docs/SAURIA_DROP_SDK_SCOPE.md`: what of the notice the delivery contains) |

**ISP samples:**

| Sample | Source | Profile | Status |
|---|---|---|---|
| `d65` (default) | `components/isp_tlm/input/D65_raw_2688x1520_5376.raw` | `shared_bggr_2688x1520_full` | cleared, `ASSET_PROVENANCE.md` |
| `outdoor1` | the RAW pointed to by `FX1_DEMO_RAW` | `shared_bggr_2688x1520_full` | **internal only**: not cleared for public release |

`outdoor1` is added only when `FX1_DEMO_RAW` is set. The default profile is
the checked-in `isp/samples/profiles/shared_bggr_2688x1520_full.csrw` under
`components/FX1_Components/`. It is a fixed test profile derived from the cleared
D65 baseline, with v2 colour/detail tuning selected on 2026-10-13, not sensor
calibration or a per-frame AWB algorithm. Packaging
does not compute WB gains from each input. Both samples refer to one delivered
`profiles/shared_bggr_2688x1520_full/` directory using `PROFILE_ID` in `job.env`.
`--sample NAME=RAW,profile=FILE` still supports another fixed profile: each
distinct profile ID is copied once, and conflicting contents under one ID fail.
Every sample's job and standalone golden use two frames: the first warms up
GTM auto, and the second produces the saved image. Its golden is generated
with the profile it actually selects.

**`SHA256SUMS`** covers what the SDK repository tracks, so it does not cover
`build/`: that repository ignores `sw/bootloader/build/`, and every user
rebuilds it. The firmware hashes are in the manifest.

## Making a release

The official package must come from a committed, clean CDC-VP tree. The
export refuses a dirty one.

```bash
export CC=/usr/bin/gcc CXX=/usr/bin/g++ PATH=/usr/bin:/bin:$PATH
cd CDC-VP && git status --short                          # must be empty

# 1. the gate: fresh Release build, package, move, run, rebuild firmware
FX1_DEMO_RAW=<outdoor1 RAW> platforms/fx1_soc/package/run_packaging_regression.sh

# 2. the release build and package (Release, FX1 only)
cmake -S . -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DCDC_BUILD_FX1_SOC=ON \
      -DCDC_BUILD_MINI_TLM=OFF -DCDC_BUILD_CPU_EVAL=OFF -DCDC_BUILD_CUSTOM_SOC=OFF -DCDC_BUILD_NOC_SOC=OFF \
      -DCDC_BUILD_TPU_V3_SOC=OFF -DCDC_BUILD_RISCV_VPP_COMPILER_VP=OFF -DFX1_DEMO_RAW=<outdoor1 RAW>
cmake --build build-rel --target fx1_soc_package -j1      # -> out/VP_FX1_V3.0

# 3. optional: a clean AlmaLinux 9 container, with the firmware rebuilt inside
platforms/fx1_soc/package/run_docker_check.sh out/VP_FX1_V3.0 --toolchain /opt/toolchains/riscv-none-elf

# 4. into the SDK repository (review and commit there by hand)
platforms/fx1_soc/package/export_to_sdk.sh out/VP_FX1_V3.0 ../fx1
```

**The gate ignores the ambient `SYSTEMC_HOME`.** A login shell may point it at
another SystemC, such as the one Fast Models ships. The gate uses
`PACKAGING_SYSTEMC_HOME`, default `/opt/systemc-2.3.4`.

`export_to_sdk.sh` leaves two files of the SDK repository for you to edit
together with the commit:
- the root `README.md`, which needs the package table row;
- `vp/VERSION`.

## Limits

- **Fixed sample geometry.** Samples are 2688×1520 BGGR `le16_raw10_msb`
  frames; `package_sdk.sh` assumes that geometry for the job and reference run.
  The shared default profile targets this mode; another mode needs a suitable profile.
- **ELF reproducibility needs the same compiler.** It holds for the same
  cross compiler; the manifest records which one was used.
- **The package is x86-64 Linux only.** It is validated on AlmaLinux 9 and in
  the `almalinux:9` container.
- **The package needs an NPU build** (`FX1_SOC_ENABLE_NPU=ON`, the default):
  `package_sdk.sh` refuses a simulator without it. `licenses/SAURIA.PROVENANCE.md`
  requires the rights owner's separate approval before an NPU-enabled binary is
  published.

### Adaptive RAW jobs

The shared default also delivers an independently generated adaptive golden.
`run_vp.sh isp SAMPLE auto` runs the bounded firmware `neutral-v1` demo policy:
one AWB probe plus two render frames, with decisions logged as CSR values.
`package_sdk.sh` compares this output against the stand-alone ISP using those
resolved controls before recording `expected_auto_nv12.sha256` in the manifest.
Other/custom profile IDs keep their fixed-mode packaging behavior. See the SDK
README for prerequisites, fallback behavior and the limits of this demo policy.
