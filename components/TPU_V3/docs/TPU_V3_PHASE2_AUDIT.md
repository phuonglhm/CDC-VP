# TPU_V3 Phase 2 — RISC-V VP++ Pre-Integration Audit

> **Historical architecture notice (D14/D15/D18/D20):** the CPU/RVV evidence,
> fixes and closure gates in this audit remain valid. References to future SVM,
> MXU or optional Sauria-backend work describe the pre-D14 plan and are
> superseded by core SRAM, independent TPU_V3 DMA, one MXU and one Transform
> block. The MXU currently uses pinned Sauria v4.2 source; Transform currently
> implements Im2Col and reports Col2Im unavailable. D15 splits its interconnect into 32-bit
> AXI4-Lite control, native banked-SRAM local data and a bidirectional external
> AXI4/NoC bridge.

Audit date: 2026-08-08
Scope: decision record **D3** (RV32GCV runtime), plan §11.2 "Pre-integration
audit and pinning" and the Phase 2 gate.

Every claim below was produced by a command run on this host or by reading the
fetched source at a named revision. Residual scope limits and later-phase work
are listed explicitly in §9 rather than presented as Phase 2 evidence.

**Headline: the runtime is viable and is pinned, but not at upstream `master`.**
Upstream master requires SystemC 3.0.1; CDC-VP is built against SystemC 2.3.4.
The newest revision that builds against 2.3.4 without extra dependencies is the
`2025.09` release tag, and it costs nothing in RVV functionality.

**Effective source is that tag plus an approved patch series** — the upstream
backport `b710fa7b`, which fixes F11, and the two downstream conformance patches
D12 and D13. The mechanism is recorded, hashed, script-applied and
CMake-verified; see F11–F13.

---

## 1. Pin decision

| Item | Value |
| --- | --- |
| Upstream | `https://github.com/ics-jku/riscv-vp-plusplus` |
| **Pinned base revision** | `7a36fe859cae242f513ca6ad16ab8238f1e82977` |
| Tag at that commit | `2025.09` |
| **Approved backport** | `b710fa7be2643b42cee92f5bbcb8cead4c0ed282` (F11) |
| **Downstream conformance patches** | D12 (RV32 index EEW=64), D13 (bus-error access-fault causes) |
| **Effective source** | base + the approved three-patch series |
| Acquisition | `cpu_models/riscv_vp_plusplus/fetch_riscv_vp_plusplus.sh` |
| Commit date | 2025-09-08 13:52:31 +0900 |
| Commit subject | `vp: core: mem: Removed invalid quantum keeper assert check for blocking transport calls` |
| License | MIT (Univ. Bremen 2017–2018, JKU Linz 2022–2023) |
| Local path | `third_party/riscv-vp-plusplus` (gitignored, not vendored) |
| Role | runtime RV32GCV hart — scalar + RVV 1.0 |
| Pin status | **frozen for TPU_V3 Phase 2**; Release and Debug gates passed (D3) |

Rejected alternatives, with the measured reason:

| Candidate | Date | Rejected because |
| --- | --- | --- |
| `master` = `9f25c79fd08902a308a9c8b052f8d16d478d8a9a` | 2026-07-03 | requires **SystemC 3.0.1** — see §2 |
| `98c61f8e4ac552a393aa9a30c95e9e6ab4a74e0e` | 2025-10-12 | last 2.3-compatible commit, but introduces a **`nlohmann/json`** dependency via `util/propertymap.h`, plus "preparations for switch from C++17 to >=20" |
| tags `2024.12` | — | older than 2025.09 with no advantage |

## 2. Finding F1 — upstream master requires SystemC 3.0.1

Commit `6f6ec679` (2025-10-11) *"vp: bump SystemC from 2.3.3 to 3.0.1"* changed
`vp/src/util/common.h`:

```cpp
/* Compatible with SystemC 3.0.1 (see systemc-3.0.1/src/sysc/kernel/sc_module.h line 477ff) */
#define SC_NAMED_THREAD(func, name) \
    SC_PROCESS_MACRO_BEGIN_         \
    this->declare_thread_process(SC_MAKE_FUNC_PTR(SC_CURRENT_USER_MODULE_TYPE, func), name) SC_PROCESS_MACRO_END_
```

SystemC 2.3.4 has neither symbol:

* `SC_PROCESS_MACRO_BEGIN_` — absent from `/opt/systemc-2.3.4/include/`;
* `declare_thread_process` takes **four** arguments
  (`sysc/kernel/sc_module.h:427`), not two.

This is a **preprocessor** error, so it fires whenever `core/rv32/iss.h` is
included at all — it cannot be avoided by not using `DirectCoreRunner`:

```text
iss.h:88:57: error: macro "declare_thread_process" requires 4 arguments, but only 2 given
util/common.h:45:9: error: 'SC_PROCESS_MACRO_BEGIN_' was not declared in this scope
```

At the pinned tag the macro is the 2.3-compatible four-argument form:

```cpp
#define SC_NAMED_THREAD(func, name) declare_thread_process(func##_handle, name, SC_CURRENT_USER_MODULE, func)
```

Two SystemC kernels cannot coexist in one process, so building the VP++ pieces
against 3.0.1 while CDC-VP uses 2.3.4 is not an option. The upgrade path is a
repository-wide SystemC migration with its own gate; it is recorded in §8 and is
not Phase 2 work.

## 3. Finding F2 — the pin costs nothing in RVV, and what it does cost

The RVV implementation is one file, `vp/src/core/common/v.h`. Post-tag commits
touching it:

```text
06c2c3b8  vp: codestyle: bump from clang-format-14 to clang-format-19
```

One commit, formatting only. **Pinning 2025.09 gives up no RVV functionality.**

It does give up scalar fixes that landed only *after* the SystemC 3.0 bump, so
they are unavailable at any 2.3-compatible revision:

| Upstream commit | What it fixes |
| --- | --- |
| `c7140542` | `fsh` / `fsw` raw value |
| `91777991` | `fmv_x_h` raw value |
| `63524fbb` | `fmax` / `fmin` for F, D, Zfh and NaN checks |
| `14e7fff5` | F/D/Zfh load/store — trap when float is disabled, set dirty |
| `b92c01d8` | float load/store cleanup and harmonisation |
| `52d376d4` | AMO handling rework, atomicity violation (lost bus lock) |
| `7a936cce` | fast quantum |
| `725e4353` | stack-pointer alignment per the ABI |

