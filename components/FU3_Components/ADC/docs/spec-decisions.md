# HDS decisions and VP policies

Reviewed on 2026-10-19 against the supplied PDF, `src/adc.cpp`, the register
definitions and the current regression tests. This is an open decision log,
not an HDS approval or a list of confirmed model defects. All decisions below
remain open unless the IP owner provides an approved specification revision.

The source document and SHA-256 are pinned in the [README](../README.md).
Page references use 1-based PDF page numbers. The cover identifies
V1.0 / March 20, 2026; the revision history includes V2.0 / April 3, 2026;
the repeated header identifies FU2 V5.7. The model follows the ADC content
on pages 7-19 of that exact file, without inferring another FU2/FU3 specification.

## Explicit requirements

- Pages 13-18: register map, reset values, field widths and RO/RW access.
- Page 15: effective timing minima of 32/4/12 cycles and effective count of 1..16.
- Page 10: POWER_DOWN -> SETUP_WAIT -> SAMPLE_HIGH -> SAMPLE_LOW -> WAIT_EOC
  -> CAPTURE -> CHECK_CNT -> SAMPLE_HIGH or DONE. DONE persists while ADC_EN=1.
- Page 10: CS=0 and SAMPCLK=0 in power-down; CS=1 during HIGH/LOW;
  BUSY deasserts and READY/DONE assert in DONE.
- Page 18: configuration is captured at sequence start; DONE is bit 0;
  software polls with a timeout.
- Page 12: active-low reset clears state and data; nominal ADC clock is 10 MHz.

## Decisions requiring confirmation

| ID | Ambiguity or missing definition | Current VP behavior | Classification |
| --- | --- | --- | --- |
| D01 | ADC_STATUS field table is missing; only DONE[0] is explicit on p18 | READY[1], BUSY[2]; idle=0, active=4, done=3 | VP policy |
| D02 | Average rounding is unspecified when the sum is not divisible by count | Unsigned integer division rounds down; 32-bit accumulator; maximum sum 65520 | VP policy |
| D03 | Effective timing/count limits do not define readback | CSRs retain raw masked values; active configuration is clamped separately | VP policy |
| D04 | p9 identifies a valid EOC rising edge; the p10 FSM tests eoc=1 | Check EOC level in WAIT_EOC; read D on the next ADC edge in CAPTURE; do not retain pulses that end before WAIT_EOC | FSM takes precedence; confirmation required |
| D05 | Disable transitions from active states are not shown | ADC_EN=0 aborts on the next ADC edge, clears flags, lowers CS/SAMPCLK and raises PWD | VP policy |
| D06 | Data retention on disable/start is unspecified | Disable retains samples/result; a new start clears all samples, result and accumulator | VP policy |
| D07 | p10 shows high/low counter comparisons, but does not fully define counter initialization, entry-edge accounting or CAPTURE/CHECK_CNT latency | WAIT_EOC/CAPTURE/CHECK_CNT each require at least one edge; setup/high/low last exactly N full cycles | VP cycle policy |
| D08 | CS/PWD/RST are not fully defined in every state | CS is high from SAMPLE_HIGH through CHECK_CNT and low in setup/done/down; PWD stays low while enabled, including DONE; macro reset follows controller reset and releases on an ADC edge | VP policy |
| D09 | PSTRB and PSLVERR are marked "Not support" on p13 | Accept aligned 4-byte accesses without byte enables; reject malformed payloads with TLM errors; ignore RO writes; TLM errors do not represent physical PSLVERR | VP contract |
| D10 | Debug, DMI, endianness and ordering are unspecified | Little-endian; side-effect-free read-only debug; no DMI; b_transport requires SC_THREAD and uses a mutex for serialization | VP contract |
| D11 | p12 specifies nominal 80 MHz to 10 MHz generation and synchronized ADC reset release, but omits divider phase, CSR crossing protocol and synchronizer depth | Platform supplies adc_clk; default CSR APB latency is 25ns; FSM observes CSRs on rising edges; divider/metastability and a multistage reset synchronizer are not modeled | Explicit hardware requirement abstracted in TLM; implementation details missing |
| D12 | Transactions overlapping reset are unspecified | Accesses entered during reset or interrupted during service return GENERIC_ERROR without committing; a reset epoch also detects pulses that have already ended | VP contract |
| D13 | SAMP_SEL=0 is called test mode without defining macro behavior | Forward SAMP_SEL=0; continue the controller FSM; external logic supplies D/EOC | Macro behavior outside scope |
| D14 | A hardware EOC timeout is not specified | Wait indefinitely for EOC; driver/testbench timeouts remain active; disable/reset can abort | No additional hardware timeout |
| D15 | Cover V1.0, history V2.0 and repeated FU2 V5.7 header disagree; pp7-8/13/20-22 contain unrelated or unfinished template material | Use the exact ADC PDF hash in the README and ADC-specific pp7-19; template text does not add UART/security features | Document baseline conflict |
| D16 | pp7-12 reference an external HHG hardmacro; this HDS does not define a complete voltage-to-code, accuracy or test-mode contract. IO_PAD HDS Table 7-2 exposes two ADC package inputs while this controller selects 16 macro channels | Supply digital D/EOC externally; implement all 16 channel selections without inventing package wiring or an analog transfer function | External specification and SoC integration dependency |

