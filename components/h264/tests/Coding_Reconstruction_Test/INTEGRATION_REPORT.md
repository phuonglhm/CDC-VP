# Coding/reconstruction and full-pipeline integration report

Latest integration update: **165/165 tests PASS**, including 36 integrated
full_codec cases. Register-driven widths/crop/QP delta and active DF now have
header checks and independent decoder comparison. See
[CONFIG_DF_REPORT.md](CONFIG_DF_REPORT.md) for this pass and
[FULL_PIPELINE_CODEC_REPORT.md](FULL_PIPELINE_CODEC_REPORT.md) for the assembly.
The sections below retain historical evidence from the 129-test stage; their
counts and separate-only codec limitations are not the latest integration status.

Date: 2026-10-11. Reviewed baseline: a759c10.
Reference: SISLAB-H264-AVC-Hardware-Architecture-specification-v2.1.11.pdf.
Scope: SystemC 2.3.4 / TLM communication and selected behavior verification.

Update: a separate CAVLC/slice encoder now produces decoder-verified I/P streams.
See [CAVLC_SYNTAX_REPORT.md](CAVLC_SYNTAX_REPORT.md) for the exact scope. Existing
communication/behavior pipeline fixtures still select non-codec EC payloads.

## Result

129/129 CTest cases passed in build/vp: 54 existing cases, 22 communication
cases, 4 original Nguyen module smoke tests with assertions enabled, 1 controller
exception/drain case, 39 behavior cases, 4 prior algorithm cases and 5 codec cases. Original smoke-test PASS messages
are not evidence of algorithmic conformance. The production-only configuration
also built successfully during the communication stage.

## Current block completion and test status (2026-10-11)

This is a handoff checklist, not a percentage of spec coverage. PASS below means
that the named tests passed within their stated scope; no block is declared fully
PDF-conformant solely from the 129-case regression. The saved LastTest.log contains
129 passed entries and zero failed entries. This documentation update did not rerun
or modify the implementation/tests.

### Two distinct verification paths

1. **Integrated VP:** Huy control/top/reset + Vinh DMA/Intra/Inter + Nguyen TQ/EC/DF/
   memory are connected. EC explicitly outputs test records. Communication and
   selected I/P behavior pass; this path has no decoder-verified H264 output yet.
2. **Standalone codec:** real CAVLC, syntax, NAL and reconstruction produce
   decoder-verified I-P-P-P at 32x32, QP 0/26/51. This bench uses prediction fixtures,
   explicit chroma coefficient vectors and DF disabled; it does not use Vinh's
   prediction engines or Huy's control/DMA path.

### Blocks with passing evidence

