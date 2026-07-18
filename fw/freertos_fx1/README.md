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

## Layout

```
Makefile            single-command cross build (riscv-none-elf)
linker.ld           FW/RTOS window: RAM0 0x8000_0000 + 16 MiB
FreeRTOSConfig.h    tick 1 kHz; CLINT µs units → configCPU_CLOCK_HZ = 1 MHz
startup/start.S     sp, mtvec = freertos_risc_v_trap_handler, .bss clear
bsp/uart.c          PL011 TX with FR.TXFF polling + task-safe mutex
bsp/plic.c          single MEIP dispatcher (claim → handler → complete);
                    overrides freertos_risc_v_application_interrupt_handler
bsp/npu_v4.c        NPU v4 driver: mutex-serialized, ISR → task notification
src/main.c          demo tasks + kernel hooks (idle hook = wfi)
src/libc_min.c      freestanding memset/memcpy/memmove/memcmp/strlen
```

The kernel is **not vendored**: it comes from `third_party/FreeRTOS-Kernel`
(MIT, pinned V11.2.0 by `tools/third_party/setup_third_party.sh`). SoC
addresses/IRQ IDs/NPU registers come only from `fw/common/include/soc/`
(`main.c` static-asserts the CLINT addresses against that ABI).

## Build & run

```bash
source tools/third_party/setup_env.sh          # toolchain + CC/CXX
tools/third_party/setup_third_party.sh         # fetches riscv-vp + FreeRTOS

make -C fw/freertos_fx1                        # NPU demo included (default)
make -C fw/freertos_fx1 NPU=0                  # public build without NPU

build-soc/platforms/VP_FX1_Full_SoC/vp_fx1_full_soc \
    --fw fw/freertos_fx1/freertos_fx1.elf --sim-ms 700 --quantum 10000
# expect: FreeRTOS FX1 PASS   (~11 s host)
```

Or use the self-checking runner: `fw/freertos_fx1/run_freertos_test.sh`.

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

## Known limits

- `--quantum 100000` (100 µs) stalls the demo after boot; use ≤10 µs
  (default 1 µs works; 10 µs is the tested sweet spot).
- The NPU demo uses fixed physical buffers (VPU_OUT0 / NPU_WEIGHTS0 /
  NPU_WORK0); no DMA-capable allocator yet (handoff §13.8 later phase).
- Milestone 6 (concurrent NPU callers, error/timeout injection) is not
  automated yet; the driver API (`npu_v4_gemm` timeout, mutex) supports it.
