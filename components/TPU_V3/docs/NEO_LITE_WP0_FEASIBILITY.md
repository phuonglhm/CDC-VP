# Neo Lite WP0 — Baseline Lock and C1 Feasibility Audit

Status: **WP0 evidence complete; pending closure on independent review.**
G5 as a whole is **in progress**: WP1–WP10 are not started.

Date: 2026-08-27

Authority: `NEO_LITE_C1_C2_IMPLEMENTATION_PLAN.md` WP0, which is subordinate to
D28 in `TPU_V3_DECISION_RECORD.md`.

WP0 exists to detect an impossible C1 source assumption **before** the model is
changed. Its exit criteria admit two outcomes for C1 — source-proven or
explicitly blocked — and this note reaches the first. Nothing here implements
any part of C1 or C2.

---

## 1. Baseline lock

Both build types, full `tpu_v3` label, on a 24-core host. Wall time depends on
`-j`, so both figures are recorded rather than one:

```text
                       tests   parallel (-j24)   sequential
Release   build-neo-d27      313 / 313, 0 skip      57.7 s      162.8 s
Debug     build-neo-d27-debug 313 / 313, 0 skip    222.9 s      671.2 s
```

Label inventory at the time of measurement, so a later count can be compared
against a stated composition rather than a bare total:

```text
tpu_v3 313   microbench 254   unit 27   negative_control 72
g3      30   g4          11   wp0        1
```

Both trees include the `wp0` test itself. An earlier draft of this note quoted
312/312 and "52 s": the count predated adding the WP0 check, the Debug tree had
been configured before that check existed and so never registered it, and the
time was a parallel run reported without saying so. All three are corrected
above.

Live identity of the baseline, read back from the instantiated components
rather than from the configuration that requested them:

| Field | Baseline | C1 target | C2 target |
| --- | --- | --- | --- |
| `vlen_bits` / `hart.vlenb` | 512 / 64 | 256 / 32 | 512 / 64 |
| `elen_bits`, `xlen` | 64, 32 | 64, 32 | 64, 32 |
| `mxu_geometry` | 64x64 | 32x32 | 64x64 |
| `mxu_datatype` | `int8xint8->int32` | same | same |
| `mxu_source_revision` | `int8_64x64@c1931405bfa6c8ce…` | `int8_32x32@…` | unchanged |
| `dma_controllers` / `dma_channels` | 1 / 1 | 1 / 2 | 1 / 4 |
| `external_axi_data_width_bits` | 64 | 128 | 256 |
| `sram_capacity_bytes` | 16 777 216 | 786 432 | 1 572 864 |
| `local_bank_width_bits` / `local_bank_count` | 128 / 4 | 128 / 4 | 256 / 8 |
| `fabric_pipeline_stages` | 2 | — | — |
| `core_period_ns` | 10 | 1.25 | 1.25 |
| `dma_max_burst_bytes` | 2048 | 128 | 256 |

The baseline is a **C2 compute baseline** and not an executable C2 profile: it
shares only MXU geometry and VLEN with C1's sibling, and differs from C2 on
SRAM capacity, bank geometry, DMA channels, AXI width, burst size and clock.
Four of those fields are still `structural_literal` in a result row —
`dma_controllers`, `dma_channels`, `external_axi_data_width_bits` and the
element widths — which is the state WP3, WP4 and WP6 exist to change.

## 2. C1's MXU32: source-proven

The pinned v4.2 manifest carries `int8_32x32` as a **named target**, not as a
template default:

```text
sauria_targets.h
  sha256 3ab0b720d1827dd21984c7db1c6928f9d99ace69cef4b1e696259fe764cde836

  {"int8_32x32", 32, 32, 8, 8, 32, 0, 17, 17, 16, 1, 4, SAURIA_DT_INT8, ""}
  {"int8_64x64", 64, 64, 8, 8, 32, 0, 18, 18, 17, 1, 4, SAURIA_DT_INT8, ""}
                 X   Y  ia ib oc op idx_a/w/o  in out
```

The header declares itself auto-generated from `sauria_targets.csv` and calls
itself "the single source of truth for geometry, element widths, packed-config
index widths, and dtype build flags".

**The index widths are why this matters.** C1's are 17/17/16 against C2's
18/18/17, and they reach the source as `-DSAURIA_ACT_IDX_W=` style flags. A
build that omitted them would take the header's defaults with no diagnostic —
and the geometry would still come out 32x32, because `NpuTop`'s template
default happens to be 32x32 as well (`TPU_V3_PHASE5_AUDIT.md` §3). That is
exactly the fall-through D28 forbids: it proves neither the selected source
target nor its index widths, and it produces plausible wrong results rather
than an error.

Golden evidence exists at the same standard Phase 5 used for 64x64:

