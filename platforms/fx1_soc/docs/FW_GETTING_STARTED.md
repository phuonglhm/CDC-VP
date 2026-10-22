# FX1 SoC VP — getting started for firmware engineers

This guide is for firmware and driver engineers who are new to the FX1 virtual
platform (VP). It takes you from a clean checkout to running, writing and
debugging bare-metal firmware against the VP's CPUs, SYS_DMA and ISP.

The VP is a functional SystemC/TLM model. It is not cycle-accurate. Every
address and interrupt number below is a **VP placeholder pending the HAS**;
all of them live in one header, so firmware that uses the header's macros
follows any later change. The platform reference, with the full firmware
contract, the test list and the limitations, is
[`../README.md`](../README.md).

## 1. What you get

- **CPUs:** two independent RV32IMAFC harts (the CVA6 extension set) on the
  RISC-V VP++ ISS, M-mode only, one ELF for both.
- **Interconnect and memory:** the FX1 bus, 512 MiB of DDR behind a DDR3
  controller model (CSRs, initialization, ECC, power states, error injection),
  a BootROM and a 1 MiB boot SRAM. By default the platform initializes the
  DDR3 before the harts start, so an ordinary image runs from DDR at once.
- **Interrupts and timer:** CLINT (`mtime` at 20 MHz) and PLIC (one M-mode
  context per hart).
- **Console:** a PL011-style UART.
- **SYS_DMA:** 8 channels, M2M on this platform.
- **ISP:** the FX1 ISP TLM model, with IDMA and ODMA, driven by the reference
  C99 driver.
- **sim-control:** a VP-only device. Firmware writes PASS or FAIL to it, and
  the simulator exits with that verdict.

## 2. Prerequisites (host)

| What | Version used for acceptance | Notes |
|---|---|---|
| Host OS | AlmaLinux 9 | Any Linux with the tools below should do |
| Host compiler | GCC/G++ 11.5 (`/usr/bin/gcc`, `/usr/bin/g++`) | |
| CMake / Ninja | CMake ≥ 3.21 (3.31 used), Ninja | |
| SystemC | 2.3.4 in `/opt/systemc-2.3.4` | Elsewhere: `-DSYSTEMC_HOME=<prefix>` |
| RISC-V cross toolchain | xPack `riscv-none-elf-gcc` 15.2 in `/opt/toolchains/riscv-none-elf` | Elsewhere: `RISCV_HOME=<dir>` or `-DFX1_RISCV_PREFIX=<dir>/bin/riscv-none-elf-` |
| RISC-V VP++ sources | pinned, patched checkout in `third_party/riscv-vp-plusplus` | Run `cpu_models/riscv_vp_plusplus/fetch_riscv_vp_plusplus.sh` once |
| FreeRTOS-Kernel | V11.2.0 (`0adc196`) in `third_party/FreeRTOS-Kernel`, for the FreeRTOS CLI image | Run `tools/third_party/setup_third_party.sh` once (gitignored, not in a fresh clone). Configure stops if it is missing or another version; elsewhere: `-DFX1_FREERTOS_KERNEL_DIR=<dir>` |
| Python 3 + numpy | `python3` on `PATH` with numpy | Needed by the full CI gate (`fx1_regression_isp_fixture_check` in the regression stage), to regenerate ISP fixtures and to convert a RAW frame for the demo (`run_isp_demo.py`). Packing NV12 and the PNG preview (`isp_pack.py`) need only the standard library. Not needed to build or run firmware, nor by `run_ci.sh --smoke-only` |

