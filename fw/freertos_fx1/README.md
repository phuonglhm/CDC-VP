# freertos_fx1 — FreeRTOS bring-up firmware for VP_FX1_Full_SoC

First RTOS milestone for CDC-VP (docs/NPU_V4_INTEGRATION_HANDOFF.md §13,
platform doc §9 acceptance checklist). Boots FreeRTOS V11.2.0 on the Bremen
rv32 backend and proves, in one deterministic run:

| Marker (UART0)          | Proves |
|---|---|
| `FreeRTOS FX1 boot`     | reset → main, UART console |
| `FreeRTOS NPU PASS`     | NPU GEMM submitted from a task, completion by PLIC IRQ17 waking the blocked task, all 1024 INT32 results verified (NPU build only) |
| `PLIC TIMER0 OK irqs=3` | PLIC claim/dispatch/complete with TIMER0 periodic IRQs into a task notification |
| `FX1 tick=N` ×3, `CLINT tick OK` | CLINT MTIP scheduler tick; `vTaskDelay(100 ms)` advances ≥100 ticks |
| `FreeRTOS FX1 PASS`     | all of the above in one run with preemptive multitasking |
| `root@fx1:~#`           | UART0 RX/RT interrupt → PLIC source 1 → FreeRTOS queue → CLI task |

## Layout

```
Makefile            single-command cross build (riscv-none-elf)
linker.ld           FW/RTOS window: RAM0 0x8000_0000 + 16 MiB
FreeRTOSConfig.h    tick 1 kHz; CLINT µs units → configCPU_CLOCK_HZ = 1 MHz
startup/start.S     sp, mtvec = freertos_risc_v_trap_handler, .bss clear
bsp/uart.c          PL011 TX mutex + interrupt-driven RX queue (PLIC source 1)
bsp/plic.c          single MEIP dispatcher (claim → handler → complete);
                    overrides freertos_risc_v_application_interrupt_handler
bsp/npu_v4.c        NPU v4 driver: mutex-serialized, ISR → task notification
src/cli.c           scoped NPU/TFLM command table, prompt, editing, dispatch
src/hw_diag.c       safe platform enumeration + restoring RO/RW/W1C checks
src/tflm_cpu.cpp    hello_world + SECDA CPU/NPU inference and golden checks
src/tflm_npu_conv.cpp
                    Conv2D im2col/tiling → NPU GEMM; CPU-side requantization
models/             reviewed SECDA simple-model C array + provenance/checksum
src/main.c          health-check tasks, CLI task + kernel hooks (idle = wfi)
src/libc_min.c      minimal freestanding libc plus libm errno hook
tests/cli_smoke.txt deterministic command input for --uart0-rx-file
```

The kernel is **not vendored**: it comes from `third_party/FreeRTOS-Kernel`
(MIT, pinned V11.2.0 by `tools/third_party/setup_third_party.sh`). SoC
addresses/IRQ IDs/NPU registers come only from `fw/common/include/soc/`
(`main.c` static-asserts the CLINT addresses against that ABI).

## Build & run

```bash
source tools/third_party/setup_env.sh          # toolchain + CC/CXX
tools/third_party/setup_third_party.sh         # fetches riscv-vp + FreeRTOS
tools/third_party/build_tflm_riscv.sh          # pinned RV32 microlite archive

make -C fw/freertos_fx1                        # NPU demo included (default)
make -C fw/freertos_fx1 NPU=0                  # public build without NPU
make -C fw/freertos_fx1 NPU=1 TFLM=1           # TFLM CPU + NPU Conv2D offload
make -C fw/freertos_fx1 NPU=0 TFLM=1           # public VP + TFLM CPU commands

build-soc/platforms/VP_FX1_Full_SoC/vp_fx1_full_soc \
    --fw fw/freertos_fx1/freertos_fx1.elf --sim-ms 700 --quantum 10000
# expect: FreeRTOS FX1 PASS   (~11 s host)
```

Or use the self-checking runner: `fw/freertos_fx1/run_freertos_test.sh`.

## Interactive and scripted console

Milestones B.1 through B.4 in `docs/NPU_CLI_TFLM_PLAN.md` are implemented. The
shell prompt is
`root@fx1:~#`; it handles backspace/delete, CR, LF, CRLF, and semicolon command
separators. `help` lists only:

```
tflm_hello  tflm_run  tflm_simple  tflm_npu
tflm_demo   hw_scan   reg_test
```

With `TFLM=1`, `tflm_hello` allocates a `MicroInterpreter` over the pinned
upstream `hello_world_int8` model and invokes it once. `tflm_run` performs the
four-sample CPU sine sweep and verifies every dequantized result.
`tflm_simple` runs all nine SECDA simple-model operators on the CPU and checks
the golden output `{-113,127}`. With `NPU=1`, `tflm_npu` replaces Conv2D with
the FX1 fixed-array GEMM adapter, while FullyConnected and the other operators
remain on the CPU. `tflm_demo` requires the CPU and NPU paths to match
bit-exactly and rejects zero hardware traffic counters.

