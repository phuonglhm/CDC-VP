# Prediction integration tests

These test-only assemblies connect Vinh's real DMA clients and prediction modules
to Vinh's H264Arb/AxiMasterBridge through DmaTransport and Huy's control/reset. No algorithm module is
modified. TQ/ITQ, entropy and deblocking arithmetic are NOT included.

## Run in this order

From components/h264 after configuring the existing SystemC 2.3.4 build:

```sh
cmake -S . -B build/vp -DH264_BUILD_TESTS=ON
cmake --build build/vp -j 8
ctest --test-dir build/vp -R "^h264_prediction_dma_clients_" --output-on-failure
ctest --test-dir build/vp -R "^h264_prediction_cmb_intra_" --output-on-failure
ctest --test-dir build/vp -R "^h264_prediction_sw_inter_" --output-on-failure
ctest --test-dir build/vp -R "^h264_prediction_control_" --output-on-failure
```

All stages: `ctest --test-dir build/vp -L prediction --output-on-failure`.
There are 15 cases: four stages at 32/64/128 bits plus three control read-fault cases.
Each executable exits nonzero on a failed check or simulation watchdog timeout;
checks remain enabled in Release. CTest also has a wall-clock timeout.

| File | Coverage |
|---|---|
| dma_clients_tb.cpp | CMB/SW YUV reads, DF reference write, NAL write, delayed completion, byte lane guards, read fault/retry |
| cmb_intra_tb.cpp | DMA-fed 16 luma4 + two chroma8 blocks; initial DC oracle, residual relationship, repeated replay, decoded feedback completion |
| sw_inter_tb.cpp | SW-backed reference, exact-match MV/residual oracle, 384 YUV samples, metadata, held Peek and final Accept |
| control_prediction_tb.cpp | CSR START, both prediction paths as a scripted workload, reference/output drain, STATUS/LEN/IRQ, failed input DMA |
| prediction_fixture.h | Shared test driver, queue adapter, explicit backend fixtures and watchdog |

## Adapter boundary

Vinh's clients currently require H264Arb. The fixture retains it as a legacy
request queue and forwards its selected request through a client-specific TLM
initiator into the production DmaTransport backed by Vinh's arbiter/bridge. It never uses the standalone
MMIO wrapper. Direct MemoryIf access throws, preventing accidental DMA bypass.
Requests are serviced serially; these tests do not prove simultaneous-client
fairness. Production priority follows Vinh's core; the legacy aging test is isolated.

CMake imports Intra/Inter libraries with their independent suites disabled in
this scope, and uses DMA client headers without importing a second
h264_dma target. Standalone module test configurations remain unchanged.

## Deliberate limits

The workload is one 16x16 picture with a patterned YUV source/reference. Intra
feedback is an identity reconstruction fixture. DF is bypassed with explicit
fixture pixels. The NAL client writes the tagged bytes 54 45 53 74, not an H264
stream. Stage 4 exercises both predictors; it does not implement encoder mode
selection or a production I/P/B schedule.

These tests check idle reset, CMB read error propagation/retry, and successful
response ordering. Mid-flight prediction reset, full-frame traversal, dual-list
B prediction, fractional oracle coverage, NAL/DF write faults and codec golden
qualification remain covered elsewhere or require later integration work.
Passing this suite is not full-pipeline codec signoff.
