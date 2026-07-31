# FlooNoC Model Pre-A1 Review — Round 3 Remediation Plan

## 1. Purpose and hard gate

This document records the findings from the third pre-A1 review, performed on
2026-07-31 after the Round 2 implementation was reported complete.

The positive regression is green:

- the fresh standalone build passed 32/32 tests;
- a fresh CDC-VP configure and `noc_soc` build passed;
- all eleven RTL cross-check runners passed, for fifteen comparisons in total;
- the firmware regression proved `__fw_dma_base = 0x10060000` and reached
  `DMA PASS`;
- the no-firmware synthetic survey reached `result   all bytes match`;
- the model negative-control runner reported 9 detected and 0 missed;
- `git diff --check` was clean;
- the frozen FlooNoC tree remained clean at
  `9a6972a5f9b8117506d1df8a6505ce1da2bc9084`.

Those results are necessary but not sufficient. Static review found two
functional correctness defects, incomplete invalid-configuration handling,
two verification gaps, an incomplete warning-policy application, and several
documentation contradictions.

**Do not begin Step A-1 until every mandatory item and every implementer
checkbox in section 12 is complete, and the separate reviewer sign-off is
checked by someone other than the implementer.**

Round 3 must not contain any A-1 implementation. A-1 remains the next functional
milestone after this gate closes: cross-check the manager-side response
unpacker against the unmodified frozen RTL.

## 2. Scope and constraints

### 2.1 Sources of truth

Use:

- FlooNoC RTL:
  `/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC`
- FlooNoC frozen revision:
  `9a6972a5f9b8117506d1df8a6505ce1da2bc9084`
- CDC-VP:
  `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP`
- Model component:
  `components/floo_noc_model`
- Previous remediation plans:
  `docs/PRE_A1_REVIEW_FIX_PLAN.md` and
  `docs/PRE_A1_REVIEW_ROUND2_FIX_PLAN.md`

Do not:

- edit the frozen FlooNoC RTL or its lock file;
- refresh an expected trace from SystemC and call that RTL evidence;
- redesign the NoC around the NPU example;
- start A-1, A-2, or A-3 in this remediation;
- discard unrelated dirty files in CDC-VP;
- hide a configuration failure with `std::_Exit()`, a fatal signal, or an
  unbounded test.

### 2.2 Mandatory build environment

