# FX1 SoC virtual platform

Functional SystemC/TLM-2.0 platform for FX1 firmware and driver bring-up.
It is not cycle accurate and does not model silicon timing or bandwidth.

Plan and decisions: `VP_BK/ISP/plan/FX1_PLATFORM_FIX_PLAN.md` section 7 (outside this repo).

**New to the platform? Start with [docs/FW_GETTING_STARTED.md](docs/FW_GETTING_STARTED.md)**
(build, run, write firmware, get RAW/YUV out, debug). This README is the reference.

## Status — stage G6 (FreeRTOS + CLI, SDK package VP_FX1_V3.0)

| Block | Model | State |
|---|---|---|
| CPU | 2 × RISC-V VP++ RV32 harts (`cpu_models/riscv_vp_plusplus`), hart ID 0/1, CVA6 extension set RV32IMAFC (`misa` D and V removed), M-mode | in |
| Bus | `components/FX1_Components/bus`, `BusConfig::fx1()` | in |
| DDR | DDR3 controller VP (`FX1_Components/ddr3`: NATIVE_R2, DDR3-1600, 2 ranks of 4 Gb x8, 4 GiB physical, ECC) behind `GenericAxiBridge`; CPU aperture 512 MiB; CSR at `DDR3_CSR`, IRQ = PLIC source 8; preinitialized (default) or firmware-init boot (`src/ddr3_subsystem.*`) | in (DDR3 subtask 5) |
| DDR_SYSCTRL | VP SoC glue of the DDR3 sideband (`src/ddr_sysctrl.*`, `fx1/fx1_ddr_sysctrl.h`): readiness mirror, P-channel requests with automatic wake, QoS sideband, cold/core reset | in (DDR3 subtask 5) |
| BootROM | 2 MiB read-only sparse RAM: code of firmware-init images (`--ddr-boot firmware`) | in |
| SRAM | 1 MiB boot SRAM (sparse RAM): data and stacks of firmware-init images | in (DDR3 subtask 5) |
| CLINT | N-hart SiFive layout (`FX1_Components/clint`), `mtime` 20 MHz | in |
| UART | `components/uart2_tlm` (PL011-style), TX to stdout / log | in |
| sim-control | VP-only PASS/FAIL finisher plus 4 test interrupt lines (`FX1_Components/sim_control`) | in |
| PLIC | `FX1_Components/plic`: 8 sources, one M-mode context per hart, MEIP per hart | in (G2; source 8 DDR3) |
| CPU port | `FX1_Components/cpu_port`: CPU payload contract check, completion, and bus-error → access-fault mapping; integration errors are kept loud | in (G2) |
| SYS_DMA | `FX1_Components/dma` (8 channels): CSR at `SYS_DMA_CSR`, master port `SYS_DMA`, IRQ = PLIC source 2, 20 ns power-on reset; request inputs tied to 0 (M2M only on the platform) | in (G3) |
| ISP | `FX1_Components/isp` TLM model: CSR at `ISP_CSR` (PERIBUS_1), masters `ISP_IDMA`/`ISP_ODMA`, IRQ = PLIC source 3, 20 ns power-on reset; driven by the reference C99 driver from firmware | in (G4a) |
| I2C0 | `components/i2c_tlm` in functional host mode (FDATA commands executed on the bus at 400 kHz SCL, ACK/NACK, repeated START): registers at `I2C0` (PERIBUS_0) behind a word-access guard, IRQ = PLIC source 9; always present, no device on the bus without `--camera` | in (CSI-2 M5a) |
| Camera (`--camera`) | `FX1_Components/mipi-csi2`: CSI-2 RX controller with the detailed D-PHY RX model, CSR at `MIPI_CSR` (PERIBUS_1), DMA master `MIPI_DMA` behind a `WriteGuard`, IRQ = PLIC source 7, 20 ns power-on reset; the board camera is a MIPI CCS register model at I2C0 address 0x10 driving the D-PHY transmit BFM (built-in pattern or `--camera-raw` frames). Needs a build with `-DFX1_BUILD_MIPI_CSI2=ON` | in (CSI-2 M5a) |
| Exclusive monitor | `FX1_Components/exclusive_monitor`: real LR/SC reservations (64 B granule) shared by both harts, SYS_DMA and ISP ODMA; AMO/SC brackets exclude every writer. CPU side through `riscv_vp_plusplus_options::exclusive_monitor`, device side through a `WriteGuard` in front of the `SYS_DMA` and `ISP_ODMA` ports | in (G4b) |

## Memory and interrupt map

Single source: `components/FX1_Components/fx1_map/include/fx1/fx1_memory_map.h`.
It is included by the platform, the bus defaults, the firmware and the firmware
linker script. **All values are VP placeholders pending the FX1 HAS.** The map
differs from `platforms/VP_FX1_Full_SoC`, where 0x1001_0000 is I2C0 and
0x1002_0000 is SPI0.

| Region | Base | Size | Path |
|---|---|---|---|
| BootROM | 0x0000_0000 | 2 MiB | SYSBUS_1 |
| SRAM | 0x0100_0000 | 1 MiB | SYSBUS_1 |
| CLINT | 0x0200_0000 | 64 KiB | PERIBUS_0 (APB) |
| PLIC | 0x0C00_0000 | 16 MiB | PERIBUS_0 (APB) |
| UART | 0x1000_0000 | 64 KiB | PERIBUS_0 (APB) |
| SYS_DMA_CSR | 0x1001_0000 | 64 KiB | PERIBUS_0 (APB); DMA registers occupy 0x0000–0x10FF |
| SIM_CTRL (VP only) | 0x1002_0000 | 64 KiB | PERIBUS_0 (APB) |
| I2C0 | 0x1003_0000 | 64 KiB | PERIBUS_0 (APB); 32-bit accesses only |
| ISP_CSR | 0x1100_0000 | 64 KiB | PERIBUS_1 (APB) |
| MIPI_CSR | 0x1104_0000 | 64 KiB | PERIBUS_1 (APB); with `--camera` only, DECERR otherwise |
| DDR3_CSR | 0x1200_0000 | 64 KiB | SYSBUS_0 (AXI4-Lite) |
| DDR_SYSCTRL (VP only) | 0x1201_0000 | 64 KiB | SYSBUS_0 |
| DDR | 0x8000_0000 | 512 MiB | SYSBUS_1 → DDR3 controller port 0 |

PLIC source IDs (`FX1_PLIC_NUM_SOURCES` = 10):
- UART = 1, SYS_DMA = 2, ISP = 3, CSI-2 RX = 7 (`--camera`; low otherwise),
  DDR3 = 8, I2C0 = 9.