These are scalar FP, atomics and timing — not RVV. Each becomes a **required
case in the Phase 2 differential corpus against Spike**, so a defect shows up as
a diff rather than as a silent wrong answer.

Two exceptions to "cannot take the fixes", both decided later:

* **`b710fa7b` is backported** (F11). It was not a numeric difference a diff
  would catch — it stopped the core executing under any sane watchdog and
  corrupted `mcycle` from reset. Two lines, no SystemC API dependency.
* **`7a936cce` and `52d376d4` are deliberately not backported.** The first is a
  larger change to quantum accounting and needs its own audit before Phase 7;
  the second needs a multi-hart AMO contention test before Phase 6, which a
  single-threaded differential run cannot provide.

## 4. Measured build recipe

Built and linked on this host with `CC=/usr/bin/gcc`, `CXX=/usr/bin/g++`,
GCC 11.5.0, CMake 3.31.8, `CMAKE_BUILD_TYPE=Release`, C++17,
`-DSC_ALLOW_DEPRECATED_IEEE_API`, against `/opt/systemc-2.3.4`.

Sources — this is the complete list, and it is deliberately short:

```text
vp/src/vendor/softfloat/*.c                 (all, with the upstream defines)
vp/src/core/common/timer.cpp
vp/src/core/common/real_clint.cpp
vp/src/core/common/instr.cpp
vp/src/core/common/debug_memory.cpp
vp/src/core/common/rawmode.cpp
vp/src/core/common/iss_stats.cpp
vp/src/core/rv32/iss.cpp
vp/src/core/rv32/syscall.cpp
```

Include paths:

```text
vp/src   vp/src/core/common   vp/src/core/rv32   vp/src/util
vp/src/vendor/softfloat/include   <boost headers>   /opt/systemc-2.3.4/include
```

softfloat defines, copied from upstream:
`SOFTFLOAT_ROUND_ODD`, `INLINE_LEVEL=5`, `SOFTFLOAT_FAST_DIV32TO16`,
`SOFTFLOAT_FAST_DIV64TO32`, `SOFTFLOAT_FAST_INT64`.

Result:

```text
libvpp_rv32_iss.a    2.4 MB
libvpp_softfloat.a   414 KB
compiler warnings    0
```

### What is excluded, and confirmed unnecessary

| Excluded | Evidence it is not needed |
| --- | --- |
| Qt / `qtbase5-dev` | referenced only under `vp/src/platform/{common,linux}` and `vp/src/util/vncserver.*` |
| `libvncserver` | only `vp/src/util/vncserver.cpp`, which is not compiled |
| `gdb-mc` + the `mpc` submodule | a separate CMake target; `rv32` links `systemc core-common softfloat` only |
| `nlohmann/json` | no reference anywhere under `vp/src` at the pinned revision |
| upstream platforms (`basic`, `tiny32`, `linux`, `hifive`, `gd32`, …) | not compiled; CDC-VP owns memory, bus, CLINT/PLIC, devices and NoC |
| vendored SystemC submodule | we build against `/opt/systemc-2.3.4` |
| Boost **compiled** libraries | see below |

Boost: `v.h` uses `boost/format.hpp` and `boost/multiprecision/cpp_int.hpp`,
both header-only. Host Boost is 1.75.0 (`boost-devel-1.75.0-13.el9_7`). The
linked probe binary's runtime dependencies are:

```text
libsystemc.so.2.3  libstdc++.so.6  libm.so.6  libgcc_s.so.1  libc.so.6
```

No `libboost_*`, no Qt, no VNC. **The portable package needs nothing it does not
already ship.**

## 5. Measured capability results

A probe program (`two_instances.cpp`) constructs two independent RV32 ISS
instances with no upstream platform, binds each to a TLM target socket it owns,
and registers no DMI range. Output:

```text
hart_id core0            : 0
hart_id core1            : 7
vlenb core0              : 64
vlenb core1              : 64
misa core0               : 0x4034312d
misa.V core0             : 1
VLEN / ELEN (compile)    : 512 / 64
vector regs              : 32
dmi ranges core0         : 0
core0 v3[0]              : 0xdeadbeef
core1 v3[0]              : 0x0
```

`misa = 0x4034312d` decodes to MXL=1 (RV32) with A, C, D, F, I, M, N, S, U and
**V** — `RV_ISA_Config`'s default is `IMACFDV + NUS`, so V is on without a
configuration change.

Against the D3 promotion gate and plan §11.2:

| Requirement | Status | Evidence |
| --- | --- | --- |
| Reproducible 64-bit build, GCC 11.5 + SystemC 2.3.4 | **met** | §4, zero warnings |
| Embeddable without Qt, VNC, an external executable or a full platform | **met** | §4 exclusion table, `ldd` output |
| RVV 1.0 present | **met** | `misa.V = 1`, `v.h` implements the RVV instruction set |
| `VLEN = 512` | **met** | compile-time constant, measured 512 |
| `ELEN = 64` | **met** | compile-time constant, measured 64 |
| `vlenb = 64` | **met** | CSR read back as 64 on both instances |
| 32 vector registers of VLEN bits | **met** | `NUM_REGS = 32`, `v_regs` sized `32 * VLENB` |
| More than one instance without mutable global architectural state | **met, with one exception** | independent `v3[0]` and `x5` per instance; exception in F5 |
| Wrapper can control hart id | **met** | `ISS(RV_ISA_Config*, hart_id)` constructor argument |
| Wrapper can control reset PC | **met** | `init(..., entrypoint, sp_base)` |
| Interrupts controllable | **met (API present)** | `trigger_/clear_{external,timer,software}_interrupt`, `external_interrupt_target`, `clint_if` |
| PC and instret accessors | **met (API present)** | `get_progam_counter()`, `csrs`, `iss_stats` |
| All CPU memory traffic redirectable to CDC-VP TLM | **met** | `CombinedMemoryInterface_T` is an `sc_module` with `tlm_utils::simple_initiator_socket isock`; instruction fetch uses the same interface when `InstrMemoryProxy` is not installed; zero DMI ranges registered |
| RV32GCV ELF executes through the wrapper | **met** | `rvv_smoke_execution`: 1374 instructions, all 14 RVV checks pass |
| Fetch, scalar and vector traffic observed on TLM | **met** | 1006 TLM requests (762 read, 244 write), 686 of them into `.vdata`, 0 errors |
| Differential agreement with the pinned Spike oracle | **not yet** | §9 |
| Expected traps for illegal vector configuration | **not yet** | §9 |
| Usable absolute simulated time | **met** after the F11 backport | first TLM request at 0 s; 28 030 ns end-to-end for a 28 us workload; `mcycle` 15 → 2439 → 2848 |
| Two harts execute independently | **met** | `rvv_smoke_execution` runs harts 0 and 7, both complete within a 1 ms watchdog |