```text
npu_demo_clean/cases/demo_gemm_32x32/   {CAPTURE_INFO.txt, case.env, config,
                                         stimuli, sauria_tmp}

  CAPTURE_INFO.txt
    sha256 ecdf4946b5ce765ddab1060d4d4c9bb6e9ea7be6b51fe862f2015973fc5e7eb9

case.env:
  EVAL_X=32
  EVAL_Y=32
  IDX_FLAGS="-DSAURIA_ACT_IDX_W=17 -DSAURIA_WEI_IDX_W=17 -DSAURIA_OUT_IDX_W=16"
  VERSION="int8_32x32"
  SHAPE="1 1 1 1 64 32 1 32 32 32 1"      A[32x64] * B[64x32] -> C[32x32]
```

`CAPTURE_INFO.txt` records the same shapes independently of `case.env`, and
carries a SHA256 for each stimulus file, so the corpus supplies its own
reference for its payload.

**Two bindings, not one.** Each stimulus must hash to the digest recorded
against *its own filename*, and `CAPTURE_INFO.txt` itself is pinned. Either
alone is insufficient: matching a digest anywhere in the file lets
`initial_dram.txt` and `gold_dram.txt` be exchanged — a golden case whose input
and expected output have swapped places — and checking the pair without pinning
the reference lets payload and manifest be edited together, which would confirm
the corpus is self-consistent rather than that it is the corpus this audit
read.

An earlier draft recorded a whole-tree SHA256 here. It has been replaced by the
`CAPTURE_INFO.txt` digest, which is the one the check actually verifies: a hash
written into a document and checked by nothing is a number that looks like
evidence. The remainder of the case is gated by content rather than by hash —
`case.env`'s flags, the three shape sources, and the known-stale
`compile_time` block below.

**The corpus disagrees with itself, and `case.env` is authoritative.**
`config/demo_manifest.json` carries a `compile_time` block reading
`EVAL_X: 16, EVAL_Y: 8`. Those are not an alternative reading of this case —
they are exactly the geometry of the `int8_8x16` row in the same manifest, so
the block is stale from a different capture. Three independent places say
32x32:

```text
case.env                     EVAL_X=32  EVAL_Y=32  VERSION="int8_32x32"
CAPTURE_INFO.txt             A 1x32x64  B 1x64x32  C 1x32x32
demo_manifest.json           "A_Mat_mvm": "1 32 64"   ... same shapes
  sauria_shapes_flat
```

WP8 must read `case.env` and the shapes, and must not read
`compile_time.EVAL_X/EVAL_Y`. The check asserts both halves: that the three
authoritative sources still agree, **and** that the stale block still holds its
known-wrong 16/8. If the corpus is ever corrected the check fails, so the
caveat gets removed rather than quietly outliving the defect it describes.

**Verdict: C1's MXU is source-proven.** WP8 has a named target, its exact index
widths and an independent golden case to extract against. It is not blocked.

## 3. C1's VLEN256: feasible by the existing patch mechanism

Three things had to hold, and all three do.

**The upstream source intends it.** `vp/src/core/common/v.h`
(sha256 `2833c3cf2caaaa42bcf7b3b70a91b08555ce5965c4219f392220641a0a37e9ee`):

```cpp
// TODO these should be compile arguments
constexpr unsigned VLEN = 512;
constexpr unsigned ELEN = 64;
```

Making `VLEN` a compile definition is the change upstream already describes,
not a reinterpretation of the model.

**The patch mechanism already exists and does not touch the pinned checkout at
configure time.** `fetch_riscv_vp_plusplus.sh` applies the approved series;
`cpu_models/riscv_vp_plusplus/CMakeLists.txt` verifies the result and states
that it "never patches". Four patches already ride this mechanism — one
upstream backport and three downstream conformance patches under D8, D12, D13
and D19 — each recorded with its hash and its reason. A VLEN patch is a fifth
of the same kind and needs no new machinery.

**The cross toolchain builds C1's ISA today.** Verified by compiling a
`vsetvli` translation unit at both target ISAs with the pinned xPack GCC
15.2.0-1:

```text
rv32gcv_zvl256b   compiles
rv32gcv_zvl512b   compiles
```

**Verdict: C1's VLEN is feasible.** WP9 is unblocked.

## 4. Every `VLEN=512` in the tree, classified

WP9 asks for this audit. None of the sites is a structural blocker; two are
hard refusals that WP1 and WP9 must make profile-aware, and the rest are
defaults, expectations or a bound that already covers C1.

