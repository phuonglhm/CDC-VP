# FlooNoC Model Pre-A1 Review Fix Plan

## 1. Purpose and hard gate

This document captures the findings from the full implementation, verification,
and documentation review performed on 2026-07-30.

**Do not start Step A-1 until every mandatory item in this document is fixed,
reviewed, and the completion gate in section 8 passes.**

> **Status, 2026-07-31: this round is complete, and a second round followed.**
>
> Every item below was implemented and verified; see the checked boxes in
> section 8 for the evidence against each. Closing this document did *not*
> close the pre-A1 gate: a second review found two functional blockers, a parent
> CMake failure, and further gaps, recorded in
> `docs/PRE_A1_REVIEW_ROUND2_FIX_PLAN.md`. **A-1 starts only after that document
> also passes.**
>
> Two items here were superseded rather than merely finished, and are marked so
> below: the negative-control mechanism (V2) and the invalid-configuration test
> written for F4's clock check.

Step A-1 remains the next functional milestone after this cleanup. It is the RTL
cross-check of `axi_chimney_manager_response`; it must not be mixed into the
pre-A1 fixes.

The review baseline was:

- standalone SystemC regression: 29/29 tests passed;
- all eleven existing SystemC-to-RTL cross-check runners passed;
- platform firmware regression passed both `DMA PASS` and the synthetic survey;
- measured integration latencies remained 11 cycles at one hop and 30 cycles at
  six hops;
- frozen FlooNoC revision:
  `9a6972a5f9b8117506d1df8a6505ce1da2bc9084`;
- the frozen FlooNoC working tree was clean;
- `git diff --check` was clean.

Those passing results remain valid for the exercised stimuli, but they do not
cover the defects and documentation overclaims listed below.

## 2. Non-negotiable working rules

1. Preserve all unrelated user changes in both repositories.
2. Do not modify the frozen FlooNoC RTL or refresh expected CSV files from the
   SystemC model.
3. Do not call a block RTL-signed unless a per-cycle cross-check against the
   unmodified frozen RTL exists and passes.