| Block / owner | Passing communication evidence | Passing behavior / algorithm evidence | Still incomplete or not verified |
| --- | --- | --- | --- |
| Control/register / Huy, Ch.4 | Register transport, configuration, start, status, IRQ and errors. | Counts, completion, drain, reactivation and recovery in tested schedules. | Completion/error/reset with the real slice encoder; general codec configurations and release-driver sequence policies. |
| Top/integration / Huy, Ch.3 | Connected real modules, consumer acceptance and output/reference completion. | 16x16/32x32 pictures, multiple macroblocks, I/P, reference-slot wrap, delayed memory. | Connect the actual codec output path; general picture scheduling and reference lifetimes. |
| Reset/synchronization / Huy, Ch.13 | Epoch invalidation and reset propagation. | Selected stage-boundary and outstanding-DMA reset/restart scenarios. | Every relevant boundary in the new real codec assembly is not yet tested. |
| AXI arbiter/bridge / Vinh implementation, Ch.5 | Bus 32/64/128, responses, faults, mixed requests. | Arbitration, 4 KiB/burst limits, data ownership and finite contention workload. | Actual codec bitstream traffic and wider sustained-load coverage; no general starvation proof. |
| CMB/SW DMA / Vinh, Ch.6 | Source/reference requests and prediction handoff. | Tested fetches, reference readiness, fault/reset recovery. | Wider picture/reference scheduling in the integrated codec path. |
| NAL/REFM DMA / Vinh, Ch.6 | Accepted words, write responses and final completion. | Capacity, EOS, output ordering and three reference-slot wrap. | Carry actual Annex-B output through the integrated path and independently decode DDR contents. |
| Intra / Vinh, Ch.7 | Evaluate/Replay/Reconstruct and feedback handshakes. | Real prediction and reconstructed feedback in the integrated I-picture cases. | Codec decoder test currently uses a fixture; full-pipeline luma16x16 and broader mode coverage remain open. |
| Inter / Vinh, Ch.8 | SW/refill, committed predictor, Peek/Accept and stall behavior. | Integrated P zero-MV/single-reference prediction matches prior reference data. | Release policy, general partition/MVP/reference choices and B integration. Wider module-local capability does not imply integrated coverage. |
| FTQ/quantizer / Nguyen, Ch.9 | Residual/configuration/input readiness and result protocol. | 4x4 transform matrix oracle, quantizer vectors, bounds; real luma levels used in decoder-verified streams. | Complete forward chroma DC gathering/quantization and Intra16x16 DC; comprehensive arithmetic coverage. |
| ITQ/reconstruction / Nguyen, Ch.9 | Output validity and reconstructed-feedback route. | Inverse DC-only analytic cases, clipping, inverse chroma DC/restored DC; codec Y/U/V reconstruction agrees with FFmpeg for the exercised streams. | Intra16x16 DC and general block-class coverage. |
| EC legacy block interface / Nguyen, Ch.10 | Load/start/read/reset/error; variable-length test payload. | Payload and transport checks pass. | A single-block NAL is not a full coded picture. Real slice lifecycle/backpressure is not integrated here. |
| CAVLC / Nguyen, Ch.10.2 | Bounded function API and atomic destination-capacity rejection; no separate streaming socket claim. | 5,100 residual vectors, 16/15/4 coefficients, nC table classes, trailing signs/levels/zeros/runs; exercised independently by FFmpeg streams. | Full integrated metadata/context lifecycle and streaming backpressure. Test parser shares numeric tables; roundtrip alone is not independent table proof. |
| Syntax/slice/NAL / Nguyen, Ch.10.3-10.5 | Bounded slice API; bit/byte formatting and capacity errors. | SPS/PPS, I4x4/P16x16 syntax, CBP, CAVLC assembly, RBSP/escaping; three four-frame streams decoded exactly. | Crop and wider required release syntax, Intra16x16/B/other partitions, full output-drain integration. |
| DF / Nguyen, Ch.11 | Plane/offset/configuration transport, reset and filtered-output protocol. | tc0/chroma/luma vectors, signed offsets, chroma QP, I/P single-reference bS; active/bypass integrated tests. | Decoder comparison with DF enabled; B bi-prediction bS, full edge/QP combinations and comprehensive arithmetic proof. |
| Shared memory / Nguyen, Ch.12 | Multiple initiators, byte masks, address checks and DMA access. | Stored source/output/reference comparisons in tested fixtures. | Full local-memory inventory, ownership/collision contracts and all buffer lifetimes of the release architecture. |

### Remaining implementation and validation priorities

1. **Nguyen + Huy:** finish the forward chroma DC route and pass actual block classes,
   coefficients and prediction metadata into SliceEncoder. Do not substitute test
   coefficient vectors in the integrated source-image path.
2. **Huy integration:** connect complete Annex-B byte output to NAL DMA, preserving
   partial words, accepted-word counts, EOS, response-based completion and capacity
   handling. Keep stable pending data under downstream stalls/reset.
3. **Team integration:** decode bytes read back from DMA memory and compare all
   reconstructed/reference planes. Repeat with stalls, errors and reset; this joins
   the two currently separate verification paths.
4. **Nguyen:** complete Intra16x16 DC, the additional required syntax and DF behavior;
   compare against independent algorithm/decoder evidence.
5. **Vinh + integration:** qualify release Inter policies and extend P/B motion,
   partition, reference and scheduling coverage based on PDF requirements.
6. **Nguyen + integration:** review the Ch.12 internal-memory ownership and lifetime
   checklist rather than treating the shared DDR fixture as the entire architecture.

CABAC is not added to the remaining-work list for this PDF's CAVLC baseline.
RTL cycle timing, physical CDC and FPGA resources are outside this VP checklist.
Extra H264 features are not automatically requirements unless required by the PDF.

For the real codec API, artifacts, source provenance and file-by-file changes, see
[CAVLC_SYNTAX_REPORT.md](CAVLC_SYNTAX_REPORT.md). Earlier sections below retain the
detailed communication/algorithm history and their explicit scope boundaries.

## Assemblies and ownership

- coding_reconstruction_tb.cpp connects Huy's control/top to Nguyen's TQ, EC, DF
  and shared memory, with a constant-128 predictor fixture and Vinh's DMA.
- ../full_pipeline_communication_tb.cpp replaces that predictor fixture with
  Vinh's Intra and Inter: two 16x16 YUV420 pictures, I then P, DF bypass.
- ../full_pipeline_behavior_tb.cpp uses the same connected modules with 32x32
  pictures (four macroblocks each), I-P-P or I-P-P-P for reference-slot wrap.
