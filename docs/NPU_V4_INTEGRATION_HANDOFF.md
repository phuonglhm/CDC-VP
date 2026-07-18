# NPU v4 Integration Handoff and FreeRTOS Next Steps

## 1. Purpose

This document is the engineering handoff for the SAURIA NPU v4 integration
into `VP_FX1_Full_SoC`.

It is intended to give a new engineer or AI agent enough context to:

1. understand what was integrated;
2. rebuild and verify the current NPU path;
3. modify the NPU wrapper or firmware without breaking the SoC ABI; and
4. continue with the next milestone: booting FreeRTOS and driving the NPU from
   an RTOS task through PLIC interrupt source 17.

Repository root:

```text
CDC-VP/
```

Integrated platform:

```text
platforms/VP_FX1_Full_SoC
```

External NPU model input:

```text
$SAURIA_NPU_ROOT
```

`$SAURIA_NPU_ROOT` must contain `npu_top.h` and its relative include tree.
Do not hard-code a developer-specific absolute path in repository files.

---

## 2. Current Status

The NPU is integrated and verified end to end in the NPU-enabled build.

The validated path is:

```text
RV32 CPU
  -> system bus
  -> NPU0 64 KiB MMIO target
  -> NPU wrapper worker
  -> NPU RAM-master transactions
  -> SAURIA signal-level core
  -> RAM0 result buffer
  -> NPU level interrupt
  -> PLIC source 17
  -> CPU machine external interrupt
  -> firmware result verification
```

Expected firmware markers:

```text
NPU v4 platform start
NPU IRQ17
NPU PASS
```

`NPU IRQ17` proves the device-to-PLIC-to-CPU interrupt path.

`NPU PASS` proves that all 1024 INT32 result elements match the software
reference result.

The default CMake configuration leaves the NPU disabled:

```cmake
CDC_ENABLE_SAURIA_NPU_V4=OFF
```

An NPU-enabled build must explicitly set:

```cmake
CDC_ENABLE_SAURIA_NPU_V4=ON
SAURIA_NPU_ROOT=<directory-containing-npu_top.h>
```

When disabled:

- the NPU component is not configured or compiled;
- the `0x100F_0000` MMIO window is not bound;
- PLIC source 17 is tied low; and
- the rest of `VP_FX1_Full_SoC` still builds and boots normally.

---

## 3. Files Added or Changed for the NPU

### 3.1 TLM component

```text
components/npu_tlm_v4_model/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── npu_tlm_v4_model.h
│   └── npu_tlm_v4_regmap.h
├── src/
│   └── npu_tlm_v4_model.cpp
└── tests/
    ├── CMakeLists.txt
    └── test_npu_tlm_v4_model.cpp
```

Responsibilities:

- provide a CPU-visible TLM target socket;
- provide a RAM-master TLM initiator socket;
- validate the programmed job;
- read row-major matrices from physical RAM;
- stage operands into the SAURIA private SRAM layout;
- pre-skew the weight matrix for the SAURIA feeder;
- start and monitor the signal-level core;
- drain the core result SRAM;
- write the INT32 result matrix to physical RAM;
- expose sticky done/error state and diagnostic counters; and
- drive a level-sensitive done/error interrupt.

### 3.2 Full-SoC integration

```text
platforms/VP_FX1_Full_SoC/CMakeLists.txt
platforms/VP_FX1_Full_SoC/src/vp_fx1_full_soc_top.cpp
platforms/VP_FX1_Full_SoC/src/vp_fx1_full_soc_top.h
platforms/VP_FX1_Full_SoC/configs/default.yaml
platforms/VP_FX1_Full_SoC/tests/run_vp_fx1_test.sh
```

Integration performed in the SoC top:

- instantiate `npu_tlm_v4_model` only when the CMake option is enabled;
- add one optional downstream MMIO target;
- add one optional upstream RAM-master port;
- bind `npu0.target_socket` at `0x100F_0000`;
- bind `npu0.master_socket` to the shared system bus;
- bind active-low `reset_n`;
- bind `irq_out` to PLIC source 17;
- tie PLIC source 17 low when the NPU is disabled; and
- report the NPU state in the platform startup banner.

