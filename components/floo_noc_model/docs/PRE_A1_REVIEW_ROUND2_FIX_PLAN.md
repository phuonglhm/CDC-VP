# FlooNoC Model Pre-A1 Review — Round 2 Remediation Plan

## 1. Purpose and hard gate

This document records the findings from the second review of the pre-A1 work,
performed on 2026-07-31.

The first remediation pass fixed many real defects and expanded the standalone
regression from 29 to 31 tests. The exercised regressions are green, but the
pre-A1 gate is **not closed**: the review found two functional blockers, one
parent-project CMake failure, incomplete response-path verification, and several
verification/documentation inconsistencies.

**Do not start Step A-1 until every mandatory item and every checkbox in
section 12 of this document is complete.**

Step A-1 remains unchanged: cross-check the manager-side response unpacker
against the unmodified frozen RTL. Do not mix A-1 implementation into this
cleanup.

The review baseline was:

- standalone SystemC regression: **31/31 passed**;
- all eleven existing SystemC-to-RTL cross-checks passed when the leaf
  dependency checkout was mirrored under `/tmp`;
- six automated model-level negative controls were detected;
- the platform firmware run reached `DMA PASS`;
- the no-firmware synthetic survey reached `result   all bytes match`;
- the firmware ELF exported
  `__fw_dma_base = 0x10060000`;
- the frozen FlooNoC revision remained
  `9a6972a5f9b8117506d1df8a6505ce1da2bc9084`;
- the frozen FlooNoC tree remained clean;
- `git diff --check` was clean;
- a fresh CDC-VP parent configure **failed**, as described in R2-F1.

Passing tests prove only the exercised cases. They do not invalidate the
findings below.

## 2. Non-negotiable working rules

1. Preserve all unrelated user changes in CDC-VP and FlooNoC.
2. Do not modify the frozen FlooNoC RTL.
3. Do not regenerate an expected CSV from the SystemC implementation.
4. Put build products and temporary dependency worktrees under `/tmp`.
5. Fix root causes and add directed regressions; do not weaken existing checks.
6. Every wait in a test must be locally bounded, and long-running tests need a
   global watchdog.
7. Keep the complete path described honestly:
   - manager request timing: signed, 141 cycles;
   - subordinate request reception and response generation: signed, 221 cycles;
   - manager-side response unpacker: implemented and unit-tested, **not yet
     RTL-signed**;
   - complete manager-AXI-to-subordinate-AXI composition: **not signed**.