- IDs 4–6 are reserved for NPU/H.264/ETH. Until those IPs exist, they are
  driven by the three **VP-only test lines** of sim-control (`FX1_SIM_CTRL_IRQ_OFF`,
  bit i → source 4 + i).

PLIC context n is hart n in M-mode. CLINT and PLIC use the SiFive register layout.

## Build

Run from the CDC-VP root. Use a host compiler, not an EDA wrapper:

One command (configure, build, smoke, bus tests, regression; JUnit XML and
logs in `<build-dir>/fx1-ci-artifacts`, exit 0 only when every stage passed).
It sets every test option the gate needs (a reused build directory whose
cache turned one off is corrected) and refuses to run when a test listed in
`ci/required_tests.txt` for a stage that runs is not registered, or when that
manifest is missing, unreadable or empty; add new required tests there. The
full run needs `python3` with numpy (the regression stage's fixture check);
`--smoke-only` does not:

```bash
platforms/fx1_soc/ci/run_ci.sh [--build-dir DIR] [--jobs N] [--smoke-only]
```

By hand:

```bash
export CC=/usr/bin/gcc CXX=/usr/bin/g++ PATH=/usr/bin:/bin:$PATH
cmake -S . -B build-fx1 -G Ninja -DCMAKE_BUILD_TYPE=Release -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
      -DCDC_BUILD_FX1_SOC=ON -DFX1_SOC_BUILD_FIRMWARE=ON -DCDC_BUILD_TESTS=ON
cmake --build build-fx1 -j1 --target fx1_all
ctest --test-dir build-fx1 -L "fx1_unit|fx1_smoke" --output-on-failure --no-tests=error
ctest --test-dir build-fx1 -R '^(bus_behavior|bus_config|bus_arbitration|uart2_tlm|uart_host_bridge)$' --no-tests=error
ctest --test-dir build-fx1 -L fx1_regression --output-on-failure --no-tests=error
```

Build `fx1_all`, not the default target: it collects every FX1 target
(simulator, harnesses, FX1 component and VP++ backend tests, firmware) and
nothing else. The default target also builds `components/isp_tlm`, which
does not link today (`isp_register_bank_test`: no `sc_main`), unrelated to FX1.
Release matters for speed: an unoptimised build runs the simulator 2–3×
slower (spinlock test: 2.0 s vs 5.1 s).

Turning the other platforms off is optional. An FX1-only configuration needs
neither the Bremen `riscv-vp` checkout nor `cpu_models/cva6_vp`.

It does need:
- the pinned VP++ checkout in `third_party/riscv-vp-plusplus`; see `cpu_models/riscv_vp_plusplus/fetch_riscv_vp_plusplus.sh`;
- with firmware on, FreeRTOS-Kernel V11.2.0 in `third_party/FreeRTOS-Kernel`, fetched at its pin by `tools/third_party/setup_third_party.sh`. `third_party/` is gitignored, so a fresh clone does not contain it. Configure stops with this hint when the kernel is missing or is another version;
- Boost headers.

### Firmware toolchain policy

`FX1_SOC_BUILD_FIRMWARE` (default ON) is the acceptance configuration.
Firmware and the `fx1_smoke` tests are mandatory, and configure **fails** when:
- no `riscv-none-elf-gcc` is found in `$RISCV_HOME/bin` or `/opt/toolchains/riscv-none-elf/bin`; or
- `FX1_RISCV_PREFIX` (e.g. `/opt/toolchains/riscv-none-elf/bin/riscv-none-elf-`)
  does not name a compiler that runs.

`-DFX1_SOC_BUILD_FIRMWARE=OFF` builds only the simulator and the unit tests. It
prints a warning and is **not** an acceptance configuration. Acceptance runs
use `ctest --no-tests=error`. The host compiler never builds firmware.

## Run

```bash
build-fx1/platforms/fx1_soc/fx1_soc --fw build-fx1/fw/fx1_soc/hello_2hart.elf \
    [--timeout-ms 1000] [--quantum-ns 1000] [--uart-log uart.txt] [--quiet-uart] [--trace] \
    [--no-exclusive-monitor] [--load ADDR:FILE ...] \
    [--uart0-socket PORT [--uart0-wait]] [--uart0-rx-file FILE [--uart0-rx-delay-us N]] [--realtime] \
    [--dump ADDR:LEN:FILE ...]   # e.g. --dump 0x88006000:3072:y.bin (DDR bytes after the run)
    [--ddr-boot preinit|firmware] [--ddr3-ecc on|off] [--ddr3-fault T_US:KIND[:ARG] ...] \
    [--uart0-rx-byte-us N] [--camera [--camera-raw FILE]]
```

**DDR3 boot modes.** With `--ddr-boot preinit` (default) the platform
initializes the DDR3 controller, runs the ECC startup scrub (about 0.67 s of
simulated time), preloads the ELF's DDR segments and every `--load` file, and
only then releases the harts. `--timeout-ms`, `--uart0-rx-delay-us` and
`--ddr3-fault` count from that release, so a DDR image behaves as before. With
`--ddr-boot firmware` the harts start at time 0 from BootROM and firmware
initializes the DDR3 (an image with DDR segments is refused). The first output
line names the boot mode and DDR3 configuration; the last lines include DDR3
counters. Details: [DDR3 integration guide §9](../../components/FX1_Components/ddr3/docs/integration_guide.md).

`--ddr3-fault T_US:KIND[:ARG]` injects a DDR3 fault from the host (stored-bit
flips, read-return and write-result faults, fatal events, refresh blocking, PHY
failure; `fx1_soc --help` lists them). `--uart0-rx-byte-us N` paces injected
console bytes (default 10 µs, about 1 Mbaud; 87 is the 115200 8N1 character
time).

UART input from the host (`components/uart_host_tlm`): `--uart0-socket PORT`
serves the console on 127.0.0.1:PORT only (connect with `tools/fx1_console.sh PORT` on the same host or in the same container;
`--uart0-wait` holds the simulation until the client connects), and
`--uart0-rx-file FILE` injects a file into RX, starting `--uart0-rx-delay-us`
of simulated time after the CPU release (deterministic, for CI). For an interactive
console use `--timeout-ms 0` (no simulated-time limit) and `--realtime`
(simulated time never runs ahead of wall-clock time; an idle system otherwise
races about 7× ahead and a finite budget ends the session within minutes), and
connect with `tools/fx1_console.sh PORT` (raw terminal, no doubled echo). The FreeRTOS CLI
firmware uses both: [`../../fw/fx1_soc/freertos/README.md`](../../fw/fx1_soc/freertos/README.md).

`--camera` adds the CSI-2 RX controller, its D-PHY and the camera sensor
(table above); without it the platform is unchanged except for I2C0, which is
always present. Firmware controls the sensor over I2C0 (CCS registers: probe
`model_id`, set `csi_data_format`, `csi_lane_mode`, output size and line /
frame length, then `mode_select` 1 to stream, 0 to stop at the end of the
frame in progress) and receives frames through the CSI-2 RX DMA. Frames come
from a built-in pattern, or from `--camera-raw FILE` (16-bit little-endian
samples, W x H per frame at the mode firmware sets, repeated). The register
set and pattern formula are in
[`ccs_sensor.h`](../../components/FX1_Components/mipi-csi2/sensor/include/fx1_csi2_sensor/ccs_sensor.h).
The camera needs a build configured with `-DFX1_BUILD_MIPI_CSI2=ON` (`run_ci.sh`
sets it); `fx1_soc --version` says whether the binary has it, and `--camera`
on a binary without it is a usage error. The D-PHY model is slow: a 640x480
frame costs minutes of host time; keep firmware frames small.

`--load` copies a host file into DDR before the harts run, after the DDR3
preinitialization (the ISP RAW-frame demo,
[`demo/README.md`](demo/README.md), uses it to process a real camera frame on
the whole platform).

Every run prints one line of exclusive-monitor counters (`[fx1_soc] atomics:`).
`--no-exclusive-monitor` falls back to upstream VP++'s bus-lock model, where no
device takes part in atomics; it exists as a negative control, not as a mode
to develop against.

Exit status:

| Code | Meaning |
|---|---|
| 0 | PASS (firmware wrote PASS to sim-control) |
| 1 | FAIL (firmware reported a code) |
| 2 | Timeout: no verdict within the simulated-time budget |
| 3 | Simulator or integration error |
| 4 | Usage error |

## Firmware contract (`fw/fx1_soc`)

- **ABI:** `-march=rv32imafc_zicsr_zifencei -mabi=ilp32f`. The xPack GCC 15
  toolchain has no rv32imafc/ilp32 multilib.
- **Boot (default, `--ddr-boot preinit`):**
  - One ELF is linked at 0x8000_0000. The platform writes its segments into
    DDR through the controller's debug port after the DDR3 initialization and
    scrub; every hart resets at its entry.
  - `crt0.S` gives each hart a 64 KiB stack from `FX1_FW_STACK_BASE`.
  - Hart 0 clears `.bss` and calls `main()`; its return value becomes the verdict.
  - Other harts wait in WFI until hart 0 calls `fx1_release_secondaries()`,
    then enter `secondary_main(hartid)`.
- **Boot from BootROM (`--ddr-boot firmware`):** images built with
  `fx1_rom_firmware()` (`common/crt0_rom.S`, `common/fx1_rom.ld.S`) run from
  BootROM with data and stacks in SRAM and bring the DDR3 up themselves
  (`fw/fx1_soc/ddr3`). `fx1_boot_image()` builds the BootROM loader
  (`fw/fx1_soc/boot`) with a DDR image embedded; it starts that image on both
  harts after readiness.
- **Ordering (RVWMO):** `volatile` is not a synchronisation primitive. Use the
  HAL barriers:
  - `fx1_mb` = `fence iorw,iorw`;
  - `fx1_wmb_io` = `fence w,o` (memory writes before a device write);
  - `fx1_release` = `fence rw,w`;
  - `fx1_acquire` = `fence r,rw`.

  The release sequence is: shared data → `fence rw,w` → flag →
  `fence iorw,iorw` → MSIP write. The released hart issues `fence iorw,iorw`
  after it observes the flag. `fx1_secondary_release` is forced into `.data`.
- **Console:**
  - `fx1_log*` print one whole line under a console lock with this hart's
    interrupts masked (saved and restored), so lines never interleave and
    interrupt handlers may log.
  - A hart that faults while printing gets nested, unlocked access.
  - `fx1_putc` and `fx1_puts` are raw and unserialised.
- **Fatal path:** `fx1_fail()`, failing `fx1_check()` and the default trap
  handler mask interrupts and print best effort, waiting a bounded time for the
  console. They always write the FAIL verdict. The default trap code is
  0xE000 | mcause.
- **SYS_DMA:** include `dma/fx1_dma_regs.h`. See
  `components/FX1_Components/dma/docs/PROGRAMMING_GUIDE.md`.
- **ISP:** compile `components/FX1_Components/isp/driver/src/fx1_isp_drv.c` into
  the image with `isp/driver/include` and `isp/include` on the include path.
  - Provide an `fx1_isp_hal` with `read32`/`write32` at `FX1_ISP_CSR_BASE + offset`
    and a `delay_cycles` for the 2 ns ISP core clock.
  - Serialise driver calls against the ISR with `fx1_irq_save()`.
  - See `tests/isp_stream/` (`isp_common.c` is the HAL) and `isp/docs/ISP_PROGRAMMING_GUIDE.md`.
  - `fx1_isp_events.frames_done` is the number of DONE outputs still waiting in
    the driver queue when the ISR ran, not the number of new completions. Do not
    sum it across interrupts. Mark the sequences of the oldest `frames_done`
    queue entries instead (`dev.queue[k].seq`), as `isp_stream/main.c` does.
  - The statistics FRAME_ID is the hardware's per-accepted-SOF count (1, 2, ...
    after reset), published at EOF. It is not the driver's `seq` (0-based).
    Read it through the driver's `fx1_isp_read_*` accessors, which re-read the
    tag around the payload.
  - `IDMA_UNDERRUN` (a level while no next input is valid) and `ODMA_OVERFLOW`
    are lossless stalls, not errors.