No frozen architectural parameter is unsupported, so plan §11.2's "Phase 2
stops" condition does **not** apply. VLEN/ELEN/vlenb are exactly the frozen
values with no configuration change and no relabelling.

## 5a. What has been implemented since the audit

| Item | Where | Test |
| --- | --- | --- |
| Freestanding RV32GCV smoke image (D4) | `fw/TPU_V3_SoC/rvv_smoke/` — `crt0.S`, `link.ld`, `main.c`, no libc or libm | `make verify`: ISA attributes report `v1p0` and `zvl512b1p0`, and every required vector instruction is in the disassembly |
| The image executed through the backend, with observed TLM traffic | `cpu_models/riscv_vp_plusplus/tests/test_rvv_smoke.cpp` | `rvv_smoke_execution` |
| D10 vector-trap gate: mid-vector fault, `vstart` resumption, `mstatus.VS`, reserved `vtype`, EEW=64 probe | `fw/TPU_V3_SoC/rvv_smoke/trap_main.c`, `tests/test_rvv_trap.cpp` | `rvv_vector_trap` |
| F5 concurrency control: two harts, different rounding modes, measured interleaving | `fw/TPU_V3_SoC/rvv_smoke/fp_main.c`, `tests/test_fp_concurrency.cpp` | `fp_concurrency_normal`, `fp_concurrency_swapped` |
| F11 backport, applied and verified | `fetch_riscv_vp_plusplus.sh`, `patches/`, CMake verification, manifest | `rvv_smoke_execution` (two harts, 1 ms watchdog, mcycle checks); refusal verified by reverting the patch |
| ISS caches confirmed off, and pinned by a test | `riscv_vp_plusplus_wrapper.cpp`; `rvv_smoke_execution` | reads (fetches + loads) must be ≥ instructions retired: 1849 ≥ 1403 with the caches off, 770 < 1406 with them on |
| Interrupt delivery: software, timer, external; cause, `mie` masking, level-triggered clear | `fw/TPU_V3_SoC/rvv_smoke/irq_main.c`, `tests/test_irq.cpp` | `interrupt_delivery` — `set_irq()` had never been executed by a test before this |
| Portable executable containing the backend | `cpu_models/riscv_vp_plusplus/portable/` | `riscv_vp_plusplus_portable` — runs from a directory holding only the binary, its SystemC libraries and one ELF, with `LD_LIBRARY_PATH` cleared |
| Spike oracle at the audited pin, standalone | `cpu_models/riscv_vp_plusplus/oracle/fetch_spike.sh` → `third_party/riscv-isa-sim` | built and run; never linked into the platform or a package (D3) |
| Differential corpus: one image, both models, 71-field canonical signature | `fw/TPU_V3_SoC/rvv_smoke/sig_layout.h`, `sig_main.c`, `crt0_sig.S`, `link_dram.ld`; `oracle/test_spike_differential.cpp` | `spike_differential` — **64 matched, 7 XFAIL (D9), 0 OPEN, 0 unexplained** |
| D12 conformance patch: all 32 RV32 index-EEW=64 encodings are illegal | `patches/0002-…`, `fw/TPU_V3_SoC/rvv_smoke/conf_main.c` | `conformance_patches`; all 32 independently check `mcause`, `vstart` and VS placement, and the negative control is verified |
| D13 conformance patch: a bus error is an access fault chosen by origin | `patches/0003-…`, `fw/TPU_V3_SoC/rvv_smoke/conf_main.c` | `conformance_patches` (six origins plus the generic-refusal response); negative control verified the same way |
| Patch mechanism generalised from one patch to a verified series | `fetch_riscv_vp_plusplus.sh`, `CMakeLists.txt`, `write_build_manifest.cmake` | configure prints the series and refuses if any member is missing or its hash changed; the manifest records kind, reference, file and hash per patch |
| Patch guard proves the *result*, not just the ingredients | `RISCV_VP_PLUSPLUS_PATCHED_FILES` in `CMakeLists.txt` | each patched file's post-patch content is hashed and no other file may be modified; both controls verified — a stray edit inside a patched file and a stray edit in an untouched file each abort configure |
| TLM global quantum guard | `riscv_vp_plusplus_wrapper.cpp` | without it the ISS silently retires nothing in Release |
| `cpu_config.hart_id` / `.reset_pc` as static properties (D5) | `cpu_models/include/cdc/cpu/cpu_base.h` | `riscv_vp_plusplus_backend` |
| `riscv_vp` refuses a hart id or reset vector it cannot honour (D5) | `cpu_models/riscv_vp/src/riscv_vp_wrapper.cpp` | build of every existing platform still passes |
| `cdc::cpu::riscv_vp_plusplus` backend | `cpu_models/riscv_vp_plusplus/` | `riscv_vp_plusplus_backend` |
| Pin enforcement at configure time | `cpu_models/riscv_vp_plusplus/CMakeLists.txt` | verified: a `master` checkout is refused, naming the SystemC 3.0.1 reason |

One refinement to D5's literal struct, and the reason for it: D5 shows
`std::uint64_t reset_pc = 0;`, but `0` is a **legitimate** reset vector — the
TPU_V3 map puts `GLOBAL_BOOT_ROM` at `0x0000_0000`, so `reset_pc == 0` must mean
"reset at address zero" and cannot simultaneously mean "unset". The field keeps
D5's name and type; only its default becomes an explicit
`cpu_config::reset_pc_unspecified` sentinel, so existing single-hart platforms
that never set it keep their backend's own vector while TPU_V3 can ask for zero
and mean it.

