# HDS V1.0 conformance mapping

Pinned source version: [SPEC_VERSION.md](../SPEC_VERSION.md). The machine-readable
identity and mandatory feature/test map are [spec-baseline.json](../spec-baseline.json).

Reviewed 2026-10-21 against `FTEL_CDC_IP_QSPI_Specification.pdf`, dated
2026-10-20, 63 physical PDF pages. The SHA-256 below identifies the supplied
source independently of its local directory.

SHA-256:
`27705ded65c0317ede7da3645c7edefbceff181d5709383e0f0a63f06d625da6`.

This baseline replaces the previous review of `QSPI_Register_Description.pdf`
and `QSPI_Wrapper_Interface.pdf`. The HDS Section 1.1 gives closed DEC decisions
precedence over earlier sources. The future CSR workbook remains DEP-001;
this model follows the supplied HDS until that workbook is delivered.

## Resolved changes

| HDS source | Required behavior implemented | Regression suffixes |
| --- | --- | --- |
| 5, pp17-18; 6.4.4, pp26-27 | FIFO depth 4/8/16/32 words, default 32; levels and thresholds in words; validated cache/prefetch/async FIFO/timeout parameters | components, fifo, threshold_words, p0_fifo, p1_parameters_* |
| 6.6 p30; 8.2.1 p47 | SCLK divisor PRESCALER+2; even SDR / multiple-of-four DDR; illegal mapped miss also sets TEF | phases, errors, hds_cache, spec_timing |
| DEC-036, pp21/46 | All registers zero during reset; SR TXFTF=1 after release even with EN=0 | registers, hds_dma_clock, hds_flags |
| DEC-038, 6.4.1 p24; CR p47 | DR prefill while disabled; no trigger on EN alone; current frame completes after EN clear; RX data retained | hds_control |
| DEC-013, p25; 6.3 p21 | Full AR checked only when ADMODE is present; selected low address/alternate bytes sent on wire; no ADSIZE overflow rejection | errors, p0_dlr, hds_control |
| DEC-024, 6.4.2 p25 | Final status read finishes at timeout; match and timeout may set SMF and STOF together, including APMS=1 | timeout, p1_polling |
| DEC-020, pp13/15/43; reset p20 | APB waits without timeout through core clock stop/reset; only AHB has an HCLK timeout | clock, p1_domains, hds_flags, hds_timeout |
| 6.1/6.2 pp19-20; 7.4 p44 | Fourth DMA clock/reset; destination-clock request, clear low interval and core acknowledgement | dma, p1_dma, hds_dma_clock |
| DEC-029, pp27/55-56 | DMA moves complete bursts of words, CPU handles residue; partial RX word is zero-padded and valid | dma, p1_dma, hds_control |
| DEC-044, 6.4.6 p28; CR p47 | ABORT request persists to boundary/STOP; TCF/BUSY/ABORT completion; FIFO flush, pending XIP error; DMA inhibited until accepted CCR; zero writes cannot cancel ABORT | hds_control, hds_dma_clock, hds_abort |
| DEC-016/027/030, pp26/39-41 | Fully associative cache; NONSEQ=1 word, SEQ=prefetch length; clamp at flash end; invalidate on miss/config/abort/disable; wait for in-flight data; discard timed-out fill | hds_cache, hds_timeout |
| 6.4.6 p28; 7.2/7.3 pp43-44 | Low two APB/AHB address bits ignored; unmapped APB read zero/error; mapped write/mode/range checks in order | protocol, xip, p0_bus_errors, hds_flags |
| DEC-012, 6.4.5 p27 | Hardware event set wins over same-cycle software W1C; level threshold reasserts after clearing | p0_irq_w1c, hds_flags |
| Table 6-11 p30 | All-skipped frame proceeds to STOP rather than being rejected | hds_control |

## Integration and abstraction boundaries

The [README](../README.md#hds-behavior-and-migration) describes the API changes,
clock/reset binding, FIFO units and programming sequences. The controller
supports custom descriptor-based NOR/PSRAM targets; flash-specific command
sets and initialization are software/device responsibilities (HDS Section 3).
The supplied NOR backend is a separate SDR test support profile.

HDS ASM-002 requires all four resets asserted together. Isolated bus reset
cancellation in old robustness tests is a VP extension, not a supported SoC
reset sequence. APB pending access through a core reset and AHB timeout are
also checked explicitly as requested by HDS Section 10.

The TLM model has one admitted access per bus and at most one XIP fetch. It
validates async FIFO depth parameters but does not model Gray-pointer payload
occupancy or exact crossing latency. DMA request synchronization and clear
acknowledgement are functional destination/core-clock steps. Serial timing
tracks phase durations, byte abort boundaries and STOP/CSHT; data reaches the
backend in at-most-four-byte chunks. Cancellation cannot reproduce external
flash effects within a partially serialized chunk.

Malformed TLM payloads are rejected with TLM errors. Polling without a data
phase and mapped commands without address/data are rejected as invalid VP
transactions; these are defensive policies beyond the explicit HDS illegal
PRESCALER / polling-DLR checks. Empty AND masks match and empty OR masks do
not, using the mathematical identity; PTR=0 expires on first evaluation.

AHB HTRANS is carried by `AhbTransaction`. Without explicit bridge metadata the
single-word TLM transaction is NONSEQ. The existing FU3 fabric still uses this
default; SEQ prefetch is tested directly through the adapter. AHB ERROR has a
functional two-cycle HCLK response delay; this is not a signal protocol checker.

Pad OE/IO/SCLK waveforms, pin-level AHB/APB assertions, analog capture timing,
RTL CDC/RDC, clock/reset SDC and exhaustive randomized clock ratios are outside
this model. HDS Section 10 remains the reference for RTL DV sign-off; no
full-spec coverage percentage or RTL equivalence is claimed.

## Functional regression contract

The default suite has 57 CTest scenarios. The baseline maps 13 functional groups
and the separate generic-NOR support group to explicit test names. The recorder
requires every mapped scenario to exist and pass, rejects unmapped/unknown
scenarios, embeds the version/date/hash and per-group results in the evidence,
and verifies the actual PDF when `--spec-pdf` is supplied.

The expanded phase test checks all 384 independent instruction/address/alternate/
data-width and SDR/DDR combinations using read/write byte oracles and serial
duration lower bounds. Sizes 1..4, every dummy count 0..31, both CKMODE settings
and SIOO are varied across the matrix; they are not an exhaustive Cartesian
product. The divider sweep checks all 256 prescaler values in each rate mode.
Cache runtime tests exercise counts/prefetch lengths 1, 2, 3, 4, 8 and 16; the
parameter contract checks every legal cache/prefetch pair and invalid fields.