- **Atomics (plan C7):**
  - LR/SC is a real reservation over the naturally aligned
    `FX1_RESERVATION_GRANULE` (64 B, VP placeholder pending HAS) containing the
    LR address. A write by any other master into that granule cancels it: the
    other hart's store, SC or AMO, a SYS_DMA write, an ISP ODMA write. The
    hart's own stores do not. An SC succeeds only with a surviving reservation
    for the same granule and consumes it either way; a reset drops it.
  - An LR does not stall the other hart. An SC or AMO excludes the other hart
    for its duration (VP++ bus lock) and every device write to its granule
    (the write waits, and the SC/AMO waits for an overlapping write already in
    flight).
  - Keep lock words and other LR/SC targets out of any granule a device writes
    (DMA buffers), or SCs fail spuriously — correct, but no forward progress
    while the device streams. Write retry loops; do not count on the granule
    size.
  - Not covered: debug (backdoor) writes, i.e. ELF loading and `--dump`.
- **Buffer ownership (plan C12):** CPU harts, SYS_DMA and the ISP masters see
  one DDR. The VP models no cache. Every buffer a device reads or writes
  changes owner twice, each time through a HAL call with the direction of the
  device's access (`FX1_DMA_TO_DEVICE`, `FX1_DMA_FROM_DEVICE`,
  `FX1_DMA_BIDIRECTIONAL`):
  - `fx1_dma_prepare(addr, bytes, dir)`: CPU → device, after the CPU's last
    access and before the device is started on or handed the buffer. This
    includes buffers the device only **writes** (DMA destination, ISP Y/UV:
    all four before `fx1_isp_start`, and each again once consumed, since the
    driver may re-queue it).
  - `fx1_dma_complete(addr, bytes, dir)`: device → CPU, after the completion
    and before the CPU's first access.
  In between the device owns the buffer: the CPU neither reads nor writes it.
  On the VP both calls are a full fence. On silicon the work depends on the
  FX1 cache/coherency, which the HAS has not fixed; for a non-coherent
  write-back cache the usual mapping is in `fx1_fw.h` (prepare: clean for
  TO_DEVICE, clean + invalidate for FROM_DEVICE/BIDIRECTIONAL; complete:
  invalidate for FROM_DEVICE/BIDIRECTIONAL), and device-written buffers must
  then cover whole cache lines. Porting means confirming that mapping against
  the HAS, not only replacing the two function bodies. The VP cannot detect a
  missing or misdirected call. Memory that the CPU and a device access
  concurrently (the C7 atomic test words) is outside this contract and needs
  coherent or uncached memory on silicon.
  - Buffers live in the regions of `fx1_memory_map.h` (`FX1_FW_SHARED_BASE`,
    `FX1_FW_RAW_BUF_BASE`, `FX1_FW_YUV_BUF_BASE`, `FX1_FW_TEST_AREA_BASE`),
    never in the image or the stacks; the linker script asserts that the
    regions are disjoint. Device-side alignment and strides follow each IP's
    guide (ISP: 16-byte AXI beats; DMA: any byte alignment).
  - Drive each IP from one hart only. Moving an IP between harts needs a lock
    and an ownership hand-over that this VP does not provide.
