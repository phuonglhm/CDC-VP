# HDS decisions and VP policies

Reviewed on 2026-10-19 against the supplied AES PDF, the controller/core source,
register definitions, tests and `tests/hds-requirements.json`. This document
records open specification decisions; it is not an HDS approval. Existing
HDS-Q01 through HDS-Q16 identifiers are preserved for requirement traceability.

## Source baseline

Source: `FTEL_CDC_IP_AES_Specification (1).pdf`, 23 pages, supplied under
`D:/Intern_doc/Intern/VP/004_IP/003_AES/`. Page numbers are 1-based physical PDF
pages. The cover identifies V1.0 / March 20, 2026; p4 includes V2.0 / April 3,
2026; repeated headers identify FU2 V5.7.

| Source record | SHA-256 |
| --- | --- |
| Historical baseline pinned in `tests/hds-requirements.json` | `b339ebbd0fdff57c6f483d062920f05b3a861e505df52b697de171c2c7229b45` |
| PDF available at the supplied path on 2026-10-19 | `0155225ce1b8e0f642d7670a9ef1250a2a362e0f65bab3844853b53d2ea30bf2` |

Normalized extracted text matches the cached `build/aes-spec.txt` on all 23
pages. This does not establish binary identity or unchanged figures. The
historical requirement manifest has not been repinned; HDS-Q17 tracks this
provenance discrepancy explicitly.

## Explicit requirements and scope

Pages 8-12 specify AES-128 and AES-256 with 128-bit blocks, encryption and
decryption, 10/14 rounds, key expansion and the FIPS 197 algorithm reference.
Pages 16-17 define CONFIG bits and KEY/BLOCK concatenation: KEY0/BLOCK0 are the
most-significant words, and AES-128 uses KEY0 through KEY3. Reserved ranges
explicitly listed on pp14-15 read zero and ignore writes. CONFIG/KEY/BLOCK
readback is zero. These details are defined, even though addressing/reset
conflicts still affect hardware integration.

AES-192, IV/chaining modes, a DMA handshake, a key vault and FIPS certification
are outside the stated feature scope. Algorithm test success does not prove
security certification or pin-level AXI compliance.

## Open decisions

`Conflict` means inconsistent statements or described delivered behavior versus
the intended usable interface. `Undefined` means an exact outcome is missing.
`VP contract` identifies a transaction-level choice. All rows remain open for
owner confirmation; a tested choice is not an approved hardware correction.