These decisions are not presented as verified HDS requirements. Tests count
`SPEC_CHECKS` and `POLICY_CHECKS` separately; one scenario may contain both.
The hardmacro timing values of 12/12.5/2.5 cycles are used to generate stimulus
and do not validate the analog hardmacro.

## Exact VP timing

ADC_EN is accepted on the edge that leaves POWER_DOWN; PWD goes low and
configuration is latched. SAMPCLK rises `max(setup,32)` cycles after that edge
and falls after `max(high,4)` further cycles. The FSM enters WAIT_EOC
`max(low,12)` cycles after the falling edge. On the next edge, EOC=1 advances
to CAPTURE; the following edge stores D; the following CHECK_CNT edge selects
DONE or the next sample. The averaged DATA register updates only on entry to DONE.

With test stimulus that raises EOC 12.5 cycles after SAMPCLK falls, the interval
from that falling edge to the next rising edge is
`max(max(low,12)+1,13)+2` cycles. This is VP latency rather than an RTL timing guarantee.
Writing ADC_EN=0 and then ADC_EN=1 between consecutive ADC edges may go
unobserved by the FSM. The driver must follow p18: disable, wait for STATUS=0,
configure and then enable.

`b_transport` consumes incoming delay before arbitration, waits for access
latency after acquiring the mutex and commits at completion. Reset during
incoming delay, before arrival, does not cancel a request that has not arrived.
Payload address, length, pointers, command, streaming width and enables are
preserved. Malformed payloads are rejected before waiting.
Multiphase `nb_transport` execution has not been verified.

## Required clarification and regression evidence

The sources explicitly define DONE[0], timing minima, sample-count clamping,
configuration capture at sequence start and the normal disable/wait-idle
sequence. These are not missing requirements. The remaining questions concern
the precise behavior around those requirements.

Test suffixes below are scenario names in [test_adc.cpp](../tests/test_adc.cpp).
Passing a policy assertion establishes reproducible VP behavior, not approval
of the corresponding HDS interpretation.

| Decisions | Confirmation needed from the IP/SoC owner | Existing regression evidence |
| --- | --- | --- |
| D01 | Complete STATUS bit table, reset values and per-state BUSY/READY values; prioritize this before driver ABI sign-off | `registers`, `lifecycle`, `reset` |
| D02-D03 | Rounding rule, accumulator requirements and raw-versus-effective CSR readback | `averaging`, `counts`, `timing_min`, `timing_programmed`, `timing_max` |
| D04, D07 | EOC edge versus level, early/held EOC handling, data capture edge and exact state-entry timing; prioritize this before cycle comparisons | `eoc_wait`, `eoc_policy`, `timing_min`, `timing_programmed` |
| D05-D06 | Abort behavior in every active state and retention/clearing of DATA and all SAMPLE registers | `disable_phases`, `lifecycle`, `snapshot` |
| D08, D11-D12 | Pin-state table, reset assertion/release contract, CSR/ADC crossings and accesses overlapping reset | `reset`, `reset_phases`, `reset_transport`, `clock_pause`; divider/CDC RTL is not verified |
| D09-D10 | Malformed/RO access handling, PPROT semantics, bus ordering and approved software transport abstraction | `protocol`, `debug`, `reset_transport`, `bus`; no pin-level APB proof |
| D13-D14, D16 | Approved macro specification/test-mode stimulus, package channel map and whether a hardware timeout is required | `channels`, `eoc_wait`, `eoc_policy`; digital stimulus only, no analog or package proof |
| D15 | Approved document revision and replacement of template sections | Source review; not a simulation test |

The 2026-10-19 Debug rebuild and rerun passed **27/27 tests**, with no failures
or disabled tests. Evidence is generated under `build/hds-review/adc-results.xml`
and `adc-regression.log` from the repository root. Earlier Release/UBSan results
are documented in [verification-results.md](verification-results.md); they are
not new runs from this documentation review. Full HDS sign-off remains open.

Related details: [verification matrix](hds-verification-matrix.md) and
[owner clarification request](hds-clarification-request.md). This decision log
also records the cross-IP source and integration findings from this review.

Close a decision only after recording an approved source revision/section and
reviewing the affected implementation, tests and requirement mapping. A passing
VP policy test does not close the corresponding specification question.

## Related IP decision logs

The same 2026-10-19 review covers the following components. Each log records
its own source baseline, open questions, current VP choices and test evidence.

| IP | Decision log | Main open areas |
| --- | --- | --- |
| AES | [AES spec-decisions.md](../../AES-256/docs/spec-decisions.md) | Reset polarity, address translation, result mux, lifecycle and source identity |
| DMA | [DMA spec-decisions.md](../../DMA0/AP_DMA/docs/spec-decisions.md) | AXI version, priority, transfer sizing, reset, timers and capabilities |
| GPIO/IO_PAD | [GPIO spec-decisions.md](../../GPIO_IO/docs/spec-decisions.md) | PADDIR/PADCFG electrical mapping, external control registers and package/wake integration |
| QSPI | [QSPI spec-decisions.md](../../QSPI/docs/spec-decisions.md) | FIFO units, DMA tails, reset domains, mapped writes and polling corner cases |