8. Before **every build**, run exactly:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head
```

## 3. Mandatory functional fixes

### R2-F1 — Repair CDC-VP parent-project CMake generation

**Severity:** blocker

A fresh parent configure fails with:

```text
install(EXPORT "cdc-components-targets" ...) includes target
"noc_interconnect" which requires target "floo_noc_model_warnings"
that is not in any export set.
```

Root cause:

- `noc_interconnect` is an installed static library;
- it links privately to the `floo_noc_model_warnings` interface target;
- a private dependency of a static library can still appear as a link-only
  export dependency;
- the warning target is intentionally not exported.

Relevant code:

- `components/floo_noc_model/CMakeLists.txt`, the
  `install(TARGETS noc_interconnect ...)` block and the
  `target_link_libraries(noc_interconnect PRIVATE
  floo_noc_model_warnings)` line.

Required resolution:

1. Do not make the warning-policy helper part of an installed target's exported
   dependency graph.
2. Apply `-Wall -Wextra -Wpedantic` directly to the component's own compiled
   target with private compile options, or use another export-safe arrangement.
3. Continue applying the warning policy to standalone tests and header
   consumers inside this project.
4. Verify both standalone and parent-project configure/build paths from fresh
   build directories.

Acceptance:

- fresh standalone configure succeeds;
- fresh CDC-VP root configure with `CDC_BUILD_NOC_SOC=ON` succeeds with exit
  code zero;
- `noc_soc` builds;
- no export-set diagnostic remains;
- the component remains warning-clean.

### R2-F2 — Preserve original AXI beat coordinates for sparse multi-beat writes

**Severity:** blocker; data corruption

`noc_interconnect::impl::absorb_request()` reconstructs the enabled byte range,
then changes `entry.addr` to the lowest enabled byte. Later,
`perform_downstream()` recomputes `beat0_addr` from this new address while
indexing `entry.write_data` and `entry.write_strb` with the original beat
numbers.

This works only while the lowest enabled byte remains in the original first
beat. It fails when one or more complete leading beats have zero `WSTRB`.

Concrete failing case:

```text
TLM address: aligned
TLM length:  16 bytes
WSTRB[0]:    0x00
WSTRB[1]:    0x01
```

The enabled byte belongs to lane 0 of original beat 1. After `entry.addr` moves
to that byte, downstream reconstruction treats original beat 1 as though it
were one beat later. The byte can be dropped while the write still returns
success.

Relevant code:

- `src/noc_interconnect.cpp`, `served_request`;
- `absorb_request()`, especially the original `beat0_addr`, `lowest`, and
  `entry.addr = lowest`;
- `perform_downstream()`, where `beat0_addr` is recomputed.

Required resolution:

1. Preserve the original AW address and/or original bus-aligned beat-0 address
   in `served_request`.
2. Never reinterpret original `write_data[beat]` or `write_strb[beat]` relative
   to a compacted downstream address.
3. Build downstream data and byte enables by mapping each enabled original
   `(beat, lane)` to its absolute byte address.
4. Keep an all-disabled write legal and side-effect free.
5. Add overflow-safe address arithmetic while touching this mapping.

Required directed end-to-end tests:

- two-beat write with the first beat fully disabled and the second enabled;
- two-beat write with the first enabled and the second fully disabled;
- three-beat write with only the middle beat enabled;
- sparse lanes in a later beat;
- a repeating short TLM byte-enable array spanning multiple beats;
- direct inspection of target memory before and after the write;
- proof that bytes outside the enabled absolute addresses are unchanged;
- all-disabled write returns OK and does not call or modify the target.

Do not test only `axi_lanes::pack_write()`. At least one regression must traverse
`noc_interconnect`, the abstract endpoints, the mesh, and downstream replay.

### R2-F3 — Define a side-effect-safe policy for unaligned and odd-length reads

**Severity:** high

For a full-width read, `perform_downstream()` aligns the downstream TLM address
down to the bus boundary and reads the entire AXI beat frame. A request for six
bytes at offset `+5`, for example, becomes a larger aligned downstream access.

This is not observable with a plain RAM target when only final readback bytes
are checked. It is observable for MMIO targets:

- an address before or after the requested TLM range can be touched;
- neighbouring registers can have read side effects;
- a narrow peripheral can reject the widened access;
- the target sees a different address and length from the caller's request.

Relevant code:

- `include/floo_noc_model/axi_lanes.hpp`, `shape_of()`;
- `src/noc_interconnect.cpp`, the `access_addr` and downstream `length`
  calculations in `perform_downstream()`;
- `tests/test_noc_interconnect.cpp`, whose odd/unaligned readback target is RAM.

Required decision:

Choose and document one v0 policy:

1. represent the requested read exactly through legal AXI transactions; or
2. reject read shapes that cannot be represented without touching bytes outside
   the requested range; or
3. explicitly constrain widened reads to memory-like targets through a real,
   enforceable target capability contract.

A prose statement alone is insufficient. The implementation must enforce the
selected policy.

Required tests:

- a spy/side-effect target records every downstream address and length;
- no successful read touches an address outside its declared supported range;
- every rejected shape returns a specific documented TLM response and never
  calls the target;
- aligned 1-, 2-, 4-, and 8-byte reads remain correct;
- aligned multi-beat reads remain correct;
- unaligned and odd-length cases follow the selected policy explicitly;
- region-end and integer-overflow cases are covered.

### R2-F4 — Complete AXI response encoding and propagation verification

**Severity:** high for the pre-A1 gate

The named `axi_resp` encoding is now correct, but the original acceptance
criterion required B- and R-path tests that distinguish:

```text
OKAY   = 0b00
EXOKAY = 0b01
SLVERR = 0b10
DECERR = 0b11
```

The current `test_noc_interconnect` unmapped-address case is rejected directly
at the TLM boundary before AXI injection. It does not prove a DECERR B or R
response path. The target-refusal case covers a read/TLM result, but not both B
and R raw response propagation.

Required changes:

1. Add compile-time or unit checks for all four exact enum values.
2. Drive crafted B responses through the model path for `EXOKAY`, `SLVERR`, and
   `DECERR`; assert the raw completion response and the intended TLM mapping.
3. Do the same for R responses, including a multi-beat R burst.
4. Verify that an intermediate R-beat error is not lost if the final beat is
   OKAY. Define how multiple R responses collapse into one TLM response.
5. Keep an upstream address rejected before injection as a TLM address error;
   do not falsely claim that this case exercised an AXI DECERR flit.
6. Add both read and write target-refusal tests so R/SLVERR and B/SLVERR are
   exercised.

Acceptance:

- the tests fail if `SLVERR` and `DECERR` are exchanged;
- the tests fail if `EXOKAY` is treated as an error;
- the tests fail if a B response code is discarded;
- the tests fail if an earlier R-beat error is overwritten by a later OKAY.

### R2-F5 — Make `axi_lanes.hpp` safe in multiple translation units

**Severity:** medium; packaging/link blocker when reused

`shape_of()`, `byte_enabled()`, `pack_write()`, and `unpack_read()` are defined
in a public header with external linkage and without `inline`.

This violates the one-definition rule when multiple translation units include
the header in one executable or shared library. The existing tests use one
source file per executable, so they do not expose it.

Required resolution:

- mark the header definitions `inline`, or move non-template implementations to
  one compiled source file with declarations in the header;
- preserve the existing public namespace and behavior.

Required test:

- compile and link a small test target containing at least two `.cpp` files that
  both include `axi_lanes.hpp`.

### R2-F6 — Make invalid configuration rejection SystemC-safe

**Severity:** medium

The invalid-clock test currently calls `std::_Exit()` because a failed
construction/elaboration leaves the SystemC hierarchy inconsistent and normal
teardown segfaults.

Root causes to address:

- the mesh is constructed before the clock period is validated;
- self-node placement is detected only during end-of-elaboration;
- one process attempts to continue using the same SystemC kernel after a failed
  construction/elaboration.

Relevant code:

- `src/noc_interconnect.cpp`, `impl` member declaration and initializer order;
- `tests/test_noc_interconnect_bad_clock.cpp`.

Required resolution:

1. Validate the clock before creating the mesh hierarchy.
2. Detect manager/target self-node conflicts as soon as the configuration is
   complete, without leaving the kernel half-elaborated.
3. If SystemC requires separate processes for independent invalid
   configurations, register separate CTest executables.
4. Remove `std::_Exit()` and allow normal C++/SystemC teardown.
5. Do not convert a configuration error into a hang or fatal signal.

Acceptance:

- both invalid cases are rejected deterministically;
- the tests return normally;
- no segmentation fault is hidden;
- the tests remain bounded.

## 4. Mandatory verification-infrastructure fixes

### R2-V1 — Make negative controls prove behavioral coverage

**Severity:** medium

`rtl_crosscheck/run_negative_controls.sh` currently counts a build failure as
successful detection. A malformed mutation or syntax error can therefore pass
without proving that a test detects the intended behavioral defect.

The script also edits the active source tree in place and restores it from a
temporary backup. `SIGKILL`, power loss, or an interrupted host can leave user
sources mutated.

Required changes:

1. A behavioral control counts as detected only if:
   - the mutated source builds successfully; and
   - the named test executes and fails for the expected reason.
2. A build failure is a control failure, not a successful detection.
3. Run mutations in a temporary source copy or temporary Git worktree, never in
   the active dirty worktree.
4. Preserve a per-control build log and test log on failure.
5. Print the temporary evidence directory before exit.
6. Keep the restored/unmodified positive suite green.
7. Keep the automated/manual classification explicit.

Acceptance:

- all existing controls build successfully and make their named tests fail;
- a deliberately syntax-breaking mutation makes the negative-control runner
  fail;
- killing a control does not modify the user's source tree;
- two concurrent negative-control runs do not share sources or build output.

### R2-V2 — Make frozen leaf dependency handling non-destructive

**Severity:** medium for reproducibility

`rtl_crosscheck/fetch_rtl_deps.sh` says it never rewrites dependency sources,
but it executes an unconditional:

```bash
git -C "$common_cells_dir" checkout --quiet --detach "$frozen_common_cells_rev"
```

This writes Git metadata and can require write access even when the correct
revision and hashes are already present. The default path also lives outside
the model's `/tmp` build area.

Required changes:

1. If an existing checkout is already at the exact revision and all compiled
   files match the frozen hashes, verify it read-only and do not run checkout.
2. If materialisation is needed, use a unique or concurrency-safe directory
   under `/tmp` by default, or an explicitly supplied cache directory.
3. Do not overwrite a dirty dependency checkout.
4. Preserve revision and per-file SHA-256 verification.
5. Keep network fetching explicit and produce a useful offline diagnostic.
6. Ensure parallel cross-check runners cannot race on an index lock.

Acceptance:

- all eleven runners work from a read-only verified dependency checkout;
- two runners can execute concurrently without `index.lock` failure;
- no tracked dependency source is modified;
- wrong revision or wrong file hash still fails.

### R2-V3 — Add regressions for every newly discovered gap

At minimum, the regression set added by this remediation must include:

- sparse multi-beat WSTRB with fully disabled leading beats;
- the selected odd/unaligned read policy against a spy/side-effect target;
- exact B and R response-code propagation;
- multi-translation-unit `axi_lanes.hpp` linkage;
- clean invalid-configuration teardown;
- CDC-VP parent configure as a CI/testable smoke check where practical.

Do not count a helper-only arithmetic test as end-to-end wrapper coverage.

## 5. Mandatory firmware-regression fix

### R2-FW1 — Never skip DMA-base proof silently

**Severity:** low, but required by the deterministic-evidence contract

`platforms/noc_soc/tests/run_firmware_regression.sh` verifies
`__fw_dma_base` only if `riscv-none-elf-nm` exists. A machine with the compiler
but without `nm` silently skips the proof.

Required resolution:

- require `riscv-none-elf-nm` when the firmware regression runs; or
- use another mandatory tool from the same toolchain to verify the absolute
  symbol;
- if the required verification tool is unavailable, return the documented skip
  code with a precise diagnostic, or fail consistently according to the chosen
  CI policy;
- never run the platform while claiming deterministic DMA-base proof if that
  proof was skipped.

Required negative controls:

- wrong `DMA_BASE` must fail before simulation;
- missing `__fw_dma_base` must fail before simulation;
- missing symbol-verification tool must not silently pass.

## 6. Mandatory documentation reconciliation

### R2-D1 — Remove remaining chimney sign-off overclaims

Until A-1 passes, remove or qualify every statement equivalent to:

- "chimney signed in both directions";
- "both chimney directions are signed";
- "chimney content and timing signed both directions".

Audit at least:

- `docs/AI_HANDOFF_CONTEXT.md`;
- `docs/STATUS.md`;
- `docs/RTL_MAPPING.md`;
- `README.md`;
- `rtl_crosscheck/run_mesh_crosscheck.sh`;
- `platforms/noc_soc/README.md`;
- the implementation report and any finish-task notes.

Use the four-part distinction in section 2 of this document.

### R2-D2 — Keep one execution order everywhere

The single order after this cleanup is:

1. Step 10.2 — done;
2. original pre-A1 cleanup — incomplete until this round closes;
3. this round-2 pre-A1 remediation — current;
4. A-1 — next only after the gate passes;
5. A-2;
6. A-3;
7. Step 10.3;
8. Step 10.1;
9. Step 10.4;
10. Step 10.5;
11. Step 11.

Update the final continuation prompt. It must not say both "10.3 next" and
"A-1 first".

### R2-D3 — Correct the v0 definition-of-done count

The current table identifies at least these unmet criteria:

1. every included leaf has a direct standalone test;
2. clock gating waits for proven mesh quiescence;
3. licensing/provenance is complete;
4. the datapath is cycle-accurate from manager AXI to subordinate AXI;
5. scoreboard-driven wrapper stress is complete.

Do not conclude that v0 is only three criteria short while five entries are
marked `NOT met`.

Keep A-1/A-2/A-3 inside the v0 completion gate, as already decided.

### R2-D4 — Describe the firmware regression exactly

Update all stale descriptions:

- the firmware run uses `--sim-us 2000`, not 500;
- the no-`--fw` run is a synthetic substitute smoke path, not a second image;
- it has reduced and different coverage;
- it does not uniquely isolate DMA faults from CPU/NoC faults;
- the firmware is built from a private source copy, so the automated regression
  no longer needs an in-source `make clean`;
- manual build instructions must be distinguished from the automated
  regression.

Audit at least:

- `docs/AI_HANDOFF_CONTEXT.md`;
- `platforms/noc_soc/README.md`;
- any command examples in reports and status files.

### R2-D5 — Reconcile the original pre-A1 checklist

`docs/PRE_A1_REVIEW_FIX_PLAN.md` currently declares a hard gate but every
completion checkbox remains unchecked while the handoff table calls the cleanup
done.

Required changes:

1. Do not mark the original plan complete merely by changing boxes.
2. Fix and verify every item first.
3. Mark each original F/V/D checkbox with:
   - completion state;
   - concrete test or file evidence;
   - date verified.
4. If an original item is superseded by this document, link to the exact R2
   item and leave it open until the R2 item passes.
5. Change the handoff execution table from `pre-A1 cleanup done` to the truthful
   state until both gates close.

## 7. Recommended implementation order

Use this order to reduce rework:

1. R2-F2 — sparse multi-beat write mapping and tests.
2. R2-F3 — read policy and side-effect-target tests.
3. R2-F4 — B/R response semantics and tests.
4. R2-F1 and R2-F5 — CMake export and ODR safety.
5. R2-F6 — invalid configuration lifecycle.
6. R2-V1 and R2-V2 — safe verification infrastructure.
7. R2-FW1 — mandatory ELF-base proof.
8. R2-D1 through R2-D5 — documentation only after behavior is final.
9. Run the complete gate in section 11.
10. Review the resulting diff. Only then mark section 12 complete.

## 8. Review-sensitive design constraints

Do not accidentally change these while fixing the findings:

- frozen FlooNoC revision and configuration remain unchanged;
- data width remains 64 bits;
- `OutFifoDepth = 2` remains the platform configuration;
- `MaxUniqueIds = 1` constrains downstream ID/order handling, not the wrapper's
  number of outstanding TLM calls;
- one TLM transaction in flight per upstream port remains wrapper policy until
  Step 10.5;
- the timed chimney is still not in the integrated datapath before A-3;
- byte-enabled writes must retain exact per-beat `WSTRB`;
- downstream target delay remains rounded up to whole network cycles;
- incoming TLM delay is spent before injection and cleared on return;
- an upstream address rejected before injection is a TLM boundary error, not
  evidence of an AXI DECERR flit.

## 9. Required compiler and standalone regression

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head

make BUILD_DIR=/tmp/floo_noc_pre_a1_round2 test
```

