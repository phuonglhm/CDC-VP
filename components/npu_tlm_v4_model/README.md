# SAURIA NPU V4.2 TLM wrapper

`npu_tlm_v4_model` adapts the signal-level SAURIA MP1 V1.1 V4.2 SystemC core
to CDC-VP. Its software interface follows the FVP V4.2 bridge:

- a 1 MiB TLM target aperture for CPU MMIO;
- native controller, profile, feeder, layer and SRAM access;
- compact rich-instruction, OBP and RCE aliases;
- read-only 64-bit views of the real V4.2 `PerfCounters` fields;
- the existing CDC GEMM path in a compatibility bank at offset `0x30000`;
- a TLM initiator socket for physical RAM DMA;
- an active-low reset input;
- a level-sensitive done/error IRQ output.

The compatibility path supports one fixed-array GEMM:

```text
C[32][32] = A[32][K] x B[K][32]
```

`A` and `B` are signed INT8 and `C` is signed INT32. `K` is in the range
`1..992`. The wrapper stages the operands into the core's private SRAMs,
including the pre-skew required by the SAURIA weight feeder, runs the
cycle-level core, and drains the result through its RAM master socket.

## External model dependency

The SAURIA source is not copied into CDC-VP. Configure its location with:

```bash
cmake -S . -B build-soc \
  -DCDC_ENABLE_SAURIA_NPU_V4=ON \
  -DSAURIA_NPU_ROOT=/path/to/v4.2_model
```

`SAURIA_NPU_ROOT` must contain `npu_top.h`, `npu_profile.h`,
`sauria_targets.h`, `driver/libsauria_cfg.h` and
`instrumentation/perf_counters.h`. There is deliberately no public default
path. With
`CDC_ENABLE_SAURIA_NPU_V4=OFF` (the default), CDC-VP does not configure or
compile this component.

The local SystemC source tree has no embedded license file. CDC-VP therefore
bundles the official upstream SAURIA license and an explicit provenance record
in `licenses/SAURIA.SHL-2.1` and `licenses/SAURIA.PROVENANCE.md`. Upstream is
`Apache-2.0 WITH SHL-2.1` with an Apache-2.0 option. The public CDC-VP source
tree contains this adapter but not the external implementation.

## Programming contract

All MMIO accesses are aligned 32-bit little-endian words. Buffer registers
contain system physical addresses; firmware must not subtract
`CDC_RAM0_BASE`.

| Offset range | Interface |
|---:|---|
| `0x00000-0x00AFF` | Native V4.2 control/configuration |
| selected offsets in `0x01200-0x01268` | VP-only V4.2 counters |
| `0x10000-0x10FFF` | Compact rich-instruction alias |
| `0x20000-0x2FFFF` | Compact OBP/RCE table aliases |
| `0x30000-0x31FFF` | CDC compatibility GEMM bank |
| `0x40000` | Native SRAM A window |
| `0x80000` | Native SRAM B window |
| `0xC0000` | Native SRAM C window |

### Native V4.2 interface

Write `NATIVE_CFG_PROFILE` before profile-dependent registers:

- `PROFILE_V1_SAURIA = 0` selects the full SAURIA feeder/layer map.
- `PROFILE_V4_LINEAR = 1` selects the alternate linear map.

The native block bases are `0x200` (controller), `0x400` (activation feeder),
`0x600` (weight feeder), `0x800` (output/PSM/OBP), and `0xA00` (layer
descriptor). Complete definitions are in `npu_tlm_v4_regmap.h` and
`soc_regs_npu_v4.h`.

The compact aliases translate the V4.2 model's sparse internal rich, OBP and
RCE addresses into the 1 MiB VP aperture. Rich floating-point scale fields use
normal IEEE-754 binary32 MMIO values.

### V4.2 performance counters

Only fields declared by V4.2 `PerfCounters` are exposed. Each is read-only and
uses a low/high 32-bit pair:

| Low | High | Counter |
|---:|---:|---|
| `0x1200` | `0x1240` | Execution cycles |
| `0x1204` | `0x1244` | Stall cycles |
| `0x1208` | `0x1248` | MAC operations |
| `0x120C` | `0x124C` | Active PE cycles |
| `0x1210` | `0x1250` | Total PE cycles |
| `0x1214` | `0x1254` | Total cycles |
| `0x1224` | `0x1264` | Systolic-array active cycles |
| `0x1228` | `0x1268` | OBP active cycles |

Derived utilization and stall fractions are software calculations, not MMIO
registers. The model's other internal debug counters are not exposed.

### CDC compatibility GEMM interface

The offsets below are local to `WRAPPER_BASE = 0x30000`. Firmware should use
the `CDC_NPU_*` definitions, which already include that base.