Use this before **every** build:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head
```

Do not trust inherited `CC`, `CXX`, `PATH`, or `SYSTEMC_HOME`.

## 3. Mandatory functional fixes

### R3-F1 — Prevent AXI `AxLEN` truncation above 256 beats

**Severity:** high; silent read-data corruption

`axi_manager_endpoint::offer()` accepts an arbitrary beat count and then stores
`beats - 1` in an eight-bit `AWLEN` or `ARLEN`:

```cpp
aw.len = static_cast<std::uint8_t>(beats - 1);
ar.len = static_cast<std::uint8_t>(beats - 1);
```

AXI `AxLEN` is eight bits, so one burst can describe at most 256 beats. The
current cast wraps instead of rejecting an unrepresentable burst.

A concrete failing read is an aligned 2049-byte payload on the 64-bit data
path:

```text
beats = ceil(2049 / 8) = 257
ARLEN = uint8_t(257 - 1) = 0
subordinate expected beats = ARLEN + 1 = 1
```

Only one R beat is returned. `unpack_read()` stops when it runs out of beats,
but the TLM transaction can still receive an OK response, leaving most of the
caller buffer unchanged.

Relevant code:

- `include/floo_noc_model/axi_endpoint.hpp`, `offer()`;
- `include/floo_noc_model/axi_endpoint.hpp`,
  `axi_subordinate_endpoint::accept_request()`;
- `include/floo_noc_model/axi_lanes.hpp`, `shape_of()`;
- `src/noc_interconnect.cpp`, `b_transport()`.

Required resolution:

1. Define one named maximum AXI burst length derived from the eight-bit
   `AxLEN`, not a scattered literal.
2. Make `axi_manager_endpoint::offer()` reject a transaction with more than
   256 beats before changing any endpoint state.
3. At the TLM wrapper boundary, reject a payload whose generated shape needs
   more than 256 beats with a documented response, preferably
   `TLM_BURST_ERROR_RESPONSE`.
4. Reject before network injection and before the downstream target is called.
5. Preserve the all-or-nothing `offer()` contract: pending flits, metadata, AW
   destination state, and ordering counters must remain unchanged.
6. Do not silently split the payload unless a separate, fully specified
   transaction-splitting design is intentionally chosen. For v0, deterministic
   rejection is the smaller and safer policy.
7. State the selected maximum in the public wrapper contract.

Required tests:

- read with exactly 256 beats is accepted and emits `ARLEN = 255`;
- write with exactly 256 beats is accepted and emits `AWLEN = 255`;
- 257-beat read is rejected before any request flit or state change;
- 257-beat write is rejected before any request flit or state change;
- the TLM wrapper returns the selected error for a payload needing 257 beats;
- a spy target proves that the rejected TLM payload never reaches downstream;
- values immediately below and above the boundary are tested for both aligned
  and non-zero lane offsets.

Acceptance:

- no narrowing cast is reachable until the beat count has been proven
  representable;
- a 257-beat read cannot complete OK with partial data;
- a negative control that removes the maximum check builds successfully and
  fails for a specific expected message.

### R3-F2 — Make address-range arithmetic overflow-safe

**Severity:** high; explicitly required but not completed in R2-F3

The following arithmetic can wrap:

```cpp
address + length - 1
entry.base + entry.size
shape.beat0_addr + shape.beats * bus_bytes - 1
shape.lane_offset + length + bus_bytes - 1
```

The mapped-region decode is also inconsistent with
`reference_address_map::decode()`. The reference model correctly uses
subtraction:

```cpp
address >= base && address - base < size
```

but `noc_interconnect::impl::decode()` uses `address < base + size`. A valid
region whose last byte is `UINT64_MAX` makes `base + size` wrap to zero and
becomes inaccessible.

Relevant code:

- `src/noc_interconnect.cpp`, `impl::decode()`;
- `src/noc_interconnect.cpp`, the whole-range check in `b_transport()`;
- `src/noc_interconnect.cpp`, full-width `frame_end`;
- `include/floo_noc_model/axi_lanes.hpp`, `shape_of()`;
- `tests/test_noc_interconnect.cpp`.

Required resolution:

1. Use subtraction-based mapped-region decode after proving
   `address >= base`.
2. Validate target mappings when `add_target()` is called:
   - size must be non-zero;
   - `base + size - 1` must not wrap;
   - overlapping regions must be rejected before mutating the target table.
3. Check the TLM transaction's last byte without performing an unchecked
   addition.
4. Check the full AXI beat-frame multiplication and final addition before
   computing `frame_end`.
5. Make the arithmetic inside `shape_of()` well-defined for every accepted
   input. Either reject zero there or document that it has a precondition and
   enforce that precondition in every caller; a public helper must not perform
   `addr % 0`.
6. Use a sufficiently wide intermediate for lane-offset, length, and beat-count
   calculations.
7. Return a deterministic TLM address/burst error at the wrapper boundary.
   Do not allow an overflow exception to escape later from the network thread.
8. Keep write-lane reconstruction overflow checks as a defensive backstop, but
   do not rely on them as the first validation point.

Required tests:

- a valid one-byte region at `UINT64_MAX` decodes and is accessible;
- a valid larger region ending exactly at `UINT64_MAX` is accessible;
- a region whose last byte would wrap is rejected by `add_target()`;
- zero-sized and overlapping regions are rejected without consuming a target
  slot;
- a payload starting at `UINT64_MAX` with length two is rejected before
  injection;
- a payload whose requested bytes fit but whose widened beat frame crosses the
  address-space end is rejected before injection;
- the same cases close to an ordinary mapped-region end remain correct;
- spy targets prove rejected accesses are not called;
- configuration rejection leaves the object in a deterministic state.

Acceptance:

- no unchecked address-end addition remains in the wrapper;
- all Round 2 requirements for region-end and integer-overflow coverage are
  now represented by named tests;
- the tests fail if subtraction-based decode is changed back to
  `addr < base + size`.

### R3-F3 — Reject default `(0,0)` self-node conflicts before elaboration

**Severity:** medium; R2-F6 is not closed

The public contract says every initiator defaults to node `(0,0)`. The current
`add_target()` check uses `placed_only=true`, so an initiator for which the
platform did not call `place_initiator()` is ignored. A target added at `(0,0)`
is therefore accepted during ordinary configuration and rejected only from
`end_of_elaboration()`.

That retains the exact class of late SystemC configuration failure R2-F6 was
meant to remove. The current bad-configuration test calls
`place_initiator(0, {0,0})` explicitly and never exercises the public default.

Relevant code:

- `include/floo_noc_model/noc_interconnect.h`, default-placement contract;
- `src/noc_interconnect.cpp`, `placed`, `reject_self_node_targets()`,
  `end_of_elaboration()`, `add_target()`, and `place_initiator()`;
- `tests/test_noc_interconnect_bad_config.cpp`.

Required resolution:

1. Treat `(0,0)` as a real placement, because the public API documents it as
   the default.
2. A target added on an initiator's default node must be rejected directly by
   `add_target()`.
3. The reverse ordering must also be rejected directly: moving an initiator
   onto an existing target.
4. Validate before committing configuration state. A failed `add_target()` must
   not increment `mapped_targets` or leave a mapped invalid entry; a failed
   `place_initiator()` must not retain the rejected position.
5. Keep any elaboration-time check only as an unreachable defensive assertion,
   not as a required path for a valid public configuration sequence.
6. Preserve normal C++ and SystemC teardown. No `_Exit`, fatal signal, or hidden
   segmentation fault is acceptable.

Required tests:

- create one initiator and add a target at `(0,0)` without ever calling
  `place_initiator()`; `add_target()` must throw immediately;
- explicitly place an initiator first, then add a target on it;
- add a target first, then place an initiator on it;
- after a rejected addition, add a legal target and prove that no slot was
  consumed by the failed call;
- after rejected placement, retain or restore the previous legal position;
- a legal layout is still accepted;
- all test executables return normally without calling `sc_start()` merely to
  discover a configuration error.

Acceptance:

- no supported configuration sequence depends on
  `end_of_elaboration()` to discover a self-node conflict;
- every invalid case is bounded and returns normally;
- a negative control that restores `placed_only=true` is detected.

## 4. Mandatory verification fixes

### R3-V1 — Require a non-empty expected failure reason for every control

**Severity:** medium

`run_negative_controls.sh` currently registers an empty expected string for:

- `offer-not-atomic`;
- `r-burst-error-lost`.

The runner checks the test log only when the expected string is non-empty.
Those two controls can therefore be counted as detected when their test fails
for an unrelated reason.

Required resolution:

1. Give every control a stable, specific expected message.
2. Make `add_control()` or a registry-validation pass reject an empty:
   - control name;
   - source file;
   - needle;
   - test name;
   - expected message;
   - defect description.
3. Keep the current rule that a build failure is a missed control, not a
   detection.
4. Keep each mutation in its own private source copy under `/tmp`.
5. Preserve configure, build, and test logs for every control.

Suggested current messages:

- `offer-not-atomic`: the uncaught diagnostic contains
  `meta_buffer: read buffer overflow`; preferably make the test report a
  stable assertion describing the lost atomicity rather than relying only on a
  SystemC uncaught-exception wrapper;
- `r-burst-error-lost`:
  `a later OKAY beat must not erase an earlier SLVERR`.

Required self-checks:

- an empty expected string makes the runner fail before building;
- a syntax-breaking mutation is reported as missed;
- a behavior mutation that fails the test for a different message is reported
  as missed;
- all real controls build, execute, and fail with their own message;
- the restored positive suite remains green.

Acceptance:

- `9 detected, 0 missed` is printed only when all nine controls have a
  non-empty verified reason;
- the completion checklist must not claim "each with a required failure
  message" while any registry entry can bypass that check.

### R3-V2 — Test exact AXI-response to TLM-response mapping

**Severity:** medium; completes the remaining R2-F4 requirement

The current endpoint test carries all four response codes through raw B and R
flits. The wrapper test separately checks a target refusal and an address
rejected before injection. It does not test the actual mapping of a crafted AXI
response through the production response-to-TLM decision:

```text
OKAY   -> TLM_OK_RESPONSE
EXOKAY -> TLM_OK_RESPONSE
SLVERR -> TLM_GENERIC_ERROR_RESPONSE
DECERR -> TLM_ADDRESS_ERROR_RESPONSE
```

The existing unmapped wrapper test explicitly states that no DECERR flit
traverses the network.

Required resolution:

1. Put the response mapping in one named function used by production
   `b_transport()` completion handling.
2. Unit-test all four exact input values against their exact TLM status.
3. Keep the raw B and R endpoint propagation tests.
4. Keep the multi-beat R rule: the first error is retained and a final OKAY
   cannot erase it.
5. Do not claim that a boundary rejection exercised a DECERR flit.
6. Add negative controls, or otherwise record reproducible injections, for:
   - discarded B response;
   - earlier R error overwritten by final OKAY;
   - EXOKAY treated as an error;
   - SLVERR and DECERR exchanged at the TLM mapper.

Acceptance:

- every AXI response code has both an exact encoding assertion and an exact TLM
  mapping assertion;
- the production switch cannot drift separately from the tested mapping;
- every stated injection fails for the intended reason.

### R3-V3 — Apply the warning policy to every model test translation unit

**Severity:** low, but required for a truthful R2-F1 sign-off

`test_noc_interconnect` and `test_axi_lanes` do not receive
`${FLOO_NOC_MODEL_WARNINGS}`. The fresh compile database confirms that these two
sources are compiled without `-Wall -Wextra -Wpedantic`.

Relevant code:

- `tests/CMakeLists.txt`, the two manually declared executable targets.

Required resolution:

1. Apply `${FLOO_NOC_MODEL_WARNINGS}` to both targets.
2. Prefer a small helper for manually declared tests if it removes repetition
   without creating an exported link dependency.
3. Do not reintroduce the Round 2 export-set failure.
4. Verify the flags in a fresh `compile_commands.json`, not only by observing
   that the compiler printed no warning.

Acceptance:

- every component-owned `.cpp` compiled by the standalone model build receives
  the warning policy;
- the fresh standalone build has no diagnostic from model/test C++;
- the fresh CDC-VP parent configure and `noc_soc` build still succeed.

## 5. Mandatory documentation reconciliation

### R3-D1 — Remove all remaining chimney sign-off overclaims

The following known stale statements must be corrected:

- `rtl_crosscheck/run_mesh_crosscheck.sh` says the chimney is signed in both
  directions;
- `AI_HANDOFF_CONTEXT.md` section 10.7 is titled
  "signed, both directions, content and timing";
- `FLOONOC_MODEL_IMPLEMENTATION_REPORT.vi.md` says
  "ĐÃ RTL-signed cả hai chiều";
- `STATUS.md` says the chimney is cross-checked in both directions;
- `RTL_MAPPING.md` has duplicate chimney rows, one describing
  "`both directions`".

Use the four-part status consistently:

1. manager request path: signed, 141 cycles;
2. subordinate request reception and response generation: signed, 221 cycles;
3. manager-side response unpacker: implemented and unit-tested, **not signed**
   until A-1;
4. composed manager-AXI-to-subordinate-AXI datapath: **not signed** until A-1,
   A-2, and A-3 complete.

Do not use "both directions" as shorthand when it can be read as including the
manager response unpacker.

### R3-D2 — Keep exactly one execution order

The authoritative order is:

1. Step 10.2 — done;
2. pre-A1 cleanup Round 1 — implementation completed;
3. pre-A1 cleanup Round 2 — implementation reviewed, Round 3 gaps found;
4. pre-A1 cleanup Round 3 — current;
5. A-1 — next only after Round 3 reviewer sign-off;
6. A-2;
7. A-3;
8. Step 10.3;
9. Step 10.1;
10. Step 10.4;
11. Step 10.5;
12. Step 11.

The final continuation prompt currently says both:

- run the Step 10 substeps starting with 10.3; and
- A-1 first.

Remove that contradiction. The prompt must lead with A-1, A-2, A-3, and only
then Step 10.3.

Rename stale headings such as `DO SECOND` so they are relative to the actual
authoritative order, or remove ordinal words from individual headings.

### R3-D3 — Describe the firmware regression accurately

Remove the remaining claims that the no-`--fw` synthetic survey:

- is a second image;
- serves the same purpose as a dedicated CPU alignment/readback ELF;
- uniquely separates a DMA fault from a CPU/NoC fault;
- adds no less coverage than a second ELF.

State instead:

- the `--fw` run is real RISC-V firmware and uses `--sim-us 2000`;
- the no-`--fw` run is a synthetic substitute smoke path with reduced and
  different coverage;
- its result is diagnostic evidence, not a decision procedure;
- the automated firmware build uses a private source copy and does not require
  an in-source `make clean`;
- manual in-source build instructions may still need `make clean`, and must be
  labelled as manual.

### R3-D4 — Reconcile counts and completion states

At minimum:

- change the standalone count from 31 to 32 wherever it describes the current
  suite;
- keep historical counts explicitly labelled as historical;
- do not mark Round 3 complete in the handoff while the implementer or reviewer
  checklist is open;
- do not check the Round 2 final reviewer box retroactively;
- update the verification matrix only with evidence actually produced;
- retain the count of five unmet v0 criteria unless implementation changes one
  of those criteria.

Suggested documentation audit:

```bash
rg -n -i \
  'chimney.*both directions|both directions.*chimney|signed.*both directions|ĐÃ RTL-signed cả hai chiều' \
  components/floo_noc_model platforms/noc_soc