Expected:

- fresh configure succeeds;
- every test passes;
- no `-Wall`, `-Wextra`, or `-Wpedantic` diagnostic from model/test C++;
- the new directed tests are registered in CTest;
- no test relies on `std::_Exit()` to hide teardown failure.

## 10. Required CDC-VP parent build and firmware regression

The configure command must be checked independently. Do not continue after a
failed configure and then report only that the later build happened to pass.

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head

cmake -S . -B /tmp/cdc_vp_noc_pre_a1_round2 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCDC_BUILD_NOC_SOC=ON \
  -DCDC_BUILD_TESTS=ON

cmake --build /tmp/cdc_vp_noc_pre_a1_round2 \
  --target noc_soc --parallel

NOC_SOC_BIN=/tmp/cdc_vp_noc_pre_a1_round2/platforms/noc_soc/noc_soc \
LOG_DIR=/tmp/noc_soc_pre_a1_round2_logs \
./platforms/noc_soc/tests/run_firmware_regression.sh
```

Expected:

- both CMake commands return zero;
- firmware log contains `DMA PASS`;
- survey log contains `result   all bytes match`;
- no trap, SystemC error, mismatch, timeout, or hidden failure;
- the ELF-base proof is mandatory and reports `0x10060000`.

## 11. Required RTL and negative-control regression

Run all eleven existing RTL cross-checks without changing frozen RTL or
refreshing expected data from SystemC:

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

The negative-control PASS is valid only when every mutation:

1. builds successfully;
2. runs its named test;
3. makes that test fail for the intended behavioral reason.

Final hygiene:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP

git diff --check
git status --short

git -C /home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC \
  rev-parse HEAD
git -C /home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC \
  status --short
```

