# Integrated codec pipeline verification

Latest result: **165/165 CTest cases PASS**, including 36 full_codec cases.
See [CONFIG_DF_REPORT.md](CONFIG_DF_REPORT.md) for the latest register/DF changes.
This supersedes the earlier status that decoder evidence exists only for a
standalone codec fixture. The older communication/behavior executables deliberately
retain their non-codec payloads for protocol testing.

## Connected path and PDF traceability

The new full_pipeline_codec_tb connects Huy's EncoderVp/control/reset, Vinh's
CMB/SW/NAL/REFM DMA and Intra/Inter engines, and Nguyen's transform, quantization,
inverse reconstruction, CAVLC/syntax/NAL and shared MemoryMap.

- PDF 3.4-3.5: source is fetched by actual CMB DMA. Real Intra prediction receives
  actual reconstructed feedback. P prediction uses actual SW DMA/Inter and the
  preceding completed reference. No synthetic coefficient or prediction vectors
  replace source-derived data in this new test.
- PDF 9.1-9.3: luma uses TqTop's TLM protocol. Chroma gathers four forward 4x4 DC
  coefficients, performs 2x2 Hadamard/quantization, and restores inverse DC before
  reconstruction. Chroma calls Nguyen's arithmetic classes directly within the
  SystemC adapter; it does not use the legacy single-block TqTop DC protocol.
- PDF 10.2-10.5: actual prediction modes and coefficients feed SliceEncoder.
  Annex-B bytes are staged into 32-bit words across NAL boundaries, sent to real
  NAL DMA and read back through the host from shared memory. Only the final byte
  tail is padded before the EOS word. No TC/TV test records are emitted.
- PDF 4/6/13: checks cover accepted word count, status/frame count, response-based
  IRQ, quiescence, no late DMA, guards, reset during NAL transfer and recovery from
  a NAL write fault. Decoder comparison uses host-read DDR output, not the local
  encoder output vector.

## Test matrix and assertions

36 cases: DMA widths 32/64/128 times QP 0/26/51 (9), plus nine scenarios
at QP 26 for each width (27): NAL fault, reset, invalid width, invalid DF offset,
positive/negative QP delta with crop and wider syntax fields, and active DF with
zero/positive/negative offsets. Memory adds 50 ns extra latency plus jitter.
Every case completes two successful activations with different source patterns;
each activation contains four 32x32 YUV420 I-P-P-P pictures. Each second activation
is preceded by reset. Fault/reset cases recover before those two activations.

For each successful activation the test checks 72 real Intra feedback operations,
4,608 real Inter accepted samples, 32 chroma groups and 384 coefficient blocks.
Source snapshots match host input; the last three reference pictures match their
DDR slots after wrap. Output matches accepted-word count and stream assembly;
FFmpeg independently decodes the DDR bytes with error detection enabled and all
6,144 Y/U/V bytes match model reconstruction (5,376 displayed bytes for cropped
cases). SPS/slice headers are also checked. Every decoder stderr error fails.

The forward chroma-DC helper additionally has a fixed analytic Hadamard/quantizer/
inverse vector and invalid-QP check in algorithm_tq. Decoder agreement demonstrates
encoder/decoder reconstruction consistency; it does not prove the chosen forward
quantizer is bit-identical to SISLAB RTL or optimal in quality/rate.

## Run

From components/h264 with the existing configured SystemC build:

    cmake --build build/vp -j 8
    ctest --test-dir build/vp -R "^full_codec_" --output-on-failure -j 6
    ctest --test-dir build/vp --output-on-failure -j 8

Python3 and FFmpeg must be discovered by CMake. Missing decoder dependencies mean
the 36 decoder cases are not registered, not that decoder validation passed.
Artifacts are under build/vp/tests/Coding_Reconstruction_Test/full_codec_output:
full_<width>_<qp>_<scenario>_<activation>.h264, .yuv and .decoded.yuv.

## Files introduced or changed in the initial integration pass

The latest incremental changes are listed in CONFIG_DF_REPORT.md.

- tq/include/ftq.h; tq/src/ftq.cpp: forward gathered chroma DC helper.
- tests/Coding_Reconstruction_Test/communication_pipeline.h: overridable coding/
  prediction hooks; existing payload behavior retained.
- tests/Coding_Reconstruction_Test/codec_pipeline.h (new): real codec adapter and
  partial-word assembly. Intra chroma DC selected consistently for U and V using
  the real engine's penalty input; luma mode decision remains the real engine's.
- tests/full_pipeline_codec_tb.cpp (new): control/DMA/memory assembly, assertions,
  faults/reset and export of DDR bitstream/reconstructed reference artifacts.
- tests/Coding_Reconstruction_Test/full_pipeline_decode.py (new): decoder comparison.
- tests/Coding_Reconstruction_Test/algorithm_tb.cpp: analytic chroma DC vector.
- tests/Coding_Reconstruction_Test/CMakeLists.txt: executable and initial 15 cases (now expanded to 36).
- Integration/codec reports: current status and limitations.

## Explicit limits

This completes an end-to-end decoder-verified **test assembly for the I/P subset**,
not every encoder feature in the PDF. Tested geometry is 32x32; one L0 reference,
P16x16 zero MV, I4x4 with common chroma DC mode, QP constant in each activation,
DF bypass or enabled, bottom crop 0 or 2, one slice per picture, frame_num/POC
widths 4/4 or 6/7, no B pictures. New DF/crop/delta cases use initial QP 26.
The adapter is deliberately test-scoped, with bounded whole-picture SliceEncoder
calls and direct chroma arithmetic calls; it is not registered as the general
released production pipeline factory. EcTop's legacy single-block socket is not
used for slice assembly. The old full_pipeline_tb stub/real selection is unchanged.

DF enabled now has decoder comparison and an assertion that filtering changes
pixels. Even actual offsets in [-12, 12] are accepted by this test adapter; odd
or larger offsets remain a release-policy alignment gap. Intra16x16, broader
motion/reference/scheduling policy, slice streaming backpressure and arbitrary
geometry remain future work. Do not label this full-PDF conformance.
No Vinh production source was modified. No commit or push was performed.
