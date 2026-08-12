# TPU_V3 Phase 0 — Source, Toolchain and License Audit

> **Historical architecture notice (D14/D15, final 2026-08-12):** this audit remains the
> evidence for Phase 0 dependency, NoC, toolchain, packaging and provenance
> findings. Its old two-MXU/SVM composition and optional-Sauria role are
> superseded. The current NEO-CORE is VP++ + core SRAM + independent TPU_V3 DMA
> + one Sauria SA + Im2Col/Col2Im Transform. Its interconnect is 32-bit
> AXI4-Lite control + native banked-SRAM local data + a bidirectional external
> AXI4/NoC bridge; there is no internal full AXI data crossbar.

Audit date: 2026-08-08
Auditor: implementation agent, following `TPU_V3_IMPLEMENTATION_PLAN.md` §16
Phase 0.

> **Superseded in part.** `TPU_V3_DECISION_RECORD.md` (initial approval
> 2026-08-08, amended through D15 on 2026-08-12) is the implementation
> authority where it and this file disagree. In particular decisions **P0-6**
> (SVM default), **P0-7** (MXU data
> types) and **P0-9** (`cpu_base` extension) have been replaced — see §9.
> Everything else here is a measurement and still stands.

This file records **measured facts about the host and the repository at the
audit date**. Every claim below was produced by a command run on this machine
or by an HTTP request to the named upstream, not from recollection. Where a
question could not be answered without work that belongs to a later phase, it
is listed as open rather than guessed.

---

## 1. Repository baseline

| Item | Value |
| --- | --- |
| Repository | `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP` |
| Branch | `dev` |
| HEAD | `8e4b95b2688c2d3c2dfc39a7f40d48e2ed9723ed` |
| HEAD date | 2026-08-08 15:17:21 +0700 |
| HEAD subject | `Add noc` |
| Working tree at audit start | clean except untracked `components/TPU_V3/` |

The tree was clean before this work started, so everything added by Phase 0 and
Phase 1 is separable from pre-existing user changes.

## 2. Host build environment

| Item | Value | Source |
| --- | --- | --- |
| OS | AlmaLinux 9.x, kernel 5.14.0-687.29.1.el9_8 | `uname` |
| Host C/C++ compiler | GCC 11.5.0 (`/usr/bin/gcc`, `/usr/bin/g++`) | `gcc --version` |
| CMake | 3.31.8 (`/usr/bin/cmake`) | `cmake --version` |
| Ninja | present (`/usr/bin/ninja`) | `command -v` |
| Python | `/usr/bin/python3` | `command -v` |
| SystemC | 2.3.4 at `/opt/systemc-2.3.4`, `lib64/libsystemc.so.2.3.4` | filesystem |
| Network access to GitHub | available (`git ls-remote` and `raw.githubusercontent.com` both succeed) | measured |

`CMAKE_CXX_STANDARD` is 17 at the top level; TPU_V3 code must compile as C++17.

> The default `PATH` on this machine puts a Synopsys `g++` wrapper ahead of the
> system compiler. Every build command in this project must export
> `CC=/usr/bin/gcc`, `CXX=/usr/bin/g++` and a `PATH` starting with
> `/usr/bin:/bin`, exactly as the plan's reference build flow does.

## 3. RISC-V cross toolchain — found, and it covers RV32GCV

Plan §9.6 recorded that no RISC-V compiler was visible in `PATH`. That is still
true of `PATH`, but a suitable toolchain **is installed** on this host and is
already used by the existing firmware Makefiles (`CROSS ?= riscv-none-elf-`,
and `fw/romcode_boot_riscv/Makefile` names the directory explicitly).

| Item | Value |
| --- | --- |
| Toolchain | xPack GNU RISC-V Embedded GCC |
| Version | 15.2.0 (2025) |
| Prefix | `/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-` |
| `-march=rv32gcv_zvl512b -mabi=ilp32d` | **accepted**, compiles cleanly |
| Effective expanded arch | `rv32imafdcv_zicsr_zifencei_zmmul_zaamo_zalrsc_zca_zcd_zcf_zve32f_zve32x_zve64d_zve64f_zve64x_zvl32b_zvl64b_zvl128b_zvl256b_zvl512b` |

Measured with:

```bash
/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-gcc \
    -march=rv32gcv_zvl512b -mabi=ilp32d -c probe.c -o probe.o
/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-gcc \
    -march=rv32gcv_zvl512b -mabi=ilp32d -Q --help=target | grep -E 'march|mabi'
```