- communication_pipeline.h assembles the modules through PipelineFactory and
  reuses the Prediction_Test request pump. communication_bench.h provides host,
  shared memory, delayed/faulting bus and scoreboards.
- dma_contention_tb.cpp independently exercises four concurrent tagged transport
  clients through the production DMA and shared memory.

These are test assemblies, not a released codec adapter. Existing
full_pipeline_tb stub/real selection remains intact. No Vinh production module
source was changed; only the prediction test fixture gained geometry parameters.
Nguyen's sources are linked directly as h264_coding_modules, not copied into tests.
The EC test payload is explicitly selected; its default entropy path is retained.

## PDF traceability and checked behavior

| PDF reference | Implemented checks | Boundary of evidence |
| --- | --- | --- |
| 3.4, 6.1 | CMB DMA snapshots equal host YUV420 pictures; raster macroblock traversal and unique block coordinates. | Tested 16x16 and 32x32 fixtures. |
| 3.4, 7, 9 | Intra prediction feeds TQ; reconstructed samples feed Intra feedback. 18 prediction/feedback operations and 24 TQ/EC blocks per I macroblock. | No independent transform or prediction golden model. |
| 3.4, 6.2, 8 | SW DMA fetches the completed prior reference; Inter zero-MV samples match its selected tile. Peek is stable until Accept. | One candidate, zero MV, one reference; no B pictures. |
| 3.5 | Stable TQ output during consumer delay; repeated START without new input rejected; next input follows output consumption. | Transaction-level acceptance, not every internal cycle. |
| 10, 6.3 | Fixed and variable EC test records, byte-to-word padding, output-memory comparison, EOS and capacity errors. | Test records are not H264 NAL/RBSP syntax. |
| 11 | Actual DfTop calls, vertical then horizontal edges, internal/external macroblock edges, bypass and active-filter paths. Active-filter vectors must change pixels. | Production bS helper for I / single-reference P; pipeline still uses zero-MV/same-reference P. Signed offsets and distinct chroma filtering are connected. |
| 6.4, 12 | Reference write acknowledgements precede reuse; four pictures wrap three slots. Last slot contents checked. | No general concurrent picture scheduler. |
| 4 | BUSY/status/IRQ, accepted output word count, live-configuration rejection, guards and absence of late DMA. | Tested scheduling and fault cases only. |
| 13 | Reset at stage boundaries and during outstanding NAL/reference/SW transactions; restart without stale work. | Not every internal module reset instant. |

Two successful activations use different input patterns to expose stale state.
Filter-toggle cases enable filtering in one activation and disable it in the next.
Reference and output checks use captured TQ data plus known host input. They verify
routing, ordering and storage consistency, not bit-exact TQ/DF correctness.

## Module/protocol changes

These local simulation operations do not introduce Chapter 4 AXI-Lite registers.

- TQ/EC/DF reset clears readiness/results; START consumes fresh residual, levels
  or current-block input. Persistent configuration is reusable until reset.
- Exact transaction lengths, valid commands, supported streaming and byte enables,
  QP <= 51 and bS <= 4 are checked before execution.
- TQ local byte address 0x09 selects intra=1/inter=0, with existing default intra.
- EcTop(name, true) selects fixed 36-byte records: ASCII TC, QP, entropy mode and
  sixteen signed little-endian 16-bit levels.
- EcTop(name, true, true) selects variable records: ASCII TV, QP, nonzero count,
  followed by index plus signed little-endian 16-bit value for each nonzero level.
  Raw length is 4 + 3 * nonzero_count bytes. The adapter zero-pads each record to
  a four-byte boundary, preflights capacity, and sends words through NAL DMA.
  This tests varying output volume without claiming valid entropy syntax.
- Reservoir/formatter overflow fails explicitly instead of silently truncating.
- Shared MemoryMap accepts multiple initiators and honors read/write byte masks;
  addresses remain fixture placement, not asserted PDF physical addresses.
- Controller catches executor std::exception and drains DMA before ERROR/IRQ.
- Negative signed left shifts in FTQ/ITQ/DF arithmetic were replaced with
  multiplication without changing the intended formulas.

Little-endian packing follows repository convention; it does not resolve the
PDF's endian TBD. Test mode does not validate the real codec path. The later codec pass below is validated separately.

## Test matrix

Communication: 11 cases per assembly (22 total). Bus widths 32/64/128 with extra
memory latency 0/50 ns; source, NAL and reference faults; active reset; deliberately
corrupted expected output must compare unequal. Memory adds its own latency.

Behavior: 39 cases:

