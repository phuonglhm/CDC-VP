# TPU_V3 Phase 2 — RISC-V VP++ Pre-Integration Audit

Audit date: 2026-08-08
Scope: decision record **D3** (RV32GCV runtime), plan §11.2 "Pre-integration
audit and pinning" and the Phase 2 gate.

Every claim below was produced by a command run on this host or by reading the
fetched source at a named revision. Where something has not been measured yet it
is listed in §9 as outstanding, not asserted.

**Headline: the runtime is viable and is pinned, but not at upstream `master`.**
Upstream master requires SystemC 3.0.1; CDC-VP is built against SystemC 2.3.4.
The newest revision that builds against 2.3.4 without extra dependencies is the
`2025.09` release tag, and it costs nothing in RVV functionality.

**Effective source is that tag plus exactly one approved upstream backport**
(`b710fa7b`), which fixes finding F11. The mechanism is recorded, hashed,
script-applied and CMake-verified; see F11.

---

## 1. Pin decision

| Item | Value |
| --- | --- |
| Upstream | `https://github.com/ics-jku/riscv-vp-plusplus` |
| **Pinned base revision** | `7a36fe859cae242f513ca6ad16ab8238f1e82977` |
| Tag at that commit | `2025.09` |
| **Approved backport** | `b710fa7be2643b42cee92f5bbcb8cead4c0ed282` (F11) |
| **Effective source** | base + backport |
| Acquisition | `cpu_models/riscv_vp_plusplus/fetch_riscv_vp_plusplus.sh` |
| Commit date | 2025-09-08 13:52:31 +0900 |
| Commit subject | `vp: core: mem: Removed invalid quantum keeper assert check for blocking transport calls` |
| License | MIT (Univ. Bremen 2017–2018, JKU Linz 2022–2023) |
| Local path | `third_party/riscv-vp-plusplus` (gitignored, not vendored) |
| Role | runtime RV32GCV hart — scalar + RVV 1.0 |
| Pin status | **candidate**; promoted by the Phase 2 gate (D3) |

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
| F11 backport, applied and verified | `fetch_riscv_vp_plusplus.sh`, `patches/`, CMake verification, manifest | `rvv_smoke_execution` (two harts, 1 ms watchdog, mcycle checks); refusal verified by reverting the patch |
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

**Required Phase 2 control:** two instances executing FP work with *different*
`frm` values, interleaved, checked against the Spike oracle. It must assert the
**exception flags as well as the numeric results** — a run can produce the right
number with the wrong flags, and flags are what firmware tests. If it fails, the
wrapper saves and restores all three globals around each ISS run slice.

Do not assume this is fine because a single-core test passed.

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

### F12 — RV32 index EEW=64 is accepted, and the plan's wording needs a spec citation

Plan §11.2 lists an "RV32 restriction on 64-bit vector index EEW" as a required
check. Measured: VP++ executes `vluxei64.v` on this RV32 hart **without
raising an illegal instruction**.

Whether that is a defect is not self-evident, and this audit does not claim it
is. Index EEW 64 does not exceed this machine's ELEN of 64, so "reserved on
RV32" needs to be pinned to a specific RVV 1.0 clause before it can be a
pass/fail criterion. Encoding either expectation into a gate now would be
asserting a reading of the spec rather than testing one.

So the trap image **records** the outcome — `SIM_EEW64_TRAPPED` and
`SIM_EEW64_MCAUSE` in the exit block — and asserts nothing about it. The Spike
differential run is what settles it: an independent oracle disagreeing is
evidence, a guess in our own test is not. Whichever way it resolves, plan §11.2
gains a spec citation.

### F11 — bogus cycle baseline before the first instruction — **RESOLVED by an approved backport**

Symptom, when the smoke image was first run through the wrapper: the core
retired zero instructions, produced no TLM traffic, and the run ended at the
watchdog. `gdb` at the first `quantum_keeper.sync()`, reached *before any
instruction executed*:

```text
#0  rv32::ISS::exec_steps (...) at iss_ctemplate.cpp:316   quantum_keeper.sync();
#1  rv32::ISS::run (...)
$1 = {m_value = 2292084375000}   // quantum keeper local time, picoseconds
$2 = {m_value = 10000}           // cycle_time = 10 ns, correct
```

2.29 **seconds** of simulated time, banked before the first fetch. The core then
slept through it, which is why a 200 ms watchdog saw an apparently dead CPU.

Cause: `commit_cycles()` runs on the first slow-path pass and does