`hw_scan` enumerates every mapped platform window from the shared SoC memory
map, prints stable identity/status values, reads the QSPI NOR JEDEC ID
`EF 40 18`, and identifies NPU0 as `SAU4`. ISP0/VPU0 are reported as reserved;
an `NPU=0` build also reports NPU0 as reserved without touching its unbound
window. The deterministic summaries are:

```text
HW_SCAN PASS implemented=25 reserved=2   # NPU=1
HW_SCAN PASS implemented=24 reserved=3   # NPU=0
```

`reg_test` runs restoring RO, RW, and W1C checks across the mapped IP. It uses
16-bit accesses for the PL022 model, masks interrupt sources while inducing
sticky causes, and restores every tested RW register. Current summaries are
`REG_TEST PASS 42/42` with NPU and `REG_TEST PASS 40/40` without NPU.

Interactive TCP session:

```bash
# Terminal 1: simulation blocks at t=0 until the TCP client connects.
build-soc/platforms/VP_FX1_Full_SoC/vp_fx1_full_soc \
    --fw fw/freertos_fx1/freertos_fx1.elf \
    --uart0-socket 5000 --uart0-wait \
    --sim-ms 60000 --quantum 10000

# Terminal 2:
nc 127.0.0.1 5000
```

Deterministic RX-file smoke tests:

```bash
fw/freertos_fx1/run_console_test.sh
BUILD_DIR=build-public NPU=0 fw/freertos_fx1/run_console_test.sh
BUILD_DIR=build-soc NPU=1 TFLM=0 fw/freertos_fx1/run_console_test.sh
```

The runner uses `--uart0-rx-delay-us 400000`. Without a delay, a long replay
starts at `t=0` and can fill the model's 16-byte hardware RX FIFO before the
FreeRTOS task enables UART interrupts. The VP option defaults to zero, so
existing ROM-code replay behavior is unchanged.

## Port integration notes

- The FreeRTOS RISC-V port **owns mtvec** (`freertos_risc_v_trap_handler`,
  installed in `start.S`). Never install another trap handler (handoff §13.4).
- MTIP is consumed by the port for the tick; everything else lands in
  `freertos_risc_v_application_interrupt_handler` → `plic.c` dispatcher.
- CLINT `mtime/mtimecmp` tick in **microseconds** on this VP, hence
  `configCPU_CLOCK_HZ = 1000000`. The port's RV32-safe 64-bit `mtimecmp`
  sequence works against `clint_tlm`'s 32-bit slice access.
- Device W1C cause is always cleared **before** the PLIC claim is completed
  (level-IRQ policy). Handlers tolerate one spurious re-claim caused by the
  delta-cycle lag between the W1C write and the level dropping.
- UART TX polls `FR.TXFF`: with a temporally decoupled CPU the 16-deep FIFO
  drains only at quantum boundaries, so blind DR writes drop characters.
- UART RX uses RX and receive-timeout interrupts at a 2-byte trigger. Its ISR
  drains `UARTDR` into a 128-byte FreeRTOS queue and clears PL011 causes before
  PLIC completion. One task owns the blocking RX API.
- TFLM is pinned by `setup_third_party.sh`; the RV32 archive build deliberately
  bypasses upstream's obsolete SiFive-toolchain/printf downloads and uses the
  repository's xPack `riscv-none-elf` compiler. The archive and upstream
  hello-world model remain outside Git under `third_party/`; the reviewed
  SECDA simple model is tracked as a 16-byte-aligned C array with its identity
  recorded in `models/ASSET_PROVENANCE.md`.
- The NPU Conv2D adapter stages im2col activations at `VPU_OUT0`, transposed
  weights at `NPU_WEIGHTS0`, and INT32 results at `NPU_WORK0`. It pads M/N to
  32, splits K at 992, accumulates partial results, then performs input
  zero-point correction, bias, per-channel requantization, output offset, and
  activation clamp on the CPU.

## Known limits

- `--quantum 100000` (100 µs) stalls the demo after boot; use ≤10 µs
  (default 1 µs works; 10 µs is the tested sweet spot).
- The NPU demo uses fixed physical buffers (VPU_OUT0 / NPU_WEIGHTS0 /
  NPU_WORK0); no DMA-capable allocator yet (handoff §13.8 later phase).
- Milestone 6 (concurrent NPU callers, error/timeout injection) is not
  automated yet; the driver API (`npu_v4_gemm` timeout, mutex) supports it.
- The custom TFLM path offloads standard, non-grouped INT8 Conv2D with
  symmetric INT8 weights. Grouped Conv2D and FullyConnected offload are not
  implemented; those cases are rejected or remain on the CPU.
- `reg_test` deliberately does not issue an unaligned/decode-miss CPU access.
  Such an access is useful at the TLM component-test level, but the current
  FreeRTOS mtvec path has no recoverable synchronous-fault probe API; injecting
  it from the CLI would terminate the firmware instead of producing a result.