| Site | Class | What C1 needs |
| --- | --- | --- |
| `architecture_config.cpp:100` `require_equal("rvv.vlen", vlen, 512)` | **hard refusal** | profile-aware validation; today it refuses C1 outright |
| `riscv_vp_plusplus_wrapper.cpp:265` refuses unless `vlen_bits()==512` | **hard refusal** | validate against the requested build profile, which WP9 already names |
| `architecture_config.cpp:111` "VLEN=512 must yield vlenb=64" | derivation check | generalise to `vlenb == vlen/8` |
| `architecture_config.h:40` `unsigned vlen = 512` | C2-specific default | comes from the profile factory in WP1 |
| `native_port.h:99` `neo_max_transfer_bytes = 64` | **maximum native payload** | no change: 64 bytes is an upper bound and C1's widest access is 32 |
| `neo_hart_port.cpp:144` comment | documentation | reword when the bound stops being VLEN-derived |
| four test expectations | C2-specific | become profile-parameterised alongside WP1 |

The distinction that matters is the fifth row. `neo_max_transfer_bytes` is
named for the largest payload a single access can *need*, and D7 already
forbids any component from *requiring* it. Reducing it for C1 would be a
change to a bound that is already correct.

## 5. What WP0 concludes, and what it does not

Both exit criteria are met:

* **the baseline is reproducible** — Release and Debug, 313/313 each, with the
  live identity table above read back from components;
* **C2 is unblocked** — its MXU and VP++ already exist at the required values;
* **C1 is source-proven, not blocked** — a named `int8_32x32` target with its
  own index widths and golden case, and a VLEN path that fits the existing
  recorded-patch policy.

What this note does **not** establish, stated because a feasibility audit is
the easiest place to overclaim:

* it does not prove the extracted 32x32 adapter will pass its golden. It proves
  the golden exists to be run against. WP8 owns the result;
* it does not prove a VLEN256 VP++ build runs correctly. It proves the
  constant is reachable by an approved mechanism and that the firmware ISA
  compiles. WP9 owns the result, including `vlenb=32`, `vlmax=8` at SEW32 and
  the RVV conformance corpus;
* it does not touch SRAM capacity, DMA channels, AXI width or the 1.25 ns
  clock. WP2 through WP5 own those, and D28 records that the profile lists do
  not specify external-memory latency, AXI clock relation, SRAM pipeline depth
  or outstanding limits at all — those stay provisional VP inputs;
* it changes no model code. Nothing in C1 or C2 is implemented.

## 6. Reproduction

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion

# Both trees must be configured *after* the wp0 test exists, or they register
# 312 instead of 313 and the difference reads as a regression rather than as a
# stale configure.
cmake -S . -B build-neo-d27-debug

# From a clean tree the runner, its five firmware images and the unrelated
# legacy RVV image all have to exist first: without `rvv_smoke_image` two CPU
# tests skip, and without the runner every microbench test does.
for tree in build-neo-d27 build-neo-d27-debug; do
  cmake --build "$tree" --target neo_core_bench_runner rvv_smoke_image \
        -j"$(nproc)"
done

ctest --test-dir build-neo-d27       -L tpu_v3 -j"$(nproc)"   # 313/313
ctest --test-dir build-neo-d27-debug -L tpu_v3 -j"$(nproc)"   # 313/313

# the feasibility claims, gated
ctest --test-dir build-neo-d27 -L wp0 --output-on-failure
```

The `wp0` label re-checks every claim in §2 and §3 against the pinned sources
rather than against this document. A feasibility note that is not re-checked
expires silently when a pin moves, and the expiry would be discovered by WP8 or
WP9 spending days on an assumption that had already stopped holding.

What it checks, and what each failure blocks:

| Check | Blocks |
| --- | --- |
| `sauria_targets.h` SHA256 equals the audited value | every C1 source claim |
| the `int8_32x32` row, whole, including index widths 17/17/16 | WP8 |
| the `int8_64x64` row, whole | the current baseline |
| `CAPTURE_INFO.txt` SHA256 equals the audited value | the golden claim as a whole |
| each stimulus present, non-empty, and hashing to the digest recorded **against its own filename** | WP8 |
| the three authoritative shape sources still agree on 32x32 | WP8 |
| `compile_time` still holds its known-stale 16/8 | this note's caveat |
| `v.h` SHA256 equals the audited value | WP9's patch text |
| `constexpr unsigned VLEN = 512` still present in that shape | WP9 |
| the cross toolchain builds `rv32gcv_zvl256b` and `rv32gcv_zvl512b` | WP9 |

**A missing cross toolchain is a failure, not a skip.** The first version of
this check skipped it and then printed the sentence claiming both ISAs build —
a machine that cannot answer the question reporting the answer. Nine mutations were run against the hardened check and each is caught: absent
toolchain; emptied stimulus; edited stimulus; edited manifest; edited VP++
header; an index width changed inside the target row; `initial_dram.txt` and
`gold_dram.txt` exchanged; `CAPTURE_INFO.txt` edited; and payload and
`CAPTURE_INFO.txt` edited together so that they agree with each other but not
with this audit.