## 6. Integration recipe

Derived from `vp/src/platform/tiny32/tiny32_main.cpp`, reduced to what CDC-VP
needs:

```cpp
RV_ISA_Config isa;                       // default IMACFDV+NUS: V is enabled
rv32::ISS core(&isa, hart_id);           // D5 hart_id, construction time
rv32::MMU mmu(core);
rv32::CombinedMemoryInterface mem_if("mem_if", core, &mmu);
mem_if.bus_lock = std::make_shared<cdc_bus_lock>();
core.init(&mem_if,  /*use_dbbcache=*/false,
          &mem_if,  /*use_lscache=*/false,
          &clint_adapter, reset_pc, stack_top);
mem_if.isock.bind(<CDC-VP core-local fabric target socket>);
```

Points the wrapper must honour:

* **Never call `dmi_add()` and never install `InstrMemoryProxy`.** Both are how
  upstream bypasses TLM for speed; either would violate plan §11.2's memory
  integration rule and `INTERFACE_CONTRACT.md` §9.
* **`use_dbbcache` and `use_lscache` stay `false` for the Phase 2 gate.** They
  are decode/load-store caches inside the ISS; whether they are TLM-transparent
  has not been measured, so enabling them is a separate, evidenced decision.
* `bus_lock_if` needs an implementation. Upstream's `BusLock` lives in
  `vp/src/platform/common/bus.h`, which CDC-VP does not use; the interface is
  four methods, so CDC-VP owns a small one (F6).
* `clint_if` is a single method, `update_and_get_mtime()`. CDC-VP's
  `clint_tlm` sits behind a thin adapter.
* Use our own runner rather than `DirectCoreRunner`: the latter calls
  `sc_stop()` when the core terminates, which is a platform decision.

## 7. Findings

### F2 — vector load/store is element-wise, not one wide access

`v.h` implements every vector load/store as a per-element loop over `evl`, each
element a separate call on the ISS data-memory interface:

```cpp
for (xlen_reg_t i = 0; i < evl; ++i) {          // v.h:773
    ...
    case 64: value = iss.mem->load_double(addr); break;
```

There are exactly eight call sites: `load_byte/half/word/double` and
`store_byte/half/word/double`. Upstream even notes it:
*"TODO: implement loads in LSCache able to handle unaligned access and
optimized for vector"*.

Consequence: one `vle64.v` at VLEN=512 becomes **8 separate 8-byte TLM
transactions**; one `vle8.v` becomes **64 single-byte transactions**. The good
news is that vector traffic therefore traverses exactly the same CDC-VP path as
scalar traffic, which is what plan §11.2 requires.

This contradicted `INTERFACE_CONTRACT.md` §6 as originally written, which
required SVM to accept a 64-byte vector transfer as one transaction and called
splitting it "inside the model" a fabrication of arbitration events.

**Resolved as decision record D7** (approved 2026-08-10): accept element-wise
traffic, no upstream fork, no coalescing by inference.

Two things the original contract got wrong, both now recorded in D7:

* it assumed an RVV instruction boundary still exists at the TLM interface. The
  ISS splits before the model is reached, and `data_memory_if` carries no
  boundary to recover, so the contract forbade the model from doing something
  the model was never in a position to do;
* "one vector register, one wide transaction" only ever described the
  unit-stride unmasked case. **Masked, strided, indexed and fault-only-first
  accesses cannot be a single 64-byte transaction under any backend** — they
  touch a subset, a non-contiguous set, a per-element computed set, or a set
  truncated by a fault.

The Phase 3 consequences (1..64-byte support proven with a synthetic initiator,
the four counter names, the prohibition on `vector_instruction_count` and on
hardware-equivalence claims, the `VP++ element-wise granularity` label) are
specified in D7 and are now reflected in `INTERFACE_CONTRACT.md` §6 and the
plan's §11.3 and Phase 3 gate.

### F3 — VLEN and ELEN are compile-time constants

```cpp
// vp/src/core/common/v.h
constexpr unsigned VLEN = 512;   // "TODO these should be compile arguments"
constexpr unsigned ELEN = 64;
constexpr unsigned NUM_REGS = 32;
```

The defaults are exactly the frozen TPU_V3 values, so nothing has to change.
Two consequences to record: a different VLEN needs a source edit rather than a
constructor argument, and **all ISS instances in one process necessarily share
one VLEN** — harmless here, since all 16 TPU_V3 cores are VLEN=512, but it means
a mixed-VLEN experiment is not possible without a fork.

### F4 — Berkeley SoftFloat already provides the BF16 primitives D6 needs

The vendored softfloat (Release 3d, Regents of the University of California,
BSD-3-Clause in file headers) includes:

```text
f32_to_bf16.c   bf16_to_f32.c   s_roundPackToBF16.c
```

Decision D6 requires BF16 operands with FP32 accumulation and *"defined and
tested BF16 round-to-nearest-even conversion, special values, and the chosen
subnormal policy"*. These files are an IEEE-correct implementation of exactly
that conversion, already in the build. **Phase 4 should use them rather than
hand-rolling BF16 rounding**, which removes the most error-prone part of the MXU
arithmetic contract.

**This solves the conversion only.** Reusing softfloat does not discharge D6.
Phase 4 still owns:

* **pinning round-to-nearest-even.** softfloat's rounding is taken from a global
  (`softfloat_roundingMode`, see F5) that the ISS writes from `fcsr.frm`, so the
  MXU would otherwise inherit whatever the last FP instruction on some hart left
  behind. The BF16 adapter must set and restore it.
* **`softfloat_detectTininess`**, which selects before- or after-rounding
  tininess detection and therefore changes subnormal results. D6 requires a
  *chosen* subnormal policy; this is the knob that expresses it.
* **`softfloat_exceptionFlags`**, which accumulates. An adapter that neither
  clears nor reads it lets one operation's flags be attributed to another.
* **fixing the FP32 accumulation order**, which softfloat does not do for us —
  D6 requires determinism across hosts, and that is a property of the reduction,
  not of the primitives.

All three globals are process-wide (F5), so the adapter must treat them as
shared state and not as its own.