The expanded arch string confirms `v`, `zve64d` and `zvl512b`, which is what
the frozen VLEN=512 / ELEN=64 / SEW≤64 requirement needs.

### Open toolchain item (Phase 2, not Phase 0)

`-print-multi-lib` lists **32 multilibs and none of them is a `v` variant**;
the closest RV32 double-float entry is
`rv32imafdc_zicsr_zifencei_zaamo_zalrsc/ilp32d`. Compiling is therefore proven;
**linking against libc/libm for a vector build is not**, because GCC will fall
back to the nearest multilib. This is not a blocker for Phase 0 or Phase 1, and
it is not a defect — a bare-metal `-nostdlib` link is unaffected. It must be
settled in Phase 2 before the RVV smoke ELF is declared to build reproducibly:
either link `-nostdlib` with our own startup, or accept the non-vector multilib
for scalar library code and document that choice.

**Toolchain pin decision:** xPack `riscv-none-elf` GCC **15.2.0-1** at the path
above. The pin is by absolute path plus version string; there is no upstream
revision hash to record for a binary distribution.

## 4. Spike (`riscv-isa-sim`) — pinned

| Item | Value |
| --- | --- |
| Upstream | `https://github.com/riscv-software-src/riscv-isa-sim` |
| **Pinned revision** | `16c0b60119f65a648643cf5d41e4e38e871f0bad` |
| Revision date | 2026-08-07 19:29:51 UTC |
| Revision subject | `Merge pull request #2363 from jerryzj/dev/jerryzj/fix-link-warnings` |
| License | BSD 3-Clause (Regents of the University of California, 2010–2017) |
| RVV present at this revision | yes — `riscv/vector_unit.h`, `riscv/v_ext_macros.h`, `riscv/isa_parser.h` all exist; `vector_unit.h` carries runtime `VLEN`, `ELEN` and `vlenb` fields |

Why a raw commit and not a tag: the repository's only release tags are
`v1.0.0` and `v1.1.0`, and both predate the vector unit entirely
(`riscv/vector_unit.h` returns HTTP 404 at `v1.1.0`). Pinning a tag here would
pin a Spike with no RVV at all. The pin is therefore an explicit commit SHA.

`VLEN` and `ELEN` are per-processor runtime values in this revision, set from
the `--varch` / ISA string, so VLEN=512 / ELEN=64 is a configuration rather
than a rebuild. That has to be **verified in Phase 2**, not assumed: this audit
read the header, it did not build or run Spike.

Redistribution: BSD-3-Clause is compatible with the packaged binary provided
the copyright notice, the condition list and the disclaimer ship with it. The
Phase 12 package must contain `licenses/RISCV-ISA-SIM-BSD-3-Clause.txt` and the
pinned SHA in `BUILD_MANIFEST.json`. Nothing from Spike has been copied into
CDC-VP by this audit.

## 5. FlooNoC model — reused as is, and its limits are hard

Component: `components/floo_noc_model`, exported as
`cdc::components::noc_interconnect`. Provenance is already recorded in
`components/floo_noc_model/PROVENANCE.md`; frozen upstream is FlooNoC
`9a6972a5f9b8117506d1df8a6505ce1da2bc9084` (`v0.8.4-10-g9a6972a`) under
SHL-0.51, with `common_cells` 1.39.0 and `axi` 0.39.9. TPU_V3 adds no new
FlooNoC provenance obligation, because TPU_V3 does not fork or copy it.

Constraints read out of the source, with the enforcement point:

| Constraint | Value | Enforced at |
| --- | --- | --- |
| Upstream initiators | 1..8 (3-bit frozen manager ID) | constructor |
| Instantiated mesh sizes | 2x2, 3x3, 4x4, 4x2, 2x4 only | `make_noc()`, `src/noc_interconnect.cpp:105` |
| Outstanding per port | 1..32 (frozen `MaxTxns`) | constructor |
| Bus width | 8 bytes | model |
| Max burst | 256 beats ⇒ 2048-byte aligned frame; longer payloads are **refused**, not split | `b_transport` |
| Same-node initiator + target | **refused** — `std::runtime_error` | `add_target`, `place_initiator`, and `end_of_elaboration` |
| Blocking target | a target that `wait()`s inside `b_transport` stalls the whole mesh in detailed mode | documented contract |

### 5.1 The NoLoopback finding, stated precisely

Plan §9.2 described this as a placement problem. It is stronger than that.
`reject_self_node_targets()` refuses **any** mapped target on a node that hosts
**any** upstream port — the check is not per initiator/target pair. Since a TPU
chip is both an initiator (it issues remote accesses) and a target (remote
chips read its aperture), every chip node would need both, and every such
configuration is rejected at `end_of_elaboration`.