The NPU core clock period used by the full SoC is:

```text
2 ns
```

Default MMIO transaction latency in the wrapper is:

```text
10 ns
```

### 3.3 Firmware ABI

```text
fw/common/include/soc/soc_memory_map.h
fw/common/include/soc/soc_irq_map.h
fw/common/include/soc/regs/soc_regs_npu_v4.h
```

These headers are the firmware-facing ABI. Driver code must use these
definitions instead of duplicating addresses or register offsets.

### 3.4 Bare-metal end-to-end firmware

```text
fw/npu_v4_irq_riscv/
├── Makefile
├── README.md
├── linker.ld
├── startup.S
└── src/main.c
```

The firmware:

- builds for RV32IMAC with Zicsr;
- places code and data at `0x8000_0000`;
- initializes deterministic activation and weight matrices;
- configures PLIC source 17;
- programs the NPU MMIO registers;
- waits using `wfi`;
- handles the NPU interrupt;
- clears the NPU device interrupt before PLIC completion; and
- checks every output element against a software-computed expected value.

### 3.5 Register drift check

```text
tools/check_regs_drift.cpp
tools/check_regs_drift.sh
```

The drift check compares the model register definitions with the firmware ABI
and must pass whenever NPU registers are changed.

---

## 4. SoC Address and Interrupt Contract

### 4.1 NPU MMIO

| Item | Value |
|---|---:|
| NPU base | `0x100F_0000` |
| MMIO size | `0x0001_0000` |
| Last address | `0x100F_FFFF` |
| CPU access width | aligned 32-bit little-endian |
| PLIC source | 17 |
| IRQ type | level-sensitive |

### 4.2 RAM

The full SoC exposes:

```text
RAM0 = 0x8000_0000 .. 0x8FFF_FFFF
size = 256 MiB
```

The wrapper accepts NPU DMA ranges only inside this RAM window.

Relevant buffer windows:

| Window | Range | Intended use |
|---|---|---|
| FW / RTOS | `0x8000_0000..0x80FF_FFFF` | firmware image, heap, task stacks |
| RAW_IN0 | `0x8100_0000..0x81FF_FFFF` | sensor/test input |
| ISP_OUT0 | `0x8200_0000..0x83FF_FFFF` | ISP output |
| VPU_OUT0 | `0x8400_0000..0x85FF_FFFF` | NPU activation input |
| NPU_WEIGHTS0 | `0x8600_0000..0x87FF_FFFF` | NPU weights |
| NPU_WORK0 | `0x8800_0000..0x8BFF_FFFF` | NPU result/work buffers |
| Reserved | `0x8C00_0000..0x8FFF_FFFF` | future use |

Firmware programs full system physical addresses. It must not subtract
`CDC_RAM0_BASE`.

Current end-to-end firmware uses:

```text
activations = CDC_VPU_OUT0_BASE
weights     = CDC_NPU_WGT0_BASE
results     = CDC_NPU_WORK0_BASE
```

### 4.3 PLIC interrupt

NPU interrupt source:

```c
#define CDC_IRQ_NPU0 17u
```

The required level-sensitive service order is:

1. read the PLIC claim register;
2. verify the claim is source 17;
3. read NPU status and interrupt cause;
4. clear `NPU_IRQ_STATUS` with W1C;
5. notify the waiting software context;
6. write source 17 to the PLIC claim/complete register.

The device cause must be cleared before PLIC completion. Completing the PLIC
while the NPU IRQ level remains asserted can immediately retrigger the
interrupt.

---

## 5. Supported Compute Operation

The first and currently only operation is:

```text
C[32][32] = A[32][K] * B[K][32]
```

Data types:

```text
A: signed INT8
B: signed INT8
C: signed INT32
```