```cpp
uint64_t cycle_counter_raw = dbbcache.get_cycle_counter_raw();
uint64_t cycle_counter_raw_inc = cycle_counter_raw - cycle_counter_raw_last;  // last = 0
quantum_keeper.inc(sc_time(cycle_counter_raw_inc, SC_NS));
```

and `DBBCache_T::get_cycle_counter_raw()` reads
`curBlock->entries[curEntryIdx + 1].cycle_counter_raw`. With `curEntryIdx = 0`
that is the dummy block's `entries[1]`, which is never initialised;
`entries[0].cycle_counter_raw` *is* explicitly zeroed.

#### Resolution: controlled backport of upstream `b710fa7b`

Approved 2026-08-10. The upstream fix is two lines in `dbbcache.h`,
`curEntryIdx = 0` → `curEntryIdx = -1` in the member initialiser and in the
block-switch path, so the dummy block's zeroed `entries[0]` is used as the
predecessor. It touches no SystemC API and applies cleanly to the base pin,
which is what makes it safe to carry alone rather than migrating to master.

| | |
| --- | --- |
| Base revision | `7a36fe859cae242f513ca6ad16ab8238f1e82977` (tag `2025.09`) |
| Backport | `b710fa7be2643b42cee92f5bbcb8cead4c0ed282` |
| Patch | `cpu_models/riscv_vp_plusplus/patches/0001-b710fa7b-dbbcache-fixed-random-cycle-counting.patch` |
| Patch SHA256 | `16758c959a534e71c23254599ac7229c0ff85d3b4e1dc67ee3c82cc96f23b8ad` |
| Effective source | base + backport, recorded in `BUILD_MANIFEST.json` |

Mechanism, and the division of responsibility that makes it auditable:

* `cpu_models/riscv_vp_plusplus/fetch_riscv_vp_plusplus.sh` is the **only** thing
  permitted to modify the checkout. It verifies the patch's SHA256, checks out
  the exact base revision, applies the patch, and verifies the result by
  reverse-applying it. It is idempotent and refuses a checkout carrying local
  modifications that are not this patch.
* CMake **verifies and never patches**. A configure step that edited
  third-party source would make the compiled binary depend on when CMake last
  ran. It checks the patch hash, the base SHA, and applied-ness — the last by
  reverse-apply, not by grepping for the changed lines, which a hand-edited tree
  could also satisfy.

Measured, before and after, two harts:

| | before | after |
| --- | --- | --- |
| First TLM request at | 2.29 s | **0 s** |
| Simulated time at end | 2 292 112 075 ns | **28 030 ns** |
| `mcycle` start / mid / end | 4623 / 7047 / 7456 | **15 / 2439 / 2848** |
| Watchdog needed | 60 s | **1 ms** |
| Functional RVV checks | passed | passed |

#### Correction: the offset was never a stable value

An earlier revision of this finding reported the offset as "deterministic across
runs and across Debug and Release", and inferred from that a baseline-subtraction
workaround might be viable. That inference was wrong, and the evidence arrived
from the negative control.

With the patch reverted and **two** harts instead of one, the offset was
46 080 ns on hart 0 and 0 s on hart 7 — neither the 2 292 084 375 ns measured
with a single hart, nor equal to each other. The value tracks whatever happens
to sit in the uninitialised entry, so it varies with process layout and hart
count. Upstream's word "random" is accurate; my single-configuration
reproducibility was a coincidence of one binary. **Baseline subtraction could
never have worked**, and the decision to backport rather than compensate was the
correct one for a reason stronger than the one originally recorded.

#### What is deliberately *not* backported with it

| Commit | Why not |
| --- | --- |
| `7a936cce` "fixed fast quantum" | a larger change to quantum accounting; needs its own audit before Phase 7 |
| `52d376d4` AMO atomicity / lost bus lock | needs a multi-hart AMO contention test before Phase 6. A single-threaded differential run against Spike cannot demonstrate atomicity between harts |

Bundling them would put three unrelated behaviour changes behind one gate.

#### Regression

`rvv_smoke_execution` is the standing gate. It runs **two harts** with a **1 ms**
watchdog and fails if any of these regress:

* the first TLM request carries a startup time offset above 1 µs;
* `mcycle` does not start at a plausible reset baseline;
* the three firmware-sampled `mcycle` values are not strictly increasing;
* either hart fails to complete within the watchdog.