- **Interrupts:**
  - `fx1_irq_save()` and `fx1_irq_restore()` mask this hart's interrupts around
    any lock that a handler on the same hart can also take.
  - PLIC helpers: `fx1_plic_*`. VP test lines: `fx1_sim_irq_set` and `fx1_sim_irq_get`.
  - Handlers clear the source at the device before `fx1_plic_complete()`,
    because sources are level-triggered.
- **Bus errors:** these become access faults: fetch → mcause 1, load → 5,
  store/AMO → 7, with `mtval` set to the address. Covered responses:
  - unmapped addresses;
  - slave errors, e.g. a write to BootROM;
  - unsupported access forms (a target's BURST/COMMAND/BYTE_ENABLE error).
  A target that never answers, or a malformed CPU payload, stops the simulator
  with an integration error (exit 3) naming the address, because it is a model
  defect, not a guest fault.
- **Traps:** `fx1_set_trap_handler()` installs a handler per hart. Handlers must
  not use floating point, because FP registers are not saved.
- **MMIO:** use 32-bit, naturally aligned accesses for every register.
  - The UART sits behind `fx1::WordAccessGuard`. `lb/lh/sb/sh` to the UART
    raise a load (5) or store (7) access fault and never reach the model.
- **Verdict:** `fx1_pass()`, `fx1_fail(code)`, or return from `main()`.

## Tests

| ctest | Checks |
|---|---|
| `fx1_sparse_ram` | Sparse paging, page crossing, byte enables, range errors, ROM write error, backdoor preload |
| `fx1_clint` | 20 MHz `mtime`, MTIP at exactly `mtimecmp × tick`, deassert on rewrite, MSIP, hart independence, `mtime` re-base, error responses |
| `fx1_sim_control` | PASS/FAIL codes, rejected writes, stop time |
| `fx1_access_guard` | Only aligned full 32-bit accesses pass; short, misaligned, partial byte-enable and null accesses are rejected without reaching the model, and a canary beyond a 1-byte payload stays intact (also with the real `uart2_tlm`) |
| `fx1_smoke_hello_2hart` | Two-hart boot, release over MSIP, separate stacks, shared data |
| `fx1_smoke_clint_irq` | On each hart, 3 timer IRQs with exactly one IRQ per deadline and none early; 3 IPIs with exactly one software IRQ each; `rdtime` agrees with MMIO `mtime` |
| `fx1_smoke_isa_misa` | `misa` = RV32IMAFC+NSU; `fld` and `vsetvli` raise illegal instruction; `fadd.s` executes |
| `fx1_smoke_irq_in_log` | A timer IRQ expiring in the middle of `fx1_log` is deferred to the end of the line, and its handler logs |
| `fx1_smoke_uart_access` | `lb`/`lh` raise mcause 5 and `sb`/`sh` raise mcause 7 at the UART address; word output still works |
| `fx1_smoke_trap_in_log` | Negative test: a load fault inside `fx1_log` ends with exit 1 and `FAIL code=0xe005`, not a timeout |
| `fx1_plic` | Priority order and lowest-ID tie-break, enable mask, claim clears pending, level re-pend after complete, latched request survives a deassert, one claim per request across two contexts, complete from a non-enabled context ignored, side-effect-free debug claim, error responses. The threshold gates notification only: a source below or equal to it is still claimable, priority 0 is neither notified nor claimable, and two contexts with different thresholds still compete for one request |
| `fx1_cpu_port` | Streaming width completed (a set width is kept). A valid access that the target refuses with BURST/COMMAND becomes GENERIC (a guest fault); ADDRESS/GENERIC pass through. Integration errors: a silent target (payload pre-set to OK, as VP++ does), INCOMPLETE, BYTE_ENABLE on a payload without byte enables, and malformed CPU payloads (command, null data, length, byte enables, streaming width), which never reach the target |
| `fx1_smoke_silent_target` | Negative test on a real VP++ hart: a target that never answers stops the run with an integration error, and the guest never reaches its PASS write |
| `fx1_smoke_plic_irq` | On hart 0: delivery with one claim and no re-entry; priority 6 → 4 → 5 (test lines 4–6); threshold gating MEIP, and a polled claim that succeeds at threshold 7; mask; level re-pend. On hart 1: woken from WFI. A shared source is claimed exactly once. The UART TX interrupt is claimed and cleared at the device |
| `fx1_dma_*` | DMA model unit tests: registers, memory, peripheral, chain, pending, arbitration, errors, reset, timing, and `idle` (an M2P channel waits 10 ms with no scheduler wakeups, then completes). `fx1_dma_bus` runs the DMA through the FX1 bus with the named-port API. `fx1_dma_regs_header` checks the C header against `registers.h` |
| `fx1_smoke_dma_m2m` | SYS_DMA on the platform: 256-byte and 1023-byte unaligned M2M copies verified byte for byte; completion through PLIC source 2 and cleared at the DMA; an unmapped destination gives WRITE_DECERR; sub-word CSR access gives load/store access faults |
| `fx1_smoke_isp_stream_basic`, `fx1_smoke_isp_stream_gtm_auto` | The ISP driven by the reference C99 driver (`isp/driver`) through an MMIO HAL. Eight 64x48 RGGB frames go through the 4-buffer rotation (each buffer used twice), with up to two frames in flight; the CPU fills every input. The ISP interrupt goes through PLIC source 3 and is acknowledged by the driver at the device. Every NV12 frame must match the CRC-32 of the pinned Python reference (`tests/isp_stream/isp_fixture.h`). At the end: IDMA/ODMA frame counters are 8 and no fatal DMA error is set. The GTM-auto run checks the temporal effect on frames 1–7. IDMA underrun is a lossless level and is only counted |
| `fx1_smoke_isp_stream_backlog` | Same stream and profile as basic, but with a lagging consumer: the oldest output is taken only once the next one is DONE too. The ISR therefore sees two outputs DONE at once and reports one output in several snapshots. Every completion must be marked exactly once, by sequence; the run also checks that a backlog of 2 occurred and that the sum of the snapshots exceeds 8, so a counter that sums snapshots would fail here |
| `fx1_smoke_isp_frame_id` | Basic + AEC (4x3 zones of 16x16) + AWB (global) + AF, with distinct context IDs. One frame at a time; after each completion the driver reads the AEC/AWB/AF publications. FRAME_ID must be 1..8, and the context IDs and the AEC global sums and counts per channel must match the reference. The NV12 still matches basic's CRC |
| `fx1_exclusive_monitor` | Monitor and `WriteGuard` unit test with hand-driven threads: SC without/with a consumed or mismatched reservation; cancellation by another master's write to the same word, the same granule or a straddling range, not by the next granule or the hart's own write; reset drops reservation and bracket; an open bracket refuses another master's overlapping write; `atomic_begin` waits for an overlapping write in flight; a guarded DMA write waits for an open bracket while a read does not; a guarded write cancels a reservation |
| `riscv_vp_plusplus_exclusive_monitor_hooks` | A real VP++ hart with a scripted monitor: exact hook sequence of `sw`, `lr.w`, a successful and a failed `sc.w`, and `amoadd.w`; LR takes no bus lock; the guest sees the monitor's SC answer and a failed SC writes nothing; `reset_cpu()` calls `reset_hart` |
| `fx1_smoke_lrsc_masters` | LR/SC against every writer. SC without a reservation, consumed and mismatched reservations. Between LR and SC (the hart waits in WFI): SYS_DMA writes the reserved word or another word of its granule → SC fails and the DMA data stays; the DMA writes the next granule → SC succeeds; hart 1 stores to the word → SC fails; to the next granule → SC succeeds; the ISP ODMA writes its output over the word → SC fails and the NV12 matches the reference. The DMA write is chained behind a padding command so it lands after the LR (checked) |
| `fx1_smoke_amo_dma_q*ns` | 128 trials of `amoor.w` on a word while SYS_DMA writes bit 31 into it, the DMA write's phase swept by the padding size: the DMA bit must survive. The test also requires `device writes held` > 0 in the platform output, i.e. DMA writes did arrive inside an AMO and waited |
| `fx1_smoke_amo_dma_no_monitor` | Negative control: the same image under `--no-exclusive-monitor` must FAIL with code 0x20 (a DMA write lost inside an AMO) |
| `fx1_regression_spinlock_2hart_q*ns` | Both harts take an LR/SC spin lock 50 000 times each around a plain `lw`/`sw` increment, plus an `amoadd.w` counter: both end at 100 000. A varying few-instruction delay between LR and SC makes both harts see the lock free at once (without it the deterministic schedule never does, and a broken monitor still counted 100 000). Requires `cancelled by hart` > 0. About 5 s host per run |
| `fx1_smoke_shared_mem_concurrent` | Plan C12: hart 0 streams the eight ISP frames (as `isp_stream_basic`) while hart 1 runs SYS_DMA copies in its own DDR region, CPU-written source to CPU-checked destination with the ownership calls; both paths are checked. Overlap is measured: hart 1's copy counter is sampled when the first frame is queued and at each of the 8 completions; hart 1 must progress in every interval, copy at least 4 times in the window and still be running when the stream ends |
| `fx1_faulting_lr` | Plan C7: a real VP++ hart and the real monitor on a RAM that faults the first LR's load: after the trap the SC fails and writes nothing; a following LR/SC pair succeeds |
| `fx1_smoke_isp_recovery` | Plan P5, real bus errors through an unmapped buffer address. A: ODMA AXI error on output 2 → firmware repairs the address, `fx1_isp_recover_ex` frees it again, the lost frame is reported by `fx1_isp_next_lost` and resubmitted; all 8 frames complete exactly once with the reference CRC, exactly one loss, a second recover with the same snapshot is a no-op. B: IDMA AXI error on input 2 → repair, recover re-arms it, the frame is retried, no loss. C: soft reset while the first input is still being read (both frame counters unchanged at the abort) → no completion, no ODMA progress and no DONE report afterwards; restart from buffer 0, 8 frames correct. D: `fx1_isp_stop` at the same point discards exactly one frame, nothing completes afterwards; start again, 8 frames correct. E: `fx1_isp_dma_reset` (DMA soft reset) right after two frames are queued back to back: both dropped, at least the second one aborted, no completion or progress after the call, the stream continues from buffer 0 without a restart, 8 frames correct. Every frame carries a software tag (`fx1_isp_queue_frame_meta`) that must come back with its output (`fx1_isp_next_done_meta`), also after A has put the input and output rotations out of step. Every phase ends with nothing in flight, no fatal DMA_ERR and the ISP line low |
| `fx1_smoke_isp_mas0710` | CSR/MAS 07/10 behaviour; every expected value comes from the Python reference (`isp_fixture.h`). A: the statistics profile, one frame at a time, frame metadata (tag, capture time, SOF/EOF) with every buffer, and an AWB ownership script: report, acquire, release and clear. While software holds both result banks, frames drop their AWB measurement (`DROP_COUNT`, `OVERRUN`); released banks are reused; status, identity and sums match the reference. B: an AWB lease held across `fx1_isp_dma_reset`, issued right after two frames are queued back to back (at least the second aborted in progress), reads the same afterwards; nothing completes after the call; the next frame is correct. C: 648x368 frames scaled to 640x360 from the first frame after start, then pass-through, then 640x360 again, the mode switched between frames while streaming; NV12 CRCs match the reference |
| `fx1_smoke_dma_recovery` | SYS_DMA: WRITE_DECERR (unmapped destination) and READ_DECERR (unmapped source) with no completion and the channel inactive, then a re-armed copy on the same channel succeeds; a chain whose second command fails completes the first (data checked) and reports the error; an error on channel 0 leaves a concurrent channel-1 copy intact; a 16-command chain paused with `CH_ENABLE = 0` after 2 commands stops progressing and resumes to exactly 16 completed commands with correct data; the IRQ line is low at the end |
| `fx1_smoke_uart_rx_q*ns` | UART RX from the host (`--uart0-rx-file`): one line received by polling `FR.RXFE`, one by the RX/receive-timeout interrupt through PLIC source 1 (cleared at the UART before the PLIC complete) |
| `fx1_smoke_freertos_cli_q*ns` | FreeRTOS V11.2.0 + CLI ([`fw/fx1_soc/freertos`](../../fw/fx1_soc/freertos/README.md)) with a scripted console session: hart 1's boot IP check with 0 failures, then system, memory/register, `fpu test` (FP context switch), `dma test`, `isp selftest`, `selftest` and `poweroff` with PASS |
| `fx1_smoke_freertos_cli_bad_input` | Bad `md`/`mw` input must be reported while the CLI keeps running: numbers above 32 bits, ranges that wrap past 4 GiB or a region end, and CLINT/PLIC register holes that fault on the bus (`bus error`, no trap) |
| `fx1_smoke_freertos_cli_hwcheck_fail` | The boot gate: an image with an injected IP-check failure must halt before the scheduler with code 0x50 |
| `uart2_tlm`, `uart_host_bridge` | The UART model and the host bridge FX1 uses (CI components stage). `uart2_tlm` includes the PL011 control-register readback the hart-1 IP check found missing (`CR`, `LCR_H`, `IBRD`, `FBRD` used to read 0), with `CR` reserved bits [6:3] reading 0 |
| `fx1_smoke_example_quickstart` | The handoff example (`fw/fx1_soc/examples/quickstart`): two-hart hello, a CLINT timer interrupt, a SYS_DMA copy with its PLIC interrupt and the ownership calls |
| `fx1_regression_isp_fixture_check` | `gen_fixture.py --check`: the committed fixture still matches the reference. Registered only when `python3` on PATH has numpy; the smoke tests never need Python |
| `fx1_regression_isp_demo_pack` | `demo/tests/test_pack.py`: `run_isp_demo.py --pack-only` on synthetic 4×4 dumps. A valid job gives a 24-byte NV12 and a PNG. A short or long dump, a bad result block, a bad stride or geometry, an incomplete frame, a wrong or missing `job.bin`, another profile, or a failing preview must exit 2 and leave no output. `python3` only (stdlib) |
| `fx1_smoke_camera_absent` | Without `--camera`: a MIPI_CSR load faults, I2C0 answers and a transfer to 0x10 is NACKed (source 9), source 7 never pends |
| `fx1_smoke_camera_wiring` | With `--camera`: CSI-2 RX probed at `MIPI_CSR`, the sensor's model_id read over I2C0 (source 9), one TPG frame through `MIPI_DMA` to DDR (source 7) equal to the colour bars |
| `fx1_smoke_camera_raw_invalid` | `--camera-raw` of an odd-length file: exit 3 with the RAW-file message before the run |
| `fx1_smoke_camera_i2c_irq` | The BSP I2C driver (`fw/fx1_soc/drivers/fx1_i2c`) in interrupt mode: sensor probe and mode with every completion claimed at PLIC source 9, NACK reported as `FX1_I2C_ENACK` |
| `fx1_smoke_camera_i2c_timeout` | I2C driver budgets, polling and interrupt mode: a write (20 µs budget) and a read (300 µs) that need longer return `FX1_I2C_ETIMEOUT` within budget + one byte and STOP; the abort leaves the bus idle, the next transfer and a register write/read-back work at once |
| `fx1_smoke_camera_raw10`, `_raw12` | CSI-2 M5b, firmware on the BSP drivers and the reference CSI-2 driver: sensor programmed over I2C0 (64x16; RAW10 4 lanes / RAW12 2 lanes), three frames through the detailed D-PHY, each interrupt claimed at PLIC source 7, every pixel checked against the sensor pattern (packed RAW10 / RAW16 layouts), stream off at a frame boundary; FRAME_COUNT = sensor frame_count, no drop, no CRC/ECC error |
| `fx1_smoke_camera_recover` | No free buffer → frame 3 dropped (FRAME_DROPPED, count 1), release → frame 4 in that side; sensor software_reset inside frame 6 → that frame delivered whole, frame numbers restart at 1; DMA stopped while streaming → no capture, restart → exact frames, FRAME_COUNT consistent |
| `fx1_smoke_camera_decerr` | A buffer side at an unmapped address: frame 2 BUF_DONE_ERR + AXI_WRITE_ERROR, DECERR at the side's base, no valid line; DMA stopped, reprogrammed, capture resumes exact |
| `fx1_smoke_camera_slverr` | `--ddr3-ecc off --ddr3-fault 6200:write-result:1` inside frame 2 (CPU only reading meanwhile): BUF_DONE_ERR + AXI_WRITE_ERROR, SLVERR at an address of the failing side, last valid line inside the frame; frame 1 exact; next frame exact |
| `fx1_smoke_camera_isp_raw12`, `_raw10` | CSI-2 M6a, the camera manager (`fw/fx1_soc/drivers/fx1_camera`): sensor 64x32 → detailed D-PHY → CSI-2 DMA (RAW16) → ISP (profile basic) → NV12. RAW12 4 lanes zero-copy (the manager writes the CSI-2 side into the ISP input slot's `IDMA_BUF_ADDR`, the side returns when that slot's `IDMA_BUF_VALID` clears); RAW10 2 lanes staged as sample << 2. Eight consecutive frames, each NV12 = the stand-alone ISP model's for that frame number (`tests/camera_isp/camera_isp_golden.h`), output geometry from the ISP driver; no drop or error; both CSI-2 sides back after stop |
| `fx1_smoke_camera_isp_csi_err` | A CSI-2 side at an unmapped address: its frames end with an error and are released without reaching the ISP; the other side's frames (1, 3, 5, 7) come out exact |
| `fx1_smoke_camera_isp_backpressure` | The application holds all four NV12 outputs: the manager stops feeding the ISP, the CSI-2 RX drops frames (counted); after the release the two held frames come out intact and the stream resumes right after the dropped frame numbers |
| `fx1_smoke_camera_isp_odma_err` | NV12 output slot 2 unmapped: ODMA error, the application repairs the slot from the manager's error hook, `fx1_isp_recover_ex`, one frame lost (counted), the following frames exact and in order |
| `fx1_smoke_camera_isp_idma_hold` | The ISP's input DMA paused by the test (`DMA_CTRL.IDMA_EN` = 0): zero-copy sides handed over and unread stay with the ISP (both held, the receiver drops, nothing overwritten); IDMA resumed, the held frames come out intact, then the stream after the drops |
| `fx1_smoke_camera_isp_idma_err` | M6a review F1: an IDMA read error on frame 2's zero-copy input, 7 ms with only the ISRs running while the sensor streams, the address repaired from the error hook, `fx1_isp_recover_ex` re-reads the input: the side stayed held, the retried output is capture 2 with the pixels of sensor frame 2, the stream resumes after the drops, nothing lost |
| `fx1_smoke_camera_isp_sensor_stop_timeout`, `_nack` | M6a review F2: the standby does not reach the sensor (I2C host disabled / wrong address): stop returns `FX1_CAMERA_ESENSOR` within its deadline (and, with the 24 ms budget, at the sensor's own 2 ms I2C timeout, not later), state 3, CSI-2 RX and ISP stopped, configure refused, the sensor still streaming; I2C back: stop again → OK, mode_select 0, the sensor's frame count stops; configure and start again, exact consecutive frames |
| `fx1_smoke_camera_isp_sensor_stop_budget` | M6a re-review R1: stop budgets shorter than the sensor's 2000 µs I2C timeout. I2C host disabled: stop(100 µs) from running and again in state 3 → `FX1_CAMERA_ESENSOR` each within 100 µs + `FX1_CAMERA_STOP_SLACK_US`; I2C back: stop(1000 µs) → OK within its bound; read-back cut (budget = a timed standby write + 30 µs) → ESENSOR within its bound although the sensor is in standby, stop again → OK; restarts exact |
| `fx1_smoke_freertos_cli_camera`, `_camera_fault`, `_camera_absent`; `fx1_regression_freertos_cli_camera_tpg` | CSI-2 M6b, the FreeRTOS CLI's `camera` commands (`fw/fx1_soc/freertos/README.md`, section Camera): with `--camera` a nominal session (hwcheck of CSI-2 RX / I2C0 / sensor, probe, 64x32 RAW12 and RAW10 captures against the golden, ISP shared with `isp selftest`), a fault session (I2C host disabled → stop ESENSOR, start refused, stop OK, recover, frames from 1 again); without `--camera` everything reports the camera absent, nothing fails; TPG 640x480 against its golden and 3840x2160 (metadata, RAW and Y/UV spot checks) in the regression stage; regression negatives: RAW file instead of the pattern (`_mismatch`), a start failing after configure (`_startfault`, test image), ODMA Y / UV of a slot redirected (`_nv12_fault`) |
| `fx1_i2c_bsp_budget` | Host test (M6a follow-up review R2): the BSP I2C driver built for the host against the real I2C0 model (functional mode, 400 kHz, 1 µs per register access). Each call starts while an earlier transaction is still aborting, swept over abort phase and budget, write and write_read: no START queued after the deadline, return within budget + `FX1_I2C_ABORT_US` (nothing sent: within the budget), OK only with the transaction done at the device |
| `fx1_smoke_camera_isp_start_fail` | M6b review F1: the I2C host lost between configure and start: `fx1_camera_start` → EDEV, still configured (state 1); I2C back: `fx1_camera_stop` from state 1 → OK, idle, sides back, the sensor in standby and not streaming; configure and start again, exact frames |
| `fx1_smoke_camera_isp_stop_held` | Stop while the application holds all four outputs and two captures wait: stop completes (~0.3 ms), the captures are cancelled and counted, the held NV12 stay readable and exact, releasing them afterwards is OK (twice is refused); configure and start again, exact frames |
| `fx1_smoke_camera_isp_timeout` | Sensor stream stopped: `fx1_camera_dequeue` returns `FX1_CAMERA_ETIMEOUT` within its budget (+200 µs), counted; stop still works |
| `fx1_regression_camera_isp_artifacts_raw12`, `_raw10` | `check_artifacts.py`: the firmware keeps the CSI-2 RAW (capture hook), the ISP input (RAW10 staging), the NV12 and a record per frame in DDR; the host checks RAW = sensor pattern, staging = RAW << 2 (0x3FF → 0xFFC), NV12 CRC = golden and SHA-256 = `fx1_isp_run_raw` on the dumped input, and the record links (capture, FRAME_ID, side, ISP sequence and slots). python3 + numpy |
| `fx1_regression_camera_isp_tpg4k` | `check_tpg4k.py`: two 3840x2160 RAW12 frames from the CSI-2 RX TPG (no sensor, no D-PHY) through the camera manager, zero-copy, RAW spot-checked by the firmware; the dumped NV12 of both = the ISP model's (CRC 0xDD6DA3BB). About 35 s host |
| `fx1_regression_camera_isp_golden_check` | `gen_golden.py --check`: the committed camera → ISP golden header matches `fx1_isp_run_raw` and the profile generator |
| `fx1_csi2_*` (label `fx1_csi2_unit`) | The CSI-2 RX component tests without PHY traffic (packet, lane management, sys_clk datapath, DMA, DMA control, controller CSR, driver), built by `run_ci.sh` (target `fx1_csi2_unit_tests`); the PHY-level tests (`fx1_phy_slow`) run in the component's own regression |
| `fx1_smoke_bus_fault` | Fetch, load and store faults with exact `mcause`/`mepc`/`mtval`; a store to BootROM faults and leaves the ROM unchanged |
| `fx1_packaging` | The SDK distribution gate (`package/run_packaging_regression.sh`, label `fx1_packaging`, not in `run_ci.sh`). A fresh Release build is packaged and moved; the build tree is made unreadable; it runs with an empty environment (smoke, every ISP sample against its golden, a wrong golden must fail). The firmware is rebuilt from the shipped sources and must be byte-identical. Also checks `sha256sum -c` on a clone without `build/` and the one-terminal interactive flow |
| `*_q10us` | Every smoke image is run again with a 10 µs quantum and must give the same verdict (plan C11) |
| `fx1_regression_perf_loop_q*ns` | Two-hart integer and memory loop. Checksums must equal independently computed references (0x1762a79a, 0x124d31b2). Prints host MIPS: about 0.8 MIPS with both harts in a Release build with the DDR3 controller (3.5 with the former RAM DDR), either quantum, depending on host load |
| `fx1_ddr3_subsystem_*` | DDR_SYSCTRL registers and access rules, P-channel requests with automatic wake (and its back-off when denied), cold/core reset sequences (held traffic aborted, launched traffic completes), preinit order and ECC-consistent preload, firmware boot refusing DDR images, `--ddr3-fault` syntax, firmware reference image = model reference, canonical DDR names for the exclusive monitor |
| `fx1_smoke_ddr3_fw_init_q*ns` | BootROM image, `--ddr-boot firmware`: identity, a rejected TIMING_COMMIT (SLVERR, blocker 0x04, IRQ_RAW[4]), initialization and readiness, memory, region decode miss → load access fault + PLIC source 8, self-refresh with automatic wake, two harts with a DDR AMO counter |
| `fx1_smoke_ddr3_fault_recovery` | BootROM image with a host fault schedule: CE (corrected, counted, IRQ), UE (access fault), read timeout → quarantine → cold reset + re-initialization, fatal hold → core reset by hart 1 → cold reset |
| `fx1_smoke_ddr3_preinit_q*ns`, `fx1_smoke_ddr3_release_order` | Preinitialized boot: the active images equal the firmware reference, two-hart traffic, CE/UE with PLIC interrupts, self-refresh while executing from DDR; a BootROM entry starts only after scrub and preload |
| `fx1_smoke_ddr3_boot_hello`, `fx1_smoke_ddr3_boot_freertos_cli` | BootROM loader: DDR3 bring-up, application copied to DDR, both harts started (hello; FreeRTOS CLI session) |
| `fx1_smoke_ddr3_boot_ready_timeout`, `_init_failure`, `_scrub_fatal`, `_ddr_image_refused` | Bring-up failures reported from BootROM/SRAM (codes 0xD304/0xD305/0xD306); a DDR image refused in firmware boot |
| `fx1_smoke_ddr3_power_recovery`, `fx1_smoke_ddr3_wake_watchdog` | A failed self-refresh exit: the denied automatic wake is retried with back-off, simulated time advances (the watchdog ends the run at `--timeout-ms`), and hart 1 recovers with a cold reset |
| `fx1_smoke_ddr3_atomic_lifecycle`, `fx1_smoke_ddr3_atomic_alias_q*ns` | CPU reservations follow DDR storage: voided by a cold/core reset, a reinitialization or a region commit, and shared by region aliases for hart stores, DMA writes and AMO brackets |

## SDK package (VP_FX1_V3.0)

`cmake --build <dir> --target fx1_soc_package` assembles the delivery for the
firmware/software teams under `out/VP_FX1_V3.0`, in the layout of the SDK
repository `fx1`. It contains:

- the portable VP (`RPATH=$ORIGIN` + `libsystemc`);
- the FreeRTOS CLI sources with a stand-alone, location-independent Makefile;
- `run_vp.sh` (smoke, run, interactive, isp);
- the ISP samples: the cleared D65 frame and, with `FX1_DEMO_RAW`, the
  internal outdoor1 frame;
- docs, licences, `BUILD_MANIFEST.json` and `SHA256SUMS`.

The release procedure, the gate and the export are described in
[`package/README.md`](package/README.md). The delivery's own README is
[`package/docs/README.md`](package/docs/README.md).

## Time model (plan C11)

The SystemC time resolution is 1 fs on every run (set first in `main`): the
detailed D-PHY model of the camera needs it, and setting it always keeps runs
with and without `--camera` timed alike. No code may build an `sc_time` before
`sc_main` (no namespace-scope `sc_time` constants).

The FX1 bus consumes annotated delay with `wait()` at every hop and returns a
delay of 0. VP++ hands its quantum-keeper local time to `b_transport` and takes
back what the bus returns. So **every CPU access, instruction fetches included,
synchronises the hart with SystemC time**:
- device side effects, interrupts and `mtime` are observed in program order;
- an interrupt raised by a device write is visible before the next instruction;
- the TLM quantum has no functional effect.

The `*_q10us` runs confirm this: the same verdicts, the same checksums, and
simulated time within a few ns. The cost is speed: the perf loop runs at about
3.5 MIPS on two harts in a Release build (about 1.5 MIPS unoptimised). Temporal decoupling (accumulating delay instead of
`wait`, with syncs at MMIO) and DMI for DDR are possible later optimisations.

## Limitations

- **Streaming width:** VP++ never sets the payload streaming width.
  `fx1::CpuPortAdapter` completes it for every CPU access. The shared wrapper is
  left unchanged for TPU_V3.
- **Performance:** about 0.8 MIPS with both harts in a Release build (3.5
  with the former RAM DDR), because every access syncs (see Time model) and
  goes through the DDR3 controller model. A DDR fetch or load costs about 36 ns
  of simulated time, much more behind a DMA burst: the bus serializes DDR
  traffic and the VP has no cache, so CPU-versus-DMA timing is pessimistic
  compared with silicon (DDR3 limitation L-19). `mcycle` uses the VP++ 10 ns cycle on both harts and does
  not reflect the silicon frequencies (600/300 MHz in the HAS).
- **Atomics:** the exclusive monitor covers both harts, SYS_DMA and ISP ODMA
  (ISP IDMA only reads). A future writing master needs its own `WriteGuard`.
  The granule (64 B) is a placeholder. An SC or AMO stalls the other hart
  entirely for its duration (VP++ bus lock), which is stronger than silicon.
  No cache, so no cache-line reservation semantics beyond the granule.
- **Shared memory:** no cache is modelled; a driver that omits
  `fx1_dma_prepare`/`fx1_dma_complete` still passes here. Passing on the
  VP says nothing about cache maintenance on silicon.
- **ISP:** this stage covers only the basic flow: one geometry (64x48), two profiles,
  statistics checked only for the FRAME_ID/context tags and the AEC global sums
  (no zone, histogram or AWB/AF value checks), and no error injection or
  recovery on the platform. The
  model's own suites cover those, and the platform fault/recovery cases are
  stage G5.
- **PLIC:** level-triggered gateways only. A request latched before the line
  drops stays pending until claimed (RISC-V PLIC rule); the driver sees a
  spurious interrupt.
- **Scope:**
  - Not modelled: caches, PMP enforcement, S-mode/Sv32, and B and Zicbom.
- **Reset and abort:** the map has no reset controller (CRG) yet, so the
  platform tests recover with the drivers' soft reset / stop / recover only.
  The DDR3 controller's resets are reachable through DDR_SYSCTRL RST_CTRL.
  SYS_DMA has no channel abort: `CH_ENABLE = 0` pauses a command (accepted
  transactions drain, re-enabling continues); only the IP reset, at power-on
  here, cancels one. ISP AXI errors are produced with unmapped buffer
  addresses and made transient by the firmware repairing the address. DDR
  errors (ECC, read-return, fatal) are injected with `--ddr3-fault`.
