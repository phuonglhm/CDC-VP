# TPU_V3 Phase 5 Audit — MXU Extraction from Sauria v4.2

> **Active evidence under D27, not the current work queue.** This audit proves
> the verified 64x64 INT8/INT32 MXU reused by the one-core DSE. The next task is
> G2/MB1 in [NEO_CORE_MICROBENCH_DSE_PLAN.md](NEO_CORE_MICROBENCH_DSE_PLAN.md).

Under D20, **MXU** is the architectural block name. This audit continues to
use **Sauria** where it identifies the pinned third-party implementation
source, symbols, profiles, tests or excluded Sauria DMA.

Phase 5's first task is "pin and record the NPU-team v4.2 source revision and
redistribution policy", and its gate requires a dependency review proving the
unrelated NPU-top blocks are absent. Both need a written answer to *which
source*, and until this document existed there was not one — the plan's status
table said only "v4.2 source audit/golden required".

Everything below was read from the tree on 2026-08-14. Where it contradicts
`TPU_V3_PHASE0_AUDIT.md` §7, this document is the later reading; §7 has been
corrected and says so.

---

## 1. The source is present, and there are three copies of it

| Copy | Path | Role |
|---|---|---|
| A | `components/npu_tlm/models/v4.2_model` | bundled |
| B | `components/npu_tlm/models/v4.2_model_Aug01` | bundled, **and the build's current default** |
| C | `/home/duyptt_HW/Documents/work/fx1/hw/tlm/MP1_V1.1/v4.2_model` | the NPU team's working copy, outside the repository |

`components/npu_tlm/CMakeLists.txt` selects `SAURIA_NPU_ROOT` if set and falls
back to **B**.

### The copies are not the same source

This is the finding that makes the pin mandatory rather than administrative.

| File | A vs B | Matters because |
|---|---|---|
| `control/instruction_decoder.h` | 257 changed lines | excluded by Phase 5, but it is the largest divergence and marks the two trees as different vintages |
| `instrumentation/perf_counters.h` | 122 | Phase 5 exposes performance counters through its own register map |
| `config_regs.h` | 23 | the Sauria register layout |
| `npu_top.h` | 20 | composition |
| `stimuli/GoldenStimuli.txt`, `gold_dram.txt`, `initial_dram.txt`, `tstcfg.txt` | differ | **the golden vectors themselves differ** |
| `systolic_array/sa_array.h` | 24 lines, **semantically identical** | see below |
| Present only in A | `npu_demo_clean/`, `.gitattributes`, `.gitignore` | the demo corpus Phase 5 draws `demo_gemm_64x64` from |
| Present only in B | `conv5x5_demo/`, `sauria_targets.csv`, build outputs (`tb_demo`, `tb_obp`, `run.log`) | B is a working directory with artifacts committed into it |

On `sa_array.h` specifically: the whole difference is one hunk in weight
propagation, and it is a restructured branch, not a behaviour change. Both
resolve to `y == 0 → wei_in_a`, `y == nsplit → wei_in_b`, otherwise
`prev_b[y-1][x]`; B just hoists a `y < nsplit` test around it. The PE array is
therefore common to both copies, which is worth knowing but does not make the
trees interchangeable — the golden vectors do not agree, and a differential run
against the wrong ones proves nothing.

**A and C agree** on every file Phase 5 would compile. B is the odd one out, and
B is what the build picks by default.

### Recommended pin

Copy **A**, `components/npu_tlm/models/v4.2_model`, because it matches the NPU
team's own working copy C byte for byte across the files Phase 5 uses, and
because it is the only copy carrying `npu_demo_clean/`, which is where the
plan's `demo_gemm_64x64` golden case comes from.

```text
tree          components/npu_tlm/models/v4.2_model
headers       26 (.h, excluding npu_demo_clean/)
sha256        418a8d8831d72e4dccb083ba99fed7630b7438a698a1a1217629f2e50365e2c5
```

Phase 5 applies this pin **locally** through `TPU_V3_SAURIA_ROOT` and refuses a
tree whose content hash differs. It deliberately does not change
`components/npu_tlm`'s fallback from B, because that would silently change the
source used by unrelated consumers. The Phase 5 target therefore never guesses
between A and B even though the shared component still does.

### The hygiene patch applied on top of the pin

D17 requires every function-local trace/debug object in the selected closure to
be compiled out before the adapter can be called instance-clean. Two
instrumentation-only patches are applied, in order, to a **build-tree copy** of
the pinned source. The shared `components/npu_tlm` source is never edited.

```text
patches    components/TPU_V3/sauria_matrix/patches/
             0001-instrumentation-only-compile-out-sa-trace-files.patch
             0002-compile-out-all-static-debug-state.patch
set hash   7f888bb7ad10bb5590d72dd647299992591e23301901a628abc4735b1be72ccf
kind       instrumentation-only: no arithmetic, address generation,
           handshake, wait or functional timing changes
```

The source, oracle and exact compiler input are independently pinned:

