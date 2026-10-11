# CAVLC and syntax implementation / validation

Latest integration update: **165/165 tests PASS**, including 36 integrated
full_codec cases. Register-driven widths/crop/QP delta and active DF now have
header checks and independent decoder comparison. See
[CONFIG_DF_REPORT.md](CONFIG_DF_REPORT.md) for this pass and
[FULL_PIPELINE_CODEC_REPORT.md](FULL_PIPELINE_CODEC_REPORT.md) for the assembly.
The sections below retain historical evidence from the 129-test stage; their
counts and separate-only codec limitations are not the latest integration status.

Date: 2026-10-11. Reference: SISLAB H.264/AVC Hardware Architecture Specification
v2.1.11, sections 9.1-9.3 and 10.1-10.5. This report supersedes earlier statements
that the repository has no actual CAVLC or usable SPS/PPS/slice builder.

## Result and exact scope

129/129 CTest cases passed, including five new codec cases. The standalone codec
bench produces 32x32 YUV420 I-P-P-P streams at QP 0, 26 and 51. FFmpeg decodes each
without errors; every Y/U/V byte matches the model's reconstruction (6,144 bytes
per four-frame sequence). This is decoder evidence for these streams, not full
H264/SISLAB conformance or all supported input combinations.

The new codec bench does not use Vinh's prediction engines, Huy's controller or
DMA. Its prediction fixture uses Intra4x4 DC/vertical/horizontal and P16x16 zero-MV
from the preceding reconstruction. Luma levels come from Nguyen's FTQ/quantizer;
chroma DC/AC levels are explicit test vectors, not a completed forward chroma
quantization pipeline. Nguyen's ITQ reconstructs both, including restored chroma DC.
This distinction matters: the existing full_pipeline_communication_tb and
full_pipeline_behavior_tb still intentionally use non-codec EC records.

## PDF mapping

| Requirement | Implementation / evidence | Remaining boundary |
| --- | --- | --- |
| 10.2 coefficient scan and block class | Frame zigzag conversion; encode_block accepts scan order with 16/15/4 coefficients. | 8-bit progressive 4:2:0; no 4:2:2/8x8-transform support. |
| 10.2 TotalCoeff, TrailingOnes, nC, coeff_token | Correct VLC selection and counts. Per-plane neighbouring contexts in SliceEncoder. | Context availability assumes one complete raster-coded picture per slice. |
| 10.2 levels and runs | Trailing signs, adaptive suffixLength, escape prefixes, total_zeros and run_before. | Tested across 5,100 seeded vectors, including signed-16 extremes; this is not exhaustive residual-vector coverage. |
| 10.2 commit and reset | Block bits staged privately; destination capacity checked before commit. Slice contexts are fresh per call. | Whole-slice transaction model, not an incremental downstream ready/valid interface. |
| 10.3 SPS/PPS | Baseline profile, level 4.0, frame-only, POC type 0, one reference, CAVLC, initial QP and deblocking-control presence. | No crop/VUI/FMO; level/profile fixed for this subset. |
| 10.3 slice and macroblock | IDR I or non-IDR I/P headers, frame_num/POC, I4x4 mode coding, chroma mode, P16x16 MVD, CBP and residuals. | No skipped MB optimization, B pictures, Intra16x16 or smaller inter partitions; constant QP within each slice. |
| 10.4 RBSP/NAL | Trailing bits, shared NAL formatter, emulation prevention and Annex-B prefix. | Raw slice API returns bytes without DMA padding; final word packing remains adapter responsibility. |
| 10.5 buffering | Explicit bounded slice buffer; capacity failure returns no partial picture. | Streaming reservoir backpressure and replacing the existing DMA test payload remain integration work. |
| 9.3 chroma reconstruction | Inverse 2x2 chroma Hadamard/scaling and optional restored DC input to the inverse 4x4 transform. | No completed forward DC gathering/quantization or Intra16x16 DC path. |

The validated decoder cases use DF disabled, frame_num 0..3, POC 0/2/4/6, one
reference and zero MV. Syntax supports other explicitly bounded fields, but decoder
coverage must be extended before claiming their correctness. The sequence API
validates coded dimensions and bit widths; it does not validate GOP/reference
lifetime across separate calls. Callers own that scheduling.

## API and compatibility

- Cavlc::encode_block(scan, max_coeff, nc, writer): returns TotalCoeff. max_coeff
  is 16, 15 (AC-only) or 4 (chroma DC); nC=-1 is required for chroma DC. It rejects
  invalid contexts, nonzero values beyond the scan length and insufficient capacity.
- Cavlc::encode(EcRequest, writer): converts natural coefficient order to frame
  scan order. EcRequest gains nc and max_coeff with compatible defaults.