rg -n \
  'DO SECOND|DO THIRD|DO FOURTH|DO FIFTH|10\\.3.*next|A-1 first|31 tests|registers 31' \
  components/floo_noc_model platforms/noc_soc

rg -n \
  'second image|serves the same purpose|no extra coverage|make clean requirement' \
  components/floo_noc_model platforms/noc_soc
```

Occurrences inside review plans that quote an old defect are allowed only when
the surrounding text clearly marks them as historical findings.

## 6. Required standalone verification

Use a fresh build directory:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head

cmake -S . -B /tmp/floo_noc_pre_a1_round3 \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
  -DFLOO_NOC_MODEL_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build /tmp/floo_noc_pre_a1_round3 --parallel

ctest --test-dir /tmp/floo_noc_pre_a1_round3 \
  --output-on-failure
```

Also inspect warning coverage:

```bash
rg -n \
  'test_noc_interconnect\\.cpp|test_axi_lanes\\.cpp|test_axi_lanes_odr.*\\.cpp|noc_interconnect\\.cpp' \
  /tmp/floo_noc_pre_a1_round3/compile_commands.json
```

Expected:

- every test passes;
- all newly required boundary/configuration/mapping tests are registered;
- every component-owned C++ translation unit has
  `-Wall -Wextra -Wpedantic`;