Required Phase 4 tests: ties, NaN, Inf, subnormal and signed zero, each checked
for both the result **and** the resulting exception flags.

Packaging follow-up: add `licenses/SOFTFLOAT-BSD-3-Clause.txt` when softfloat is
linked.

### F5 — the softfloat state is process-global, not thread-local

```c
extern THREAD_LOCAL uint_fast8_t softfloat_roundingMode;   // softfloat.h:74
```

`THREAD_LOCAL` looks like isolation. It is not. Both
`include/softfloat/softfloat.h` and `softfloat_state.c` contain

```c
#ifndef THREAD_LOCAL
#define THREAD_LOCAL
#endif
```

and **nothing defines it** — not upstream's
`vp/src/vendor/softfloat/CMakeLists.txt`, not upstream's `vp/CMakeLists.txt`,
and not CDC-VP's `cpu_models/riscv_vp_plusplus/CMakeLists.txt`. The macro
expands to nothing, so these are plain globals:

```c
THREAD_LOCAL uint_fast8_t softfloat_roundingMode  = softfloat_round_near_even;
THREAD_LOCAL uint_fast8_t softfloat_detectTininess = init_detectTininess;
THREAD_LOCAL uint_fast8_t softfloat_exceptionFlags = 0;
```

Even if it *were* `thread_local` it would not isolate anything here: SystemC
processes are coroutines on one OS thread. So all three are shared by every ISS
instance in the process, and by the Phase 4 MXU BF16 adapter (F4).

`v.h::set_fp_rm()` writes `softfloat_roundingMode` from `fcsr.frm`, from two
call sites.

Assessed risk: low but not provable. It is written immediately before the
arithmetic that reads it, and pure vector FP arithmetic performs no memory
access, so no `wait()` — and therefore no switch to another core — can occur in
between. `softfloat_exceptionFlags` is the weaker one: it **accumulates**, so
one hart's flags are visible to another with no interleaving hazard required at
all.

**Control implemented and run** — `fp_concurrency_normal` and
`fp_concurrency_swapped`, over `fw/TPU_V3_SoC/rvv_smoke/fp_main.c`.

Design, and why each part is there:

* the operands cannot agree between modes. `1.0f + 1.5 x 2^-24` sits 0.75 ulp
  above 1.0, so round-to-nearest yields `0x3f800001` and round-toward-zero
  `0x3f800000`. A hart inheriting the other's mode computes a visibly different
  number rather than a rounding-invisible one;
* hart 0 truncates, hart 1 rounds to nearest, and each re-sets `frm` **every
  iteration** — 200 iterations each;
* both numeric results and `fflags` are asserted, per iteration. Flags matter
  independently: `softfloat_exceptionFlags` accumulates, so a leak there needs
  no interleaving hazard at all;
* both creation orders are run as separate cases, because creation order decides
  which hart SystemC schedules first;
* the interleaving is **measured, not assumed**. The harness counts how often
  the accessing hart changes and fails below 100. A green result from two harts
  that ran one after the other would be evidence of nothing.

Result:

| | normal order | swapped order |
| --- | --- | --- |
| hart switches | 1890 | 1889 |
| accesses (hart 0 / hart 1) | 3352 / 3354 | 3352 / 3354 |
| result mismatches | 0 | 0 |
| `fflags` mismatches | 0 | 0 |
| `frm` readback mismatches | 0 | 0 |

**No leak observed**, under roughly one hart switch every 3.5 accesses, in Debug
and Release.

Consequently the save/restore mitigation is **not** implemented. There is no
demonstrated failure to justify it, and adding it would remove the evidence that
would justify it later.

What this does **not** prove, and why the control stays in the regression: it
shows the FP paths *these instructions* take call `set_fp_rm()` before use. It
does not show that every FP path does — `set_fp_rm()` has only two call sites in
`v.h`. Any new consumer of the shared globals must be re-checked against this
control, and the **Phase 4 BF16 adapter (F4) is exactly such a consumer**: it
will write `softfloat_roundingMode`, `softfloat_detectTininess` and
`softfloat_exceptionFlags` itself.

### F6 — `BusLock` lives in the platform layer

`bus_lock_if` (4 methods) is in `core/common/bus_lock_if.h`, but the only
implementation is `BusLock` in `vp/src/platform/common/bus.h`, which CDC-VP does
not use. CDC-VP therefore owns a small implementation; the probe already
contains one. It matters for `lr`/`sc` and AMO correctness, so it is not
optional and it must be the *same* instance across the cores that share an
address space.

### F7 — the vector register file is never freed

`VExtension`'s constructor does `v_regs = malloc(NUM_REGS * VLENB)` and there is
no destructor. That is 2 KiB per ISS instance, 32 KiB at the Revision 1 maximum
of 16 cores — negligible, and irrelevant for a process that constructs its cores
once. Recorded so it is not mistaken for a leak introduced by CDC-VP, and worth
an upstream issue rather than a local patch.

### F8 — the PC is protected, and `init()` is a restart, not an architectural reset

```cpp
protected:
    // protected: must not modified directly (would break FastISS)
    uxlen_t pc = 0;
```