Consequence for the phase plan:

* **Phase 7 (one chip + global memory) is reachable today.** The single chip
  occupies one node as an initiator; boot ROM, global control and global RAM
  sit on other nodes as targets. No chip aperture needs to be a NoC target.
* **Phase 8 (multi-chip) is blocked** until this is resolved. Chip-to-chip
  traffic requires each chip aperture to be a NoC target at the chip's own
  node.

Feasibility note in favour of solving it inside the wrapper: `node_state` in
`src/noc_interconnect.cpp` already carries independent `has_manager` and
`has_subordinate` flags, and the source comment says the *placement rule* — not
the network structure — is what prevents a node hosting both. `NoLoopback = 1`
ties only the Eject-input to Eject-output crossbar leg, so a node can inject
and eject; what it cannot do is deliver a flit it addressed to itself.

Three candidate resolutions, to be decided in Phase 7 and implemented before
Phase 8:

1. **Wrapper-level local bypass with an opt-in same-node mapping.** Add an
   explicit API (for example `add_target(..., local_port = N)`) that maps a
   region to a node which also hosts port `N`, and short-circuits an access
   *from that port* to the target socket without injecting a flit, while
   accesses from other ports route through the mesh and eject normally. A
   self-addressed flit is then never created, so the NoLoopback tie-off is
   never exercised. Cost: a change to an RTL-signed component, requiring a new
   negative control proving the bypass path is taken and that no flit is
   injected.
2. **Reconfigure the router with `NoLoopback = 0`.** Cost: a FlooNoC
   configuration change plus re-running the router cross-checks; the frozen
   RTL evidence in `docs/STATUS.md` no longer applies unchanged.
3. **Separate the chip's initiator node from its target node.** Zero code
   change, but it falsifies the floorplan — a chip's own aperture would appear
   one or more hops away from itself, which corrupts exactly the latency
   numbers detailed mode exists to produce. Recommended only as a temporary
   bring-up hack, clearly labelled.

Recommendation: option 1. It keeps the frozen RTL configuration intact, and the
local-bypass behaviour it needs is required by plan §11.8 anyway.

> **Approved as decision D1**, with the ownership made explicit: the bypass
> mapping names *which manager port owns* a co-located target, an access from
> that port injects no flit and consumes no outstanding slot, an access from
> any other manager routes and ejects normally, and a self-addressed
> transaction without a valid mapping is refused during elaboration. Scheduled
> as a **Phase 7 prerequisite**, not a Phase 8 discovery.

### 5.2 Mesh size versus chip count

With one aggregated manager per chip the 8-manager limit caps the system at
**8 TPU chips = 16 TPU cores**, on any mesh size. A 4x4 mesh has 16 nodes but
still admits at most 8 chips; the remaining nodes are available for global
memory and control targets, which is convenient rather than wasteful. The
`mesh_4x4.yaml` configuration named in the plan is therefore *8 chips on a 4x4
mesh*, not 16.

Additionally, on a 2x2 mesh with the same-node restriction unresolved, at most
3 nodes may host chips because at least one node must host the global targets.
The platform must validate `chips + global_target_nodes <= mesh_x * mesh_y` and
say so in the error message rather than failing inside the NoC.

## 6. Existing scalar CPU wrapper (`cpu_models/riscv_vp`)

Audited `cpu_models/include/cdc/cpu/cpu_base.h` and
`cpu_models/riscv_vp/include/riscv_vp_wrapper.h`.

Facts:

* `cpu_base` is a small `sc_module` contract: `instr_bus()`, `data_bus()`,
  `has_unified_bus()`, `raise_irq()`, `set_irq(cause, level)`, `load_elf()`,
  `reset_cpu()`, `get_pc()`, `backend_name()`, `get_instret()`.
* `riscv_vp_cpu` returns the **same socket** from `instr_bus()` and
  `data_bus()` and reports `has_unified_bus() == true`; it wraps the Bremen
  rv32 ISS, loads the ELF backdoor through the bus, and `init()`s the ISS in
  `start_of_simulation()`.
* There is **no hart-ID accessor** and **no reset-PC setter** in `cpu_base`.
  TPU_V3 needs both (`mhartid = chip_id * 2 + core_id`, plan §11.6).
* There is no RVV anything. `riscv_vp` must not be presented as RV32GCV.