```text
base headers (26)          418a8d8831d72e4dccb083ba99fed7630b7438a698a1a1217629f2e50365e2c5
demo_gemm_64x64 oracle     1e9dc76c03032bb214544935f1f12c6df63aee1ba2eab4d4c3c97e19a972cb54
patched headers (26)       c1931405bfa6c8cebb33deb645b4f4cf827fbe49fb91562104ca6b0b10ad8f98
```

Both tree hashes use one recipe, and it is written down because the first
attempt was not reproducible: `sha256sum` prints the filename beside the digest,
so hashing its output from two different directories gave two different answers
for identical content.

```bash
cd <tree> && find . -name '*.h' -not -path './npu_demo_clean/*' -printf '%P\n' \
  | LC_ALL=C sort \
  | while read -r f; do printf '%s  %s\n' "$(sha256sum "$f" | cut -d' ' -f1)" "$f"; done \
  | sha256sum
```

Verified: both patches apply cleanly to copy A, and the patched source compiles
at `SystolicArray<64, 64, int8_t, int8_t, int32_t>` — the `int8_64x64` geometry
— with `SAURIA_DEBUG=0 SAURIA_TRACE_FILES=0`, elaborating two independent full
adapter/composition instances.

**Scope, stated precisely.** Patch 0001 prevents the trace files from being
created. The first implementation still replaced some files with null streams,
which left mutable function-local `static` streams, flags and counters shared by
all template instances. Patch 0002 compiles the complete trace/debug blocks out
and removes the last stateless `debug.h` singleton. A binary-symbol gate now
refuses any `_ZZN6sauria...` function-local-static symbol, while the two-instance
run requires that no `trace_sysc/` output is created and that both engines
produce independent results. The reserved `NpuTop_std` instance-name guard is
kept as a second line of defence, not used as the proof.

---

## 2. Redistribution policy

From `licenses/SAURIA.PROVENANCE.md`, unchanged by this audit:

- upstream architecture is `bsc-loca/sauria`, revision
  `2bb469e4e4ab7413b88c985b4c83a98b9544c827`, `Apache-2.0 WITH SHL-2.1`;
- **the SystemC implementation in these three copies is not upstream.** It
  follows Sauria's architecture, module naming and register addressing, but the
  official upstream implementation is SystemVerilog and Python. The SystemC
  model is internal-only;
- a public CDC-VP release ships neither the implementation nor an NPU-enabled
  binary, and publishing such a binary needs separate approval from the rights
  owner of the SystemC implementation.

**Consequence for Phase 5, and it is a design constraint rather than a
footnote:** the adapter must *wrap* this source where it sits, reached through
`TPU_V3_SAURIA_ROOT`, and must not copy it into `components/TPU_V3/`. Copying would
create a second fork to keep in step with the NPU team and would put
internal-only source into the TPU_V3 tree, which Phase 12 then has to strip out
of every package. This matches how `cdc::cpu::riscv_vp_plusplus` treats VP++:
pin it, verify it, compile it where it lies, never fork it.

It also means the Phase 5 target must be **off by default** and must refuse to
appear in a package unless the licence and provenance files are present — the
rule Phase 0 §7 already carries into Phase 12.

---

## 3. The `int8_64x64` profile

`sauria_targets.h` is auto-generated from `sauria_targets.csv` by
`tools/gen_targets.py` and describes itself as the single source of truth for
geometry, element widths and packed-config index widths. It defines **eight**
profiles:

```text
int8_8x16    int8_32x32    int8_64x64
FP16_8x16    FP16_32x32    FP16_64x64
int16_8x16   int16_32x32
```

The one Phase 5 names:

```c
{"int8_64x64", 64, 64, 8, 8, 32, 0, 18, 18, 17, 1, 4, SAURIA_DT_INT8, ""}
```

read against the `SauriaTarget` field order:

| Field | Value | Phase 5 requires | |
|---|---|---|---|
| `X`, `Y` | 64, 64 | X=64, Y=64 | ✓ |
| `ia_w`, `ib_w` | 8, 8 | INT8 activation / weight | ✓ |
| `oc_w` | 32 | INT32 accumulation / output | ✓ |
| `op_type` | 0 (integer) | integer PE arithmetic | ✓ |
| `idx_a`, `idx_w`, `idx_o` | 18, 18, 17 | index widths 18/18/17 | ✓ |
| `in_bytes`, `out_bytes` | 1, 4 | consistent with 8-bit in, 32-bit out | ✓ |
| `dtype` | `SAURIA_DT_INT8` | INT8 | ✓ |
| `build_flags` | `""` | — | INT8 is the model's default dtype, so it needs no `-D` |

Every parameter Phase 5 names is satisfied exactly by an existing profile. No
new profile has to be authored.

**The empty `build_flags` is a trap worth stating plainly.** The FP16 and INT16
profiles carry `-DNPU_*` flags; INT8 carries none, because it is what the model
compiles as by default. So a build that selects nothing at all still produces
INT8 — and produces it at whatever geometry the *template defaults* say, which
is where the next section matters.