Constraints:

| Parameter | Constraint |
|---|---|
| M / HEIGHT | exactly 32 |
| N / WIDTH | exactly 32 |
| K | `1..992` |
| operation | GEMM |
| format | INT8 × INT8 accumulated to INT32 |
| A layout | row-major `A[32][K]` |
| B layout | row-major `B[K][32]` |
| C layout | row-major `C[32][32]` |
| destination alignment | 4-byte aligned |

Activation stride:

```text
SRC_STRIDE_BYTES = bytes between consecutive rows of A
```

If `SRC_STRIDE_BYTES` is zero, the wrapper uses `K`. A nonzero stride must be
at least `K`.

Required buffer sizes:

```text
A span = stride * 31 + K
B size = K * 32
C size = 32 * 32 * sizeof(int32_t) = 4096 bytes
```

The wrapper converts normal row-major firmware data into the internal core
layout. Firmware must not pre-skew the weights.

---

## 6. Register Interface

### 6.1 Common accelerator bank

| Offset | Register | Access | Meaning |
|---:|---|---|---|
| `0x0000` | `CTRL` | RW/pulse | enable, start, soft reset, global IRQ enable |
| `0x0004` | `STATUS` | RO/W1C | busy, sticky done/error, idle |
| `0x0008` | `IRQ_ENABLE` | RW | enable done/error causes |
| `0x000C` | `IRQ_STATUS` | RO/W1C | sticky done/error causes |
| `0x0010` | `SRC_ADDR` | RW | physical address of A |
| `0x0014` | `DST_ADDR` | RW | physical address of C |
| `0x0018` | `SCRATCH_ADDR` | RW | reserved |
| `0x001C` | `SRC_SIZE_BYTES` | RW | available A span |
| `0x0020` | `DST_SIZE_BYTES` | RW | available C bytes |
| `0x0024` | `WIDTH` | RW | must be 32 |
| `0x0028` | `HEIGHT` | RW | must be 32 |
| `0x002C` | `SRC_STRIDE_BYTES` | RW | A row stride; zero means K |
| `0x0030` | `FORMAT` | RW | must be `INT8_INT8_INT32` |
| `0x0034` | `OP_MODE` | RW | must be GEMM |
| `0x0038` | `WEIGHTS_ADDR` | RW | physical address of B |
| `0x003C` | `PARAM_ADDR` | RW | reserved |
| `0x0040` | `WEIGHTS_SIZE_BYTES` | RW | available B bytes |

### 6.2 NPU-specific bank

| Offset | Register | Access | Meaning |
|---:|---|---|---|
| `0x1000` | `K_DIMENSION` | RW | reduction dimension |
| `0x1004` | `ZERO_THRESHOLD_FP32` | RW | IEEE-754 threshold bits |
| `0x1008` | `ROWS_ACTIVE` | RW | active-row mask |
| `0x100C` | `DILATION_PATTERN` | RW | feeder dilation pattern |
| `0x1010` | `CYCLE_COUNT` | RO | last core execution cycles |
| `0x1014` | `BYTES_READ` | RO | last RAM read byte count |
| `0x1018` | `BYTES_WRITTEN` | RO | last RAM write byte count |
| `0x101C` | `LAST_ERROR` | RO | last diagnostic error |
| `0x1020` | `CORE_ID` | RO | `0x53415534`, ASCII `"SAU4"` |

Reset defaults relevant to the current operation:

```text
STATUS           = IDLE
K_DIMENSION      = 64
ZERO_THRESHOLD   = 0.0f
ROWS_ACTIVE      = 0xFFFF_FFFF
DILATION_PATTERN = 1
LAST_ERROR       = NONE
```

### 6.3 Control bits

```text
CTRL.ENABLE     bit 0
CTRL.START      bit 1, pulse
CTRL.SOFT_RESET bit 2, pulse
CTRL.IRQ_EN     bit 3

STATUS.BUSY     bit 0
STATUS.DONE     bit 1
STATUS.ERROR    bit 2
STATUS.IDLE     bit 3

IRQ.DONE        bit 0
IRQ.ERROR       bit 1
```