- no model/test C++ warning is emitted;
- no test uses `std::_Exit()`;
- all tests are bounded.

## 7. Required CDC-VP parent build and firmware regression

Configure and build from a fresh parent directory:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head

cmake -S . -B /tmp/cdc_vp_noc_pre_a1_round3 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCDC_BUILD_NOC_SOC=ON \
  -DCDC_BUILD_TESTS=ON

cmake --build /tmp/cdc_vp_noc_pre_a1_round3 \
  --target noc_soc --parallel

NOC_SOC_BIN=/tmp/cdc_vp_noc_pre_a1_round3/platforms/noc_soc/noc_soc \
LOG_DIR=/tmp/noc_soc_pre_a1_round3_logs \
./platforms/noc_soc/tests/run_firmware_regression.sh
```

Expected:

- the parent configure itself returns zero;
- `noc_soc` builds;
- DMA-base proof reports `0x10060000`;
- firmware reaches `DMA PASS`;
- synthetic mode reaches `result   all bytes match`;
- no trap, SystemC error, mismatch, timeout, or hidden failure appears.

## 8. Required RTL and negative-control regression

Run every existing RTL runner:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head

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

./rtl_crosscheck/run_negative_controls.sh
```

The positive RTL results must remain exact. A negative-control PASS is valid
only when every registered mutation:

1. configures and builds;
2. executes its intended test;
3. makes that test fail;
4. matches a non-empty expected reason.

Do not change the frozen RTL or regenerate expected RTL traces from SystemC.

## 9. Repository hygiene

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP

git diff --check
git status --short

git -C /home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC \
  rev-parse HEAD
git -C /home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC \
  status --short
```

Expected:

- `git diff --check` prints no error;
- all CDC-VP changes are intentional and reviewable;
- frozen FlooNoC HEAD is exactly
  `9a6972a5f9b8117506d1df8a6505ce1da2bc9084`;
- frozen FlooNoC tracked status is empty;
- generated builds and mutation copies remain under `/tmp`.

## 10. Required implementation report

When the fixes are complete, append an evidence section to this file. For each
item include:

- files changed;
- root cause;
- selected policy;
- test names;
- negative-control name and required message;
- exact fresh build/test result;
- date verified.

Do not use only prose such as "fixed" or "tested". Link every claim to a test,
command result, or exact source location.

## 11. Expected file areas

The implementation will likely touch:

- `include/floo_noc_model/axi_endpoint.hpp`;
- `include/floo_noc_model/axi_lanes.hpp`;
- `include/floo_noc_model/noc_interconnect.h`;
- `src/noc_interconnect.cpp`;
- `tests/test_axi_endpoint.cpp`;
- `tests/test_axi_lanes.cpp`;
- `tests/test_noc_interconnect.cpp`;
- `tests/test_noc_interconnect_bad_config.cpp`;
- `tests/CMakeLists.txt`;
- `rtl_crosscheck/run_negative_controls.sh`;
- `docs/AI_HANDOFF_CONTEXT.md`;
- `docs/STATUS.md`;
- `docs/RTL_MAPPING.md`;
- `docs/FLOONOC_MODEL_IMPLEMENTATION_REPORT.vi.md`;
- `README.md`;
- `platforms/noc_soc/README.md`;
- `rtl_crosscheck/run_mesh_crosscheck.sh`.

This is guidance, not permission to rewrite unrelated files.

## 12. Round 3 completion gate

### Functional correctness

- [x] **R3-F1** — `axi_pkg::max_burst_beats`; `offer()` rejects before building, `b_transport` before injection.
- [x] **R3-F1** — boundaries tested at lane offsets 0, +1 and +7, each as an *adjacent* accepted/refused pair (2048/2049, 2047/2048, 2041/2042); spy target proves a rejected transfer never arrives. The public contract in `noc_interconnect.h` states the 256-beat limit, the response code, and why the wrapper refuses rather than splits.
- [x] **R3-F2** — subtraction-based decode, checked transaction end, division-checked frame multiply, `shape_of` rejects zero length and spans in 64 bits.
- [x] **R3-F2** — the top region is written and read back, and the byte at `UINT64_MAX` is separately written, read back and compared; control `region-decode-addition`.
- [x] **R3-F3** — `placed` removed; `(0,0)` is a real placement.
- [x] **R3-F3** — validation precedes commit in both calls; no slot consumed, no port moved; no `sc_start`, no `_Exit`. Atomicity is asserted through observable state — after a refused move, a target on the port's old node is still refused — and both halves carry controls (`self-node-check-skips-default`, `place-initiator-not-atomic`).

### Verification infrastructure

- [x] **R3-V1** — verified by blanking one expectation: the runner aborts before building.
- [x] **R3-V1** — 17 detected, 0 missed.
- [x] **R3-V2** — `noc_interconnect::tlm_status_for()` is called by both `b_transport` and the test.
- [x] **R3-V2** — all four registered as controls and detected.
- [x] **R3-V3** — 30 TUs, none without `-Wall`.

### Documentation

- [x] **R3-D1** — mesh runner, handoft 10.7 heading, RTL_MAPPING, STATUS and the Vietnamese report all corrected.
- [x] **R3-D2** — one table, 12 rows; ordinal words removed from step headings.
- [x] **R3-D3** — the survey is described as a reduced substitute smoke path, explicitly not a decision procedure.
- [x] **R3-D4** — 32 everywhere; Round 2 reviewer box left unchecked.

### Final evidence

- [x] configure 0, build 0.
- [x] 32/32.
- [x] exit 0.
- [x] exit 0.
- [x] proved from the ELF.
- [x] `DMA PASS`.
- [x] present in the survey log.
- [x] 15 comparisons, all exact.
- [x] 17 detected, 0 missed.
- [x] clean.
- [x] `9a6972a…`, 0 dirty files.
- [x] the three audit greps in section 5 return no overclaim. They do return
      matches outside the review plans, and the earlier wording of this line —
      "only historical quotations inside review plans" — was wrong about that.
      The matches are: `noc_interconnect.h` naming "both directions are signed"
      as the overclaim to avoid; `STATUS.md` recording that it *was* the wrong
      summary; a Step 6 record of what was unverified at that point in the
      sequence; a rule quoting "DO SECOND" as the kind of stale heading to
      avoid; and "**A-1 first**", which is the current instruction. Each is a
      negation, a correction, a dated record, or live guidance.

### Reviewer-only sign-off

- [x] A final human/AI reviewer independently finds no remaining pre-A1
      blocker. **Signed 2026-07-31**, after a fourth review round. Verdict:
      *"Đủ điều kiện để đi tiếp A-1 — reviewer sign-off: PASS."*

      Ticked on the reviewer's instruction, not by the implementer's own
      judgement. The round-4 findings were seven acceptance and documentation
      defects — three of them tests that could not observe the defect they were
      named after, proven by mutation — and a round-5 pass that found one
      remaining wrong comment (`AWADDR`) and no code, test or roadmap conflict.
      All are fixed; see sections 14 and 15.

**The implementer must leave the reviewer-only box unchecked. Only after that
box is checked may the next agent begin Step A-1.** That box is now checked, so
Step A-1 is unblocked.

---

## 13. Implementation report — 2026-07-31

### R3-F1 — AxLEN truncation

- **Files:** `include/floo_noc_model/axi_types.hpp`,
  `include/floo_noc_model/axi_endpoint.hpp`, `src/noc_interconnect.cpp`,
  `tests/test_axi_endpoint.cpp`, `tests/test_noc_interconnect.cpp`.
- **Root cause reproduced before fixing.** A standalone probe against the
  unmodified endpoint printed
  `offer(257 beats) = 1, ARLEN = 0  (a subordinate reads this as 1 beats)`.
- **Policy:** deterministic rejection, not splitting. Splitting needs its own
  ordering and response-collapse rules and v0 has none.
  `axi_pkg::max_burst_beats = 1u << len_width` is the single named constant.
- **Enforced twice:** `offer()` throws before building anything, so the
  narrowing cast is unreachable with an unrepresentable count; `b_transport`
  returns `TLM_BURST_ERROR_RESPONSE` before injection.
- **Tests:** `test_axi_endpoint` — 256-beat read/write accepted with
  `ARLEN`/`AWLEN` = 255, 257-beat read and write rejected with no queued flit
  and no counter movement. `test_noc_interconnect::test_burst_length_limit` —
  the same boundary at the wrapper, at lane offset 0 and +1, plus a spy target
  proving a rejected transfer never arrives.
- **Negative control:** `axlen-truncated`, expects
  `257-beat read must be rejected`.

### R3-F2 — Overflow-safe address arithmetic

- **Files:** `src/noc_interconnect.cpp`, `include/floo_noc_model/axi_lanes.hpp`,
  `tests/test_noc_interconnect.cpp`, `tests/test_noc_interconnect_bad_config.cpp`.
- **Root cause:** `impl::decode()` used `addr < base + size`, which wraps for a
  region whose last byte is `UINT64_MAX`. `reference_address_map::decode()` had
  always used subtraction; the wrapper's copy had drifted.
- **Changes:** subtraction-based decode; `add_target()` validates non-zero size,
  non-wrapping end and non-overlap **before** consuming a slot; the transaction
  end and the beat frame are checked by subtraction and a division-checked
  multiply; `shape_of()` rejects `length == 0` rather than evaluating `addr % 0`,
  and computes the beat span in 64 bits.
- **Tests:** `test_noc_interconnect::test_address_space_end` — a payload running
  off the address-space end refused, a beat frame crossing a region end refused,
  the same shape inside the region accepted, and a region ending at `UINT64_MAX`
  **written and read back** including the byte at `UINT64_MAX` itself.
  `test_noc_interconnect_bad_config` — zero-sized, wrapping and overlapping
  regions refused, and a refused call proven not to consume a slot.
- **Negative control:** `region-decode-addition`, expects
  `region ending at UINT64_MAX must be writable`.
- **The control found a gap in the first version of this test.** The test
  originally only checked that `add_target` *accepted* the top region; the
  decode defect only shows on *access*, so the control was reported `MISSED`
  until a real read/write was added.

### R3-F3 — Default `(0,0)` is a real placement

- **Files:** `src/noc_interconnect.cpp`,
  `tests/test_noc_interconnect_bad_config.cpp`.
- **Root cause:** `placed_only=true` skipped ports the platform never placed, so
  a target at the documented default `(0,0)` was accepted during configuration
  and refused only from `end_of_elaboration()` — the late failure R2-F6 existed
  to remove.
- **Changes:** the `placed` vector is gone; `(0,0)` counts like any other
  placement. Both `add_target()` and `place_initiator()` validate **before**
  committing, so a refusal consumes no slot and does not move a port.
- **Accepted cost, stated:** `add_target(...,{0,0})` followed by
  `place_initiator(0,{1,1})` is now refused although it would have ended legal.
  A configuration that depends on moving a port afterwards relies on a transient
  the public contract does not promise.
- **Tests:** target on an unplaced port's default node refused by `add_target`;
  reverse ordering refused; refused calls proven not to consume a slot or move a
  port; a legal layout still accepted. All return normally, no `sc_start`.

### R3-V1 — Every control has a required reason

- **Files:** `rtl_crosscheck/run_negative_controls.sh`,
  `tests/test_axi_endpoint.cpp`.
- `add_control()` rejects an empty name, file, needle, test, expect or why, and
  the expected-message check is now unconditional. Verified: blanking one
  expectation aborts the runner before any build.
- `offer-not-atomic` now fails on a stable assertion
  (`must refuse the offer, not throw`) rather than on an uncaught SystemC
  exception wrapper.
- **14 controls, 14 detected, 0 missed.**

### R3-V2 — The response mapping is one named function

- **Files:** `include/floo_noc_model/noc_interconnect.h`,
  `src/noc_interconnect.cpp`, `tests/test_noc_interconnect.cpp`.
- `noc_interconnect::tlm_status_for()` is what `b_transport` calls and what the
  test asserts on, so the two cannot drift.
- All four codes asserted exactly, plus `SLVERR != DECERR`.
- **Negative controls:** `tlm-mapping-slverr-decerr-swapped`,
  `tlm-mapping-exokay-as-error`, `b-response-discarded`, `r-burst-error-lost`.

### R3-V3 — Warning policy

- **Files:** `tests/CMakeLists.txt`.
- **Confirmed the finding first:** the fresh compile database showed
  `test_noc_interconnect.cpp` and `test_axi_lanes.cpp` compiled without the
  flags. The round-2 sign-off had checked that no warning was *printed* rather
  than that the flags were *passed* — a compiler that is not asked will not
  complain.
- A `floo_model_test_options()` helper is applied to every test target.
- **Verified in `compile_commands.json`:** 30 translation units, none without
  `-Wall`.

### Gate results, 2026-07-31

```text
standalone (fresh)      configure 0, build 0, 32/32 tests, 0 warnings
compile_commands.json   30 TUs, 0 without -Wall
parent (fresh)          configure 0, build 0
firmware regression     DMA base 0x10060000, DMA PASS, all bytes match
11 RTL cross-checks     15 comparisons, all exact
negative controls       14 detected, 0 missed
git diff --check        clean
FlooNoC frozen          9a6972a5f9b8117506d1df8a6505ce1da2bc9084, 0 dirty
no std::_Exit in any test
```

---

## 14. Round-3 reviewer findings and their remediation — 2026-07-31

The round-3 reviewer withheld sign-off. The verdict was that the F1/F2/F3
implementations were correct but that the *acceptance* around them was not: some
of the tests could not observe the defect they were named after, one required
control was missing, and several documents still overclaimed. All seven findings
were reproduced before being fixed.

Three of them share one root cause worth stating plainly: **a test that asserts a
response code without arranging for the guard under test to be the only thing
that could produce it proves nothing.** In each case a mutation deleting the
guard left the test green.

### F1 — the beat-frame test never reached the beat-frame guard

`test_noc_interconnect` asserted `near_base + region_size - 4` with length 8.
`region_size` is `0x1000`, so the region ends bus-aligned and the *requested*
last byte already falls outside it. The whole-range decode rejects that several
checks earlier; the frame guard was never evaluated. Deleting the guard entirely
left the test passing.

Reaching the guard needs a region whose end is **not** bus-aligned. Added
`frame_base`, region `0x1004`, kind `memory`, bound to a spy target backed by
`0x2000` bytes:

- a 6-byte access at `+0x0FFE` has every requested byte inside the region and a
  beat frame running to `+0x1007`, four bytes past it;
- `memory` kind, so the widened-read policy lets the read through and the frame
  guard is the only remaining check;
- backing storage larger than the region, so an unguarded overrun does **not**
  fail on its own — it lands inside the backing store and reports success. Both
  the response code and `accesses.empty()` are asserted, on the write path and
  the read path.

New test `test_beat_frame_guard`; new control `beat-frame-guard-removed`. The
old assertion is kept, renamed to say what it actually tests (the whole-range
decode) so it cannot be mistaken for frame coverage again.

### F2 — `place_initiator()` atomicity was not observable

The test re-placed the port at its original node and checked the call succeeded.
An implementation that commits the position *before* throwing also passes that:
moving a port to an empty node is legal wherever the port started. The mutation
was injected and the test stayed green.

Atomicity is now read out of observable state: after a refused move, adding a
target on the port's **old** node must still be refused. If the rejected move had
been committed, that node would be free and the addition would succeed. A second
check moves the port for real and confirms the old node then opens up, using a
distinct address so it cannot fail as a side effect of the first.

New control `place-initiator-not-atomic`, detected with exactly one failing
assertion — its own.

### F3 — no negative control for the `placed_only` regression

Section 3 required "a negative control that restores `placed_only=true` is
detected"; none existed. There is no `placed` flag left to restore, so the
control reproduces its observable rule instead: skip the self-node conflict check
when the target is going onto node `(0,0)`, where every unplaced port sits. That
brings back exactly what R3-F3 removed — a target on an unplaced port's
documented default accepted at configuration time and rejected later from
`end_of_elaboration()`.

New control `self-node-check-skips-default`.

### F4 — the burst limit was neither published nor tested at its edge

Two separate gaps.

`noc_interconnect.h` documented "bursts are split into beats" and never stated
the ceiling. The contract now states the 256-beat limit, that the wrapper
**refuses** rather than splits, the response code, that the target is never
called, and that the limit applies to the *beat frame* rather than the payload —
so the longest accepted payload is 2048 bytes at `+0`, 2047 at `+1` and 2041 at
`+7`. It also records why splitting was rejected: it needs an ordering rule
between the pieces and a way to combine their responses, and `MaxUniqueIds = 1`
does not leave the ordering free to choose.

The test's own comment was wrong. It claimed 2040 bytes was the last 256-beat
transfer at `+1`; the beat count is `ceil((lane_offset + length) / 8)`, so:

```text
ceil((1 + 2047) / 8) = 256 beats   accepted
ceil((1 + 2048) / 8) = 257 beats   refused
```

2040 is seven bytes short of the edge, so an off-by-one in the beat count would
have passed on the accepting side. Every offset now pins an **adjacent** pair,
and `+7` was added as the widest lane offset.

### F5 — the continuation prompt contradicted the execution order

One paragraph ordered `10.2 -> 10.3 -> 10.1 -> 10.4 -> 10.5`; the next said
"A-1 first". Section 14's table is authoritative and puts A-1, A-2 and A-3 ahead
of 10.3. The prompt now carries the full remaining order as one sequence, states
why A-3 precedes 10.3, and says explicitly that section 14 wins on conflict.

### F6 — chimney documentation still overclaimed

`STATUS.md` said the chimney was "cross-checked in both directions" and that
"what is left is no longer datapath modelling". Neither is true: the manager-side
response unpacker is unsigned, and A-1 to A-3 are datapath work. Both statements
were rewritten to name the three signed quadrants, the unsigned fourth, and what
each of A-1, A-2 and A-3 does to the datapath.

`RTL_MAPPING.md` had duplicate rows for `axi_chimney.hpp` — and, unflagged but
the same defect, for `meta_buffer.hpp`, `rob_order_gate.hpp`, `axi_endpoint.hpp`
and `noc_interconnect`. Merged to one row per file, keeping the accurate text.

### F7 — an evidence claim the test did not support

The section 13 report said the byte at `UINT64_MAX` was written and read back.
The test only wrote it. Accepting a write proves the decode matched; it takes the
read to prove the byte that came back is the one that went in, at precisely the
address where a wrapping arithmetic error lands. The read-back and comparison
were added.

### Gate after remediation

```text
standalone (fresh)      configure 0, build 0, 32/32 tests, 0 warnings
compile_commands.json   30 TUs, 0 without -Wall
parent (fresh)          configure 0, build 0
firmware regression     DMA base 0x10060000, --fw PASS, survey PASS
                        re-run against a freshly rebuilt in-tree binary