| Offset | Register | Access | Description |
|---:|---|---|---|
| `0x0000` | `CTRL` | RW/pulse | `ENABLE[0]`, `START[1]`, `SOFT_RESET[2]`, global `IRQ_EN[3]` |
| `0x0004` | `STATUS` | RO/W1C | `BUSY[0]`, sticky `DONE[1]`, sticky `ERROR[2]`, `IDLE[3]` |
| `0x0008` | `IRQ_ENABLE` | RW | Per-cause enable: `DONE[0]`, `ERROR[1]` |
| `0x000C` | `IRQ_STATUS` | RO/W1C | Sticky done/error causes |
| `0x0010` | `SRC_ADDR` | RW | Physical address of row-major `A[32][K]` |
| `0x0014` | `DST_ADDR` | RW | Physical address of row-major `C[32][32]` |
| `0x0018` | `SCRATCH_ADDR` | RW | Reserved by this implementation |
| `0x001C` | `SRC_SIZE_BYTES` | RW | Available source span, including row stride |
| `0x0020` | `DST_SIZE_BYTES` | RW | Must be at least 4096 |
| `0x0024` | `WIDTH` | RW | Must be 32 |
| `0x0028` | `HEIGHT` | RW | Must be 32 |
| `0x002C` | `SRC_STRIDE_BYTES` | RW | Bytes between A rows; 0 means `K` |
| `0x0030` | `FORMAT` | RW | Must be `1` (`INT8_INT8_INT32`) |
| `0x0034` | `OP_MODE` | RW | Must be `0` (`GEMM`) |
| `0x0038` | `WEIGHTS_ADDR` | RW | Physical address of row-major `B[K][32]` |
| `0x003C` | `PARAM_ADDR` | RW | Reserved by this implementation |
| `0x0040` | `WEIGHTS_SIZE_BYTES` | RW | Must be at least `K * 32` |
| `0x1000` | `K_DIMENSION` | RW | GEMM reduction dimension, `1..992` |
| `0x1004` | `ZERO_THRESHOLD_FP32` | RW | IEEE-754 binary32 bits passed to the core |
| `0x1008` | `ROWS_ACTIVE` | RW | SAURIA active-row mask |
| `0x100C` | `DILATION_PATTERN` | RW | SAURIA feeder dilation pattern |
| `0x1010` | `CYCLE_COUNT` | RO | Core run cycles for the last job |
| `0x1014` | `BYTES_READ` | RO | RAM bytes read for the last job |
| `0x1018` | `BYTES_WRITTEN` | RO | RAM bytes written for the last job |
| `0x101C` | `LAST_ERROR` | RO | Error code from `npu_tlm_v4_regmap.h` |
| `0x1020` | `CORE_ID` | RO | `0x53413432` (`"SA42"`) |

The complete C/C++ definitions are in:

- model ABI: `include/npu_tlm_v4_regmap.h`;
- firmware ABI: `fw/common/include/soc/regs/soc_regs_npu_v4.h`.

### Compatibility start and interrupt sequence

1. Program all buffer, size, dimension, format, and operation registers while
   `BUSY=0`.
2. Set the desired bits in `IRQ_ENABLE`.
3. Write `CTRL.ENABLE=1` and, for interrupts, `CTRL.IRQ_EN=1`.
4. Write the same value with `CTRL.START=1`.
5. Poll `STATUS`, or service PLIC source 17.
6. Clear the device `IRQ_STATUS` cause before completing the PLIC claim.
7. Clear sticky `STATUS.DONE` or `STATUS.ERROR` with W1C if required.

The IRQ equation is:

```text
irq_out = CTRL.IRQ_EN && ((IRQ_ENABLE & IRQ_STATUS) != 0)
```

## Address and size checks

The wrapper accepts DMA ranges only within the current full-SoC RAM window
`0x8000_0000..0x8FFF_FFFF`. `DST_ADDR` must be 4-byte aligned. A failed
validation or DMA transaction completes the command with `STATUS.ERROR`,
`IRQ_STATUS.ERROR`, and a diagnostic `LAST_ERROR`; it does not silently clip a
buffer.

## Build and verification

Select the host compiler before configuring or building:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head -n 1

cmake -S . -B build-soc \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_ENABLE_SAURIA_NPU_V4=ON \
  -DSAURIA_NPU_ROOT=/home/phuonglhm/FX/fx1_02/hw/tlm/MP1_V1.1/v4.2_model \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DCDC_BUILD_TESTS=ON
cmake --build build-soc -j"$(nproc)"
ctest --test-dir build-soc -R npu_tlm_v4_model --output-on-failure
```

The component test checks the numerical result, RAM-master traffic, sticky
IRQ/W1C behavior, native profile/SRAM access, rich alias readback, performance
counter access, invalid MMIO and DMA ranges, and external reset. The full-SoC
firmware test is in `fw/npu_v4_irq_riscv`.

## Known limitations

- The rich-instruction registers and OBP/RCE tables are exposed, but rich DMA
  is not connected to VP guest RAM and there is no agreed rich busy/done ABI.
- Native accesses do not use the compatibility IRQ/status registers. New
  runtime software must poll the native controller until a native interrupt
  contract is defined.
- The compatibility GEMM path remains fixed at `32 x 32`, supports
  `K = 1..992`, and returns INT32 output.
- Internal counters not declared in V4.2 `PerfCounters` have no VP MMIO
  addresses.