IRQ equation:

```text
irq_out = CTRL.IRQ_EN && ((IRQ_ENABLE & IRQ_STATUS) != 0)
```

Registers that configure a job cannot be modified while `STATUS.BUSY` is set.

---

## 7. Job Programming Sequence

The required software sequence is:

1. Confirm `CORE_ID == 0x53415534`.
2. Confirm `STATUS.BUSY == 0`.
3. Prepare A and B in physical RAM.
4. Clear or initialize the C buffer if required by the test.
5. Program `SRC_ADDR`, `SRC_SIZE_BYTES`, and `SRC_STRIDE_BYTES`.
6. Program `WEIGHTS_ADDR` and `WEIGHTS_SIZE_BYTES`.
7. Program `DST_ADDR` and `DST_SIZE_BYTES`.
8. Program `WIDTH=32`, `HEIGHT=32`, and `K_DIMENSION`.
9. Program the format and GEMM operation.
10. Clear stale `IRQ_STATUS` and sticky `STATUS` causes.
11. Enable the required done/error causes in `IRQ_ENABLE`.
12. Write `CTRL = ENABLE | IRQ_EN`.
13. Write `CTRL = ENABLE | IRQ_EN | START`.
14. Wait for an interrupt or poll `STATUS`.
15. On interrupt, clear the device cause before PLIC completion.
16. Check `STATUS`, `LAST_ERROR`, counters, and the C matrix.

Recommended ordering before `START`:

```c
__asm__ volatile("fence rw, rw" ::: "memory");
```

The current VP has no modeled caches, so no cache-maintenance operation is
required. A cache-maintenance abstraction should still be introduced in the
FreeRTOS driver so the software architecture remains portable to a cached
target.

---

## 8. Error Behavior

The NPU exposes these error codes:

| Value | Name | Meaning |
|---:|---|---|
| 0 | `NONE` | no error |
| 1 | `DISABLED` | start requested without enable |
| 2 | `BUSY` | start/reset or protected write while busy |
| 3 | `INVALID_DIMENSIONS` | width, height, or K unsupported |
| 4 | `INVALID_FORMAT` | format is not INT8/INT8/INT32 |
| 5 | `INVALID_OPERATION` | operation is not GEMM |
| 6 | `INVALID_ADDRESS` | range outside RAM or bad alignment |
| 7 | `INVALID_SIZE` | programmed size/stride is insufficient |
| 8 | `DMA_READ` | RAM-master read failed |
| 9 | `DMA_WRITE` | RAM-master write failed |
| 10 | `CORE_DEADLOCK` | core asserted deadlock |
| 11 | `CORE_TIMEOUT` | core did not finish within the wrapper timeout |
| 12 | `RESET_ABORTED` | external reset aborted the operation |

Invalid job parameters complete with:

```text
STATUS.ERROR = 1
IRQ_STATUS.ERROR = 1
LAST_ERROR = diagnostic code
```

Invalid MMIO width, alignment, or offset returns a TLM error response instead
of silently accepting the transaction.

---

## 9. Wrapper Execution Model

The wrapper contains a SystemC worker thread.

At a high level, one job performs:

1. validate all dimensions, sizes, alignment, and RAM ranges;
2. issue RAM-master reads for A and B;
3. soft-reset the signal-level core;
4. program the core configuration registers;
5. stage A into the core's activation SRAM;
6. convert B from row-major to the pre-skewed feeder layout;
7. pulse core start;
8. monitor done, deadlock, timeout, and external reset;
9. read the output SRAM;
10. round each core output lane to signed INT32;
11. issue one RAM-master write for C; and
12. set sticky status and IRQ state.

Only one job can be in flight. There is no hardware queue, descriptor ring, or
multi-context support.

---

## 10. Build and Run

### 10.1 Host compiler environment