### The 32x32 fallback the plan warns about is real

```c
template <int X_DIM = 32, int Y_DIM = 32, ...> class NpuTop;        // npu_top.h
template <int X_DIM = 32, int Y_DIM = 64, ...> class SystolicArray; // sa_array.h
```

Two different defaults, neither of them 64x64, and the geometry is a *template
parameter* rather than anything read from `sauria_targets.h` at runtime. An
adapter that omits the arguments compiles cleanly and silently produces a 32x32
array — or, for the array alone, a 32x64 one. Phase 5's requirement for "a
negative control that fails if an omitted parameter silently falls back to
32x32" is not defensive writing; it is the exact failure this code shape
produces.

The index widths are equally silent: they arrive as `-DSAURIA_ACT_IDX_W=18`
style flags (`IDX_64x64` in the Makefile), so a build that forgets them gets the
default widths with no diagnostic.

---

## 4. Module inventory: keep, exclude, replace

The model is ~11k lines of cycle-level SystemC across these modules. Phase 5
says to extract "matrix multiplication plus only required feeder/sequencer/
result collection" and to exclude "Sauria DMA, profile/instruction top, OBP and
RCE".

| Module | File | Lines | Phase 5 |
|---|---|---|---|
| `SystolicArray` | `systolic_array/sa_array.h` | 381 | **keep** — the PE array, the thing being extracted |
| PE | `systolic_array/sa_processing_element.h` | 212 | **keep** — the array is made of these |
| `IfmapFeeder` ×2 | `data_feeder/ifmap_feeder.h` | 1317 | **keep** — required feeder (lane A/B) |
| `WeightFeeder` ×2 | `data_feeder/wei_feeder.h` | 885 | **keep** — required feeder (lane A/B) |
| `Control` ×2 | `control/main_controller.h` | 804 | **keep** — the sequencer |
| `Psm` ×2 | `psm/psm_top.h` | 439 | **keep** — result collection |
| `PerfCounters` | `instrumentation/perf_counters.h` | 61 | **keep** — Phase 5's register contract requires performance counters |
| `Sram` | `sram/sram_top.h` | 702 | **replace** — see §5 |
| `ConfigRegs` | `config_regs.h` | 1216 | **keep** — see the correction below. Originally classified "replace" |
| `SauriaDma` | `control/sauria_dma.h` | 248 | **exclude** — plan §11.5 gives TPU_V3 its own DMA, and Phase 4 built it |
| `InstructionDecoder` | `control/instruction_decoder.h` | 822 | **exclude** — instruction/profile top |
| `Obp` ×2 | `psm/obp_top.h` | 420 | **exclude** |
| `ReconfigurableEngine` ×2, `ReductionEngine` ×2 | `psm/re_rce.h` | 509 | **exclude** |
| `NpuTop` | `npu_top.h` | 1798 | **exclude** — it is the composition being replaced |

Kept: roughly 4,100 lines of model. Excluded: roughly 3,800, plus the 1,798-line
top. The extraction is close to an even split, which is a reasonable sanity
check that "matrix only" is a meaningful reduction rather than a relabelling.

### The kept set is closed — the exclusion boundary holds

The plan's exclusion list is only usable if the modules Phase 5 keeps do not
*need* the ones it drops. That was assumed rather than checked. It has now been
checked, by extracting every port binding in `npu_top.h` and asking which
signals a kept module reads that only an excluded module drives.

There are 46, and they divide cleanly:

| Source | Count | Disposition |
|---|---|---|
| `ConfigRegs` | 41 | kept — it is the config distributor, see the correction below |
| `Sram` | 5 | replaced by `tile_staging_store` — `s_srama_data_a/b`, `s_sramb_data_a/b`, `s_sramc_rdata_a/b` |

**Nothing at all comes from `Obp`, `Rce`, `ReductionEngine`, `SauriaDma` or
`InstructionDecoder`.** The three `Control` inputs that looked most likely to
drag the output-buffer processor in — `i_outbuf_done`, `i_finalwrite`,
`i_shift_done` — are all driven by `Psm`, which Phase 5 keeps.

So the composition needs no stubs and no substitutes for the excluded blocks:

```text
ConfigRegs  ->  Control x2, IfmapFeeder x2, WeightFeeder x2, Psm x2
tile_staging_store  <->  IfmapFeeder x2, WeightFeeder x2, Psm x2
                         SystolicArray
```

This is the dependency review plan §16's gate asks for, and it passes: "matrix
only" is a real cut through this design rather than a relabelling.

### `V4_CFG_MAP` covers blocks Phase 5 excludes

`ConfigRegs`' address map is `NpuTop`'s, so it contains registers for the
output-buffer processor — `F_OBP_CFG_A/B`, `F_REQUANT_SCALE_A/B`,
`F_REQUANT_SHIFT_A/B`. Phase 5 excludes OBP, and
`sauria_compute_core_fields()` correspondingly never fills those fields in.