- SliceEncoder::parameter_sets(sequence): Annex-B SPS and PPS.
- SliceEncoder::picture(sequence, slice, macroblocks, capacity): complete slice NAL.
  Macroblocks are raster ordered; luma blocks use AVC block scan. Chroma AC arrays
  exclude DC; separate chroma_dc arrays carry transformed/quantized 2x2 levels.
- BitWriter also accepts a pre-sized vector. The caller must keep storage alive
  and must not resize it during writing. Default picture capacity is 1 MiB.
- NalFormatter::format_rbsp returns unpadded Annex-B bytes; wrap_nal_unit retains
  the legacy fixed-buffer/word-padding API.
- Itq::inverse_transform optionally takes an already scaled DC coefficient;
  inverse_chroma_dc produces the four restored DC values for a chroma macroblock.

The original EcTop single-block transaction now uses real CAVLC and a real header
builder, but one coefficient block is not an entire picture. Its original test is
only a transport/framing smoke test. CABAC remains a legacy path outside this PDF's
CAVLC target; it is not validated as standards-compliant.

## New tests

| Test | Evidence |
| --- | --- |
| codec_cavlc | 5,100 vectors across 4/15/16 coefficients and all nC table classes, known +1 bit pattern, invalid-context and capacity atomicity. |
| codec_syntax | Independent SPS/PPS field parser, deterministic fresh slice contexts, invalid geometry and capacity rejection. |
| codec_decode_qp0 / qp26 / qp51 | External FFmpeg decoder, fatal errors enabled, exact reconstruction comparison for four 32x32 YUV420 frames. |

The test CAVLC parser shares numeric VLC tables with the encoder, so roundtrip
alone cannot prove table correctness. FFmpeg decoding is independent of those
compiled tables and validates the exercised stream combinations. Decoder tests
need Python3 and FFmpeg. CMake reports a warning and omits those three cases if
either dependency is missing; the resulting lower test count is not equivalent
to this machine's 129-case validation.

From components/h264 with the configured build:

    cmake --build build/vp -j 8
    ctest --test-dir build/vp -R "^codec_" --output-on-failure
    ctest --test-dir build/vp --output-on-failure -j 8

Use the anchored regex above: the label regex `codec` also matches `non-codec`.
Artifacts are under build/vp/tests/Coding_Reconstruction_Test/codec_output:
codec_qp0/26/51.h264, corresponding .yuv reconstruction and .decoded.yuv output.
Generated artifacts and downloaded reference sources remain under build.

## Files changed in this pass

| File(s) | Purpose |
| --- | --- |
| ec/include/cavlc.h; ec/src/cavlc.cpp | Real residual CAVLC, scan conversion and atomic block encoding. |
| ec/include/cavlc_tables.h (new) | Numeric VLC lookup tables with upstream attribution. |
| ec/third_party/NOTICE.md; COPYING.LGPLv2.1 (new) | Provenance and license of the extracted table data. |
| ec/include/ec_types.h | Bounded vector bit-buffer support; block/context metadata. |
| ec/include/syntax_builder.h; ec/src/syntax_builder.cpp | Explicit sequence/slice/MB configuration and syntax builders. |
| ec/include/slice_encoder.h; ec/src/slice_encoder.cpp (new) | Stateful-within-call frame slice assembly, neighbour contexts and NAL output. |
| ec/include/nal_formatter.h; ec/src/nal_formatter.cpp | Shared bounded Annex-B formatter for block and slice interfaces. |
| tq/include/itq.h; tq/src/itq.cpp | Inverse chroma DC and restored-DC injection. |
| ec/test/CMakeLists.txt | Include slice builder in standalone module build. |
| ec/test/test_ec.cpp | Correct smoke-test wording; no standalone conformance claim. |
| tests/Coding_Reconstruction_Test/codec_tb.cpp (new) | CAVLC/syntax tests and deterministic I/P stream generation. |
| tests/Coding_Reconstruction_Test/decode_check.py (new) | FFmpeg invocation and byte-for-byte reconstruction comparison. |
| tests/Coding_Reconstruction_Test/CMakeLists.txt | Register new codec tests and optional decoder dependencies. |
| tests/Coding_Reconstruction_Test/INTEGRATION_REPORT.md; CAVLC_SYNTAX_REPORT.md | Current results and scope. |

Table source: [FFmpeg n7.1 h264_cavlc.c](https://github.com/FFmpeg/FFmpeg/blob/n7.1/libavcodec/h264_cavlc.c).
The extracted table file retains LGPL-2.1-or-later attribution and the license copy.
No upstream decoder implementation was copied. Existing Vinh production modules
were not modified. No git commit or push was performed.