So `cpu_base::reset_cpu()` cannot poke the PC the way the Bremen wrapper does
(`impl_->iss.pc = entry_pc_`). `ISS::init()` is the sanctioned way to place the
PC without breaking FastISS, and reading it uses the public
`get_progam_counter()` (upstream's spelling, typo included).

**But `init()` is not an architectural reset, and must not be described as
one.** Its whole body is:

```cpp
this->instr_mem = instr_mem;
this->mem       = data_mem;
this->clint     = clint;
regs[RegFile::sp] = sp;
pc = entrypoint;
void *fast_abort_and_fdd_labelPtr = genOpMap();
dbbcache.init(use_dbbcache, isa_config, hartId, instr_mem, opMap, ..., entrypoint);
lscache.init(use_lscache, hartId, data_mem);
cycle_counter_raw_last = 0;
```

It assigns the memory interfaces, `sp`, the PC, the caches and the cycle
baseline. It does **not** clear the general-purpose registers, the FP or vector
register files, the CSRs, `vstart`, `instret`, pending interrupts, or the
privilege level. A hart put through it keeps almost all of its architectural
state.

The honest description of what `reset_cpu()` currently provides is therefore
**"restart at the reset PC and reinitialise the caches"**, and the code says
exactly that. Nothing in this audit demonstrates full reset semantics, and no
later phase may cite it as if it did.

**Open item, and it needs its own decision before Phase 5 reset sequencing.**
`ARCHITECTURE.md` §6 requires hierarchical platform → chip → core → component
reset, and Phase 5 wires core reset. A restart is not enough for that: two harts
"reset" this way would retain stale GPR, CSR and vector state, and a reset test
would pass while proving nothing. The options — extend upstream, drive reset by
reconstructing the ISS, or define a narrower reset contract and say so — are a
decision, not an implementation detail, and are listed in §9.

### F9 — the two vendored softfloat copies are not interchangeable

Both the Bremen `riscv_vp` backend and VP++ vendor Berkeley SoftFloat, and both
upstream CMakeLists create a target literally named `softfloat`, so adding
VP++'s via `add_subdirectory` collides:

```text
add_library cannot create target "softfloat" because another target with the
same name already exists.
```

Sharing one of them would be worse than the collision. Measured difference:

| Copy | `.c` sources | BF16 primitives |
| --- | --- | --- |
| `third_party/riscv-vp/vp/src/vendor/softfloat` | 218 | **none** |
| `third_party/riscv-vp-plusplus/vp/src/vendor/softfloat` | 230 | `f32_to_bf16.c`, `bf16_to_f32.c`, `s_roundPackToBF16.c` |

Linking the Bremen copy into the VP++ ISS would silently change floating-point
behaviour and would remove exactly the BF16 primitives F4 wants for Phase 4.
CDC-VP therefore builds a private `softfloat_vpp` target from VP++'s own source
set and defines.

### F12 — RV32 index EEW=64 was wrongly accepted — **FIXED (D12)**

Plan §11.2 lists an "RV32 restriction on 64-bit vector index EEW". Measured
before the fix: VP++ executed `vluxei64.v` on this RV32 hart without raising an
illegal instruction.

When first recorded this audit declined to call it a defect, because index EEW
64 does not exceed this machine's ELEN of 64. That doubt was misplaced, and the
reason is worth keeping: **ELEN is not the yardstick here, XLEN is.**

**Specification.** RVV 1.0 §18.2, defining the `V` extension: *"The V extension
supports all vector load and store instructions … except the V extension does
not support EEW=64 for index values when XLEN=32."* §7.3 supplies the
enforcement rule: *"An implementation must raise an illegal instruction
exception if the EEW is not supported for offset elements"*, and notes that a
profile may cap index EEW at XLEN below ELEN.

**Root cause.** Spike enforces it explicitly in `VI_CHECK_ST_INDEX`:

```cpp
require(elt_width <= std::min(P.VU.ELEN, (reg_t)P.get_xlen()));
```

VP++ has no XLEN-dependent check anywhere in its vector unit.
`vp/src/core/common/v.h` is written against a fixed `typedef uint64_t
xlen_reg_t; // TODO change to generic`, so an RV32 hart runs the RV64 index
path.

**Fix: downstream conformance patch, decision record D12.** All **32** encodings
that carry an index EEW raise an illegal instruction in the RV32 decode: the
four unit forms `v[sl][ou]xei64.v` and the 28 segment forms
`v[sl][ou]xseg[2-8]ei64.v`.

The segment forms are not an afterthought. The first version of this patch
covered only the four unit forms, and the corpus did not notice because it
probes `vluxei64.v` — 28 encodings stayed wrong behind a green gate. The
restriction is on the *index* EEW, which every one of the 32 carries; Spike
applies it to all of them through one `VI_CHECK_ST_INDEX`. The gate now asserts
a 32-bit mask, and a patch covering only the unit forms is rejected (verified). Expressed by
refusing the encodings in `vp/src/core/rv32/iss_ctemplate.cpp` rather than by an
XLEN test, because that file *is* the RV32 build; the RV64 file is untouched and
keeps all 32.

**Placement is part of the fix, not an implementation detail.** The check is the
first statement of each `OP_CASE`, before `stats.inc_loadstore()` and before
`prepInstr()`. A check inside `vLoadStore()` — the obvious place, and where the
index-EEW arithmetic already lives — would produce the right `mcause` and still
be wrong: `prepInstr()` sets `mstatus.VS` to Dirty before it runs, so an
instruction that never executed would have dirtied vector state. Gate
`conformance_patches` measures all five consequences, and `mstatus.VS` staying
Clean is the one that pins the placement.

Measured after the fix: all 32 trap with `mcause` 2, `vstart` and the
destination register unchanged, `mstatus.VS` still Clean, and **zero** bus
requests to the operand buffer. Spike and VP++ agree exactly on both F12 fields
in the differential corpus.

### F13 — an unmapped access reported a page fault — **FIXED (D13)**

Found by the differential corpus, not predicted by the source review.

The corpus runs a vector load off the end of RAM. Spike reported `mcause` 5,
`EXC_LOAD_ACCESS_FAULT`; VP++ reported `mcause` 13, `EXC_LOAD_PAGE_FAULT`.

**Correction to an earlier draft of this finding.** It said the core is
constructed with `mmu == nullptr`. That is wrong: `riscv_vp_plusplus_wrapper.cpp`
passes `&mmu` to `CombinedMemoryInterface`. The conclusion is unchanged, for a
better reason — `satp.MODE` is Bare, so no translation is performed and no page
fault can arise however the MMU object is wired.

That correction also rules out the tempting fix. Choosing the cause from
`mmu != nullptr` would have been wrong in exactly this configuration, and would
have quietly become wrong again the day an MMU is enabled. The rule has to be
about *where the failure happened*, not about which objects exist:

| Failure | Cause |
| --- | --- |
| translation, PTE or permission, inside the MMU | page fault, 12 / 13 / 15 |
| bus, decode or target refusal, after translation | access fault, by origin |

**Root cause.** `CombinedMemoryInterface_T::_do_transaction` in
`vp/src/core/common/mem.h` mapped *any* TLM error to a page fault, without
reading the response status and without regard to the origin of the access.

**Fix: downstream conformance patch, decision record D13.** `MemoryAccessType`
is threaded from the caller down into `_do_transaction`, and the cause follows
the origin:

| Access origin | `mcause` |
| --- | --- |
| instruction fetch | 1, instruction access fault |
| load, including vector load | 5, load access fault |
| store, AMO, vector store | 7, store/AMO access fault |

Deriving the cause from the TLM command alone would have been the easy version
and would have been wrong twice: an instruction fetch is a TLM read, and an AMO
that fails on its read half must still report 7 — the privileged specification
has no "AMO load access fault".

**TLM response policy.** Only `TLM_ADDRESS_ERROR_RESPONSE` and
`TLM_GENERIC_ERROR_RESPONSE` — a target legitimately refusing the access —
become a guest trap. `TLM_INCOMPLETE_RESPONSE` and the command, burst and
byte-enable protocol errors are model or integration defects and now raise a
`std::runtime_error` instead. Turning one of those into a guest fault would hand
firmware a plausible trap for a bug it cannot have caused, and bury the real
failure.

**Residual, recorded rather than left to be rediscovered.** A bus error during a
page-table walk is reported as a load access fault, because `mmu_memory_if` does
not carry the originating access type. Strictly it should report the origin's
type. Widening that interface is outside D13's scope and the path is unreachable
at `satp.MODE = Bare`; the comment in `mem.h` says so at the call site.

Measured after the fix, in gate `conformance_patches`: fetch 1, scalar load 5,
scalar store 7, vector load 5, vector store 7, AMO 7, each with `mtval` at the
faulting address and `vstart` naming the element for both vector cases.

### D9 re-audit — three of the five named commits are unreachable at rv32gcv

Decision record D9 names five post-pin scalar-FP commits and requires the corpus
to carry each as an expected diff, failing both on an *unexpected* diff and on an
expected one *disappearing*. The list was assembled from commit titles. Read
against the pinned rv32 source, and then measured, only two of the five produce
an observable difference at the target ISA.

| Commit | What it changes in **rv32** | Reachable at rv32gcv? | Measured |
| --- | --- | --- | --- |
| `63524fbb` fmax/fmin NaN | v2.2 semantics for F, D and Zfh; plus an `f16_isNaN` mask fix | **yes**, via `fmin.s`/`fmax.s`/`fmin.d` | 4 XFAIL fields |
| `14e7fff5` FP load/store dirty bit and disabled-float trap | adds `fp_prepare_instr()` and `fp_set_dirty()` to the F, D and Zfh load/store cases | **yes**, via `flw` | 3 XFAIL fields |
| `c7140542` fsh/fsw raw value | rv32 hunks are `FSH` (`f16(RS2).v` → `u16`) and `FSD` (`f64(RS2).v` → `u64`) | **no** | `fsd_raw_*`, `fsw_raw` all match |
| `91777991` `fmv_x_h` raw value | Zfh only | **no** | both models raise illegal instruction |
| `b92c01d8` float load/store harmonisation | address type widths: `uint64_t`→`uxlen_t` for FLH/FSH, `uint32_t`→`uxlen_t` for FLD/FSD | **no** | `flw_nanbox_hi`, `fsd_raw_*` match |

Two distinct reasons for "no", and they are worth separating:

* **Zfh is not in the target ISA.** `fsh`, `flh`, `fadd.h` and `fmv.x.h` must
  raise an illegal instruction, and the corpus asserts exactly that on both
  models rather than recording an expected diff — the rule set for this gate.
  Measured: all four trap with `mcause` 2 on both models.
* **The change is a no-op at XLEN=32.** `FSD`'s `f64(RS2).v` already returns
  `regs[idx].v` unchanged, so `u64(RS2)` is the same value; and `uxlen_t` *is*
  `uint32_t` on rv32, so `b92c01d8` cannot alter an rv32 address computation.
  `FSW` at the pin already used the raw `u32(RS2)` accessor.

**Resolved: D9's wording is amended, its inventory is not.** All five commits
stay in the upstream provenance record — dropping them would lose the fact that
this pin predates them. The *expected-diff table* holds only the two observable
commits, seven fields in total, and the "an expected diff that disappeared is a
failure" rule applies to exactly those seven.

The other three keep their coverage as ordinary regression checks rather than as
expected diffs: `91777991` is asserted to be an illegal instruction on both
models, and `c7140542` and `b92c01d8` are asserted to be exact matches. That is
the right shape — they are not differences to be tolerated, they are agreements
to be maintained — and it removes the trap in the original wording, where an
expected diff that never appears is indistinguishable from one that vanished.

## 8. Upgrade path to upstream master

Recorded so the pin is a decision rather than a dead end. Adopting master (and
the scalar fixes in §3) requires, as one coordinated change:

1. migrate CDC-VP to SystemC 3.0.x — this is repository-wide: every platform,
   `SYSTEMC_HOME`, the packaged `libsystemc.so*`, and the FlooNoC model's RTL
   cross-check evidence, which was produced against 2.3.4;
2. provide `nlohmann/json` for `util/propertymap.h`;
3. re-check the C++ standard: master carries "preparations for switch from C++17
   to >=20" while CDC-VP is C++17;
4. re-run this audit and the Phase 2 gate at the new revision.

Until then the pin stays at `2025.09` and §3's fix list is covered by
differential testing.

## 9. Closure status and residual scope limits

The Phase 2 gate is closed. The following items are explicit scope limits or
later-phase work, retained so they are not mistaken for missing measurements:

* nothing from the Phase 2 gate list remains open. F12 and F13 were fixed
  under D12 and D13 rather than accepted as carried deviations, and the
  differential corpus is at 64 matched / 7 XFAIL / 0 open / 0 unexplained;
* three things this audit had asserted but not measured were found wrong on
  review, and each now has a test rather than a claim:
  the ISS caches were enabled while the code comment beside them said
  otherwise (P2-5); D12 covered 4 of the 32 encodings that carry an index EEW;
  and the portable-executable requirement was met by no binary — the unit tests
  carry an absolute SystemC RPATH and the packaged platform does not link the
  backend. The pattern is the lesson: every one was a *documented* property
  with no gate behind it;
* `TPU_V3_VPP_LINKED` in the platform manifest is still `FALSE`, and that is
  correct — the platform instantiates no core before Phase 5. The
  portable-executable requirement is met by `rvv_runner`, not by the platform
  binary, and the manifest describes the package rather than the test tree;
* the AMO cases from §3 are still not in a corpus. A single-hart differential
  run cannot demonstrate atomicity between harts, which is why `52d376d4`
  keeps its own pre-Phase-6 multi-hart test rather than being folded in here;
* `use_dbbcache` / `use_lscache` TLM transparency is unmeasured — they stay off;
* the corpus compares *architecturally visible* state only: values the program
  itself computes and stores. Two models that disagree internally but hide it
  from the program will match. Reading each simulator's private register file
  would compare more, but through two different debug interfaces, where a
  difference in the interfaces is indistinguishable from a difference in the
  models;
* **full architectural reset — decided by D19 (2026-08-18), not yet
  implemented.** `reset_cpu()` today is still a restart at the reset PC plus
  cache reinitialisation (F8); GPRs, FP/vector registers, CSRs, `vstart`,
  `instret`, pending interrupts and privilege level survive it. What changed is
  that the contract is no longer open: D19 fixes the four state classes and the
  gate, Phase 7 implements it, and D19 additionally records two things this
  audit did not reach — the cycle accumulator behind `mcycle` is private, so a
  mid-run reset re-adds the pre-reset count into simulated time, and a hart
  that has executed `sys_exit` cannot be revived in this build.

## 10. Decisions taken in this audit

| # | Decision | Value | Rationale |
| --- | --- | --- | --- |
| P2-1 | VP++ frozen revision | `7a36fe859cae242f513ca6ad16ab8238f1e82977` (tag `2025.09`) | newest revision building against SystemC 2.3.4 with no added dependencies; costs no RVV functionality |
| P2-2 | Do not migrate SystemC in Phase 2 | stay on 2.3.4 | a 3.0.x migration is repository-wide and would invalidate the FlooNoC cross-check evidence |
| P2-3 | VP++ acquisition | fetch into `third_party/riscv-vp-plusplus`, no vendoring, **no unrecorded patching**; apply only the approved F11 backport and D12/D13 downstream conformance patches | D3 and plan rule 11. Each patch is classified, content-hashed, script-applied, CMake-verified and recorded in the package manifest |
| P2-4 | Compiled scope | the nine translation units in §4 plus softfloat | excludes Qt, VNC, gdb-mc, nlohmann and every upstream platform |
| P2-5 | DMI and ISS caches | no DMI ranges, no `InstrMemoryProxy`, `use_dbbcache = use_lscache = false` | all CPU traffic must traverse CDC-VP TLM; cache transparency unmeasured. **Now enforced by a test**: the flags were briefly `true` during F11 diagnosis while the comment beside them still said `false`, and nothing caught it. `rvv_smoke_execution` requires reads ≥ instructions retired, which `dbbcache` breaks immediately |
| P2-6 | Vector access granularity | accept element-wise traffic, label it in metrics; amend `INTERFACE_CONTRACT.md` §6 | the ISS splits before the model sees it; the alternatives are forking upstream or guessing |
| P2-7 | BF16 conversion for Phase 4 | reuse vendored softfloat `f32_to_bf16` / `bf16_to_f32` / `s_roundPackToBF16` | IEEE-correct and already in the build; removes the riskiest part of D6 |
| P2-8 | §3 fix list | becomes required differential-corpus cases, except F11 which is backported | the rest are unavailable at any 2.3-compatible revision |
| P2-9 | F11 | backport upstream `b710fa7b` onto the base pin; no baseline subtraction, no SystemC 3.0.1 migration | two lines, no SystemC API dependency, applies cleanly; the offset is not a stable value so compensation was never viable |
| P2-10 | F11 closure | **a Phase 2 closure gate** — Phase 2 is not complete while `mcycle` is wrong | later phases build timing on it; deferring to Phase 7 would mean discovering it under load |
| P2-11 | Differential method | one image compiled once, run on both models, comparing a firmware-written canonical signature block located by the `begin_signature` / `end_signature` symbols | comparing what the *program* can see is the one interface both models are obliged to implement identically; comparing private register files would compare two debug interfaces instead |
| P2-12 | Oracle isolation | Spike runs as a **child process**, never linked | a linked oracle would share the allocator, the build flags and — decisively — one copy of Berkeley SoftFloat's process-global rounding state, the exact hazard F5 exists to guard |
| P2-13 | Corpus image placement | linked at `0x8000_0000` with its own `link_dram.ld`, while the three VP++-only images stay at 0 | Spike's boot ROM sits at `0x1000` and shadows a flat image based at 0, with no option to move it; `0x8000_0000` is also `GLOBAL_RAM_OR_HBM`, so the corpus runs where Phase 10 firmware will. Rebasing the three passing gates at closure time would risk three green results for no gain — Phase 5 should do it |
| P2-14 | Expected-diff table provenance | every entry derived by reading the pinned VP++ source *before* the first run | a table filled in from observed output cannot fail: it agrees with the model by construction. All seven predicted XFAILs materialised, which is what made the two that did not — F12 and F13 — worth reporting |
| P2-15 | Patch taxonomy | the series distinguishes `upstream-backport` from `downstream-conformance` in the script, the CMake verification and the manifest | they behave differently when the pin moves: a backport disappears, a downstream patch has to be re-checked against the new base and ideally offered upstream. Calling both "backports" would hide the one fact a future maintainer needs |
| P2-16 | Where a conformance check goes | at the decode site, before `stats.inc_loadstore()` and `prepInstr()` — not inside `vLoadStore()` | `prepInstr()` dirties `mstatus.VS`. A check further in would produce the right `mcause`, satisfy the oracle comparison, and still let an instruction that never executed count a load, drive the bus and modify architectural state |

P2-6 changes a written contract and should be confirmed before Phase 3 depends
on the SVM counter semantics.

P2-13 leaves the repository with two firmware memory layouts. That is a
deliberate, dated trade-off and not a permanent one: Phase 5 should move the
remaining three images to the real address map and delete `link.ld`.

> **Not closed in Phase 5 (checked 2026-08-18).** `link.ld` still exists and
> five images still link against it, so both layouts are still live. The item
> is now carried in plan §21 rather than left implied by this paragraph; it
> does not block Phase 7 and must close by the Phase 10 firmware work.