That matters more than it looks. A loader that walks the whole map writes those
registers from whatever the field array happens to hold, and an uninitialised
array means **stack garbage** — so the same job configures the engine
differently on each call. It was observed exactly that way: three registers
diverged between two identical calls, one reading `17685856`.

The loader now skips them and zero-initialises the array. Skipping is the real
fix: writing a register of an absent block would also leave a register trace
implying OBP is part of the composition.

Two things are worth carrying forward from how this was found:

* the **determinism check** is what exposed it — "the same job twice must
  produce byte-identical configuration". A correctness test against expected
  values would not have, because the garbage was in registers no correctness
  test looks at;
* the first measurement of "how many registers depend on K" was **5**, and it
  was wrong: three of those were the garbage fields. The true number is 2. A
  test asserting 5 would have passed for the wrong reason and then failed the
  moment the bug was fixed.

### Correction: `ConfigRegs` is kept, not replaced

The first version of this table put `ConfigRegs` in the "replace" column,
reasoning that TPU_V3 defines its own 32-bit AXI4-Lite register map so Sauria's
register file is redundant. That conflates two different things and would have
made Phase 5 far riskier than it needs to be.

`ConfigRegs` is not a firmware interface competing with `SA_CONTROL`. It is the
*config distributor*: it is loaded through a host port and drives the ~107
loop-bound and step signals that the feeders, `Control` and the PSM consume.
Replacing it means re-deriving all 107 values by hand, and a single wrong bound
produces a plausible wrong result — the failure mode this whole phase is built
to avoid.

Note also that `ConfigRegs` is **not** in the plan's exclusion list, which names
"Sauria DMA, profile/instruction top, OBP and RCE". Reading "TPU_V3 defines its
own register map" as "delete Sauria's" was my inference, not the plan's
instruction.

So the config path is layered, and each layer is owned by whoever should own it:

```text
SA_CONTROL (frozen TPU_V3 AXI4-Lite map)   <- firmware-visible, TPU_V3 owns it
  -> job {M, N, K, addresses, strides}
  -> SauriaLayerDesc                       <- the translation Phase 5 must write
  -> sauria_compute_core_fields()          <- the source's own encoder
  -> sauria_encode_core_config()           <- the source's own packer
  -> ConfigRegs host port                  <- the source's own distributor
  -> the ~107 config signals
```

Only the third line is new code. Everything below it is the path the source
already uses and the golden cases were captured through, which is what makes a
source-versus-adapter differential mean something: the two run the *same*
configuration encoder, so a divergence is in the adapter and not in a re-derived
config that was subtly different all along.

### The GEMM mapping, taken from the golden case rather than inferred

`SauriaLayerDesc` is a **convolution** descriptor, so GEMM has to be expressed as
one. The demo case settles exactly how, and it is worth quoting because guessing
it would be the single easiest way to get Phase 5 quietly wrong:

```text
cases/demo_gemm_64x64/case.env
  TITLE="GeMM 64x64"
  DESC="1x1 conv / GeMM, Cin=256, Cout=64, 64x64 output on the 64x64 array."
  VERSION="int8_64x64"
  SHAPE="1 1 1 1 256 64 1 64 64 64 1"
  REGION_BYTES=65536
```

`SHAPE`'s eleven fields are `SauriaLayerDesc` in declaration order, so for
`C[M x N] = A[M x K] . B[K x N]`:

| Field | Demo value | GEMM meaning |
|---|---|---|
| `B_w`, `B_h` | 1, 1 | 1x1 kernel — this is what makes it a GEMM |
| `d`, `s` | 1, 1 | no dilation, unit stride |
| `c_til` | 256 | **K**, input channels |
| `k_til` | 64 | **N**, output channels |
| `h_til`, `w_til` | 1, 64 | **M** as output spatial, laid out as 1 x M |
| `X_used`, `Y_used` | 64, 64 | the array actually in use |
| `preload_en` | 1 | **0 for Phase 5** — D17 refuses C preload |

The case is `C[64 x 64] = A[64 x 256] . B[256 x 64]`, which is also the first
golden case the plan names.

### The kept modules are not instance-clean

This table answers "which modules" and, as first written, stopped there. It is
the wrong place to stop: what is *inside* a kept module decides whether the
extraction can satisfy `INTERFACE_CONTRACT.md` at all.

The kept modules carry mutable process-global state and write trace files from
the compute path:

| Where | What |
|---|---|
| `debug.h:16` | `#ifndef SAURIA_DEBUG / #define SAURIA_DEBUG 1` — debug defaults **on**, despite reading as opt-in |
| `sa_array.h:74`, `:100` | `static std::ofstream` → `trace_sysc/sa_macq_dump.csv`, `sa_macq_cell_dump.csv`. Behind **no macro**, reachable from compute |
| `ifmap_feeder.h:1234` | `static std::ofstream` → `ifmap_pop_logical.csv`, gated by a **runtime instance-name test** (`name().find("NpuTop_std")`), not a macro |
| `wei_feeder.h` | three more `static std::ofstream` |
| `psm_top.h` | one more |