Put `/usr/bin` first on `PATH` and use the system compilers. Some hosts carry
vendor binutils wrappers (`nm`, `as`) earlier on `PATH`, and those break the
build:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
```

## 3. Build and run everything

From the repository root, one command configures, builds and runs the FX1
tests:

```bash
platforms/fx1_soc/ci/run_ci.sh                     # build dir: build-fx1-ci, -j1
platforms/fx1_soc/ci/run_ci.sh --smoke-only --jobs 4
```

- **Stages:** configure, required-test manifest check, build of target
  `fx1_all`, smoke (labels `fx1_unit` and `fx1_smoke`), components (the FX1
  bus and UART model tests, by name), regression (label `fx1_regression`).
- **Outputs:** `build-fx1-ci/fx1-ci-artifacts/` receives a summary, JUnit XML
  per stage, the ctest logs, and every firmware UART log.
- **Exit status:** 0 means every stage passed.
- **Required tests:** `platforms/fx1_soc/ci/required_tests.txt` lists the tests
  the gate requires. The script stops before building when a listed test of a
  stage that runs is not registered, or when the manifest is missing,
  unreadable or empty. With `--smoke-only` the regression entries are not
  checked; those tests need python3 with numpy.

The same thing by hand:

```bash
cmake -S . -B build-fx1 -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCDC_BUILD_FX1_SOC=ON -DFX1_SOC_BUILD_FIRMWARE=ON -DCDC_BUILD_TESTS=ON
cmake --build build-fx1 --target fx1_all -j1
ctest --test-dir build-fx1 -L "fx1_unit|fx1_smoke" --no-tests=error
ctest --test-dir build-fx1 -L fx1_regression --no-tests=error
```

Build `fx1_all`, not the default target. The default target also builds
unrelated components of this repository, and one of them (`components/isp_tlm`)
does not link today.

## 4. Run one firmware image

```bash
build-fx1/platforms/fx1_soc/fx1_soc --fw build-fx1/fw/fx1_soc/example_quickstart.elf
```

Expected output, between the SystemC banner and the summary:

```text
[h0] hello from hart 0
[h1] hello from hart 0x00000001
[h0] timer interrupt taken
[h0] DMA copy verified
[h0] PASS
[fx1_soc] RESULT PASS
```

- `[hN]` prefixes the console line of hart N.
- The `[fx1_soc]` lines report simulated time, instructions per hart, host
  MIPS, the exclusive-monitor counters and the verdict.

| Option | Meaning |
|---|---|
| `--fw <elf>` | Image to load into DDR (required). Every hart starts at its entry |
| `--timeout-ms N` | Simulated-time budget from CPU release, default 1000; `0` = no limit |
| `--ddr-boot preinit\|firmware` | `preinit` (default): the platform initializes and scrubs the DDR3 (about 0.67 s of simulated time before the harts start), then loads the image. `firmware`: the harts start from BootROM at once and firmware brings the DDR3 up (BootROM images only) |
| `--ddr3-fault T_US:KIND[:ARG]` | Host DDR3 fault T_US µs after CPU release, e.g. `20000:ce:0x90000100` (repeatable; `--help` lists the kinds) |
| `--ddr3-ecc on\|off` | DDR3 ECC build option, default on |
| `--realtime` | Simulated time never runs ahead of wall-clock time (interactive consoles) |
| `--quantum-ns N` | TLM quantum, multiple of 10 ns, default 1000. Results must not depend on it; the smoke tests run 1 µs and 10 µs |
| `--uart-log FILE` | Copy of the console |
| `--quiet-uart` | No console on stdout |
| `--trace` | Print every bus transaction (time, port, initiator, address, target) |
| `--load ADDR:FILE` | Before the harts start, copy `FILE` into DDR at `ADDR` (repeatable): host data such as a RAW frame. Preinit boot only |
| `--uart0-socket PORT` (`--uart0-wait`) | UART console on TCP 127.0.0.1:PORT; connect with `nc`. With `--uart0-wait` the run waits for the client |
| `--uart0-rx-file FILE` (`--uart0-rx-delay-us N`, `--uart0-rx-byte-us N`) | Inject `FILE` into UART RX, starting N µs of simulated time after CPU release, one byte per `--uart0-rx-byte-us` (default 10; use 87, the 115200 8N1 character time, for the FreeRTOS CLI) |
| `--dump ADDR:LEN:FILE` | After the run, write `LEN` bytes of DDR at `ADDR` to `FILE` (repeatable) |
| `--no-exclusive-monitor` | Upstream VP++ LR/SC model. Negative controls only |

| Exit | Meaning |
|---|---|
| 0 | PASS |
| 1 | FAIL: firmware reported a code (`RESULT FAIL code=0x...`) |
| 2 | Timeout: no verdict within the budget (the PC of every hart is printed) |
| 3 | Simulator or integration error, e.g. a model that never answered |
| 4 | Usage error |

## 5. Write your own firmware

1. **Copy the example.** Start from
   [`fw/fx1_soc/examples/quickstart/main.c`](../../../fw/fx1_soc/examples/quickstart/main.c).
2. **Register it as a test** in `fw/fx1_soc/CMakeLists.txt`, next to the other
   tests:

   ```cmake
   fx1_smoke_test(my_test TIMEOUT_MS 200
       SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/tests/my_test/main.c
       EXTRA_SOURCES ...)     # optional: more .c files
   ```

   This builds `my_test.elf` with the cross compiler. It also registers
   `fx1_smoke_my_test` and `fx1_smoke_my_test_q10us`, the same image under
   two quanta. Pass means exit 0; a `RESULT FAIL/TIMEOUT/ERROR` line fails
   the test. If the test is part of the acceptance gate, also add both names
   to `platforms/fx1_soc/ci/required_tests.txt`: `run_ci.sh` refuses to
   report PASS when a listed test is missing.
3. **Rebuild** with `cmake --build build-fx1 --target fx1_all`, then run the
   test with `ctest --test-dir build-fx1 -R my_test` or call the simulator
   directly.

What the build gives every image:

- **ABI:** `-march=rv32imafc_zicsr_zifencei -mabi=ilp32f -mcmodel=medany -O2`,
  freestanding, no C library. GCC built-ins are disabled.
- **Startup:** `fw/fx1_soc/common/crt0.S` sets up a 64 KiB stack per hart.
  Hart 0 clears `.bss` and calls `main()`; its return value is the verdict
  (0 is PASS). Hart 1 waits in WFI until hart 0 calls
  `fx1_release_secondaries()`, then runs `secondary_main(hartid)`.
- **Linker script:** `fw/fx1_soc/common/fx1.ld.S` links the image at
  0x8000_0000 and asserts that the firmware regions below are disjoint.

### Headers

| Header | Content |
|---|---|
| `fx1/fx1_memory_map.h` | Addresses, IRQ numbers, PLIC contexts, firmware regions, reservation granule: the only place for placeholders |
| `fx1_fw.h` | HAL: MMIO, barriers, buffer ownership, console, verdict, CLINT/PLIC, traps, interrupts, hart start-up, lock |
| `dma/fx1_dma_regs.h` | SYS_DMA register map |
| `fx1_isp/fx1_isp_csr.h` | ISP register map, generated from the CSR schema |
| `fx1_isp/fx1_isp_drv.h` | Reference ISP driver (compile `isp/driver/src/fx1_isp_drv.c` into the image) |

### Memory map and interrupts (placeholders)

| Region | Base | Size |
|---|---|---|
| BootROM | 0x0000_0000 | 2 MiB (code of `--ddr-boot firmware` images) |
| SRAM | 0x0100_0000 | 1 MiB (data/stacks of `--ddr-boot firmware` images) |
| CLINT | 0x0200_0000 | SiFive layout |
| PLIC | 0x0C00_0000 | SiFive layout, context n = hart n M-mode |
| UART | 0x1000_0000 | 32-bit access only |
| SYS_DMA CSR | 0x1001_0000 | 32-bit access only |
| sim-control (VP only) | 0x1002_0000 | |
| I2C0 | 0x1003_0000 | 32-bit access only, `i2c_tlm` register map (`components/i2c_tlm/include/i2c.h`) |
| ISP CSR | 0x1100_0000 | 64 KiB |
| CSI-2 RX CSR | 0x1104_0000 | `--camera` only, `fx1_csi2/fx1_csi2_regs.h`; drivers in `fw/fx1_soc/drivers` (I2C0, sensor, CSI-2 MMIO HAL) and `mipi-csi2/driver`, examples `fw/fx1_soc/tests/camera` |
| DDR3 CSR | 0x1200_0000 | 64 KiB, `fx1_ddr3/fx1_ddr3_regs.h` |
| DDR_SYSCTRL (VP only) | 0x1201_0000 | `fx1/fx1_ddr_sysctrl.h` |
| DDR | 0x8000_0000 | 512 MiB |

| DDR firmware region | Base | Use |
|---|---|---|
| `FX1_FW_IMAGE_BASE` | 0x8000_0000 | Code and data (4 MiB) |
| `FX1_FW_STACK_BASE` | 0x8040_0000 | Stacks, 64 KiB per hart |
| `FX1_FW_SHARED_BASE` | 0x8050_0000 | Shared data, descriptors |
| `FX1_FW_RAW_BUF_BASE` | 0x8100_0000 | ISP RAW inputs |
| `FX1_FW_YUV_BUF_BASE` | 0x8800_0000 | ISP NV12 outputs |
| `FX1_FW_TEST_AREA_BASE` | 0x9000_0000 | Scratch for tests |

PLIC sources: UART = 1, SYS_DMA = 2, ISP = 3, CSI-2 RX = 7 (`--camera` only), DDR3 = 8,
I2C0 = 9. Sources 4 to 6 are VP test lines driven by sim-control. All sources are level-triggered, so clear the source at
the device before `fx1_plic_complete()`.

## 6. Rules that matter on this platform

The full contract is in [`../README.md`](../README.md), "Firmware contract".
The rules people trip over:

- **Ordering.** `volatile` orders nothing between harts or devices. Use the
  barriers: `fx1_mb`, `fx1_wmb_io`, `fx1_release`, `fx1_acquire`.
- **Buffer ownership (plan C12).** Every buffer a device touches changes owner
  through a call that carries the direction of the device's access:
  - `fx1_dma_prepare(addr, bytes, dir)`: before the device starts, also for
    buffers the device only writes;
  - `fx1_dma_complete(addr, bytes, dir)`: after the device reports completion,
    before the CPU reads.

  On the VP both calls are a fence. On silicon they become cache maintenance
  that depends on the HAS. The VP cannot tell you that you forgot one.
- **Atomics (plan C7).** LR/SC is a real reservation, with a 64-byte granule
  placeholder. A write by the other hart, SYS_DMA or the ISP ODMA into the
  granule cancels it. Write retry loops, and keep lock words out of DMA
  buffers.
- **One owner per IP.** Drive each IP from one hart only.
- **MMIO.** Use 32-bit aligned accesses. Byte or halfword accesses to UART or
  DMA registers raise access faults.
- **Trap handlers** run with interrupts masked and must not use FP. Install
  them per hart with `fx1_set_trap_handler()`.
- **Bus errors** become access faults: fetch → mcause 1, load → 5,
  store/AMO → 7, with `mtval` set to the address. A model that never answers
  is a simulator error (exit 3), not a guest fault.

## 7. FreeRTOS and the CLI

`fw/fx1_soc/freertos` contains FreeRTOS V11.2.0 on hart 0 with a UART CLI.
- **Hart 1 at boot:** a bare-metal IP check; any failure halts the boot.
- **FP context:** f0–f31 and `fcsr` are saved per task.
- **Commands:** cover the system, memory and registers, ISP, SYS_DMA and a
  `selftest`.

Run it interactively:

```bash
build-fx1/platforms/fx1_soc/fx1_soc --fw build-fx1/fw/fx1_soc/freertos_cli.elf \
    --uart0-socket 5000 --uart0-wait --timeout-ms 0 --realtime