- 12 normal/filter/wrap/filter-toggle cases across 32/64/128-bit buses.
- 3 filter-offset cases (alpha=4, beta=2) across the same bus widths.
- 18 capacity, live-configuration, delayed-completion, memory-fault, reset and
  injected-stage-error cases. TQ/EC/DF/EOS errors are adapter-injected exceptions;
  reset at a held checkpoint covers that boundary, not arbitrary internal cycles.
  The early_reference case holds after reference completion and checks that the
  activation has not completed prematurely; it does not force an illegal SW read.
  Capacity exhaustion verifies error/drain/reset, without a subsequent large-buffer
  retry. Other reset/error paths proceed to successful activations.
- 6 DMA contention cases across three bus widths and two added latencies. Four
  tagged clients issue 64 mixed-length requests, including 4 KiB crossings.
  Checks cover data ownership, completion, grant serialization and burst limits.
  This finite transport stress is not a general starvation proof or four codec
  engines running concurrently.

## Run from components/h264

Using the existing SystemC-configured build/vp cache:

    cmake --build build/vp -j 8
    ctest --test-dir build/vp -L communication --output-on-failure
    ctest --test-dir build/vp -L behavior --output-on-failure
    ctest --test-dir build/vp -L algorithm --output-on-failure
    ctest --test-dir build/vp --output-on-failure -j 8

Examples (BUS_WIDTH EXTRA_LATENCY_NS [SCENARIO]):

    build/vp/bin/coding_reconstruction_tb.exe 32 50
    build/vp/bin/full_pipeline_communication_tb.exe 128 50
    build/vp/bin/full_pipeline_behavior_tb.exe 32 50 filter
    build/vp/bin/full_pipeline_behavior_tb.exe 64 50 wrap
    build/vp/bin/full_pipeline_behavior_tb.exe 32 50 reset_sw

On this machine CMake/CTest are in
C:/Xilinx/Vivado/2024.2/tps/win64/cmake-3.24.2/bin.

## Remaining scope

The connected pipeline can now test communication and the selected behaviors
above. It is not fully PDF-conformant or decoder-conformant. The original communication result does not validate entropy syntax. The later codec
pass adds CAVLC and a constrained I/P syntax implementation; general syntax, motion
and reference choices, independent full arithmetic coverage, general dimensions
and a production released-pipeline adapter remain outstanding. CABAC is outside
the PDF CAVLC baseline. Active DF routing and pixel changes are tested, but
standard-exact luma/chroma filtering is not established.

## Algorithm improvement pass (2026-10-11)

This pass improves Nguyen's numerical/protocol implementation and the integration
adapter. It does not implement a complete encoder. No Vinh production source was
modified, no commit/push was performed, and all edits stay inside components/h264.
The estimates discussed previously are not measured coverage or acceptance criteria.

### Features and fixes

| Area / PDF reference | Before | After / evidence |
| --- | --- | --- |
| DF thresholds, 11.2 | Many tc0 entries were shifted or too large (e.g. indexA=51, bS=3 was 47). | Correct 52-row table; final entry is 25. Golden chroma vector checks clipping at this boundary. |
| DF chroma, 11.2 | Luma equations applied to every plane. | Explicit chroma path changes only p0/q0; weak tc=tc0+1, strong two-sample update. Pipeline applies nonlinear chroma QP mapping with zero chroma-QP offset. |
| DF offsets, 11.2 / Table 20-3 | Filter used QP alone; adapter rejected nonzero offsets. | Signed actual alpha/beta offsets clipped with QP into threshold indices. Full-pipeline tests pass offsets from DFCON through local TLM operations. |
| DF bS, 11.3 | Limited strength formula embedded in the fixture. | Shared production helper prioritizes intra, nonzero coefficients, reference mismatch and quarter-sample MV discontinuity. Covers progressive single-reference P; no B bi-prediction policy. |
| TQ arithmetic, 9.1 / 9.4 | Quantizer multiplied coefficients in int and silently narrowed results. | 64-bit magnitude/product, QP validation, explicit overflow rejection. FTQ transform checked against direct matrix multiplication on 128 seeded residual blocks. |
| ITQ, 9.3 / 9.4 | 32-bit intermediates and unchecked residual narrowing. | 64-bit intermediates and checked 16-bit residual range; analytic DC-only inverse vectors across all 52 QPs, clipping and extreme-input rejection. No special-DC algorithm completion is claimed. |
| Entropy primitives, 10.3 | Signed Exp-Golomb only. | Unsigned 32-bit ue(v), including UINT32_MAX, and existing signed 16-bit se(v) share the implementation. Independent parser roundtrips all 65,536 signed inputs plus unsigned boundaries. |
| Reservoir / RBSP, 10.4 | Byte alignment did not insert the RBSP stop bit. Oversized bit writes could partially mutate the buffer. | Stop-one bit followed by zero padding before escaping; length/capacity preflight for atomic field rejection. Every initial bit offset is tested, including an aligned payload requiring a new stop byte. |
| NAL, 10.4 | Header fields silently masked; word padding described as RBSP termination. | Invalid header fields rejected; exact escaping bytes and capacity-failure preservation checked. DMA padding is explicitly separate from RBSP termination. |