Seven `trace_sysc/*.csv` writers. A function-local `static` is shared by every
instance of the enclosing template, so the two SAs of a two-core chip share one
file handle and one set of counters — and `INTERFACE_CONTRACT.md` §"No mutable
global or static state" names this case exactly: "a `static` scratch buffer in a
compute kernel is a defect even when tests pass single-threaded, because SystemC
processes interleave at `wait()` boundaries."

`-DSAURIA_DEBUG=0` is not a fix, because the two worst offenders are not guarded
by it. Decision record D17 records what Phase 5 requires instead: mandatory
`SAURIA_DEBUG=0` and `SAURIA_TRACE_FILES=0`, a recorded and hash-verified
instrumentation-only patch that compiles the trace streams out, and a
two-instance gate proving no `trace_sysc/` appears and the instances are
independent.

---

## 5. The finding that shapes the adapter: where operands and results actually live

Phase 5 says to "connect operand/result accesses through the native local-SRAM
port rather than direct source backing". Reading the modules shows what that
costs, and it is more than swapping a pointer.

The kept modules do not take addresses and fetch data. They are clocked RTL-style
blocks that talk to Sauria's `Sram` over **signal-level SRAM buses**. `Psm`, for
example, exposes:

```cpp
sc_out<uint32_t>                     o_sramc_addr;
sc_out<bool>                         o_sramc_wren;
sc_out<bool>                         o_sramc_rden;
sc_out<sramc_mask_t<Y_DIM>>          o_sramc_wmask;
sc_out<psum_vector_t<Y_DIM,T_PSUM>>  o_sramc_wdata;
sc_in <psum_vector_t<Y_DIM,T_PSUM>>  i_sramc_rdata;
```

and the feeders have the matching pattern on SRAM-A (activations) and SRAM-B
(weights). So the data path is:

```text
local SRAM ──?── SRAM-A ──> IfmapFeeder ─┐
                                         ├─> SystolicArray ──> Psm ──> SRAM-C ──?── local SRAM
local SRAM ──?── SRAM-B ──> WeightFeeder ┘
```

The `?` is what Phase 5 has to build: a shim presenting Sauria's three
signal-level SRAM ports on one side and `neo_local_sram_if` on the other. It is
not a wrapper around `Sram` — it replaces it — and it has to satisfy the timing
the feeders assume (a read issued on one clock edge answering by the next),
which a `b_transport` through an arbitrated fabric does not naturally do.

**Settled by decision record D17 (2026-08-14): buffered tile staging.**

The deciding fact is not performance, it is that the source has **no
`ready`/`stall` input anywhere on the feeder or the controller**. Reads are
pipelined as `rden_q1`/`rden_q2`, and there is no signal by which a memory can
tell the feeder to wait. So pass-through — one native transaction per SRAM
access — is not a slower option, it is an unsafe one: a bank conflict in the
`arbitrated` fabric makes data arrive late, nothing absorbs the delay, and the
feeder pipeline advances past data that is not there. That produces wrong
results which read as an arithmetic defect.

Making pass-through correct would mean adding back-pressure to the feeders and
controller, which modifies the source — and an adapter that must modify the
source before it can be compared against the source proves nothing. Buffering
is also what the RTL does: the array has tile buffers so it can retire one
result per cycle without a memory in the loop.

D17 records the design, the constraints it imposes (native port only, no
`SauriaDma`, `Sram` replaced rather than wrapped, feeders/array/PSM unmodified,
native transactions from an `SC_THREAD` only), its relationship to D16 —
prefetch and writeback are fully in D16's scope; buffering only keeps D16's
back-pressure out of the compute pipeline — and the rule that
`total_time = prefetch_time + source_compute_time + writeback_time` with only
the middle term cycle-correlated to Sauria.

---

## 6. Audit recommendations and their closure

This section was originally the audit's implementation backlog. All Phase 5
items are now closed:

1. **Source pin:** `TPU_V3_SAURIA_ROOT` defaults to copy A, verifies the base
   and golden hashes at configure time, patches only a build-tree copy and
   verifies the post-patch hash. Copy B is refused with both expected and found
   hashes in the diagnostic.
2. **Public contract:** `sauria_matrix_if` and the geometry-reporting,
   geometry-independent `SA_CONTROL` AXI4-Lite register map are frozen. The
   128x128 promotion does not change their programming semantics.
3. **Configuration gate:** the target extracts `int8_64x64` from
   `sauria_targets.h`, checks all eleven relevant fields and binds them to the
   actual template arguments with `static_assert`. Omitted, 32x32, 128x128 and
   unsupported profile selections are refused.
4. **Instance hygiene:** the two instrumentation-only patches, source hash,
   binary-symbol gate and two-instance execution gate close D17's mutable-static
   finding.
5. **Adapter:** private A/B/C tile stores replace the source `Sram`; one
   `SC_THREAD` performs native-port prefetch, source-timed compute and native-
   port writeback. No backing pointer and no external AXI/NoC master exist.