Set this before configuring or building:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head -n 1
```

### 10.2 Configure the NPU-enabled platform

```bash
export SAURIA_NPU_ROOT=/path/to/v4_model

cmake -S . -B build-soc \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DCDC_BUILD_TESTS=ON \
  -DCDC_ENABLE_SAURIA_NPU_V4=ON \
  -DSAURIA_NPU_ROOT="$SAURIA_NPU_ROOT"
```

Verify the build mode:

```bash
grep '^CDC_ENABLE_SAURIA_NPU_V4' build-soc/CMakeCache.txt
```

Required result:

```text
CDC_ENABLE_SAURIA_NPU_V4:BOOL=ON
```

### 10.3 Build the platform and component test

```bash
cmake --build build-soc \
  --target vp_fx1_full_soc test_npu_tlm_v4_model \
  -j"$(nproc)"
```

### 10.4 Run the component test

```bash
ctest --test-dir build-soc \
  -R npu_tlm_v4_model \
  --output-on-failure
```

The component test covers:

- a non-power-of-two `K=17` GEMM;
- numerical result checking;
- RAM-master byte counters;
- sticky done/error state;
- IRQ assertion and W1C deassertion;
- invalid MMIO width and address;
- invalid DMA range; and
- active-low external reset.

### 10.5 Run the full-SoC regression

```bash
SAURIA_NPU_ROOT="$SAURIA_NPU_ROOT" \
BUILD_DIR=build-soc \
platforms/VP_FX1_Full_SoC/tests/run_vp_fx1_test.sh --no-build
```

Required result:

```text
[PASS] elaboration clean (all bindings OK)
[PASS] firmware console output: 'Hello from RISC-V'
[PASS] NPU GEMM DMA + PLIC IRQ17 end-to-end
ALL CHECKS PASSED
```

### 10.6 Run only the NPU firmware manually

```bash
export PATH=/opt/toolchains/riscv-none-elf/bin:/usr/bin:/bin:$PATH

make -C fw/npu_v4_irq_riscv clean
make -C fw/npu_v4_irq_riscv

build-soc/platforms/VP_FX1_Full_SoC/vp_fx1_full_soc \
  -c platforms/VP_FX1_Full_SoC/configs/default.yaml \
  --fw fw/npu_v4_irq_riscv/npu_v4_irq.elf \
  --sim-ms 2