platforms/fx1_soc/tools/fx1_console.sh 5000     # second terminal, then type: help
```

`--timeout-ms 0` removes the simulated-time limit (end with `poweroff`).
`--realtime` keeps simulated time at wall-clock pace while the system is
idle; without it, idle time runs about 7× faster than real time.

See [`../../../fw/fx1_soc/freertos/README.md`](../../../fw/fx1_soc/freertos/README.md)
for the boot sequence, the command list, how to add a command and how
interrupts reach tasks.

## 8. Drivers and examples to start from

| Topic | Where |
|---|---|
| SYS_DMA programming | [`components/FX1_Components/dma/docs/PROGRAMMING_GUIDE.md`](../../../components/FX1_Components/dma/docs/PROGRAMMING_GUIDE.md); examples `tests/dma_m2m`, `tests/recovery/dma_recovery.c` (errors, chains, pause/resume) |
| ISP programming | [`components/FX1_Components/isp/docs/ISP_PROGRAMMING_GUIDE.md`](../../../components/FX1_Components/isp/docs/ISP_PROGRAMMING_GUIDE.md); MMIO HAL `tests/isp_stream/isp_common.c`; streaming `tests/isp_stream/main.c`; statistics `frame_id.c`; error and reset recovery `tests/recovery/isp_recovery.c`; frame metadata, AWB result ownership, DMA reset and Resizer `tests/isp_stream/isp_mas0710.c` |
| Interrupts | `tests/plic_irq`, `tests/clint_irq` |
| Two harts, atomics | `tests/hello_2hart`, `tests/atomic/*` |
| Two harts and two devices at once | `tests/shared_mem/hart1_dma.c` with `isp_stream/main.c` |

All paths in this table are under `fw/fx1_soc/` unless they start with
`components/`.

### ISP fixture and profiles

The ISP tests need no external data:

- **Input.** `tests/isp_stream/isp_fixture.h` holds a deterministic 64×48 RGGB
  RAW formula (firmware recomputes it). It also holds three register profiles
  (`basic`, `basic + GTM auto`, `basic + statistics`) as `{offset, value}`
  tables. A second, cheaper formula gives the 648×368 frames of the Resizer
  case, which has its own `basic` profile.
- **Expected results.** The same header holds the NV12 CRC-32 and the
  statistics expected for every frame. It also holds the expected outcome of
  every step of the AWB ownership script (`isp_mas0710.c`) and the CRCs of the
  three Resizer frames.
- **Provenance.** The values come from the pinned Python reference of the ISP
  component, and the header records the SHA-256 of the reference files used.

Regenerate the header after changing the reference or the profiles, then check
it:

```bash
python3 fw/fx1_soc/tests/isp_stream/gen_fixture.py
python3 fw/fx1_soc/tests/isp_stream/gen_fixture.py --check
```

The `--check` step is the `fx1_regression_isp_fixture_check` test, registered
when numpy is available. These are project reference values, not silicon
golden data.

## 9. Get RAW and YUV out of a run

`--dump` copies DDR to files after the run. The ISP tests use 4 buffer slots of
0x2000 bytes:

- RAW input `i` at `0x8100_0000 + i*0x2000`: 64×48 16-bit samples, stride 128,
  so 6144 bytes;
- Y of output `i` at `0x8800_0000 + i*0x2000` (64×48, stride 64, 3072 bytes);
- UV of output `i` 0x1000 above its Y (1536 bytes).

```bash
build-fx1/platforms/fx1_soc/fx1_soc --fw build-fx1/fw/fx1_soc/isp_stream_basic.elf \
    --dump 0x81000000:6144:raw0.bin \
    --dump 0x88000000:3072:y0.bin --dump 0x88001000:1536:uv0.bin
cat y0.bin uv0.bin > frame.nv12      # NV12: Y plane, then interleaved UV
python3 components/FX1_Components/isp/tools/nv12_preview.py \
    --nv12 frame.nv12 --width 64 --height 48 --out frame.png
```

After the 8-frame stream, slot `i` holds frame `i + 4`, because each buffer is
used twice. A preview that looks right is not a pass criterion; the CRCs are.

### A real RAW frame through the whole platform

`--load ADDR:FILE` is the other direction of `--dump`: the simulator copies a
host file into DDR before the harts start. The ISP demo uses it to process a real camera
frame. The host loads the RAW, a register profile and a job descriptor; the
firmware on hart 0 drives the ISP through the driver; the host takes the NV12
back and compares it with the stand-alone ISP model:

```bash
platforms/fx1_soc/demo/run_isp_demo.py --raw <frame.raw> --build-dir build-fx1 --out /tmp/isp_demo
```

See [`../demo/README.md`](../demo/README.md) for the options, the outputs and
the DDR layout.

## 10. Debugging

- **Read the FAIL code.** `RESULT FAIL code=0xNN` is the code your firmware
  passed to `fx1_check()` or `fx1_fail()`. The test prints what it checked on
  the line before. The default trap handler reports `0xE000 | mcause`.
- **Timeouts.** A `RESULT TIMEOUT` prints every hart's PC. Look the PC up with
  `riscv-none-elf-objdump -d <elf>`.
- **Bus traffic.** `--trace` prints every routed transaction; combine it with
  `--quiet-uart` and grep for an address or an initiator (`source=N`).
- **Timing.** Simulated time is not silicon time. A CPU memory access costs
  about 100 ns on this VP, because every access synchronises, and the ISP
  processes a 64×48 frame in a few µs. Do not tune delays against these
  numbers.
- **Quanta.** If a result changes between `--quantum-ns 1000` and `10000`,
  you have a race in your firmware or a VP time-model bug. Report it.

## 11. Known limitations

The authoritative list is [`../README.md`](../README.md), "Limitations". In
short:

- No caches, PMP, S-mode or Zicbom (cache-maintenance instructions).
- The harts run every instruction from DDR (no cache) through a bus that
  serializes DDR traffic: CPU timing is pessimistic, especially next to DMA
  traffic. Do not calibrate CPU-versus-device races on the VP.
- DDR_SYSCTRL (DDR3 sideband glue) is VP-only; DDR3 field encodings that the
  specification leaves open are provisional (use the macros).
- SYS_DMA runs M2M only: no peripheral handshake partner is integrated yet.
- There is no reset controller in the map (except the DDR3 resets in
  DDR_SYSCTRL). IP-level recovery uses the drivers' soft reset; a SYS_DMA
  command cannot be cancelled, only paused (`CH_ENABLE = 0`).
- The exclusive monitor's granule and SC/AMO stall behaviour are VP
  abstractions.
- Every address and IRQ number is a placeholder pending the HAS.
