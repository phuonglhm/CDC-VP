# ISP TLM Line-Granular Refactor

## Implemented

- Replaced pixel-typed internal FIFO links with bounded, slot-backed typed line channels.
- Added explicit token ownership, credits, ready queues, and event-driven stage scheduling.
- Converted the logical ISP stages to line-granular processing with stage-local storage for spatial kernels.
- Added separate issue, retire, input-wait, output-wait, and feedback timing metrics.
- Added AWB and AEC next-frame feedback barriers.
- Preserved configured WB base gains while applying prior-frame AWB feedback.
- Preserved DG gain index zero through the feedback latch.
- Added safe operation when AWB is enabled and WB is disabled.
- Added metadata-addressed sink placement for Scale and YUV420 plane output.
- Replaced the architecture sweep mock path with real pre-elaborated line-pipeline execution.
- Updated the main testbench to compare four consecutive frames, including DG feedback transitions through gain index zero.
- Added a `--wb-off` test mode to exercise AWB feedback with WB disabled.
- Made mirror padding safe for very small dimensions.

## Build

The configured SystemC environment is available through the project Docker image.
Run this from the repository root:

```bash
  cmake --build build/bremen \ --target tb_pipeline tb_line_primitives \
```

## Tests

Run the default four-frame oracle comparison:

```bash
  ./build/bremen/components/isp_tlm/systemc/tb_pipeline
```

Run the AWB-enabled and WB-disabled feedback path:

```bash
  ./build/bremen/components/isp_tlm/systemc/tb_pipeline --wb-off
```

Run the line-channel and feedback primitive contract test:

```bash
  ./build/bremen/components/isp_tlm/systemc/tb_line_primitives
```

Expected main pipeline results are 1536 oracle bytes, 1536 captured bytes, four 384-byte frames, zero differing bytes, zero MSE, and zero maximum error.

## Deferred

The edge-dimension, constrained-backpressure, and timing-invariant matrix was deferred.
Full coherent sweep-candidate binding and the broader real-fixture scheduler baseline were also not run in this verification pass.