| ID | Source and unresolved issue | Current VP behavior | Required confirmation |
| --- | --- | --- | --- |
| HDS-Q01 | Conflict: p11 section 6.2 and p18 AES-R01/R05 describe an active-low wrapper connected to active-high synchronous AES state; comments also claim asynchronous reset | One coherent synchronous active-low reset for controller/core | Approved wrapper polarity/inversion and assertion/release semantics |
| HDS-Q02 | Undefined: p10 and p18 AES-R03 forward low address bits while pp14-17 use consecutive local CSR indices | Byte offset = local index times 4; aligned words, no high-address alias | Byte/word translation, aliasing and platform decode |
| HDS-Q03 | Conflict: pp15-18/AES-R02 describe RESULT1/2 absent from the delivered mux; full-block readback is unusable | Default returns all four words MSW-first; optional `ObservedRtlMux` returns zero for RESULT1/2 and the low word for RESULT0/3 | Approved result slices and whether corrected or delivered behavior is the baseline; RESULT0 truncation is an inference, not an RTL equivalence result |
| HDS-Q04 | Undefined: p18 AES-R04 says unsupported burst/size requests may fail to reach a response path | Malformed/unsupported TLM requests return errors rather than waiting indefinitely | Hardware error response and bounded completion rule |
| HDS-Q05 | Undefined: p10 and p18 AES-R06 advertise configurable bus widths around fixed 32-bit CSRs | Fixed 32-bit target | Legal parameter ranges and width adaptation |
| HDS-Q06 | Undefined: pp11-12 explicitly provide no fixed latency/throughput guarantee | Configurable positive latencies; defaults INIT=16 and NEXT=64 clock cycles | Timing measurement points and any guaranteed latency per key length/direction |
| HDS-Q07 | Undefined: p11 and pp14-16 omit a complete reset-value/retention table | Clear key, block, result, configuration, schedule and pending work; READY=1, VALID=0 | Reset values and cancellation/retention of in-flight work |
| HDS-Q08 | Undefined: pp12/16-17 describe the normal sequence, not writes or new commands while busy | Reject KEY/BLOCK/CONFIG writes and nonzero INIT/NEXT commands while busy; zero commands are harmless | Ignore, reject, queue or accept each busy-time access |
| HDS-Q09 | Undefined: p16 permits two command bits but gives no simultaneous-command rule | INIT takes precedence when INIT and NEXT are both set | Priority or illegal-command behavior |
| HDS-Q10 | Undefined: p17 requires INIT before NEXT but omits the violation outcome | Reject NEXT until key initialization completes, without changing state | Behavior before INIT or during initialization |
| HDS-Q11 | Undefined: pp12/16 define READY/VALID meanings but not every clear/retention edge | INIT/NEXT clear VALID; reads do not clear it; retain old result until atomic completion | Flag lifecycle, read-to-clear behavior and result visibility while busy |
| HDS-Q12 | Undefined: pp12/16-17 omit schedule invalidation/reuse rules | Any key write or KEYLEN change requires INIT again; direction-only changes reuse the schedule | Key/config update rules, including writes of unchanged values |
| HDS-Q13 | Undefined: pp12/17 do not identify exact input sampling edges | INIT snapshots key; NEXT snapshots input block and direction | Command acceptance and configuration sampling instants |
| HDS-Q14 | Conflict: p7 mentions APB/FIFOs/interrupts, whereas p9 excludes interrupt/DMA and pp12-14 define AXI; a UART heading remains | AXI-oriented TLM endpoint without IRQ, DMA handshake, FIFO, APB or UART implementation | Correct overview and interface scope |
| HDS-Q15 | Conflict: cover/history/header revisions and the missing section 8.1 reference on p17 do not agree | Retain explicit source identities above | Approved revision, corrected section references and normative status of AES-R01 through AES-R06 |
| HDS-Q16 | Undefined/integration: pp14-17 define local indices through 0x33, not the SoC aperture or all invalid accesses | Existing FU3 bus base 0x40000000 with 64 KiB window; model aperture 0x400 bytes; unspecified slots inside read zero/ignore writes, as do RO writes | Platform aperture and handling of unlisted slots/RO writes; do not generalize the explicitly reserved ranges without approval |
| HDS-Q17 | Provenance gap: the current PDF hash differs from the historical requirement manifest despite matching normalized extracted text | Record both hashes; retain the historical machine-readable baseline | Confirm which exact file is authoritative and whether figures/metadata changed before repinning reports |

HDS-Q01, HDS-Q02 and HDS-Q03 are the highest-priority hardware/driver interface
blockers. Functional software can use the documented VP choices, but those
choices must not be described as fixes already present in delivered RTL.

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
internal CDC (p11); CDC is not an unspecified feature that this IP must invent.

## Regression evidence and closure

| Decisions or requirements | Existing test evidence (suffixes) | Limit |
| --- | --- | --- |
| Algorithm, rounds and round keys | `spec_encrypt128`, `spec_decrypt128`, `spec_encrypt256`, `spec_decrypt256`, `spec_upper_key128`, `spec_trace128`, `spec_trace256` | FIPS known answers/trace fixtures; no RTL waveform or certification proof |
| Q01/Q07 and reset-time transport | `policy_reset`, `policy_reset_edges`, `policy_reset_entry`, `policy_reset_boundaries`, `policy_concurrency` | Chosen reset policy |
| Q02-Q05/Q16 and access behavior | `policy_registers`, `policy_protocol`, `policy_result_mux`, `policy_strobes`, `policy_access_matrix`, `policy_hds_fields`, `policy_bus_lanes` | Chosen CSR/TLM map, not repaired RTL decode |
| Q06/Q08-Q13 | `policy_timing`, `policy_commands`, `policy_status_flow`, `policy_busy_matrix`, `policy_latency_1`, `policy_latency_2`, `policy_latency_17`, `policy_latency_257` | VP lifecycle and configured timing |
| Q14/Q15/Q17 | Source/interface review | No dynamic test can approve the source baseline |

CTest names use the prefix `fu3_aes_`. The 2026-10-19 Debug rebuild and rerun
passed **39/39 tests**, with no failures or disabled tests: seven spec tests,
28 policy tests and four reference-oracle tests. Evidence is in
`build/hds-review/aes-results.xml` and `aes-regression.log` from the repository
root. This review did not rerun Release or UBSan.

When an owner resolves an item, record the approved revision/section, update
the affected behavior and expectations, then update
[hds-requirements.json](../tests/hds-requirements.json). Q17 is a source-provenance
finding recorded here, not an additional implemented requirement. A green VP
regression does not close an open HDS decision.

See [testing.md](testing.md) for the existing test workflow and fixture sources.