```

Required UART output:

```text
NPU v4 platform start
NPU IRQ17
NPU PASS
```

### 10.7 Check the model/firmware register ABI

```bash
tools/check_regs_drift.sh
```

Required result:

```text
register drift check: PASS
```

---

## 11. Current Verification Evidence

The following were verified with GCC/G++ 11.5.0:

- NPU-disabled platform build;
- NPU-disabled full-SoC elaboration;
- normal bare-metal UART firmware on the NPU-disabled platform;
- NPU-enabled platform build;
- NPU wrapper component CTest;
- model/firmware register drift check;
- NPU-enabled full-SoC elaboration;
- NPU `K=64` bare-metal firmware;
- PLIC source 17 claim/complete path; and
- all 1024 INT32 result values.

---

## 12. Known Limitations

The current NPU integration intentionally has these limits:

- fixed `M=32` and `N=32`;
- only GEMM is supported;
- only signed INT8 inputs and signed INT32 outputs are supported;
- only one job can be active;
- no descriptor queue;
- no scatter-gather;
- no IOMMU or virtual addressing;
- no cache model or cache coherency protocol;
- no DMI path for NPU RAM traffic;
- no performance model for bus contention;
- no power or clock gating of the NPU;
- no preemption or cancellation of a running job except reset;
- no FreeRTOS driver yet;
- no user/kernel separation;
- single-hart CPU;
- M-mode only;
- loosely timed system behavior even though the wrapped core advances on a
  local 2 ns SystemC clock.

The `CYCLE_COUNT` register is a core execution diagnostic. It must not be
interpreted as full-system wall-clock performance or silicon timing.

---

## 13. Next Milestone: FreeRTOS

> **Status 2026-07-18: milestones 1-5 COMPLETE** via `fw/freertos_fx1`
> (FreeRTOS-Kernel V11.2.0, pinned in `third_party/` by
> `tools/third_party/setup_third_party.sh`). The run prints
> `FreeRTOS FX1 boot`, `CLINT tick OK`, `PLIC TIMER0 OK irqs=3`,
> `FreeRTOS NPU PASS`, `FreeRTOS FX1 PASS`; self-checking runner:
> `fw/freertos_fx1/run_freertos_test.sh` (`NPU=0` for the public build).
> Milestone 6 (concurrency/error injection) remains open.
>
> Related platform fixes made for RTOS-scale runs (~12x host speedup, from
> ~190 s to ~16 s of host time per simulated second while mostly idle):
> the SAURIA core clock in `npu_tlm_v4_model` is now gated (toggles only
> while the worker consumes edges), `pmu_tlm`'s five 1-100 ns polling
> threads are event-driven (`kick_` event), and the unused free-running
> `trng0_clk` sc_clock in `VP_FX1_Full_SoC` is a static signal. All 37
> component tests and the bare-metal NPU/romcode regressions still pass.
> Known limit: `--quantum 100000` (100 us) stalls the FreeRTOS demo after
> boot; `--quantum 10000` is the tested sweet spot.

### 13.1 Goal

Boot FreeRTOS on `VP_FX1_Full_SoC`, run multiple tasks, and execute the NPU GEMM
from a task that blocks until PLIC source 17 wakes it.

Minimum demonstration:

```text
FreeRTOS scheduler starts
  -> periodic CLINT tick works
  -> UART console task runs
  -> NPU task submits GEMM
  -> task blocks
  -> NPU IRQ17 arrives
  -> ISR clears the device and completes PLIC
  -> ISR wakes the NPU task
  -> task verifies result
  -> UART prints "FreeRTOS NPU PASS"
```

### 13.2 Existing hardware support usable by FreeRTOS

| Requirement | Current VP support |
|---|---|
| CPU | single-hart RV32IMAC, M-mode |
| firmware RAM | `0x8000_0000..0x80FF_FFFF` |
| scheduler tick | CLINT `mtime/mtimecmp`, microsecond units |
| software interrupt | CLINT MSIP |
| external interrupt controller | PLIC, machine context |
| console | UART0 |
| NPU interrupt | PLIC source 17 |
| NPU DMA memory | physical RAM0 |

Recommended initial FreeRTOS configuration:

```text
single hart
M-mode
no MMU
no caches
configTICK_RATE_HZ = 1000
preemptive scheduling enabled
minimal heap implementation first
UART polling for the first boot milestone
```

At 1000 Hz, the CLINT tick interval is:

```text
1000 microseconds
```

because this VP models `mtime` in microseconds.

### 13.3 Proposed new firmware tree

```text
fw/freertos_fx1/
├── Makefile
├── linker.ld
├── FreeRTOSConfig.h
├── startup/
│   ├── start.S
│   └── trap_entry.S
├── port/
│   └── RV32_MMODE/
├── bsp/
│   ├── clint.c
│   ├── plic.c
│   ├── uart.c
│   └── npu_v4.c
├── include/
│   ├── clint.h
│   ├── plic.h
│   └── npu_v4.h
└── src/
    ├── main.c
    ├── console_task.c
    └── npu_task.c