Expected frozen revision:

```text
9a6972a5f9b8117506d1df8a6505ce1da2bc9084
```

## 12. Round-2 completion gate

All boxes must be checked with concrete evidence before A-1 starts.

### Functional correctness

- [x] **R2-F1** — the warning policy is a CMake *variable*, not an INTERFACE
      target, so it never enters the export set. Fresh parent configure exit 0,
      `noc_soc` build exit 0, zero warnings.
- [x] **R2-F2** — every enabled `(beat, lane)` is resolved to its absolute byte
      address once, in `absorb_request`; `write_data`/`write_strb` are no longer
      carried downstream at all. Five end-to-end cases in
      `test_noc_interconnect::test_sparse_multibeat_writes`; negative control
      `sparse-beat-renumbering`.
- [x] **R2-F3** — policy 3: `noc_interconnect::target_kind{mmio, memory}`,
      defaulting to `mmio`. A widened read to an `mmio` target is refused with
      `TLM_BURST_ERROR_RESPONSE` before injection. A spy target proves the
      target is never called; negative control `widened-read-policy-removed`.
- [x] **R2-F4** — `static_assert`s pin all four encodings; B and R carry each
      code end to end; an R burst keeps the first error (`worse_of`). All four
      acceptance injections detected: swapped codes and EXOKAY-as-error stop the
      build, discarded B code and erased R error fail their test.