4. Put generated build products under `/tmp`, not in the source tree.
5. Use the following environment before every build:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head
```

6. Fix root causes and add focused regressions. Do not weaken existing tests,
   timeouts, trace comparisons, or protocol checks to obtain a pass.
7. Every new test wait must have a local bound and the complete test must have a
   global watchdog.
8. Keep Step A-1 out of this change. Its strengthened specification is recorded
   in section 7 for later execution.

## 3. Mandatory implementation fixes

### F1 — Implement or reject the complete TLM generic-payload contract

**Severity:** high

The public header says that unsupported byte-enable patterns are rejected, but
`noc_interconnect::b_transport()` does not inspect byte enables. The downstream
payload is always sent with a null byte-enable pointer.

The same entry point also ignores the incoming streaming width and treats every
command that is not a write as a read. This can silently reinterpret
`TLM_IGNORE_COMMAND`.

Relevant code:

- `include/floo_noc_model/noc_interconnect.h`, around the "Not modelled"
  contract;
- `src/noc_interconnect.cpp`, `pack_beats()`,
  `perform_downstream()`, and `b_transport()`;
- `tests/test_noc_interconnect.cpp`, whose current payloads all use null byte
  enables.

Required behavior:

1. Explicitly validate the TLM command.
   - Accept only `TLM_READ_COMMAND` and `TLM_WRITE_COMMAND`.
   - Return `TLM_COMMAND_ERROR_RESPONSE` for unsupported commands.
2. Define and implement streaming-width handling.
   - Linear accesses where `streaming_width >= data_length` may proceed.
   - A wrapped or repeated streaming transfer must either be implemented
     correctly or rejected with `TLM_BURST_ERROR_RESPONSE`.
3. Define byte-enable handling.
   - A null byte-enable pointer means all payload bytes are enabled.
   - Supported byte-enable patterns must be translated to the correct AXI
     `WSTRB` lanes.
   - Unsupported patterns must be rejected explicitly, never silently converted
     to a full write.
   - Reads with byte enables must follow a documented policy rather than being
     ignored accidentally.
4. Validate pointer and length combinations before accessing the data buffer.
5. Preserve the response classification at the TLM boundary.

Required focused tests:

- partial write with contiguous byte enables;
- a disabled byte remains unchanged in target memory;
- non-contiguous byte-enable pattern, according to the selected support policy;
- repeating/wrapped streaming width;
- `TLM_IGNORE_COMMAND`;
- null data pointer with non-zero length;
- normal null-byte-enable read and write remain unchanged.

### F2 — Correct AXI response encoding

**Severity:** high

`src/noc_interconnect.cpp` currently uses numeric value `1` and labels it
`SLVERR`. AXI response encodings are:

| Value | AXI response |
|---|---|
| `0b00` | `OKAY` |
| `0b01` | `EXOKAY` |
| `0b10` | `SLVERR` |
| `0b11` | `DECERR` |

Required changes:

1. Stop using unexplained response literals in the interconnect.
2. Use named constants or a strongly typed model enum matching `axi_pkg`.
3. Map a downstream target failure that reached the target to `SLVERR`.
4. Map an interconnect address-decode failure to `DECERR` when represented on an
   AXI response path.
5. Preserve the existing TLM address-error response for a request rejected at
   the TLM boundary before injection.
6. Add B and R response-path tests that distinguish `EXOKAY`, `SLVERR`, and
   `DECERR`; a generic non-zero-only check is insufficient.

### F3 — Correct AXI byte-lane placement for narrow and unaligned accesses

**Severity:** high before Step A-3

`pack_beats()` currently places the first payload byte in lane zero and builds
`WSTRB` from bit zero, independent of `address % bus_bytes`. That is not the AXI
lane placement for a narrow access at a non-zero byte offset.

The current abstract endpoint/replay path can hide this defect because it
canonicalizes the data back into a linear TLM buffer. The defect will become
visible when Step A-3 drives real AW/W/AR signals through the timed chimney.

Required changes:

1. Define the supported alignment policy for the v0 wrapper.
2. For every supported transfer, derive WDATA, WSTRB, AxSIZE, AxLEN, and beat
   boundaries from both address and length.
3. Reconstruct read data from the correct returned AXI lanes.
4. Reject transfers that the chosen v0 policy cannot represent correctly.
5. Check region and AXI boundary crossing over the full transfer range, not only
   the first byte address.

Required directed tests:

- 32-bit access at byte offset `+4` on the 64-bit data bus;
- 16-bit access at byte offset `+6`;
- aligned 1-, 2-, 4-, and 8-byte accesses;
- supported unaligned access;
- a 6-byte and another odd-length tail;
- a transfer crossing an 8-byte beat boundary;
- a transfer crossing a mapped target-region boundary;
- readback and direct inspection of target memory;
- inspection of generated AXI request fields, not only final TLM data.

### F4 — Make TLM delay handling explicit and cycle-safe

**Severity:** medium

The wrapper spends simulated time and resets the caller's annotated delay to
zero, but currently discards any non-zero incoming delay. A downstream target's
annotated latency is converted to cycles by truncation, so any fractional cycle
is rounded down.

Required changes:

1. Define how a non-zero incoming `delay` is consumed before network injection.
2. Never silently discard caller time.
3. Convert target annotated delay to the first legal response cycle. A non-zero
   fractional cycle must not become zero latency.
4. Either carry sub-cycle residual time correctly or document and test a
   conservative ceiling policy.
5. Reject a zero or otherwise unusable network clock period at construction.
6. Document the contract for downstream targets that call `wait()` internally
   instead of annotating delay; such a call currently blocks the single network
   process.

Required tests:

- non-zero incoming delay;
- target delay of less than one network cycle;
- target delay of one and a fractional number of cycles;
- exact integer-cycle target delay;
- invalid zero clock period.

### F5 — Make `axi_manager_endpoint::offer()` failure atomic

**Severity:** medium; required before increasing outstanding concurrency

`axi_manager_endpoint::offer()` currently appends request flits before all
metadata-buffer and ordering-gate operations have succeeded. An exception can
leave a partially mutated endpoint.

Required changes:

1. Validate all preconditions and capacity before mutating any endpoint state.
2. Commit request flits, metadata, destination state, and ordering state as one
   logical operation.
3. On a refused or failed offer, `pending_`, the metadata buffer, destination
   state, and ordering counters must be unchanged.
4. Add a focused capacity/failure test that snapshots state before the failed
   offer and proves that nothing changed.

Do not remove `port_busy` or introduce concurrent outstanding TLM transactions
as part of this fix. That remains Step 10.5.

### F6 — Correct the `test_noc_interconnect` memory target

**Severity:** medium for test integrity

The test target can set `TLM_ADDRESS_ERROR_RESPONSE` in `access()`, after which
its `b_transport()` unconditionally overwrites the response with
`TLM_OK_RESPONSE`.

Required changes:

1. Set OK only if the access completed successfully.
2. Make `transport_dbg()` report the actual number of bytes served.
3. Add an out-of-range test proving the error is not overwritten.

## 4. Mandatory verification and coverage fixes

### V1 — Make the documented `test_noc_interconnect` coverage real

The current documentation claims that this test covers `AxSIZE` preservation,
per-node hold-off, and the self-node placement guard. The current test does not
directly prove those items.

Either add the missing checks or reduce the claim. For this cleanup, add the
checks because the same coverage is required by Step 10.3.

Required additions:

- 1-, 2-, 4-, 6-, and 8-byte transactions plus multi-beat bursts;
- exact AxSIZE/AxLEN/WSTRB inspection where applicable;
- a self-node placement configuration that must fail before traffic starts;
- target-latency tests that separate hop count from target delay;
- verification that `last_latency_cycles()` excludes only the requesting
  transaction's target hold-off;
- multiple initiators with a scoreboard;
- bounded completion for every transaction;
- a global watchdog;
- reset-time submission and idle-to-active wake-up;
- unmapped access and complete-range decode checks.

The current `while (!other->finished)` polling loop must become bounded.

### V2 — Strengthen negative-control auditability

The existing negative controls are documented in prose but are not generally
re-runnable from the repository.

Required changes:

1. Preserve the existing positive golden traces.
2. Add a small, maintainable negative-control mechanism where practical, or
   record exact reproducible patch/injection instructions and expected failing
   trace fields.
3. Do not generate expected CSV files from the SystemC implementation.
4. State clearly which negative controls are automated and which remain manual.

### V3 — Reconcile the pre-edge/post-edge rule

Rule 9b in `AI_HANDOFF_CONTEXT.md` says every cross-check samples both pre-edge
and post-edge. The route-select harness currently records only a post-edge row.

Required resolution:

- either upgrade the route trace to record both phases and regenerate its
  expected trace from the frozen RTL; or
- narrow the rule precisely, with a technical justification for combinational,
  content-only, and legacy signed harnesses.

Do not leave a universal rule that the existing harness violates.

### V4 — Remove current compiler warnings

With common warning flags enabled, `floo_router.hpp` reports `-Wreorder` because
the constructor initializer list does not follow member declaration order.

Required changes:

1. Reorder declarations or initializers without changing construction behavior.
2. Build the complete component with at least:

```text
-Wall -Wextra -Wpedantic
```

3. Record any deliberately accepted warning with a specific justification.

## 5. Mandatory documentation corrections

### D1 — Correct the chimney sign-off terminology

The documentation currently says "both chimney directions are signed", while
Step A correctly says that `axi_chimney_manager_response` is new and has no RTL
cross-check.

Until A-1 passes, use this exact distinction:

- manager request path timing: signed, 141 cycles;
- subordinate request reception and response generation: signed, 221 cycles;
- manager-side response unpacker: implemented and unit-tested, **not signed**;
- complete manager-AXI-to-subordinate-AXI composed path: **not signed**.

Correct at least:

- `docs/AI_HANDOFF_CONTEXT.md`;
- `docs/STATUS.md`;
- `README.md`;
- the P3/P7 phase summaries;
- the immediate continuation prompt.

### D2 — Define one execution order

The Step 10 execution table and the final continuation prompt currently disagree.
The table also incorrectly calls Step 10.3 the only unsigned correctness layer.

After this pre-A1 cleanup completes, the single authoritative order must be:

1. Step 10.2 — done;
2. Step A-1 — next;
3. Step A-2;
4. Step A-3;
5. remaining Step 10 work in an explicitly justified order;
6. Step 11 only after the detailed path is a valid calibration reference.

If Step 10.3 must precede A-2 or A-3 because its wrapper fixes are prerequisites,
state that explicitly in one place and copy the same order everywhere. Do not
leave two competing roadmaps.

### D3 — Reconcile the v0 timing target and definition of done

The frozen scope says the target is cycle-accurate from manager AXI port to
subordinate AXI port. The current definition of done omits A-1/A-2/A-3 and says
v0 is only three criteria short.

Choose and document one consistent definition:

- if full AXI-port composition is a v0 requirement, add A-1/A-2/A-3 to the v0
  completion gate; or
- if v0 ends at signed mesh plus model-level TLM integration, revise the timing
  target and clearly move full chimney composition into the next version.

Given that Direction A has already been selected, the recommended decision is to
make A-1/A-2/A-3 part of the v0 completion gate.

Also replace the statement "every included leaf has a standalone test — 29
tests" with an accurate statement. The CTest count is not the leaf-module count,
and `rr_arb_tree.hpp` and `meta_buffer.hpp` do not have direct standalone
`test_*.cpp` executables.

### D4 — Correct the `test_noc_interconnect` coverage matrix

Update `AI_HANDOFF_CONTEXT.md` and `STATUS.md` only after V1 is implemented.
Every claimed item must point to a concrete assertion or scenario in the test.

### D5 — Describe the firmware regression accurately

`run_firmware_regression.sh` runs:

1. the platform with a firmware ELF; and
2. the same platform without `--fw`, using the built-in synthetic traffic
   source.

The second run is not a second firmware image and is not equivalent to a
dedicated CPU alignment/readback ELF. Describe it as a substitute smoke path
with different and reduced coverage.

Also:

- do not claim that the two modes uniquely isolate DMA faults from all CPU/NoC
  faults;
- make build and log outputs safe for concurrent CTest/build-tree execution;
- strengthen the proof that the DMA base override reached the ELF;
- preserve logs and the existing timeout/error scanning behavior.

### D6 — Fix stale comments and auxiliary documents

Audit and correct at least:

- `include/floo_noc_model/axi_chimney.hpp`:
  it is no longer request-path-only;
- `include/floo_noc_model/axi_chimney_pack.hpp`:
  subordinate response timing is now signed;
- `include/floo_noc_model/axi_noc.hpp`:
  the mesh cross-check now exists;
- `include/floo_noc_model/axi_endpoint.hpp`:
  it supports multi-beat writes, and the final-beat strobe policy must be
  described accurately;
- `tests/test_axi_noc.cpp`:
  inter-node mesh timing is now signed;
- `docs/P0_SCOPE.md`:
  remove the remaining "output FIFO initially disabled" contradiction;
- `docs/RTL_MAPPING.md`:
  remove stale `future meta_buffer` and `future reorder_buffer` entries and add
  the manager-side response unpacker status;
- `README.md`:
  list all eleven existing RTL cross-check runners;
- `docs/AI_HANDOFF_CONTEXT.md`:
  include `test_axi_chimney_manager_response.cpp` in the file layout;
- `docs/FLOONOC_MODEL_IMPLEMENTATION_REPORT.vi.md`:
  update stale pre-Step-9/9.2 claims or label the relevant sections explicitly
  as a historical snapshot.

### D7 — Correct the `MaxUniqueIds` platform diagnostic

`platforms/noc_soc/src/noc_soc_top.cpp` currently says
`MaxUniqueIds = 1` permits only one outstanding transaction per manager.

Replace that message with the actual reason:

- `MaxUniqueIds = 1` selects an in-order metadata FIFO and does not impose a
  one-outstanding limit;
- the current one-transaction-per-upstream-port limit comes from
  `noc_interconnect` using one waiter and `port_busy`;
- observed low congestion is a result for the current wrapper and workload, not
  a general FlooNoC property.

### D8 — Document runtime mesh-size constraints

The wrapper currently instantiates only:

- 2x2;
- 3x3;
- 4x4;
- 4x2;
- 2x4.

Document that restriction in the public constructor contract and handoff, or
replace the fixed dispatch with an implementation that genuinely supports the
claimed runtime dimensions. Do not imply arbitrary rectangular sizes while
throwing for most of them.

### D9 — Correct Step 11 references

Step 11 currently asks the fast mode to reproduce the "section 10.11 latency
figures", but section 10.11 contains cross-check trace lengths, not end-to-end
latency targets.

Point the calibration requirement to the actual end-to-end latency section and
state that the current 11/30-cycle values include abstract transactors. After
A-3, record new full-chimney calibration values rather than silently retaining
the old ones.

## 6. Firmware-regression infrastructure fixes

The current script uses an in-source firmware `make clean`, a fixed default log
directory, and a substring grep of the disassembly.

Required changes:

1. Use a unique per-run log directory unless `LOG_DIR` is explicitly supplied.
2. Prevent concurrent invocations from racing over the same firmware output.
   Prefer an isolated output/build directory if supported; otherwise use a
   narrowly scoped lock with a useful timeout and diagnostic.
3. Verify the DMA base override using a deterministic symbol, relocation,
   disassembly expression, or runtime-visible value rather than the substring
   `10060` appearing anywhere.
4. Keep the real-firmware and synthetic-mode runs bounded.
5. Preserve complete logs on every failure.
6. Keep `SKIP_RETURN_CODE 77` behavior when the RISC-V toolchain is genuinely
   unavailable.

## 7. Step A-1 specification after this gate

This section is a future contract only. Do not implement it until section 8 is
complete.

The A-1 harness must drive `floo_rsp_in` on the unmodified frozen
`floo_axi_chimney.sv` and compare the SystemC
`axi_chimney_manager_response` path per cycle.

Minimum required coverage:

1. Accept an AW first, prove the B outstanding counter increments, then inject B
   and prove it decrements only on handshake.
2. Accept an AR first, prove the R outstanding counter increments, then inject a
   multi-beat R response.
3. Prove non-final R beats do not release the counter.
4. Prove the final R beat releases it exactly once.
5. Exercise manager B and R back-pressure independently.
6. Trace:
   - `axi_in_rsp_o.b`;
   - `axi_in_rsp_o.r`;
   - B and R valid/ready;
   - `floo_rsp_out_ready`;
   - B and R counter-bank state;
   - response channel and ID;
   - RLAST.
7. Refuse a request-channel value presented on the response link.
8. Sample pre-edge and post-edge using the synchronous BFM discipline.
9. Add bounded waits and a global watchdog.
10. Run a negative control that forces every R beat to look final; it must fail
    on the counter trace.

A-1 is the twelfth RTL cross-check only after the exact trace comparison and its
negative control pass.

## 8. Pre-A1 completion gate

All boxes below must be checked before starting A-1.

### Implementation

- [x] **F1** — done 2026-07-30. `src/noc_interconnect.cpp` `b_transport`
      validates command, length, data pointer, streaming width and byte-enable
      length before touching the payload; byte enables are translated to
      `WSTRB`. Evidence: `test_noc_interconnect::test_payload_contract`.
- [x] **F2** — done 2026-07-30, extended by R2-F4. `axi_pkg::axi_resp` mirrors
      `axi_pkg.sv`; `DECERR` for decode failure, `SLVERR` for target refusal.
      Evidence: `static_assert`s in `test_axi_types`, B/R propagation in
      `test_axi_endpoint`, both-direction refusal in `test_noc_interconnect`.
- [x] **F3** — done 2026-07-30, corrected by R2-F2 and R2-F3. Per-beat `WSTRB`,
      lane placement from `address % bus_bytes`, whole-range decode. Evidence:
      `test_axi_lanes` (13 shapes), `test_noc_interconnect` lane and sparse
      multi-beat cases.
- [x] **F4** — done 2026-07-30. Incoming delay is spent then cleared; target
      delay rounds up; a non-positive clock period is refused. Evidence:
      `test_noc_interconnect::test_delay_contract`,
      `test_noc_interconnect_bad_config`.
- [x] **F5** — done 2026-07-30. `offer()` validates, checks capacity, builds
      into a scratch list, then commits. Evidence: the atomicity block in
      `test_axi_endpoint`; negative control `offer-not-atomic`.
- [x] **F6** — done 2026-07-30. OK only on success, real `transport_dbg` byte
      count, byte enables honoured. Evidence: the out-of-range and partial-write
      cases in `test_noc_interconnect`.

### Verification

- [x] **V1** — done 2026-07-30. Bounded waits, global watchdog, scoreboard,
      `last_latency_cycles()` semantics, reset-time and wake-up, self-node
      rejection, exact `AxSIZE`/`AxLEN`/`WSTRB` via `test_axi_lanes`.
- [x] **V2** — done 2026-07-30, **superseded by R2-V1**. The runner exists;
      round 2 rewrote it to require a successful mutated build plus a failure
      for the expected reason, and to work off-tree.
- [x] **V3** — done 2026-07-30. Route-select records both phases (golden
      recaptured from RTL); rule 9b narrowed to cycle cross-checks with the
      three clockless harnesses tabulated.
- [x] **V4** — done 2026-07-30, **corrected by R2-F1**. `-Wall -Wextra
      -Wpedantic`, zero warnings. The helper target it originally used broke
      the parent export and is now a variable.
- [x] **Bounded waits and watchdog** — done 2026-07-30. `test_noc_interconnect`
      has per-wait deadlines and a `watchdog` module.

### Documentation

- [x] **D1** — done 2026-07-30, re-audited by R2-D1.
- [x] **D2** — done 2026-07-30, updated by R2-D2 to include both cleanup rounds.
- [x] **D3** — done 2026-07-30, count corrected by R2-D3 (five unmet, not three).
- [x] **D4** — done 2026-07-30.
- [x] **D5** — done 2026-07-30, corrected by R2-D4 (`--sim-us 2000`, private
      source copy, no in-source `make clean`).
- [x] **D6** — done 2026-07-30. The Vietnamese report is labelled a historical
      snapshot rather than rewritten.
- [x] **D7** — done 2026-07-30.
- [x] **D8** — done 2026-07-30.
- [x] **D9** — done 2026-07-30.

### Required regression

Run the compiler sanity commands first, then:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head

make BUILD_DIR=/tmp/floo_noc_pre_a1_fix_test test
```