11 RTL cross-checks     15 comparisons, all exact
negative controls       17 detected, 0 missed   (14 -> 17)
git diff --check        clean
FlooNoC frozen          9a6972a5f9b8117506d1df8a6505ce1da2bc9084, 0 dirty
```

The reviewer-only box in section 12 remains unchecked. It is the reviewer's to
check, not the implementer's, and these findings are why.

---

## 15. Round-5 review — sign-off — 2026-07-31

One finding, non-blocking, and it corrected a claim rather than a behaviour.

### `AWADDR` described wrongly

Section 14's beat-frame rationale said `AWADDR` is "beat 0's aligned address".
It is not. `src/noc_interconnect.cpp` sets `txn.addr` to the raw TLM address,
and `axi_endpoint.hpp` assigns `aw.addr = txn.addr` unchanged, so `AWADDR` is
the possibly unaligned transaction start. The aligned beat-0 base exists only
inside `axi_shape::beat0_addr`, which is a model-side computation and never
reaches an AXI field.

The argument survives the correction, and is now stated as it actually works:
`AWSIZE` and `AWLEN` define the beat sequence, and it is that sequence's final
beat that crosses the region boundary. The frame is still a property of the
burst rather than of this wrapper's strobe-exact replay, which is why the write
is refused even though nothing is corrupted today.

Comment-only change to `tests/test_noc_interconnect.cpp`; no code, no test
outcome, no roadmap item affected.

### Sign-off

The reviewer found no remaining code, test or roadmap defect and signed the
section 12 box. Evidence at sign-off:

```text
standalone              32/32 PASS
warning coverage        30/30 TUs carry -Wall -Wextra -Wpedantic
negative controls       17 detected, 0 missed
git diff --check        clean
generated build files   none inside the component
FlooNoC frozen          9a6972a5f9b8117506d1df8a6505ce1da2bc9084, clean
parent / firmware / 11 RTL runners
                        unaffected — the round-5 change is a comment
```

**Step A-1 is unblocked.** A checkpoint commit was taken before it started, so
the pre-A1 state is recoverable.