- [x] **R2-F5** — the free functions are `inline`; `test_axi_lanes_odr` links two
      translation units. Removing one `inline` gives
      `multiple definition of shape_of`.
- [x] **R2-F6** — the clock is validated before the mesh is built, and self-node
      conflicts are rejected in `add_target`/`place_initiator`. No `_Exit`
      anywhere; `test_noc_interconnect_bad_config` returns normally and also
      checks that a legal layout is still accepted.

### Verification infrastructure

- [x] **R2-V1** — detection needs build success, test failure, *and* an expected
      message. A syntax-breaking mutation is reported `MISSED ... does not
      build`; a wrong-reason failure is reported `not for the expected reason`.
- [x] **R2-V1** — each control runs in its own copy under `/tmp`. Verified by
      `SIGKILL`ing a run mid-flight: source checksums unchanged.
- [x] **R2-V2** — `verify_common_cells` checks revision and every SHA-256
      without writing; `.git/HEAD` mtime is unchanged across runs, and the whole
      checkout works `chmod -R a-w`.
- [x] **R2-V2** — materialisation is serialised by `flock` with a 300 s timeout
      and a diagnostic; two concurrent runs complete with zero `index.lock`
      errors. A dirty checkout is refused rather than overwritten.
- [x] **R2-V3** — sparse multi-beat (5 cases), widened-read policy (spy target),
      B/R propagation, multi-TU linkage, clean invalid-config teardown, and the
      parent configure in the gate. Three new negative controls registered.