6. **Golden and edge execution:** the source `demo_gemm_64x64` oracle drives
   both direct-staging and native-fabric paths. A 7x13x5 job covers inactive rows
   and columns, strides and weight-row zero padding.

One repository-wide fact is deliberately unchanged: `components/npu_tlm` still
falls back to copy B for its own consumers. Phase 5 neither uses nor silently
changes that fallback. No Sauria source has been copied into
`components/TPU_V3/`; only the adapter and reviewed patch files live there.

## 7. What the first working GEMM changed about §4 and §6

Everything above §7 was written before any job had run. Running one invalidated
four of its claims, and each of them had passed a test first. They are recorded
here rather than edited away, because the pattern is the useful part: every one
was a case where a check existed, passed, and could not have failed.

### 7.1 The composition was multiplying zeros

The bindings were generated from `npu_top.h` by filtering to the kept module
*instances*. That keeps every binding a signal appears in and drops every
`SC_METHOD` of `NpuTop` that wrote one, so eight signals ended up with a reader
and no writer:

```text
s_act_arr_to_array_a/b   the array's activation inputs
s_wei_arr_to_array_a/b   the array's weight inputs
s_start_internal_a/b     the Control FSMs' start
s_ctrl_reset_internal    the Control FSMs' soft reset
s_nsplit                 the array's lane split
```

`test_matrix_composition.cpp` elaborated, ran, and passed throughout. It could
not have failed: SystemC requires *ports* to be bound and diagnoses a dangling
one, and an `sc_signal` with no writer is legal — it holds its default value for
the whole simulation. Nothing in the SystemC API exposes "does this signal have a
writer", so the property now has a source-level gate,
`tests/driver_coverage_gate.cmake`, and the five `NpuTop` processes are ported
into `matrix_composition.h`.

One of the five has to stay clocked. `debug_ref_stream_mux` is an `SC_METHOD` on
`i_clk.pos()`, so it is a *pipeline register* in the array's operand path; a
combinational pass-through would compile, elaborate, and shift the array one
cycle against the feeders' `rden_q1`/`rden_q2` recovery, arriving as wrong
arithmetic. Its other leg — a debug reference-stream injector — is gated on a
member initialised `false` and assigned nowhere, so it is dead in the source too
and is not reproduced.

### 7.2 The config loader was indexing two unrelated enumerations

§4's layering claim was "both sides run the same configuration encoder, so a
divergence is in the adapter". The implementation did not: it wrote

```cpp
std::uint64_t fields[::sauria::F_CFG_COUNT] = {};        // sauria::CfgFieldId
::sauria::sauria_compute_core_fields(desc, target, fields);
for (entry : ::sauria::V4_CFG_MAP)
    writes.push_back({entry.local_addr, fields[entry.field]});  // sauria::CfgField
```

`RegMapEntry::field` is `sauria::CfgField` from `config_map.h`; the encoder fills
an array indexed by `sauria::CfgFieldId` from `sauria_cfg_layout.h`. Different
types, different orders, different lengths. Both are enums with implicit integer
conversions, so it compiled silently, gave every register some unrelated field's
value, and read past the end of the array for any `CfgField` beyond
`F_CFG_COUNT`.

The out-of-bounds tail is what first drew attention — a determinism check caught
three registers changing between two calls with the same job, one of them reading
`17685856`. The diagnosis at the time was "the encoder does not fill the excluded
blocks' fields", which was wrong; the fix (zero-init, skip OBP) removed the
*symptom* for three registers and left the in-range majority quietly incorrect
with no observable difference at all. A correctness test could not have caught
that either, because before §7.1 was fixed the array was computing on zeros.

Two more divergences came out of the same reading:

* **The pinned golden case runs the other profile.** `ConfigRegs` defaults to
  `PROFILE_V1_SAURIA` and `tb_evaluate.cpp` never writes the profile register, so
  `demo_gemm_64x64` was captured through the V1 map and V1 addressing —
  `npu_profile.h` calls that the bit-exact path. The loader was writing
  `PROFILE_V4_LINEAR`, the legacy linear one.
* **`V1_CFG_MAP` is not a function from address to field.** `CFG_OUT_OFFSET +
  0x20`, `+0x24` and `+0x28` each appear twice, first as `TIL_CKSTEP`,
  `INACTIVE_COLS` and `PRELOAD_EN` and again as the lane-A OBP registers. Which
  one an address means is decided by `cfg_lookup`'s first-match order. A loader
  driven from the map depends on that ordering without saying so.

So the loader now reproduces the sequence `apply_decoded_config_to_npu()` writes —
the sequence the golden case was captured through — taking addresses from the
named constants in `sauria_types.h`. The values were never the raw fields anyway:
the register takes `incntlim + 1`, `dil_pat` is truncated to its low 32 bits, and
the bit masks are carried one byte per host lane.

`NSPLIT` is the one value neither the encoder nor the source's testbench
supplies. Its reset default is `Y_DIM / 2`, which splits the array between the
two lanes; the prefetch controller fills lane A only, so leaving it there routes
half the rows to an empty operand bank. The loader writes the full row count,
meaning "one unsplit array".

