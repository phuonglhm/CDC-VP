# TPU_V3 Phase 6 Audit — Transform (Im2Col Capability)

> **Active evidence under D27, not the current work queue.** This audit proves
> the Im2Col-only Transform reused by the one-core DSE. Col2Im remains absent.
> The next task is G2/MB1 in
> [NEO_CORE_MICROBENCH_DSE_PLAN.md](NEO_CORE_MICROBENCH_DSE_PLAN.md).

Date: 2026-08-18

Result: **complete for the approved Im2Col-only Revision 1 scope**.

Authority: decisions D18 and D20. **Transform** is the architectural block;
Im2Col is its implemented Revision 1 operation. Col2Im is an unavailable
operation, not an unfinished placeholder inside the completed scope.

## 1. Source audit and pin

The audited NPU-team tree is:

```text
/home/duyptt_HW/Documents/work/fx1/hw/tlm/MP1_V1.1/v4.2_model
```

The project-bundled copy used by the default build has the same evidence
hashes. The tree contains `data_feeder/ifmap_feeder.h` and
`data_feeder/wei_feeder.h`; no file, type or operation identified as Col2Im was
found. `IfmapFeeder` is not a reusable standalone transform: it includes FIFO,
skew, controller and fixed SRAM-timing behavior required by the Sauria array.

Phase 6 therefore extracts only the address/layout semantics established by:

| Evidence | SHA-256 | Finding |
| --- | --- | --- |
| `data_feeder/ifmap_feeder.h` | `156334177cfe1682faac049ce48745d9657ed4fcd5d58eb1c0bf5bffa3692ba0` | Im2Col-oriented IFMAP address generation, stride and dilation |
| `driver/libsauria_mem.h` | `d5310c80b5e283a1ae133e9ba7b05d74c3559056f2a6ebd42f607391e328c982` | input is contiguous `[C_in][A_h][A_w]` |
| `driver/sauria_golden.h` | `57327c4c91c7dcc1fa2d95509fee6f83e60eea96bf80be68afbd589a67aaae3b` | `A[c][oy*s+kh*d][ox*s+kw*d]`, cross-correlation |

CMake recomputes all three SHA-256 values and refuses a missing or modified
source. The generated provenance header records the complete hashes and
exports source tag `0x15633417` through MMIO.

One source comment refers to `addr_gen_rtl.py`, but that file is not present in
the audited MP1_V1.1 tree. It is not claimed as evidence. The accepted golden
is the compiled `sauria_golden.h` convolution reference above.

## 2. Frozen semantic subset

The implemented descriptor is:

```text
operation       Im2Col
input           signed INT8, contiguous CHW
output          signed INT8, row-major [OH*OW][C*KH*KW]
column order    c, ky, kx
stride          non-zero H/W
dilation        non-zero H/W
padding         all four values zero
```

Non-zero padding is refused because the pinned source/golden exposes no
padding field. Col2Im is refused because no implementation or overlap rule was
found. Neither case is silently emulated.

This omission does not block forward inference. `Transform (Im2Col) -> MXU` produces the
output-feature matrix, and RVV/software can perform post-processing and layout
interpretation. Backward/transposed-convolution scatter and overlap-add are not
part of Revision 1.

## 3. Component boundary

`components/TPU_V3/image_transform` builds
`cdc::components::tpu_v3_image_transform` with:

* a 32-bit AXI4-Lite target;
* one native local-SRAM requester identified as `transform`;
* an asynchronous worker and level completion/error IRQ;
* buffered input staging and row-wise output writes;
* no external master, no Sauria/NPU-top link, no DMA dependency and no SRAM
  backing pointer.

The dependency gate strips comments and scans source, archive symbols and the
link interface. It proves that the component depends only on the native-port,
common and SystemC targets.

## 4. Verification evidence

The Phase 6 tests are:

| Test | Evidence |
| --- | --- |
| `tpu_v3_transform_im2col_contract` | exact small matrix order; stride/dilation cases; the pinned NPU convolution golden factored through extracted Im2Col plus independent integer GEMM; datatype/padding/Col2Im refusals; source tag |
| `tpu_v3_image_transform` | actual core SRAM and native fabric; register protocol; asynchronous start; output and byte/request counters; IRQ/W1C; unavailable Col2Im with zero traffic; capacity error; abort/replacement start; active hierarchical reset and epoch ownership |
| `tpu_v3_transform_independence` | no external master, DMA, NPU top, Sauria or SRAM implementation dependency |

Review found one IRQ timing mismatch: the Transform implementation used a zero-time delayed
event, adding a second delta beyond the DMA/MXU convention, while its test also
sampled one ABORT transition without any delta. `update_irq()` now immediately
wakes the sole writer method, so the `sc_signal` value is visible after one
delta. The gate enforces that bound and checks deassertion after every W1C,
preventing stale IRQ state from satisfying a later assertion.

Measured result with GCC/G++ 11.5.0 and SystemC 2.3.4:

```text
tpu_v3_image_transform              PASS
tpu_v3_transform_im2col_contract    PASS
tpu_v3_transform_independence       PASS
3/3, 0 failed
```

The component was also configured with `CDC_BUILD_TPU_V3_SOC=OFF`, repository
tests OFF and only `CDC_BUILD_TPU_V3_IMAGE_TRANSFORM=ON`; the same 3/3 gate
passed. This exposed and fixed a top-level integration defect: the original
`components/CMakeLists.txt` added the TPU_V3 tree only for the SoC switch, so a
standalone transform request was silently ignored and its source variable was
reported unused. Either extracted-accelerator option now adds the reusable
component tree and enables its tests independently of the platform.

Closure controls:

* Transform OFF plus a nonexistent `TPU_V3_TRANSFORM_ROOT` configures and
  builds `tpu_v3_soc`; the default/off build does not inspect the source.
* Appending one comment byte to a temporary copy of `ifmap_feeder.h` makes
  configuration fail with both expected and actual SHA-256 values.
* The standalone gate passes 3/3 in both Release and Debug. The complete
  Release `tpu_v3` label passes **38/38 with zero skips** after a full build,
  including all 14 Phase 5 Sauria gates, DMA, fabrics, CPU, firmware and
  packaging.

Final review closure (2026-08-18): the descriptor busy-gate was simplified to
the actual contiguous register range after proving that its sixteen explicit
exclusions were all outside that range and therefore dead. Documentation and
runtime reporting now consistently say **no padding; all four padding fields
must be zero**, rather than using the ambiguous phrase "zero-padding". The
Transform gate remains 3/3 after this cleanup, and the reviewer accepted
Phase 6 as closed.

## 5. Phase 7 handoff

Phase 6 delivers a component but does not compose it into the current platform.
Until Phase 7 links it, the platform manifest must continue to say that the
transform is not linked/selectable. Phase 7 shall enable the exact pinned
Im2Col capability and use:

```text
DMA -> Transform (Im2Col) -> MXU64 -> RVV
```

It shall keep the Col2Im capability zero and shall not add a placeholder step.
