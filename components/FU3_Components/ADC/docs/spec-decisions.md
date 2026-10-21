# Specification basis and model decisions

Reviewed on **2026-10-21** against the PDF identified in the [README](../README.md),
the complete controller implementation and regression. Page numbers below are
1-based PDF pages. No confirmed digital functional mismatch was found within
the documented TLM scope. Specification gaps and chosen VP behavior are recorded
below; they are not a list of proven model defects.

The cover says V1.0 / March 20, 2026; history includes V2.0 / April 3, 2026;
the repeated FU2 V5.7 header is inconsistent. Use the exact pinned PDF hash.
The IP_new file is byte-identical to the previously reviewed `_v2.pdf`.
Unrelated UART examples, guidelines and the secure-boot appendix are template
content, not additional ADC requirements or instructions for this project.

## Requirements checked

| PDF pages | Requirement | Implementation |
| --- | --- | --- |
| 13-18 | 23 CSRs, offsets 0x00-0x58, access types, widths and reset values | `Adc::read/write/reset_registers`, register constants |
| 8-9, 14 | 16 channels, 12-bit D and mode selection | Typed ports; configuration snapshot at sequence start |
| 15 | Effective sample count 1-16; timing minima 32/4/12 | Clamped active configuration; masked raw CSR readback |
| 10, 15-18 | Capture individual samples and average | 32-bit sum, maximum 65520; result published at DONE |
| 10, 18-19 | FSM order, sticky DONE, clear status on disable | Clocked FSM and status derived from state |
| 12 | Active-low reset clears state/data | Asynchronous assertion, including with stopped ADC clock |
| 18 | Base 0x500A0000 and SYSBUS_1/PERIBUS_1 path | Tested through the existing FU3 bus |

The complete requirement-to-test mapping is maintained once, in
[hds-requirements.json](../tests/hds-requirements.json).

## STATUS: behavior versus bit assignment

The HDS specifies the following:

- Page 13: ADC_STATUS is read-only at offset 0x000C.
- Page 10: DONE deasserts BUSY and asserts READY and DONE.
- Page 18: poll DONE at bit 0; disable and wait until all three flags clear.
- Page 19: after disable, wait for STATUS[2:0]=000.

The PDF has no field table assigning the individual BUSY/READY positions.
The VP ABI chooses BUSY[2], READY[1], DONE[0]:

| State | STATUS[2:0] | Basis |
| --- | --- | --- |
| POWER_DOWN/reset | 000 | Required idle/clear behavior |
| SETUP_WAIT through CHECK_CNT | 100 | VP choice of active flags and BUSY bit |
| DONE | 011 | Required flag values; READY bit position is a VP choice |

DONE remains set while enabled. Disable clears status on the next ADC edge;
software must wait for idle as the HDS instructs. No STATUS logic defect was
found. READY/BUSY mapping needs an owner definition only to establish exact
compatibility with an external driver/RTL that consumes those individual bits.

## EOC, CS and timing

EOC is level-tested in WAIT_EOC and D is read on the following CAPTURE edge,
as shown by the page 10 FSM. Page 9 calls the rising edge valid; that wording
does not establish that the controller must implement an edge detector.
With the page 11 normal waveform, EOC stays high until 2.5 cycles after the
next sampling rise. Minimum HIGH=4 cycles lets EOC return low before the next
conversion wait. No normal-conversion defect was found from level sensing.
Short early pulses and a stuck-high EOC remain abnormal-input policy cases.

CS is high from SAMPLE_HIGH through CHECK_CNT and low in setup/done/down.
M is latched for the entire sequence. This follows the page 10 HIGH/LOW
description and satisfies the controller-pin conditions of high CS during
sampling and low CS while changing M. Page 11 depicts a low-CS interval and
channel changes that the fixed-channel sequence does not reproduce. This
alone does not demonstrate a controller defect; which waveform to use for an
exact RTL comparison remains an owner question. Internal ISAMPCLK is outside
the controller model.