The watchdog size is itself part of the gate: 1 ms is generous for a 28 µs
workload and far below any plausible recurrence of the defect, so a return
fails here instead of being absorbed by a longer bound. Both the CMake refusal
and the runtime failure were verified by reverting the patch. Debug and Release
produce identical results.

If upstream publishes an official backport on a SystemC 2.3-compatible branch,
move the pin to that commit, delete the local patch, and re-run the whole
Phase 2 gate.

### F10 — `core/rv32/iss.h` includes a platform header

```cpp
#include "platform/gd32/nuclei_core/nuclei_csr.h"
```

A core header reaching into a specific vendor platform. It is header-only and
compiles, so it is not a blocker, but it means `vp/src` must stay on the include
path and the `platform/` directory cannot be pruned from the fetched tree.

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

## 9. Not yet measured — the rest of the Phase 2 gate

Listed so nothing here is mistaken for cleared:

* the pinned Spike oracle has not been built, and no differential corpus has
  been run. Preflight done: the pin `16c0b601...` is still upstream `HEAD`, all
  build dependencies (autoconf, automake, g++, make, dtc, python3, boost) are
  present, and the repository is small;
* **F12** — whether `vluxei64.v` should be illegal on RV32 is recorded but
  undecided; the differential run resolves it and plan §11.2 then gains a spec
  citation;
* traps for illegal vector configuration, `vstart` restart behaviour,
  `mstatus.VS` transitions and the RV32 64-bit index EEW restriction are
  untested;
* `use_dbbcache` / `use_lscache` TLM transparency is unmeasured — they stay off;
* the §3 scalar-FP and AMO cases are not yet in a corpus;
* the F5 interleaved-`frm` concurrency control does not exist yet, and it must
  assert exception flags as well as results;
* **full architectural reset is undecided.** `reset_cpu()` today is a restart at
  the reset PC plus cache reinitialisation (F8); GPRs, FP/vector registers,
  CSRs, `vstart`, `instret`, pending interrupts and privilege level survive it.
  This needs its own decision before Phase 5 reset sequencing, not a Phase 5
  discovery.

## 10. Decisions taken in this audit

| # | Decision | Value | Rationale |
| --- | --- | --- | --- |
| P2-1 | VP++ candidate revision | `7a36fe859cae242f513ca6ad16ab8238f1e82977` (tag `2025.09`) | newest revision building against SystemC 2.3.4 with no added dependencies; costs no RVV functionality |
| P2-2 | Do not migrate SystemC in Phase 2 | stay on 2.3.4 | a 3.0.x migration is repository-wide and would invalidate the FlooNoC cross-check evidence |
| P2-3 | VP++ acquisition | fetch into `third_party/riscv-vp-plusplus`, no vendoring, **no unrecorded patching — only the approved F11 upstream backport** | D3 and plan rule 11. Amended 2026-08-10: a recorded, content-hashed, script-applied and CMake-verified upstream backport is auditable in a way an unrecorded edit is not |
| P2-4 | Compiled scope | the nine translation units in §4 plus softfloat | excludes Qt, VNC, gdb-mc, nlohmann and every upstream platform |
| P2-5 | DMI and ISS caches | no DMI ranges, no `InstrMemoryProxy`, `use_dbbcache = use_lscache = false` | all CPU traffic must traverse CDC-VP TLM; cache transparency unmeasured |
| P2-6 | Vector access granularity | accept element-wise traffic, label it in metrics; amend `INTERFACE_CONTRACT.md` §6 | the ISS splits before the model sees it; the alternatives are forking upstream or guessing |
| P2-7 | BF16 conversion for Phase 4 | reuse vendored softfloat `f32_to_bf16` / `bf16_to_f32` / `s_roundPackToBF16` | IEEE-correct and already in the build; removes the riskiest part of D6 |
| P2-8 | §3 fix list | becomes required differential-corpus cases, except F11 which is backported | the rest are unavailable at any 2.3-compatible revision |
| P2-9 | F11 | backport upstream `b710fa7b` onto the base pin; no baseline subtraction, no SystemC 3.0.1 migration | two lines, no SystemC API dependency, applies cleanly; the offset is not a stable value so compensation was never viable |
| P2-10 | F11 closure | **a Phase 2 closure gate** — Phase 2 is not complete while `mcycle` is wrong | later phases build timing on it; deferring to Phase 7 would mean discovering it under load |

P2-6 changes a written contract and should be confirmed before Phase 3 depends
on the SVM counter semantics.
