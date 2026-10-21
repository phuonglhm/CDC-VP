# HDS decisions and VP policies

This document records open specification decisions and the model's current
policies; it is not an HDS approval. HDS-Q01 through HDS-Q16 identify the
questions that require confirmation from the HDS owner.

## Source baseline

Source: `FTEL_CDC_IP_AES_Specification.pdf`, 22 pages, supplied on 2026-10-21.
Page numbers are 1-based physical PDF pages. The cover identifies V1.0 /
March 20, 2026; p3 includes V2.0 / April 3,
2026; repeated headers identify FU2 V5.7.

The exact PDF SHA-256 is recorded in the [component README](../README.md).

Section 6.2 (p10) explicitly requires synchronous active-low reset,
matching the existing model. Section 9.1 (p16) and Appendix B AES-R01/R05 (p17)
still describe the previous reset mismatch, so HDS-Q01 is only partially resolved.

## Explicit requirements and scope

Pages 7-11 specify AES-128 and AES-256 with 128-bit blocks, encryption and
decryption, 10/14 rounds, key expansion and the FIPS 197 algorithm reference.
Pages 15-16 define CONFIG bits and KEY/BLOCK concatenation: KEY0/BLOCK0 are the
most-significant words, and AES-128 uses KEY0 through KEY3. Reserved ranges
explicitly listed on pp13-14 read zero and ignore writes. CONFIG/KEY/BLOCK
readback is zero. These details are defined, even though addressing/reset
conflicts still affect hardware integration.

AES-192, IV/chaining modes, a DMA handshake, a key vault and FIPS certification
are outside the stated feature scope. Algorithm test success does not prove
security certification or pin-level AXI compliance.

## Open decisions

`Conflict` means inconsistent statements or described delivered behavior versus
the intended usable interface. `Undefined` means an exact outcome is missing.
`VP contract` identifies a transaction-level choice. Q01-Q16 still need the
stated confirmations. A tested VP choice
does not establish that delivered RTL was corrected.

| ID | Source and unresolved issue | Current VP behavior | Required confirmation |
| --- | --- | --- | --- |
| HDS-Q01 | Conflict remaining: p10 section 6.2 now requires active-low synchronous reset, but p16 section 9.1 and p17 AES-R01/R05 retain the previous active-high-core mismatch | Model already matches section 6.2; no polarity change needed | Remove/update stale review text or identify it explicitly as historical; confirm delivered RTL follows the new main requirement |
| HDS-Q02 | Undefined: p9 and p17 AES-R03 forward low address bits while pp13-16 use consecutive local CSR indices | Byte offset = local index times 4; aligned words, no high-address alias | Byte/word translation, aliasing and platform decode |
| HDS-Q03 | Conflict: pp14-17/AES-R02 still describe RESULT1/2 absent from the delivered mux | Default returns all four words MSW-first; optional `ObservedRtlMux` returns zero for RESULT1/2 and the low word for RESULT0/3 | Approved result slices and corrected-versus-delivered baseline; RESULT0 truncation remains an inference |
| HDS-Q04 | Undefined: p17 AES-R04 still permits unsupported burst/size requests not to reach a response path | TLM validation errors rather than indefinite waits | Hardware error response and bounded completion rule |
| HDS-Q05 | Undefined: p9 and p17 AES-R06 advertise configurable widths around fixed 32-bit CSRs | Fixed 32-bit target | Legal parameter ranges and width adaptation |
| HDS-Q06 | Undefined: pp10-11 explicitly provide no fixed latency/throughput guarantee | Positive configurable latencies; defaults INIT=16 and NEXT=64 clock cycles | Timing scope, measurement points and guarantees per key length/direction |
| HDS-Q07 | Undefined: p10 and pp13-15 still omit a complete reset-value/retention table | Clear data/config/schedule/pending work; READY=1, VALID=0 | Reset values and cancellation/retention of in-flight work |
| HDS-Q08 | Undefined: pp11/15-16 describe normal operation only | Reject KEY/BLOCK/CONFIG writes and nonzero commands while busy | Ignore/reject/queue/accept behavior per busy-time access |
| HDS-Q09 | Undefined: p15 gives no simultaneous INIT/NEXT rule | INIT takes precedence | Priority or illegal-command behavior |
| HDS-Q10 | Undefined: p16 requires INIT before NEXT but omits violation behavior | Reject NEXT until key initialization completes | Behavior before INIT or during initialization |
| HDS-Q11 | Undefined: pp11/15 define READY/VALID meanings but not every lifecycle edge | INIT/NEXT clear VALID; reads retain it; old result held until atomic completion | Flag set/clear rules, read side effects and busy-time result visibility |
| HDS-Q12 | Undefined: pp11/15-16 omit schedule invalidation/reuse rules | Key write/KEYLEN change requires INIT; direction-only changes reuse schedule | Update rules, including unchanged values and unused key words |
| HDS-Q13 | Undefined: pp11/16 do not define exact sampling edges | INIT snapshots key; NEXT snapshots input/direction | Acceptance and sampling instants |
| HDS-Q14 | Partial correction: UART heading and AES diagram captions were fixed; p6 overview still mentions FIFOs/interrupts/APB while p8 excludes interrupt output and pp11-13 define AXI | AXI-oriented TLM endpoint without IRQ/DMA handshake/FIFO/APB/UART | Confirm scope and correct remaining overview prose |
| HDS-Q15 | Conflict: cover/history/header revisions remain inconsistent and section 8.1 referenced on p16 is absent | Pin the exact new PDF rather than infer revision approval from its filename | Approved revision and normative/historical status of Appendix B review entries |
| HDS-Q16 | Undefined/integration: pp13-16 give local indices, not SoC aperture or every invalid access | FU3 base 0x40000000/64KiB bus window; model aperture 0x400; unlisted slots read zero/ignore writes, as do RO writes | Platform aperture, aliases, unlisted slots and RO-write behavior |