### 7.3 The operand layout is transposed

The array's activation address generator walks channel-major: `ACT.CHSTEP` is the
tile's spatial extent, so activation element `(k, m)` is at flat index
`k * M + m`. The captured DRAM image confirms it independently — the bytes at the
case's A offset equal `A.T` flattened, and the preload region equals `P.T`.
Weights are the other way round, `(k, n)` at `k * N + n`, matching B as stored.

The frozen `job` interface says `A[M x K]` row-major, so the transpose belongs to
the adapter. Reads from core SRAM stay linear and burst-sized; only the placement
into the staging store is permuted.

`describe_gemm` also had `X_used`/`Y_used` set from the array's geometry rather
than the problem's. At exactly 64x64 — where the golden case lives — the two are
identical; anywhere else, claiming 64 columns for a 32-channel problem makes the
weight feeder fetch 64 channels' worth per step and read past the tile.

### 7.4 The hygiene patch covered two of nine trace writers

§1 recorded that the feeder and PSM trace writers are gated at run time on the
instance name containing `NpuTop_std`, and are therefore unreachable from a
composition that never uses that name. The run-time guard covers the *writes*. It
does not cover the **construction**, and in `wei_feeder.h`

```cpp
static std::ofstream raw_wei_trace("trace_sysc/weight_raw_buf.csv");
static bool raw_header = false;
...
if (inst_name.find("NpuTop_std") != std::string::npos) { ... }
```

declares the stream outside the guard. An `ofstream` creates its file when it is
constructed. The first real GEMM produced `trace_sysc/weight_raw_buf.csv` from a
composition that never uses the reserved name — caught by the existing
two-instance hygiene gate, which had not seen it before only because it never ran
a job that reached the weight feeder's fetch path.

Patch 0001 first redirected those writers to null streams. Review then found
that this still left the null streams, header flags and debug counters as
function-local mutable statics, and a Debug build exposed one final `debug.h`
sink. Patch 0002 compiles the entire trace/debug blocks out when
`SAURIA_TRACE_FILES=0`/`SAURIA_DEBUG=0` and makes the stateless sink a returned
value. The naming rule is now only a second line of defence; `nm` proves the
adapter binary contains no Sauria function-local static state.

### 7.5 What is now proved, and by what

```text
tpu_v3_sauria_gemm_staged    C = A . B, 4096/4096 exact, operands staged by hand
tpu_v3_sauria_gemm_adapter   the same job over neo_local_sram_if, same oracle
tpu_v3_sauria_driver_coverage every internal signal has a driver
```

The two GEMM tests share an oracle and differ in one variable, which is what
makes the second one diagnostic: a failure there is in the prefetch/writeback
controllers and not in the array, the configuration or the golden data.

`C_compute` is used rather than `C_Mat` because `C_Mat = C_compute + preloads`
and D17 refuses C preload. The test checks `C_compute == A . B` itself before
using it, so the oracle is verified rather than trusted; INT8 x INT8 -> INT32 is
exact integer arithmetic, so there is no tolerance to argue about.

Negative controls run against the differential:

| perturbation | result |
| --- | --- |
| stage A row-major instead of channel-major | fails |
| swap the result indexing to `result[m][n]` | fails, 4032/4096 |
| remove the per-job soft reset | **passes** |

The third is recorded as not covered. `done_latch_logic` already clears the latch
on a new start, so the explicit soft reset is defence-in-depth that no test
distinguishes. It is kept, and it is not claimed as tested.

### 7.6 Edge tiles and the ViT source case

The first adapter worked only when `N == 64`. It flattened `K*N` weights into
physical 64-lane SRAM-B words; for a partial-width tile, the tail of B row `k`
therefore became the beginning of row `k+1`. The feeder consumes one full
physical word per K step. The adapter now stages one vector per K, fills its N
live columns and zero-pads columns `N..63`.

The 7x13x5 test now passes exactly against an independent INT32 oracle. It also
uses non-natural A/B/C strides, proving the inactive rows/columns and writeback
indexing rather than assuming that a square 64x64 case generalises. Tiling is
still intentionally absent: any `M` or `N` above 64 is refused as
`dimension_exceeds_array`; this is a stated capability limit, not a silent
split.

There is a source-controlled limitation behind that edge result.
`ConfigRegs::ROWS_ACTIVE` iterates over `Y_DIM` but accepts a bit only when
`byte_idx < 4`; its host port therefore programs rows 0..31 and cannot deassert
rows 32..63 at the 64-row Phase 5 geometry. Those upper bits remain at the
source's power-on value of `true`. The adapter does **not** claim to have loaded
all 64 mask bits: inactive staging rows are zero-filled and writeback reads only
components `0..M-1`, which is why the 7x13x5 edge result remains exact. Changing
the source reset value of the upper mask bits would break the full 64x64 golden
case and requires a new source revision or an explicit wider register contract;
it must not be hidden by the phrase "byte-spread writes the row mask".