### Firmware and documentation

- [x] **R2-FW1** — missing `riscv-none-elf-nm` now exits 77 with a diagnostic
      and does **not** run the platform. Wrong base and missing symbol both fail
      before simulation.
- [x] **R2-D1** — audited across the handoff, STATUS, RTL_MAPPING, both READMEs
      and the mesh runner; no `both chimney directions are signed` remains
      outside the two review plans quoting it.
- [x] **R2-D2** — one table in the handoff, now listing both cleanup rounds,
      with A-1 next. The continuation prompt matches it.
- [x] **R2-D3** — five unmet criteria, enumerated explicitly rather than
      summarised, so the count cannot drift from the table again.
- [x] **R2-D4** — `--sim-us 2000`, private source copy, no in-source
      `make clean` in the automated path, and manual commands labelled as such.
- [x] **R2-D5** — all 20 boxes in `PRE_A1_REVIEW_FIX_PLAN.md` marked with
      evidence and date; two entries flagged as superseded by R2 items.

### Final evidence

- [x] Fresh standalone build: **32/32 tests pass**, zero warnings.
- [x] Fresh parent configure and `noc_soc` build: both exit 0.
- [x] All eleven runners pass (15 comparisons total).
- [x] **9 detected, 0 missed**, each with a required failure message.
- [x] `DMA PASS`, with the ELF base proved as `0x10060000`.
- [x] `result   all bytes match`.
- [x] `git diff --check` clean.
- [x] `9a6972a5f9b8117506d1df8a6505ce1da2bc9084`, zero dirty files.
- [x] Audited as part of R2-D1 to R2-D5.
- [ ] A final human/AI review finds no remaining pre-A1 blocker. **This box is
      the reviewer's, not the implementer's — left unchecked deliberately.**

Only after all boxes above are complete may the next agent begin Step A-1.
