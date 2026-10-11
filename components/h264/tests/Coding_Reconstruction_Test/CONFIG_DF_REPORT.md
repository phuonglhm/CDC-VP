# Register-to-syntax and active deblocking verification

Date: 2026-10-11. Scope: SystemC 2.3.4 / TLM VP.
Reference: SISLAB-H264-AVC-Hardware-Architecture-specification-v2.1.11.pdf.

## Result

**165/165 CTest cases PASS**, including **36 integrated full_codec cases**.
This pass adds 21 cases to the preceding 144-case regression. The final
build/vp/Testing/Temporary/LastTest.log records 165 passed tests and zero failures.
LastTestsFailed.log can retain failures from an earlier run and is not the final
regression result. No Vinh production sources were changed; no commit or push.

## Implemented behavior and PDF mapping

| Area | Change and evidence | PDF reference |
| --- | --- | --- |
| Syntax widths | SPARA1 LOG2_FN/LOG2_POC reach SPS and slice headers; tests parse widths 6/7 as well as default 4/4 | Chapter 4, Table 20-3; Chapter 10 |
| Bottom crop | SPARA2 crop reaches SPS; coded/reference storage stays 32x32 while crop=2 produces a 32x28 decoded display image | Table 20-3; Chapter 10 |
| QP delta | Signed SPARA2 delta reaches slice syntax and effective TQ/DF QP; tests cover +3 and -3 at initial QP 26 | Table 20-3; Chapters 9-11 |
| Active DF | DFCON enable and signed offsets reach both filtering and slice headers; decoder pictures match filtered reconstruction | Chapters 10-11; Table 20-3 |
| Chroma boundary strength | Codec adapter uses corresponding luma coefficient flags; chroma samples select the luma edge strength per two samples | Chapter 11 |
| Invalid configuration | Width 3 and odd DF alpha offset 3 cause ERROR; reset and two subsequent valid activations succeed | Chapters 4/13; adapter restriction below |

Tests require active DF to change pixels. A smooth staircase source exercises
filtering, including zero offsets, alpha/beta +4/+2 and -4/-2. Slice syntax carries
half those offsets. When strengths differ along one chroma edge, the integration
adapter applies DfTop using the appropriate strength for each output row.

The Python checker reads SPS and all four slice headers independently of the
encoder to verify widths, crop, frame_num/POC, QP delta and DF fields. It then runs
FFmpeg on the actual bitstream read back from DDR and compares every displayed
Y/U/V byte against model reconstruction. Parser support is limited to the subset
emitted by this bench; it is not a general H.264 parser.

## Matrix

Every scenario runs at DMA bus widths 32, 64 and 128 bits:

- Normal operation at QP 0, 26 and 51: 9 cases.
- NAL write fault and reset during NAL transfer at QP 26: 6 cases.
- Invalid width and invalid DF offset recovery: 6 cases.
- Widths 6/7, bottom crop 2, QP delta +3 or -3: 6 cases.
- Active DF at zero, positive and negative offsets: 9 cases.

Total: 36 integrated codec cases. Each performs two successful activations, each
with four 32x32 YUV420 I-P-P-P pictures. Uncropped comparisons cover 6,144 bytes per
activation; cropped comparisons cover 5,376 bytes. DMA/status/IRQ, reference DDR,
output counts and guard checks remain enabled. The final full regression also
covers the existing communication, behavior, prediction and algorithm tests.

## Files changed in this development pass

- ec/include/syntax_builder.h: sequence bottom-crop configuration.
- ec/src/syntax_builder.cpp: crop validation and SPS encoding.
- tests/Coding_Reconstruction_Test/codec_pipeline.h: register-to-syntax mapping,
  active DF configuration, luma nonzero flags and chroma strength selection.
- tests/Coding_Reconstruction_Test/communication_pipeline.h: overridable edge and
  sample strength hooks, supporting different strengths along an edge.
- tests/full_pipeline_codec_tb.cpp: register scenarios, filtering source patterns,
  changed-pixel assertion and invalid-configuration recovery.
- tests/Coding_Reconstruction_Test/full_pipeline_decode.py: header inspection,
  cropped reference comparison and decoder validation.
- tests/Coding_Reconstruction_Test/CMakeLists.txt: 21 additional codec cases.
- This report and the existing integration/codec reports: updated evidence/limits.

## Remaining limits and next work

The integrated codec adapter remains test-scoped: fixed 32x32 geometry, four
pictures, one slice per picture, one L0 reference, I4x4/common chroma DC and P16x16
zero MV. It is not yet a general released production pipeline factory. Chroma
arithmetic is called directly; the legacy single-block EcTop socket is not the
whole-slice assembly interface. SliceEncoder buffers a bounded whole picture;
streaming entropy-output backpressure is not established by this pass.

DFCON has signed five-bit fields in the PDF. For decoder-consistent slice signaling,
this adapter currently accepts only even actual offsets in [-12, 12] when filtering
is enabled. Rejecting odd or larger offsets is an explicit adapter policy, not a
claim that the PDF prohibits every other five-bit value. Released behavior for
these values still needs alignment; this restriction prevents claiming complete
register-range conformance.

New crop/delta/DF scenarios use QP 26. Active DF decoder checks use the staircase
pattern, not an exhaustive edge/QP matrix. Fault/reset decoder cases still use DF
bypass. Broader motion, Intra16x16/B modes, arbitrary geometry, GOP scheduling and
combined crop/DF/fault scenarios remain unverified here.

Next priorities: resolve the DF offset policy against the released implementation;
add combined configuration/fault scenarios and wider DF edge/QP coverage; define
streaming output backpressure and generalize geometry/scheduling. Decoder agreement
proves reconstruction consistency for these cases, not full PDF conformance.

## Reproduce

From components/h264, with the existing configured build:

    cmake --build build/vp -j 8
    ctest --test-dir build/vp -R "^full_codec_" --output-on-failure -j 6
    ctest --test-dir build/vp --output-on-failure -j 8

Python3 and FFmpeg must be discovered by CMake. If missing, decoder cases are not
registered; their absence is not a PASS. Artifacts are in
build/vp/tests/Coding_Reconstruction_Test/full_codec_output.