HDS-Q02 and HDS-Q03 remain direct driver-interface blockers. HDS-Q01 now
concerns inconsistent document sections: the main reset requirement agrees
with the model, while the review appendix still describes old delivered RTL.

## TLM contract

- Call `b_transport` from an `SC_THREAD`; each access is an aligned 32-bit word.
- Register bytes are little-endian. Byte enables merge stored write lanes;
  disabled read lanes preserve the caller's bytes. The FU3 fabric handles burst
  decomposition and narrow accesses separately from this endpoint.
- Incoming delay is consumed, accesses are serialized, and the core tick runs
  before a register access commits on a clock edge. DMI is unavailable and debug
  is read-only, untimed and non-destructive.
- A sampled reset clears state and invalidates callers waiting on delay, a mutex
  or a clock edge. A request entered while reset is low remains invalid even if
  reset releases before arrival. This is a VP cancellation policy.
- Payload and sequencing failures return the TLM errors documented in
  [registers.md](registers.md); they do not establish AXI BRESP/RRESP timing.

Generic TLM does not model separate AXI channel handshakes, ID capture/return,
WLAST/RLAST or physical S-box sharing. The HDS describes one clock domain with no
internal CDC (p10); CDC is not an unspecified feature that this IP must invent.

## Test mapping and closure

| Decisions or requirements | Existing test evidence (suffixes) | Limit |
| --- | --- | --- |
| Algorithm, rounds and round keys | `spec_encrypt128`, `spec_decrypt128`, `spec_encrypt256`, `spec_decrypt256`, `spec_upper_key128`, `spec_trace128`, `spec_trace256` | FIPS known answers/trace fixtures; no RTL waveform or certification proof |
| Q01/Q07 and reset-time transport | `policy_reset`, `policy_reset_edges`, `policy_reset_entry`, `policy_reset_boundaries`, `policy_concurrency` | Polarity/sampling match new section 6.2; reset values and TLM cancellation remain model choices |
| Q02-Q05/Q16 and access behavior | `policy_registers`, `policy_protocol`, `policy_result_mux`, `policy_strobes`, `policy_access_matrix`, `policy_hds_fields`, `policy_bus_lanes` | Chosen CSR/TLM map, not repaired RTL decode |
| Q06/Q08-Q13 | `policy_timing`, `policy_commands`, `policy_status_flow`, `policy_busy_matrix`, `policy_latency_1`, `policy_latency_2`, `policy_latency_17`, `policy_latency_257` | VP lifecycle and configured timing |
| Q14/Q15 | Source/interface review | No dynamic test can approve the source baseline |

CTest names use the prefix `fu3_aes_`. Test outcomes are runtime artifacts
under `build/`; this document records requirements and model conventions.

When an owner resolves an item, record the approved revision/section, update
the affected behavior and expectations, then update the decisions and test
mapping in this document. A green VP regression does not close an open
HDS decision.

See [testing.md](testing.md) for the existing test workflow and fixture sources.