Audit-time consequence (the `spike_rvv` runtime choice is superseded by §12):
no new RV32GCV backend can satisfy plan §11.2 through today's `cpu_base`
unchanged. Decision D5 supersedes the audit-time setter proposal:
Phase 2 adds `hart_id` and `reset_pc` as static construction properties in
`cdc::cpu::cpu_config`; it does not add default no-op virtual setters. A
backend must apply the requested properties or reject the configuration
explicitly. This keeps platform construction backend-agnostic without making
an unsupported setting appear to succeed.

Third-party record: the Bremen ISS is **not** vendored; it is fetched into
`third_party/riscv-vp` (MIT) by `tools/third_party/setup_third_party.sh`.
Spike should follow the same pattern — fetched into `third_party/riscv-isa-sim`
at the pinned SHA — rather than being copied into the repository.

## 7. Sauria / `npu_tlm` audit

| Item | Value |
| --- | --- |
| CDC-VP integration | `components/npu_tlm`, target `cdc::components::npu_tlm`, gated by `CDC_ENABLE_SAURIA_NPU_V4` (default OFF) |
| Bundled model copies | `components/npu_tlm/models/v4.2_model` and `.../v4.2_model_Aug01` |
| External model root | `/home/duyptt_HW/Documents/work/fx1/hw/tlm/MP1_V1.1/v4.2_model` — present |
| Override | `SAURIA_NPU_ROOT` cache/env variable; the bundled `v4.2_model_Aug01` is the fallback |
| Array dimensions | **32 x 32** — `sauria_targets.h` defines `FP16_32x32` and `int16_32x32`, both with X=32, Y=32 |
| Data types | FP16 (`SAURIA_DT_FP16`) and INT16 (`SAURIA_DT_INT16`) |
| License | upstream `bsc-loca/sauria` architecture, `Apache-2.0 WITH SHL-2.1`; the SystemC implementation is internal-only and is **not** shipped in public CDC-VP. See `licenses/SAURIA.PROVENANCE.md` and `licenses/SAURIA.SHL-2.1`. |

This confirms plan §8.3 and §11.4: the available Sauria instance is a 32x32 NPU
top, not a bare 128x128 MXU, and it offers FP16/INT16 rather than the INT8/BF16
set a TPU-class MXU would normally advertise. It cannot be used unchanged as
the detailed MXU. No Sauria source has been copied by this audit, and TPU_V3
adds no new Sauria redistribution obligation while the Sauria backend is off.

**Redistribution rule carried into Phase 12:** the TPU_V3 package must refuse to
include a Sauria-derived backend unless `CDC_ENABLE_SAURIA_NPU_V4=ON` *and* the
Sauria license/provenance files are present. Building a public package with
Sauria linked in is not authorized by this audit.

## 8. Packaging infrastructure

`cmake/modules/CdcPortable.cmake` provides `cdc_make_portable(target)`
(`$ORIGIN` build+install RPATH, `BUILD_WITH_INSTALL_RPATH`, copies the
`libsystemc.so*` chain next to the binary) and `cdc_package_platform(target)`
(assembles `${CDC_PACKAGE_ROOT}/<target>/` with the binary, the SystemC runtime
and `configs/`). `CDC_PACKAGE_ROOT` defaults to `<source>/out` and is
overridable, which is what lets a packaging regression build into a private
directory.

`platforms/noc_soc/CMakeLists.txt` is the working reference for extending that
base package with licenses and provenance via a `POST_BUILD` command on
`<target>_package`. TPU_V3 follows the same shape and adds firmware and a build
manifest.

## 9. Decisions taken in Phase 0

> **Three of these were superseded on 2026-08-08 by
> `TPU_V3_DECISION_RECORD.md` (D1–D6), which is the implementation authority
> where the two disagree.** The original text is kept because it records what
> was measured and proposed at the time; the Status column says what holds now.