The source's ViT program was also built and run. It prints PASS, but all nine
matrix operations take its `[EMULATION]` path; the Sauria control state remains
idle and the final source counters report zero cycles, zero MACs and zero PE
utilisation. It is therefore **not a Sauria-array golden case** and must not be
reported as a source-versus-adapter differential. Phase 5 instead carries a
64x64x64 projection generated with the same deterministic ViT operand recipe
and compares it exactly with an independent INT32 oracle. The source QKV shape
64x64x192 is explicitly refused because this phase has no N tiling. Full chained
ViT belongs after tiling/single-core composition and the workload phase.

### 7.7 Reset, abort and counter ownership

The adapter is one sequencer thread with a persistent `start_pending` flag, not
a bare event. A replacement START issued after abort/reset while the old thread
is still blocked in a native access is therefore not lost. Job generation owns
completion/error publication and `C_BYTES_DONE`; reset separately closes the
native-traffic counter epoch, so an old response cannot repopulate freshly
zeroed local request/byte counters.

The gate parks the old job inside a deterministic blocking native target,
starts the replacement before releasing the old response, and runs both abort
and reset variants. The new jobs complete with exact results; only their three
native requests and six bytes appear after reset. At NEO-CORE integration,
reset is hierarchical and must reset the adapter and local fabric together, so
the fabric drops an old beat before it reaches SRAM. An accelerator-only abort
cannot retract a native request already accepted by the fabric; firmware must
treat the affected C region as undefined until the adapter has unwound, and
only `C_BYTES_DONE` may be used to identify committed output.

Review found that `SA_CONTROL` defeated that ownership rule by taking one
snapshot immediately after ABORT cleared `BUSY`. If an accepted C write returned
afterwards, the adapter correctly increased its committed count but
`observe_completion()` returned forever because firmware-visible `BUSY` was
already zero; `C_BYTES_DONE` could under-report by one native chunk (at most 64
bytes). The control target now keeps an abandoned-accounting owner open through
ABORT and active reset. Clock edges reconcile its snapshot, and MMIO/debug reads
consult the live owner so they do not wait for another edge. W1C of `ABORTED`
does not transfer ownership; the next `START` does. The boundary test commits a
C write inside a blocking target, aborts and acknowledges status before the
response returns, then requires both memory and `C_BYTES_DONE` to report exactly
four bytes. Reverting the reconciliation makes that test fail.

## 8. Phase 5 completion evidence

The implemented closure is `SystolicArray`, PE, `IfmapFeeder`, `WeightFeeder`,
two `Control` instances, two `Psm` instances and the source `ConfigRegs` used as
its internal configuration distributor. The firmware-visible register target
is TPU_V3's separate 32-bit AXI4-Lite `SA_CONTROL`. `NpuTop`, `SauriaDma`, the
instruction decoder, OBP, RCE and reduction engine are absent by source scan,
binary-symbol scan and CMake link-interface scan. `Sram` is replaced by the D17
tile store; `ConfigRegs` is deliberately kept, not replaced.

Measured on the review host (VP host measurements, **not** RTL area/power or PD
predictions):

| Configuration | Wall time | Peak RSS |
| --- | ---: | ---: |
| one adapter, source golden + 7x13x5 + ViT-derived projection | 1.12 s | 11,764 KiB |
| two full adapters, elaboration/reset/identity | < 0.01 s at timer resolution | 13,488 KiB |

Final regressions and review stress on 2026-08-18:

* Release: 14/14 `sauria` and 35/35 `tpu_v3` pass;
* Debug: 14/14 `sauria` and 35/35 `tpu_v3` pass;
* source/config/provenance, dependency closure, two-instance hygiene,
  function-local-static, golden, edge, async control/IRQ and reset/abort epoch
  gates all pass;
* one earlier Debug `ctest -R sauria -j4` invocation reported 7/14 failures,
  but retained no individual failing test or useful failure output. It did not
  reproduce in clean, serial or parallel reruns. The final parallel stress gate
  used `--repeat until-fail:10 -j4` and recorded 140/140 passing test executions
  in each of Release and Debug. This is retained as a **non-reproduced flaky
  observation**, not classified as a product defect and not hidden with
  `RUN_SERIAL`. If it recurs, the exact failing test, command and
  `Testing/Temporary/LastTest.log` must be preserved before any fix is proposed;
* `git diff --check` is clean.

A full **Debug all-target** build still reaches an unrelated existing ISP test
that fails to link because `isp_register_bank_test` provides no `sc_main` for
SystemC. Building all TPU_V3/VP++ targets explicitly succeeds, and the complete
35-test TPU_V3 Debug suite then passes. This external failure is not counted as
Phase 5 evidence and was not modified.

With the explicit limitations above (INT8/INT32 only, one 64x64 tile, no BF16,
no 128x128 and no full ViT), Phase 5 is complete. The next implementation phase
is Phase 6, implementation of the Transform block's Im2Col capability.