ADC_EN is accepted on the edge leaving POWER_DOWN. PWD falls and configuration
is latched. SAMPCLK rises after `max(setup,32)` full ADC cycles, stays high for
`max(high,4)` cycles, then SAMPLE_LOW lasts `max(low,12)` cycles. WAIT_EOC,
CAPTURE and CHECK_CNT follow. Consequently the entire low interval includes
FSM/EOC latency. It meets the specified minimum; an exact total-low-duration
interpretation is not established by the HDS state description.

With the simplified test stimulus, D changes 12 cycles after SAMPCLK falls
and EOC rises at 12.5 cycles. Between samples the VP low interval is
`max(max(low,12)+1,13)+2` cycles: 15 cycles at defaults. Page 11 actually
references internal ISAMPCLK. The `macro_phase` test delays the effective
sampling edge by one cycle, giving D/EOC at 13/13.5 cycles after SAMPCLK falls;
the default low interval then becomes 16 cycles and the samples remain correct.
These are stimulus-dependent VP measurements, not RTL cycle equivalence.

## Other VP choices and integration boundaries

Stable decision IDs are retained for references from tests and downstream notes.

| ID | Topic | Current behavior / remaining definition |
| --- | --- | --- |
| D01 | STATUS | READY[1]/BUSY[2] and active-state values are VP ABI choices; DONE[0] and clearing are explicit. |
| D02 | Fractional average | Unsigned integer division rounds down; HDS does not specify rounding. |
| D03 | Clamp readback | Read masked raw values; clamp the active configuration separately. |
| D04 | EOC | Follow the level-based FSM; normal held EOC works; early/stuck-input behavior is policy. |
| D05 | Disable during conversion | Abort on the next ADC edge, clear flags and drive power-down outputs; mid-state transitions are not fully specified. |
| D06 | Data lifetime | Disable retains data; new start clears all sample slots/result/sum. |
| D07 | Cycle accounting | Full-cycle setup/high/low counters; subsequent FSM stages add low-time latency. |
| D08 | Macro outputs | CS follows FSM; PWD remains low in DONE; macro reset asserts asynchronously and releases on an ADC edge. |
| D09 | APB/TLM accesses | Aligned four-byte transfers without byte enables; RO writes ignored; malformed payloads receive TLM errors, not physical PSLVERR. PPROT is not modeled. |
| D10 | Transport | Little-endian, no DMI, read-only zero-time debug; b_transport requires SC_THREAD and serializes accesses. Multiphase nonblocking transport is unverified. |
| D11 | Clock/CDC/reset | Platform supplies ADC clock; default CSR latency 25 ns. No 80-to-10 MHz divider, metastability or multistage reset synchronizer. |
| D12 | Reset during transport | Reject accesses entered during reset or interrupted during service, including short reset pulses; no partial commit. |
| D13 | Test mode | Forward SAMP_SEL=0; continue FSM; external macro supplies D/EOC. Analog test-mode behavior is unspecified. |
| D14 | Missing EOC | Wait indefinitely; software/testbench timeouts and disable/reset remain available. No invented hardware timeout. |
| D15 | Source identity | Follow pinned ADC content, not inconsistent revision headers or unrelated template text. |
| D16 | Analog/package integration | External HHG macro supplies D/EOC. Analog transfer, accuracy and package-to-channel wiring need separate specifications. |

`b_transport` consumes incoming delay before arbitration and commits after
access latency. Reset before transaction arrival does not cancel a later valid
arrival. Payload metadata is preserved. A disable-enable pulse between ADC
edges can go unobserved; use disable/wait-idle/configure/enable.

Analog behavior, pin-level APB, divider/CDC implementation and CPU/SoC boot
are not proven by this component's tests. Keep `hds_signoff: false`; passing
policy checks establishes reproducible model behavior, not an approved RTL ABI.