```

Reuse the existing ABI headers under:

```text
fw/common/include/soc/
```

Do not duplicate the SoC memory map, IRQ IDs, or NPU register offsets in the
FreeRTOS tree.

### 13.4 Critical trap-handler change

The bare-metal NPU test directly writes:

```text
mtvec = trap_handler
```

That approach cannot be copied unchanged into the FreeRTOS application.

The FreeRTOS RISC-V port must own the machine trap entry because it performs:

- register context save and restore;
- scheduler tick processing;
- task switching;
- exception handling; and
- interrupt dispatch.

The FreeRTOS trap entry must distinguish at least:

```text
MTIP: machine timer interrupt from CLINT
MEIP: machine external interrupt from PLIC
MSIP: machine software interrupt if used by the port
exceptions/ecall as required by the selected port
```

The NPU handler must be called from the MEIP/PLIC branch. Do not install a
second independent `mtvec` handler from the NPU driver.

### 13.5 CLINT tick implementation

CLINT registers:

```text
mtimecmp = CDC_CLINT_BASE + 0x4000
mtime    = CDC_CLINT_BASE + 0xBFF8
```

Both are 64-bit counters accessed by an RV32 CPU. Use an RV32-safe sequence
when updating `mtimecmp` to avoid a transient compare value:

1. write the high word to `0xFFFF_FFFF`;
2. write the new low word;
3. write the new high word.

On each tick:

1. read the current 64-bit `mtime`;
2. program the next `mtimecmp`;
3. call the FreeRTOS tick handler;
4. request a context switch when required; and
5. restore the selected task context.

Do not assume the CLINT unit is CPU cycles. It is microseconds in this VP.

### 13.6 PLIC dispatcher

Create a single PLIC machine-context dispatcher.

Recommended interface:

```c
typedef void (*plic_handler_t)(uint32_t source, void *arg);

void plic_init(void);
int  plic_register_handler(uint32_t source,
                           plic_handler_t handler,
                           void *arg);
void plic_enable(uint32_t source, uint32_t priority);
void plic_dispatch(void);
```

For source 17, the dispatcher calls the NPU ISR.

The dispatcher must always complete a valid claim, including unexpected
sources, after the device-specific handler has deasserted the device level.

### 13.7 Proposed FreeRTOS NPU driver

Recommended API:

```c
typedef struct {
    const int8_t *activations;
    const int8_t *weights;
    int32_t *output;
    uint32_t k;
    uint32_t activation_stride;
} npu_gemm_job_t;

int npu_v4_init(void);
int npu_v4_submit(const npu_gemm_job_t *job);
int npu_v4_wait(TickType_t timeout);
int npu_v4_gemm(const npu_gemm_job_t *job, TickType_t timeout);
void npu_v4_isr(void);
```

Driver state should contain:

- a mutex protecting the single hardware context;
- one task notification or binary semaphore for completion;
- the last device status;
- the last interrupt cause;
- the last error code; and
- an initialized flag.

Suggested task flow:

```text
take NPU mutex
  -> validate job
  -> prepare/flush buffers
  -> clear stale status
  -> program MMIO
  -> fence
  -> start
  -> block on task notification
  -> inspect status/result
  -> invalidate result buffer if required
  -> release mutex
```

Suggested ISR flow:

```text
read NPU IRQ_STATUS
  -> read NPU STATUS
  -> clear NPU IRQ_STATUS
  -> store completion state
  -> notify waiting task from ISR
  -> request yield if a higher-priority task was woken