DF equations/tables were cross-checked with the upstream
[FFmpeg loop-filter implementation](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/h264_loopfilter.c)
in addition to the project PDF. This is an implementation cross-check, not proof
of bit-exact equivalence to the SISLAB release. No third-party implementation was
added to the repository.

### Local DF interface additions

These are simulation addresses, not new encoder AXI-Lite registers:

| Address | Payload | Meaning |
| --- | --- | --- |
| 0x2C | one byte, 0/1 | Luma/chroma filter selection. |
| 0x30 | one signed two's-complement byte, -16..15 | Actual alpha offset (not divided by two). |
| 0x34 | one signed two's-complement byte, -16..15 | Actual beta offset. |

Reset restores luma selection and zero offsets. Invalid writes preserve the old
configuration. Existing QP input remains the effective edge QP; callers own
neighbor-QP averaging. The current pipeline uses a common picture QP, so averaging
is identical on both sides. Chroma mapping currently assumes zero chroma QP offset.
The bS helper accepts one reference identity per side and MVs in quarter-luma units.

### Files changed in this pass

Paths below are relative to components/h264. Earlier communication-stage changes
remain in the working tree but are not counted as new algorithm work here.

| Files | Change |
| --- | --- |
| tq/src/ftq.cpp | Safe quantization arithmetic and range/QP validation. |
| tq/src/itq.cpp | Wide inverse arithmetic, QP and residual-range validation. |
| tq/src/tq_top.cpp | Translate numerical exceptions into invalid result and TLM error. |
| df/include/df_filter.h; df/src/df_filter.cpp | Correct tc0, distinct chroma filtering, offsets, bS helper and chroma-QP mapping. |
| df/include/df_types.h; df/src/df_top.cpp | Store and transport plane/offset settings; reset defaults. |
| ec/include/ec_types.h | Atomic bit-field capacity checks and RBSP termination. |
| ec/include/exp_golomb.h; ec/src/exp_golomb.cpp | Unsigned Exp-Golomb API and boundary-safe coding. |
| ec/src/ec_top.cpp | Terminate RBSP before NAL formatting on the existing non-test path. |
| ec/src/nal_formatter.cpp | Header validation and padding clarification. |
| tests/Coding_Reconstruction_Test/communication_pipeline.h | Connect production DF helpers and actual register offsets. |
| tests/Coding_Reconstruction_Test/communication_bench.h | Filter-offset full-pipeline scenario. |
| tests/Coding_Reconstruction_Test/algorithm_tb.cpp (new) | Matrix/analytic/byte-vector tests, independent Exp-Golomb parser and DF TLM/reset tests. |
| tests/Coding_Reconstruction_Test/CMakeLists.txt | Four algorithm cases and three filter-offset cases. |
| tests/Coding_Reconstruction_Test/INTEGRATION_REPORT.md | Updated results, changes, interface and limitations. |

### Validation and remaining work

Seven CTest cases were added: algorithm_tq, algorithm_df, algorithm_ec,
algorithm_protocol, and behavior_filter_offsets_32/64/128. Assertions in the new
bench throw on failure even in Release builds. This earlier pass brought the suite to 124 cases; the subsequent codec pass brings it to 129.
These tests use fixed expected values, a direct transform matrix, analytic inverse
DC values and an independent Exp-Golomb parser; they do not merely compare a module
with another call to itself. The full-pipeline scoreboard still uses captured module
outputs and is not an independent end-to-end codec oracle.

At the end of this earlier algorithm pass, outstanding work included gathered
Intra16x16/chroma DC, CAVLC and syntax. The codec pass now adds inverse chroma DC,
CAVLC and constrained I/P syntax (see the linked report). General B-picture bS, independent
full TQ/DF conformance across all inputs, and a complete production pipeline adapter.
Memory architecture was not changed in this pass. The full pipeline continues to
select explicit non-codec EC payloads; improving RBSP/Exp-Golomb primitives does not
by itself make the legacy single-block EC transaction a complete coded picture.
Use the new SliceEncoder for the validated codec subset.