Run all eleven existing RTL cross-check scripts with distinct `/tmp` build
directories where the scripts support an override:

```bash
./rtl_crosscheck/run_route_select_crosscheck.sh
./rtl_crosscheck/run_stream_fifo_crosscheck.sh
./rtl_crosscheck/run_wormhole_arbiter_crosscheck.sh
./rtl_crosscheck/run_router_crosscheck.sh
./rtl_crosscheck/run_axi_sizing_crosscheck.sh
./rtl_crosscheck/run_chimney_req_crosscheck.sh
./rtl_crosscheck/run_chimney_rsp_crosscheck.sh
./rtl_crosscheck/run_rob_crosscheck.sh
./rtl_crosscheck/run_chimney_timing_crosscheck.sh
./rtl_crosscheck/run_chimney_rsp_timing_crosscheck.sh
./rtl_crosscheck/run_mesh_crosscheck.sh
```

Run the platform firmware regression:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP

LOG_DIR=/tmp/noc_soc_pre_a1_fix_logs \
./platforms/noc_soc/tests/run_firmware_regression.sh
```

Final repository checks:

```bash
git diff --check
git status --short

git -C /home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC \
  rev-parse HEAD
git -C /home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC \
  status --short
```

Expected final evidence:

- standalone regression passes with all newly added cases;
- all eleven pre-existing RTL cross-checks remain exact;
- firmware reaches `DMA PASS`;
- synthetic mode reaches `result   all bytes match`;
- the frozen FlooNoC HEAD remains exactly
  `9a6972a5f9b8117506d1df8a6505ce1da2bc9084`;
- the frozen FlooNoC tree remains clean;
- `git diff --check` reports no error;
- no documentation claim exceeds the evidence produced by the tests.

Only after all of the above is true may the Agent begin Step A-1.