```

The PLIC dispatcher completes source 17 after `npu_v4_isr()` returns.

### 13.8 Memory allocation strategy

For the first RTOS milestone, avoid dynamic placement ambiguity. Use fixed
NPU buffer windows or explicit linker sections outside the FW/RTOS window.

Example:

```text
activation buffer -> VPU_OUT0
weight buffer     -> NPU_WEIGHTS0
result buffer     -> NPU_WORK0
```

Later, add a DMA-capable buffer allocator that guarantees:

- physical RAM0 placement;
- required alignment;
- non-overlap;
- lifetime through asynchronous completion; and
- cache-maintenance hooks.

Task stacks and the FreeRTOS heap must stay inside the FW/RTOS window and must
not overlap the fixed NPU buffers.

### 13.9 FreeRTOS verification sequence

Implement milestones in this order:

#### Milestone 1: scheduler boot

- boot from `0x8000_0000`;
- start two tasks;
- print from a low-priority UART task;
- verify periodic context switches.

#### Milestone 2: CLINT tick

- receive repeated MTIP interrupts;
- rearm `mtimecmp`;
- run `vTaskDelay()` accurately enough for functional testing;
- verify no interrupt storm.

#### Milestone 3: PLIC framework

- install one generic MEIP dispatcher;
- validate claim/complete with an existing peripheral;
- validate nested masking assumptions.

#### Milestone 4: NPU polling driver

- run the existing `K=64` GEMM from one task;
- poll status first to isolate the RTOS memory/driver path;
- verify all 1024 outputs.

#### Milestone 5: interrupt-driven NPU

- enable PLIC source 17;
- block the NPU task on a notification/semaphore;
- wake it from the NPU ISR;
- verify clear-before-complete ordering;
- print `FreeRTOS NPU PASS`.

#### Milestone 6: concurrency and errors

- run a console task while the NPU is active;
- serialize two tasks requesting the NPU;
- repeat multiple jobs with different K values;
- test an invalid address and recover from `STATUS.ERROR`;
- test timeout handling;
- verify the mutex is always released on error.

### 13.10 FreeRTOS definition of done

The FreeRTOS milestone is complete when:

- the scheduler runs on the full SoC;
- CLINT provides a stable periodic tick;
- UART output remains functional across task switches;
- one PLIC dispatcher handles machine external interrupts;
- NPU source 17 wakes a blocked task;
- the device cause is cleared before PLIC completion;
- the NPU driver serializes callers;
- repeated GEMM results match software reference values;
- error and timeout paths do not deadlock the scheduler;
- the existing bare-metal NPU regression still passes; and
- the new FreeRTOS test can run unattended and emit a deterministic PASS
  marker.

---

## 14. Guardrails for Future Changes

An engineer or AI agent modifying this integration should preserve these
rules:

1. Do not change the NPU base address or PLIC source without updating the SoC
   map, firmware headers, tests, and documentation together.
2. Do not create duplicate register definitions in a new driver.
3. Do not bypass the system bus with direct RAM backdoor access in normal NPU
   execution.
4. Do not let the NPU master access memory outside RAM0.
5. Do not complete PLIC source 17 before clearing the NPU interrupt cause.
6. Do not modify job registers while the NPU is busy.
7. Do not assume NPU buffers are virtual addresses.
8. Do not assume caches will remain absent forever; keep buffer maintenance
   hooks in the driver API.
9. Do not interpret `CYCLE_COUNT` as full-system performance.
10. Do not let the FreeRTOS application replace the port's machine trap entry.
11. Keep the NPU-disabled platform build working.
12. Run component, firmware, and register-drift tests after ABI changes.

---

## 15. Primary References in the Repository

Read these files before changing the integration:

```text
components/npu_tlm_v4_model/README.md
components/npu_tlm_v4_model/include/npu_tlm_v4_model.h
components/npu_tlm_v4_model/include/npu_tlm_v4_regmap.h
components/npu_tlm_v4_model/src/npu_tlm_v4_model.cpp
components/npu_tlm_v4_model/tests/test_npu_tlm_v4_model.cpp

platforms/VP_FX1_Full_SoC/src/vp_fx1_full_soc_top.cpp
platforms/VP_FX1_Full_SoC/configs/default.yaml
platforms/VP_FX1_Full_SoC/tests/run_vp_fx1_test.sh

fw/common/include/soc/soc_memory_map.h
fw/common/include/soc/soc_irq_map.h
fw/common/include/soc/regs/soc_regs_npu_v4.h
fw/npu_v4_irq_riscv/src/main.c

docs/peripheral_memory_map.md
docs/clock_reset_and_bus_protocol.md
```

The current bare-metal firmware is the executable reference for the first
FreeRTOS NPU driver. Reuse its register order, buffer layout, result formula,
and device-clear-before-PLIC-complete behavior, but integrate its interrupt
logic into the FreeRTOS port's central trap dispatcher.