| # | Decision | Value | Status |
| --- | --- | --- | --- |
| P0-1 | Spike pinned revision | `16c0b60119f65a648643cf5d41e4e38e871f0bad` | holds only for the golden/differential role; amended **D3** selects RISC-V VP++ as runtime |
| P0-2 | Spike acquisition | fetch into `third_party/riscv-isa-sim` (gitignored), do not vendor | holds for reference tests (D3); it is not the runtime backend |
| P0-3 | Cross toolchain | xPack `riscv-none-elf` GCC 15.2.0-1 | holds; **D4** adds the freestanding `-nostdlib` Phase 2 environment |
| P0-4 | Max chips this revision | 8 | holds; **D2** names it the Revision 1 backend limit and lists what a 16-chip system would require |
| P0-5 | `mesh_4x4` config meaning | 8 chips on a 4x4 mesh | holds (D2) |
| P0-6 | SVM default capacity | 4 MiB | **superseded by D6**: the reference capacity is **16 MiB**, the full window. Smaller values are labelled bring-up configurations |
| P0-7 | MXU data types, Phase 4 | INT8 and FP32 | **superseded by D6**: the reference arithmetic is **BF16 × BF16 with IEEE FP32 accumulation**. INT8 × INT8 → INT32 is an optional later extension |
| P0-8 | Same-node initiator/target | wrapper-level local bypass | holds and is sharpened by **D1**: keep `NoLoopback = 1`, make the bypass *owner-aware*, treat it as a **Phase 7 prerequisite** |
| P0-9 | `cpu_base` extension | defaulted `hart_id`/`reset_pc` virtuals | **superseded by D5**: put them in `cpu_config` as static properties. A default no-op setter would let a 16-hart platform elaborate while every backend still reports `mhartid = 0` |

The rationale for each original proposal is in the section it came from; the
Phase 0 values were explicitly temporary, exactly as plan §16 Phase 0 allows,
and the decision record is where they were resolved.

## 10. Phase 0 gate assessment

| Gate criterion (plan §16) | Status | Evidence |
| --- | --- | --- |
| Dependencies and licenses documented | met | §3–§8 of this file |
| Frozen/unfrozen decisions match the plan | met | §9; frozen values untouched, new values marked temporary |
| No source copied without provenance | met | nothing copied; VP++ and Spike are fetched later at pinned SHAs |
| Architecture and address-map reviews contain no unresolved contradiction blocking scaffolding | met, with one recorded blocker for Phase 8 | `ARCHITECTURE.md`, `ADDRESS_MAP.md`, `INTERFACE_CONTRACT.md`; the NoLoopback blocker (§5.1) affects Phase 8, not scaffolding |

**Phase 0: complete.** The one substantive finding — that multi-chip NoC
traffic is blocked by the same-node target refusal, not merely inconvenienced
by it — is recorded, has a recommended resolution, and does not block Phases 1
through 7.

## 11. Not audited (deliberately deferred)

These need work that belongs to a later phase and are listed so they are not
mistaken for cleared items:

* RISC-V VP++ actually building on this host, its RV32+RVV ISS being embeddable
  without the complete upstream platform/GUI stack, and VLEN=512/ELEN=64 being
  reachable (Phase 2).
* The pinned Spike golden oracle building and agreeing with the VP++ runtime on
  the agreed differential corpus (Phase 2).
* Whether a vector multilib is needed for a libc link (Phase 2).
* Sauria parameterizability to 128x128, its internal `wait()` behaviour, SRAM
  address generation and clock ownership (Phase 9 — plan §11.4 lists the full
  checklist).
* FlooNoC detailed-mode elaboration cost at 8 chips (Phase 8).
* Whether `mstatus.VS`, `vstart` restart and the RV32 64-bit index EEW
  restriction behave as the plan requires (Phase 2 tests).

## 12. Post-Phase 1 addendum: RISC-V VP++ runtime selection

After this audit, the project identified `ics-jku/riscv-vp-plusplus` and its
RVV documentation. Decision D3 was amended accordingly:

- RISC-V VP++ is the selected TPU_V3 runtime backend and provides scalar plus
  RVV 1.0 execution in one RV32 ISS;
- Spike remains only an independent golden/differential reference;
- the planned `cdc::cpu::spike_rvv` runtime wrapper is superseded by
  `cdc::cpu::riscv_vp_plusplus`;
- the existing Bremen `cpu_models/riscv_vp` remains in place for current
  CDC-VP platforms and is not combined with a separately advancing vector ISS.

Evidence available at this addendum:

```text
Translated paper:
  /home/duyptt_HW/Desktop/TPU_V3/docs/RISC-V_VP++_ban_dich_tieng_Viet.docx.md
Upstream:
  https://github.com/ics-jku/riscv-vp-plusplus
License:
  MIT
Upstream claim:
  RVV 1.0 integrated into RV32 and RV64 ISSs
```

This evidence selects the backend family but does not yet verify a project
revision or the frozen TPU_V3 parameters. Phase 2 must select an immutable
VP++ candidate SHA and prove GCC 11.5/SystemC 2.3.4 buildability, VLEN=512,
ELEN=64, `vlenb=64`, TLM memory redirection, multi-instance isolation,
identity/reset/interrupt control, static/portable linkage and differential
agreement with Spike. Phase 0 and Phase 1 remain accepted; this is a Phase 2
pre-integration gate, not retroactive implementation evidence.
