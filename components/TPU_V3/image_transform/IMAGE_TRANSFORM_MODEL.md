# TPU_V3 Transform Model

This component implements the Phase 6, Revision 1 **Transform** block with an
Im2Col-only capability. It is a standalone SystemC/TLM IP intended to be
composed into each NEO-CORE in Phase 7. `image_transform` is the retained
implementation identifier; D20 names the architectural block Transform.

## Capability

| Item | Revision 1 |
| --- | --- |
| Im2Col | available |
| Col2Im | unavailable; `START` returns `unavailable_operation` |
| input | signed INT8, contiguous CHW |
| output | signed INT8, row-major `[OH*OW][C*KH*KW]` |
| window order | `c`, `ky`, `kx` |
| stride | independent non-zero height/width |
| dilation | independent non-zero height/width |
| padding | descriptor values must all be zero |
| external memory port | none |

For one output position `(oy, ox)` and one matrix column `(c, ky, kx)`, the
output byte is:

```text
matrix[oy * OW + ox][(c * KH + ky) * KW + kx]
    = input[c][oy * stride_h + ky * dilation_h]
              [ox * stride_w + kx * dilation_w]
```

This is cross-correlation order; the kernel axes are not reversed.

## Interfaces

`image_transform` exposes:

* one 32-bit AXI4-Lite-style TLM target named `control`;
* one `neo_local_sram_if` port named `local`;
* one level-sensitive completion/error IRQ.

The register ABI is in
`include/tpu_v3/transform/image_transform_registers.h`. The target accepts
only aligned 4-byte accesses with all bytes enabled. Reserved reads return
zero, writes to reserved/read-only registers are dropped, `transport_dbg` is
side-effect free, and DMI is never granted.

`START` snapshots one descriptor, sets `BUSY` and returns immediately. The
worker first reads the complete source tensor through the native port into
private staging, then emits one matrix row at a time through that same port.
Native requests are at most 64 bytes. This preserves arbitration, bounds,
response errors and traffic counters; neither the adapter nor its reference
helper touches core-SRAM backing directly.

The staging is a functional TLM policy, not a claim that RTL must buffer the
whole input or that the result is cycle-correlated with a hardware line buffer.

## Completion and reset

`DONE`, `ERROR` and `ABORTED` are sticky W1C status bits. The level IRQ is high
for `DONE` or `ERROR` when enabled, settles within one SystemC delta after a
state change, and uses a single writer process; abort does not raise it. A
second `START` while busy is refused and increments `OVERRUN_COUNT`.

Reset and abort advance the job generation, clear `BUSY` and abandon the old
worker. Bytes already committed to SRAM are not rolled back. A replacement
`START` owns `BYTES_DONE` as soon as it is accepted, so a late response from an
old generation cannot corrupt the new job's count. Reset also begins a new
traffic-counter epoch; hierarchical NEO-CORE reset must reset the engine and
local fabric together.

## Source provenance

The implementation is an extraction of semantics, not a wholesale copy of
`IfmapFeeder`. That feeder is coupled to Sauria FIFO/skew/control and has no
standalone Transform interface. Configuration verifies these v4.2 evidence
hashes before compiling:

| File | SHA-256 |
| --- | --- |
| `data_feeder/ifmap_feeder.h` | `156334177cfe1682faac049ce48745d9657ed4fcd5d58eb1c0bf5bffa3692ba0` |
| `driver/libsauria_mem.h` | `d5310c80b5e283a1ae133e9ba7b05d74c3559056f2a6ebd42f607391e328c982` |
| `driver/sauria_golden.h` | `57327c4c91c7dcc1fa2d95509fee6f83e60eea96bf80be68afbd589a67aaae3b` |

The default evidence root is
`components/npu_tlm/models/v4.2_model`; it may be overridden with
`TPU_V3_TRANSFORM_ROOT`, but the hashes still have to match.

## Why Col2Im is not required now

The forward pipeline is:

```text
DMA -> Transform (Im2Col) -> MXU -> RVV
```

The matrix-engine result already represents the output feature map in matrix
form. RVV/software can add bias/activation and interpret or reshape its rows.
Col2Im is required for scatter/overlap-add operations such as some backward or
transposed-convolution paths, which are outside Revision 1. A future Col2Im
implementation needs a separately pinned source and an explicit overlap and
accumulation contract; it must not be inferred from PSM result ordering.

## Build and gate

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head

cmake -S . -B build-tpu-v3-transform \
    -DCDC_BUILD_TPU_V3_IMAGE_TRANSFORM=ON \
    -DCDC_BUILD_TPU_V3_TESTS=ON \
    -DTPU_V3_TRANSFORM_ROOT=/path/to/v4.2_model
cmake --build build-tpu-v3-transform \
    --target test_im2col_contract test_image_transform -j"$(nproc)"
ctest --test-dir build-tpu-v3-transform \
    -R 'tpu_v3_(image_transform|transform_)' --output-on-failure
```

The gates cover pinned-source convolution equivalence, exact matrix order,
MMIO refusals, unavailable Col2Im, native-SRAM traffic, partial errors,
replacement start, abort/reset epochs, IRQ/W1C and dependency independence.
