# FX1 DDR3 SystemC/TLM Virtual Platform Implementation Plan

**Document status:** Planning baseline, not an implementation-complete claim  
**Plan version:** 0.1  
**Date:** 2026-08-24  
**Current interaction mode:** `SCOPED-CHANGE` — planning document only  
**Planned delivery mode:** `FULL/NEW-IP`, delivered in gated profiles  
**Target repository:** `CDC-VP/components/FX1_Components/ddr3`  
**Specification repository:** `VP_FX1_BK` is evidence-only and must not receive model source

## 1. Purpose

This document is the implementation and verification plan for the FX1 DDR3
SystemC/TLM model used by the software-oriented virtual platform. It defines:

- the accuracy boundary and supported profiles;
- evidence and change-control rules for the weekly DDR specification updates;
- the target architecture, public interfaces, configuration contract and
  repository structure;
- mandatory TLM, storage, reset, timing and integration behavior;
- phased implementation tasks, tests, release gates and exit criteria;
- the limitations that must remain explicit until the DDR specification is
  sufficiently frozen.

This plan does not authorize a cycle-accurate or RTL-equivalent claim. The
model is intended to provide evidence-backed software-visible behavior for
firmware, drivers, DMA clients and full-platform workloads.

## 2. Executive Decision

No current DDR3 specification artifact is sufficient for a complete
firmware-programmable DDR controller model. The implementation shall therefore
be delivered through separately named profiles rather than hiding missing
behavior behind defaults or fake success.

### 2.1 Delivery profiles

| Profile | Fidelity | Status at plan baseline | Purpose |
|---|---|---|---|
| `VP_PREINIT_RAM` | `SW-LT-DATA` | Implementable now | Pre-initialized DDR memory aperture used by CPU and DMA clients |
| `VP_CTRL_PV` | `SW-PV/SW-LT` | Blocked by register-map freeze | Programmer-visible controller registers, initialization state and basic IRQ/error behavior |
| `VP_CTRL_LT_DATA` | `SW-LT-DATA` | Blocked by controller-contract freeze | Control plane plus memory data movement, reset, power and error ordering |
| `VP_DETAILED_TIMING` | mixed LT/detailed | Deferred | Optional bank/row/timing backend for selected performance studies |
| `RTL_FAITHFUL` | RTL-oriented | Out of scope | Cycle/handshake matching belongs to a separate RTL-faithful acceptance plan |

### 2.2 Immediate implementation boundary

Revision 1 shall implement only `VP_PREINIT_RAM`:

- one byte-addressed, memory-like TLM target aperture;
- deterministic sparse storage with a configured logical capacity;
- full validation for the accepted generic-payload subset;
- byte-enable semantics, debug transport and configurable functional latency;
- explicit initialization, preload and power-cycle policy;
- integration into `VP_FX1_Full_SoC` in place of its generic `memory_tlm` RAM;
- CPU, DMA and boot/smoke evidence.

Revision 1 shall not expose placeholder APB registers, fake training success,
fake ECC, fake refresh, fake power-state transitions or fake interrupts.

## 3. Accuracy Claim

The maximum safe claim for Revision 1 is:

> Evidence-backed conformance to the declared `VP_PREINIT_RAM` software-visible
> memory-aperture and TLM transaction contract, within the listed platform,
> transaction, reset and workload profile. No claim is made for DDR3 PHY/DFI,
> JEDEC command timing, training, controller registers, arbitration
> microarchitecture, cycle timing or full hardware reset behavior.

The following phrases are prohibited until separately evidenced:

- cycle-accurate DDR3 controller;
- hardware-equivalent DDR3;
- DFI-compliant implementation;
- JEDEC timing-accurate model;
- complete DDR initialization/training model;
- full ECC/power-management support.

## 4. Evidence Ledger

### 4.1 Baseline artifacts

| ID | Artifact | Revision/hash | Role | Baseline status |
|---|---|---|---|---|
| `SRC-DDR-SPEC-001` | `VP_FX1_BK/ddr3/docs/FTELBK_DDR_HAS_V0.1.docx.md` | V0.1, 2026-08-01, SHA-256 `8c8e12bbbae98db77b9563a86813168061185a7275d4c71e336952cf0ed0a268` | DDR architecture and intended control/data behavior | `fact + missing + conflict`; not sufficient for full controller ABI |
| `SRC-VP-RULE-001` | `01-systemc-tlm-sw-platform-skill.md` | SHA-256 `96227ed240bebfca95b56cbd478861c470458808967db8dceb899fd065403b0e` | Modeling, TLM and release rules | working standard |
| `SRC-CDC-001` | CDC-VP repository | commit `8d94ca325c59402c9c1cea421e9f7d15ee00da73`, branch `dev` | Build, packaging and integration baseline | fact; unrelated worktree changes already exist |
| `SRC-CDC-MEM-001` | `components/memory_tlm` | same repository commit | Existing generic RAM reference | implementation input only; not normative and not reusable unchanged |
| `SRC-CDC-SPARSE-001` | `components/TPU_V3/common/sparse_memory` | same repository commit | Deterministic sparse-storage reference | implementation input; optional provenance source, not a DDR dependency |
| `SRC-CDC-FX1-001` | `platforms/VP_FX1_Full_SoC` | same repository commit | Current FX1 platform map and boot wiring | platform fact, not a frozen hardware spec |

### 4.2 Known source gaps

The following artifacts are referenced or required but are not present in the
FX1 component tree at this baseline:

- authoritative APB register schema with offsets, field access, masks and reset
  values;
- complete interrupt source/map/clear contract;
- firmware DDR initialization and failure-recovery sequence;
- approved SoC address, reset, clock, power and IRQ integration specification;
- PHY/DFI integration profile and training ownership contract;
- approved DDR capacity/topology for the FX1 platform;
- reference firmware header/driver and architecture tests;
- golden error/ECC/reset vectors.

Missing evidence must remain `OPEN`; an implementation constant or passing old
test does not promote it to a hardware fact.

### 4.3 Authority order

Until the project owner publishes another order, use:

1. approved DDR errata or signed RTL/silicon behavior;
2. frozen DDR programmer's specification and register schema;
3. frozen FX1 SoC integration contract;
4. production firmware header, initialization code and board description;
5. the current DDR HLS;
6. existing VP code and tests.

Conflicts between levels 1–4 require an owner decision or waiver. Code shall not
silently select one interpretation.

## 5. Weekly Specification Update Workflow

Every new DDR spec version shall pass this workflow before changing model
behavior:

1. Record filename, declared version/date, SHA-256 and source owner.
2. Preserve the previous ledger row; never overwrite its revision identity.
3. Diff semantic text and tables. Embedded base64 images may be excluded from
   text diff, but changed diagrams must still be reviewed visually.
4. Classify each delta as `clarification`, `new fact`, `behavior change`,
   `conflict`, `deviation removal` or `editorial`.
5. Map changed sections to requirement IDs, code, tests, documentation and
   platform wiring through `TRACEABILITY.md`.
6. Re-open every affected gate. A previously passing gate does not remain
   passing after its source contract changes without re-test.
7. Update the decision log and feature matrix.
8. Implement only after the affected contract is approved or the owner records
   an explicit assumption/waiver.
9. Store the effective spec revision in test logs and the platform startup log.

Suggested read-only comparison commands:

```bash
sha256sum /path/to/old_spec.md /path/to/new_spec.md
git diff --no-index --word-diff=plain /path/to/old_spec.md /path/to/new_spec.md
rg -n -i 'TBD|conflict|proposed|provisional|open|unsupported' /path/to/new_spec.md
```

## 6. Feature Matrix

### 6.1 Revision 1: `VP_PREINIT_RAM`

| Feature | Status | Contract |
|---|---|---|
| Byte-addressed memory aperture | `SUPPORTED` | Region-local address `0..capacity-1`; platform owns global base |
| CPU instruction/data access | `SUPPORTED` | Through current serialized TLM interconnect profile |
| DMA read/write access | `SUPPORTED` | Same memory aperture and visibility rules as CPU |
| Unaligned accesses | `SUPPORTED` | Byte semantics; no artificial alignment rejection |
| Repeating byte enables | `SUPPORTED` | Enabled lanes update/read; disabled read lanes remain unchanged |
| Wrapped streaming transfer | `UNSUPPORTED` | Refused with `TLM_BURST_ERROR_RESPONSE`, no side effect |
| Debug read/write | `SUPPORTED` | Untimed backdoor; exact returned-byte count; write permission is explicit configuration |
| Sparse backing | `SUPPORTED` | Untouched pages read according to configured initial-content policy |
| Dense backing | `DEFERRED` | Candidate only if a measured workload needs DMI/contiguous backing |
| DMI | `UNSUPPORTED` in Revision 1 | Explicit denial; current `bus_router` does not forward DMI anyway |
| Preload image | `SUPPORTED` | Explicit optional resource; bounds checked before simulation |
| Snapshot/persistence | `DEFERRED` | No silent host-file persistence |
| Hardware reset controller state | `UNSUPPORTED` | No controller state exists in this profile |
| DDR contents across CPU/peripheral reset | `SUPPORTED` by platform profile | Retained unless an explicit power-cycle API is called |
| Power-cycle content policy | `SUPPORTED` | Explicit configured policy; no hidden random/default contents |
| APB control/status registers | `UNSUPPORTED` | No placeholder address or fake successful writes |
| Initialization/training | `UNSUPPORTED` | Profile is explicitly pre-initialized |
| IRQ/error log | `UNSUPPORTED` | TLM response reports aperture transaction errors |
| ECC/injection/scrubbing | `UNSUPPORTED` | No fake correction or counters |
| Refresh/self-refresh/power-down | `UNSUPPORTED` | No JEDEC lifecycle claim |
| Four AXI ports/QoS/FR-FCFS | `UNSUPPORTED` | Current bus loses DDR port/QoS identity |
| IOMMU/coherency/cache maintenance | `UNSUPPORTED/PARTIAL` | Owned by platform; model is plain shared memory |
| Exclusive/atomic bus protocol | `PARTIAL` | CPU wrapper/interconnect owns locking; target supplies indivisible callback operations only |

### 6.2 Future controller profiles

`VP_CTRL_PV` and `VP_CTRL_LT_DATA` remain `DEFERRED` until their entry gates in
Section 18 pass. Deferred features must behave as unsupported in released
Revision 1 and must not be advertised to firmware.

## 7. Frozen Decisions for Revision 1

| ID | Decision | Rationale |
|---|---|---|
| `D-DDR-001` | Deliver a named pre-initialized memory profile first | Full controller ABI is not frozen |
| `D-DDR-002` | Reusable component uses region-local addresses | `bus_router` and platform own global address translation |
| `D-DDR-003` | Capacity, latency, maximum transfer and content policy are explicit typed configuration | Avoid hidden hardware assumptions |
| `D-DDR-004` | Use deterministic page-backed sparse storage | Avoid allocating the entire logical DDR capacity on the host |
| `D-DDR-005` | Do not link DDR against optional `TPU_V3` targets | FX1 DDR must remain independently buildable |
| `D-DDR-006` | Existing `memory_tlm` is reference-only | Its generic-payload and DMI contracts are insufficient for this release claim |
| `D-DDR-007` | No DMI in Revision 1 | Sparse pointer lifetime plus router DMI absence makes a grant load-bearing risk |
| `D-DDR-008` | LT target annotates delay and never calls `wait()` | Match current CDC-VP blocking-transport style |
| `D-DDR-009` | Revision 1 requires zero incoming delay at the target boundary | Prevent host-call order from contradicting logical-arrival order with multiple requesters |
| `D-DDR-010` | No APB/register placeholder until a canonical schema is approved | Prevent ABI churn and fake firmware success |
| `D-DDR-011` | No per-cycle DDR scheduler thread | Event-driven behavior and host speed are acceptance requirements |
| `D-DDR-012` | Current platform base/size are compatibility inputs, not reusable-model defaults | `0x8000_0000/256 MiB` comes from current platform code, not the DDR HLS |

## 8. Explicitly Unfrozen Decisions

These items require owner/spec input and shall remain visible in the open-item
ledger:

- final FX1 DDR global base, populated size and topology;
- APB base/window and complete register layout;
- register reset values and invalid-write policy;
- initialization start/enable/complete sequence and timeout;
- training ownership between firmware, controller and PHY;
- IRQ number, source set, polarity, mask/status/raw/inject/clear semantics;
- ECC storage layout, enable lock, counters and error response;
- reset/power content-retention matrix;
- cacheability, coherency, IOMMU and security attributes;
- four-port identity/QoS preservation and same-time arbitration;
- whether a future detailed backend is useful enough to justify its cost;
- whether dense backing/DMI is needed after performance measurement.

## 9. Target Repository Structure

Revision 1 shall follow the existing CMake/install conventions while keeping
DDR public headers namespaced:

```text
components/FX1_Components/ddr3/
|-- CMakeLists.txt
|-- include/cdc/fx1/ddr3/
|   |-- ddr3_config.h
|   |-- ddr3_profile.h
|   |-- ddr3_storage.h
|   `-- ddr3_tlm.h
|-- src/
|   |-- ddr3_config.cpp
|   |-- ddr3_storage.cpp
|   `-- ddr3_tlm.cpp
|-- tests/
|   |-- CMakeLists.txt
|   |-- test_ddr3_config.cpp
|   |-- test_ddr3_storage.cpp
|   |-- test_ddr3_tlm_contract.cpp
|   |-- test_ddr3_debug_dmi.cpp
|   |-- test_ddr3_multi_requester.cpp
|   `-- test_ddr3_platform_integration.cpp
`-- docs/
    |-- DDR3_VP_IMPLEMENTATION_PLAN.md
    |-- MODEL_CONTRACT.md
    |-- TRACEABILITY.md
    |-- LIMITATIONS.md
    |-- DECISION_LOG.md
    `-- RELEASE_STATUS.md
```

Future controller-profile additions, only after schema freeze:

```text
|-- regspec/                     # canonical source selected by owner
|-- include/cdc/fx1/ddr3/
|   |-- ddr3_registers.h         # generated/authoritative declarations
|   `-- ddr3_controller.h
|-- src/ddr3_controller.cpp
`-- tests/
    |-- test_ddr3_registers.cpp
    |-- test_ddr3_init_reset.cpp
    |-- test_ddr3_irq_ecc.cpp
    `-- test_ddr3_power.cpp
```

No `.o`, `.a`, build directory or generated runtime log may be written into the
source tree.

## 10. Public API and Configuration Contract

### 10.1 Namespace and build target

- C++ namespace: `cdc::components::fx1::ddr3`
- Library target: `fx1_ddr3_tlm`
- Alias: `cdc::components::fx1_ddr3_tlm`
- Public include root: `include/cdc/fx1/ddr3`
- Language level: C++17, inherited from CDC-VP
- Public dependency: `SystemC::systemc`

### 10.2 Required typed configuration

The reusable component shall receive a typed immutable configuration before
elaboration. At minimum:

| Field | Type/unit | Requirement |
|---|---|---|
| `profile` | enum | Required; Revision 1 accepts only `vp_preinit_ram` |
| `capacity_bytes` | `uint64_t`, bytes | Required, non-zero, host-indexable |
| `access_latency` | `sc_time` | Required, non-zero at platform time resolution |
| `max_transfer_bytes` | `uint64_t`, bytes | Required, non-zero and no larger than capacity |
| `storage_backend` | enum | Required; Revision 1 accepts `sparse_pages` |
| `initial_content_policy` | enum | Required; `zero`, deterministic seeded pattern, or required preload |
| `seed` | `uint64_t` | Required for any patterned/randomized policy; logged |
| `debug_write_policy` | enum | Required; deny or allow loader backdoor |
| `reset_content_policy` | enum | Required; retain, reinitialize or reload |
| `incoming_delay_policy` | enum | Required; Revision 1 uses strict zero-delay |
| `report_category` | string | Required non-empty hierarchical logging category |

The reusable target shall not own a global base address. Platform configuration
owns base and routing.

### 10.3 Validation

Validation shall occur during construction or before simulation starts and
shall reject:

- zero capacity, latency or maximum transfer;
- arithmetic overflow in page count, index allocation or span calculations;
- a preload larger than capacity or a required preload that cannot be opened;
- an initial-content policy without its required seed/resource;
- a requested backend/profile combination not implemented;
- a duration quantized to zero by the platform time resolution;
- mutable construction-time configuration after elaboration.

Every diagnostic shall name the instance and field. The model shall log the
effective configuration, its source and spec revision once after validation.

## 11. Component Architecture

```text
typed immutable configuration
            |
            v
   ddr3_tlm target adapter
   - generic-payload validation
   - response mapping
   - annotated LT delay
   - debug/DMI policy
            |
            v
   ddr3_storage pure C++ core
   - sparse page index
   - read/write/strobes
   - preload/reinitialize
   - allocation metrics
```

Future profile, kept outside Revision 1:

```text
APB/CSR TLM target --> controller state/scheduler --> memory data aperture
                              |
                              +--> IRQ/reset/power ports
                              `--> optional detailed timing/backend
```

### 11.1 `ddr3_storage`

The storage core shall be plain C++ with no SystemC socket, host wall clock or
global mutable state. It owns:

- logical capacity and deterministic page index;
- lazy page allocation;
- byte reads/writes and enabled-lane semantics;
- preload and reinitialization behavior;
- current/peak allocated page and byte metrics.

It shall not own platform addresses, simulation time, reset signals or TLM
response codes.

Unallocated-page contents follow the explicitly configured initialization
policy. Reads shall not allocate pages. A failed read/write shall be atomic at
the storage API boundary: no destination buffer or storage byte changes.

### 11.2 `ddr3_tlm`

The SystemC wrapper owns:

- one Revision-1 target socket named `memory_socket`;
- payload validation and TLM response selection;
- timing annotation and incoming-delay admission;
- debug access and explicit DMI denial;
- instance-scoped reporting;
- future reset/power hooks without fake current behavior.

The wrapper shall not implement a hidden APB register file or DDR command
scheduler.

## 12. TLM-2.0 Contract

### 12.1 Normal transport validation order

`b_transport` shall complete validation before changing storage or enabled read
lanes:

1. Set current-call response to `TLM_INCOMPLETE_RESPONSE` and
   `dmi_allowed=false`.
2. Accept only `TLM_READ_COMMAND` and `TLM_WRITE_COMMAND`.
3. Reject null data pointer and zero data length.
4. Reject data length above configured maximum.
5. Check address and full span by subtraction; never mask or wrap an address.
6. Validate streaming width. Width zero is invalid; wrapped streaming
   (`streaming_width < data_length`) is unsupported in Revision 1.
7. Validate byte-enable pointer/length and every repeating enable value.
8. Validate required extensions and incoming-delay policy.
9. Execute the storage operation.
10. Add configured access latency to delay.
11. Set the final response explicitly.

### 12.2 Response mapping

| Failure | Response | Side effect |
|---|---|---|
| Unsupported/ignore command | `TLM_COMMAND_ERROR_RESPONSE` | none |
| Address/span outside capacity | `TLM_ADDRESS_ERROR_RESPONSE` | none |
| Wrapped/unsupported streaming | `TLM_BURST_ERROR_RESPONSE` | none |
| Invalid byte-enable encoding/length | `TLM_BYTE_ENABLE_ERROR_RESPONSE` | none |
| Null pointer, zero length, oversized request, delay-policy violation | `TLM_GENERIC_ERROR_RESPONSE` | none |
| Accepted read/write | `TLM_OK_RESPONSE` | enabled lanes only |

Failed reads must leave the entire initiator buffer unchanged. Disabled read
lanes must remain unchanged on a successful partial read. The target must not
modify command, address, pointers, lengths, streaming width, byte enables or
write data.

### 12.3 Byte enable semantics

- A null byte-enable pointer means every byte is enabled and byte-enable length
  is ignored.
- A non-null pointer requires non-zero byte-enable length.
- The enable array repeats over the transfer.
- Only `TLM_BYTE_ENABLED` and `TLM_BYTE_DISABLED` are valid.
- A write allocates a sparse page only if at least one byte in that page is
  enabled.

### 12.4 Timing and ordering

The target shall not call `wait()` in `b_transport`. Revision 1 accepts only
zero incoming delay at its boundary, performs the operation in the current
SystemC kernel context, and annotates configured access latency on return.

This is an explicit integration constraint caused by the current shared
`bus_router`, which does not preserve DDR port/QoS identity or provide a
logical-arrival scheduler. A non-zero incoming delay is refused without side
effect and has a mandatory negative test.

Initiators and test probes must consume the returned delay before issuing an
access whose ordering depends on completion. The existing minimal
`tests/support/tlm_probe.h` is not a timing oracle because it ignores delay; DDR
tests shall provide a timed probe.

Same-time CPU/DMA requests are serialized by the current interconnect/kernel
profile. This is `PARTIAL`, not a model of the four-port FR-FCFS DDR arbiter.

### 12.5 `transport_dbg`

Base debug behavior:

- uses only command, address, data pointer and data length;
- accepts zero length and null pointer only for that zero-length case;
- never waits, annotates time, changes status/IRQ/controller state or allocates
  for a read;
- returns the exact number of bytes transferred, clipped at the end of the
  aperture;
- returns zero for ignore/unsupported command or unusable pointer;
- writes storage only when `debug_write_policy` explicitly permits loader
  access;
- does not use response status as the success oracle.

### 12.6 DMI

Revision 1 denies DMI. The denial path shall initialize the descriptor and
publish a safe aperture range/access policy as required by the selected TLM
contract. Tests shall prove there is no stale grant after reuse.

DMI may be reconsidered only after:

- a dense or page-range pointer lifetime contract is designed;
- reset/reinitialize invalidation is implemented;
- the interconnect forwards grant and invalidation ranges;
- DMI per-byte latency matches equivalent normal transport;
- performance evidence demonstrates material benefit.

## 13. Initialization, Reset and Power Contract

### 13.1 Revision-1 initialization

`VP_PREINIT_RAM` means controller initialization/training is outside the model.
The platform must select exactly one explicit content policy:

- `zero`: untouched bytes read zero;
- `seeded_pattern`: deterministic documented pattern with logged seed;
- `required_preload`: simulation fails before start if the image is unavailable
  or does not fit.

For migration compatibility, `VP_FX1_Full_SoC` may select `zero`, matching the
current dense RAM behavior. That selection is a platform deviation, not a DDR3
hardware reset fact.

### 13.2 Reset matrix for Revision 1

| Event | Configuration | Storage | In-flight access | DMI |
|---|---|---|---|---|
| CPU/peripheral reset | unchanged | retained | current blocking call completes | unsupported |
| Model reinitialize API | immutable config retained | follows configured reset policy | legal only when no callback active | unsupported |
| Platform power cycle | instance reconstructed or explicit power-cycle hook | reinitialize/reload as configured | no old epoch may publish | unsupported |
| Hardware DDR controller reset | `N/A` in this profile | `UNSUPPORTED` | `UNSUPPORTED` | unsupported |

No host callback thread may mutate storage. Preload/reinitialize occurs before
simulation or from SystemC kernel context under the documented lifecycle rule.

## 14. Current CDC-VP Integration Facts and Risks

### 14.1 Existing FX1 platform

At the planning baseline, `VP_FX1_Full_SoC` uses:

```text
RAM base: 0x8000_0000
RAM size: 0x1000_0000 (256 MiB)
Model:    cdc::components::memory_tlm
```

These are current platform-code facts, not approved DDR hardware facts. The
Revision-1 integration shall preserve them to avoid an unrelated address-map
migration, while marking them `COMPAT` until the SoC integration spec confirms
them.

### 14.2 Existing `memory_tlm` audit

It shall not be reused unchanged because it currently:

- allocates the full logical capacity as a dense vector;
- does not validate zero length, streaming width or byte enables;
- does not guarantee failed-read buffer invariance for every invalid payload;
- grants DMI while the platform router does not forward DMI;
- uses one transaction latency in `b_transport` but publishes it as DMI
  per-byte latency;
- has only a minimal negative test matrix.

The DDR component may reuse concepts, but it needs its own evidence-backed
contract and tests.

### 14.3 Existing `bus_router` constraints

The router:

- translates global addresses to region-local addresses;
- forwards blocking and debug transport;
- restores address on normal returns;
- does not forward DMI;
- does not preserve upstream port identity/QoS in a DDR extension;
- has no explicit DDR arbitration or temporal-decoupling policy.

Revision 1 must document these constraints. A future multi-port controller
profile requires an interconnect/extension change, not merely an internal DDR
scheduler.

## 15. Build, Install and Packaging Plan

### 15.1 Build option

Introduce a scoped option while the component is under development:

```cmake
option(CDC_BUILD_FX1_DDR3 "Build the FX1 DDR3 TLM component" OFF)
```

`components/CMakeLists.txt` adds
`FX1_Components/ddr3` only when enabled. After the standalone, platform and
distribution gates pass, the owner may promote it to the default required
dependency of `VP_FX1_Full_SoC`.

### 15.2 CMake target

The component CMake shall:

- define `fx1_ddr3_tlm` and alias `cdc::components::fx1_ddr3_tlm`;
- expose only the public include directory;
- link `SystemC::systemc` publicly and private dependencies privately;
- install/export through `cdc-components-targets`;
- build tests only under `CDC_BUILD_TESTS`;
- set CTest `TIMEOUT` on every test;
- avoid source-tree generated artifacts.

### 15.3 Reference build commands

```bash
cmake -S /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP \
      -B /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/build-fx1-ddr3 \
      -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
      -DCDC_BUILD_FX1_DDR3=ON \
      -DCDC_BUILD_TESTS=ON

cmake --build /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/build-fx1-ddr3 \
      --target fx1_ddr3_tlm -j"$(nproc)"

ctest --test-dir /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/build-fx1-ddr3 \
      -R '^fx1_ddr3_' --output-on-failure
```

The final package gate shall additionally install to a temporary prefix and
build an out-of-tree consumer using only installed headers and exported
targets.

## 16. Verification Strategy

### 16.1 Independent oracles

Tests shall not derive all expected values from implementation constants.
Oracles shall come from:

- explicit literals reviewed against the TLM contract;
- source-spec values when frozen;
- a simple independent byte-array/reference transaction interpreter;
- end-to-end CPU/DMA data checks;
- mutation controls that deliberately break the implementation.

### 16.2 Configuration tests

- valid minimum/representative/large capacities;
- zero and overflowed capacities;
- page-count/host-index overflow;
- missing preload, oversized preload and permission failure;
- zero/quantized latency;
- invalid profile/backend/policy combinations;
- immutable configuration after elaboration;
- effective configuration logging.

### 16.3 Pure storage tests

- untouched read according to selected policy;
- write/read round trip at start, page edge and final byte;
- transfers crossing one and multiple page boundaries;
- capacity edge and `uint64_t` overflow spans;
- repeating strobe patterns and all-disabled writes;
- failed operations have no partial commit;
- read does not allocate; disabled write page does not allocate;
- allocation/current/peak metrics;
- reinitialize/retain/reload policies;
- multi-instance independence;
- deterministic result and allocation behavior across repeated runs;
- large logical capacity without dense host-memory commitment.

### 16.4 TLM negative matrix

For each rejected payload, verify response plus invariance of storage, read
buffer, payload attributes and DMI hint:

- ignore/unknown command;
- null data pointer;
- zero data length;
- request over configured maximum;
- address equal to capacity;
- address/span overflow;
- streaming width zero;
- wrapped streaming width;
- non-null byte-enable with zero length;
- illegal byte-enable value;
- missing/invalid required extension if one is introduced;
- non-zero incoming delay under strict Revision-1 policy;
- payload object reused after a previous success/failure.

### 16.5 Debug and DMI tests

- zero-length debug access;
- partial debug transfer at aperture end;
- debug ignore command returns zero;
- debug read does not allocate or change time;
- debug write allow/deny policies;
- debug return count is the only success oracle;
- DMI denied with no stale descriptor/grant on object reuse.

### 16.6 Timing and concurrency tests

- target adds exactly configured annotated latency;
- timed initiator consumes delay and observes progress;
- strict zero incoming-delay admission has no rejected side effect;
- CPU-like and DMA-like initiators write disjoint and overlapping regions;
- repeated same-time stress produces deterministic serialized results within
  the declared interconnect profile;
- no target `wait()`, deadlock or lost access;
- internal watchdog and CTest hard timeout.

The overlap result is evidence only for the declared serialized bus profile,
not for the HLS four-port FR-FCFS policy.

### 16.7 Platform/firmware tests

- replace generic FX1 RAM without changing current base/size;
- CPU fetch/load/store smoke from DDR when the selected boot flow uses it;
- DMA memory-to-memory round trip through the same DDR target;
- CPU writes then DMA reads, and DMA writes then CPU reads;
- out-of-range CPU/DMA access fails through the platform path;
- firmware/guest reports PASS/FAIL, not host fixed-time success;
- effective model profile, capacity, latency, seed and spec hash appear in log;
- reset smoke verifies the declared retention behavior;
- boot/smoke performance compared with existing `memory_tlm` baseline.

### 16.8 Mutation controls

At least these deliberate defects must make the relevant test fail:

- alias out-of-range addresses with an address mask;
- accept a wrapped streaming transfer;
- modify disabled read lanes;
- partially commit before discovering an invalid span;
- allocate a sparse page on read;
- ignore returned annotated delay in the timing oracle;
- clear DDR on CPU reset despite the retention contract;
- leave the generic `memory_tlm` bound in the platform integration test.

## 17. Implementation Phases and Gates

### Phase 0 — Evidence, contract and decision baseline

Tasks:

- create `MODEL_CONTRACT.md`, `TRACEABILITY.md`, `LIMITATIONS.md` and
  `DECISION_LOG.md`;
- record source hashes and current platform commit;
- turn every Revision-1 feature into a requirement ID;
- record current spec conflicts/TBDs and owner questions;
- freeze the `VP_PREINIT_RAM` configuration and TLM subset;
- mark future controller features deferred/unsupported.

Gate:

- all implemented behavior traces to a fact, explicit platform compatibility
  decision or approved assumption;
- no APB/register/IRQ address is invented;
- every open item has an owner and exit criterion.

### Phase 1 — Repository skeleton and configuration

Tasks:

- add build option, target, namespaced headers and install/export rules;
- implement typed configuration and construction-time validation;
- add configuration tests and CTest timeouts;
- keep default CDC-VP behavior unchanged while the component is opt-in.

Gate:

- configure/build works with component enabled and disabled;
- invalid configurations fail before simulation with instance/field diagnostic;
- clean out-of-tree build produces no source-tree artifact.

### Phase 2 — Pure sparse storage core

Tasks:

- implement deterministic page-backed storage;
- implement read/write/strobe, preload and content lifecycle;
- implement allocation metrics;
- add independent reference-model tests and large-capacity RSS check.

Gate:

- storage test matrix passes in Debug and Release;
- no out-of-range partial commit or failed-read buffer modification;
- large logical DDR elaborates without dense capacity allocation;
- mutation controls demonstrate test sensitivity.

### Phase 3 — TLM target contract

Tasks:

- implement validation/response mapping in the specified order;
- implement annotated LT delay without `wait()`;
- implement debug access and explicit DMI denial;
- implement payload invariance and logging;
- add timed and adversarial probes.

Gate:

- complete normal/debug/DMI negative matrix passes;
- non-zero incoming delay rejection is tested;
- no response path returns incomplete;
- strict warnings and sanitizer runs have no issue.

### Phase 4 — FX1 platform integration

Tasks:

- link `cdc::components::fx1_ddr3_tlm` into `VP_FX1_Full_SoC`;
- replace only the RAM instance, preserving current `0x8000_0000/256 MiB`
  compatibility map;
- pass explicit platform configuration, including zero-initialized profile and
  current 10 ns compatibility latency unless a reviewed value supersedes it;
- preserve Boot ROM and internal flash behavior;
- add CPU/DMA integration tests and effective configuration log;
- remove the old DDR-aperture `memory_tlm` dependency only after tests prove the
  new component is actually bound.

Gate:

- platform build and package pass;
- CPU/DMA shared-data checks pass;
- address-map overlap check passes;
- guest-visible PASS/FAIL is observed;
- existing unrelated platform smoke tests do not regress.

### Phase 5 — Firmware, stress and performance acceptance

Tasks:

- run representative boot and memory/DMA workloads;
- stress page crossings, large sparse writes and CPU/DMA overlap;
- record simulated time, host elapsed time, RSS and software progress;
- compare against the generic `memory_tlm` baseline;
- tune only after evidence, without changing functional contract.

Gate:

- performance/RSS thresholds approved from measured baseline;
- no nondeterminism across repeated seeded runs;
- no timeout, unexpected skip or hidden fallback;
- release report contains feature/limitation matrix.

### Phase 6 — `VP_CTRL_PV` promotion after spec freeze

Entry gate — all required before coding:

- approved canonical register schema;
- complete offsets/access/reset/masks;
- APB base/window and bus access policy;
- initialization/enable/status sequence;
- full IRQ/error register contract;
- firmware header/sequence and owner-reviewed conflicts.

Tasks after entry:

- generate or implement the authoritative register bank following the owner
  selected schema convention;
- add an APB/control TLM socket and initialization state machine;
- gate memory acceptance according to the frozen contract;
- implement IRQ/error logging and warm reset semantics;
- add register, polling, interrupt and error-path tests.

Gate:

- register drift/access/reset tests pass;
- production/reference initialization sequence reaches ready;
- negative init path and timeout are observable;
- no Revision-1 compatibility shortcut is enabled in strict profile.

### Phase 7 — `VP_CTRL_LT_DATA` lifecycle features

Entry gate:

- reset/power/DFI ownership contract frozen;
- ECC and maintenance behavior frozen;
- data visibility and error ordering defined;
- CPU/DMA/IOMMU/coherency integration profile approved.

Candidate tasks:

- functional initialization/training latency;
- reset-in-flight epoch cancellation;
- self-refresh and power transition state;
- ECC correction/detection, counters and fault injection;
- error response/IRQ ordering after committed data;
- optional multi-port request extension and arbitration profile.

Gate:

- async/reset/error ordering and firmware ISR tests pass;
- all destination visibility precedes completion/IRQ;
- no old epoch publishes after reset;
- capability reporting matches implemented features exactly.

### Phase 8 — Distribution and release signoff

Tasks:

- install/export and out-of-tree consumer test;
- Debug/Release, strict warnings and supported sanitizers;
- full applicable gate matrix and deviation ledger;
- clean package without source/build artifacts;
- performance and configuration manifest;
- owner release review.

Gate:

- `GO` only if every required gate is `SATISFIED/N/A` with no waiver;
- `CONDITIONAL-GO` only with approved, unexpired waivers and no open item;
- otherwise `NO-GO`.

## 18. Future Controller-Profile Entry Checklist

The model must not cross from Revision 1 to controller profiles until every row
is resolved:

| ID | Required fact | Current state | Exit criterion |
|---|---|---|---|
| `OPEN-DDR-001` | APB register offsets/access/reset/masks | Missing/TBD | Approved canonical schema and generated/drift-checked artifacts |
| `OPEN-DDR-002` | `ctrl_enable` behavior | TBD | Reset value, write semantics and traffic-gating sequence frozen |
| `OPEN-DDR-003` | `init_start/init_done` ownership | TBD | Firmware/controller/PHY sequence and timeout frozen |
| `OPEN-DDR-004` | Initialization step timings | TBD | Units, rounding, ranges and defaults frozen |
| `OPEN-DDR-005` | Soft reset offsets/status/retention | Offsets TBD | Full reset matrix and same-time policy approved |
| `OPEN-DDR-006` | IRQ map and clear/inject semantics | TBD | Complete source-to-register-to-SoC IRQ map approved |
| `OPEN-DDR-007` | Error log fields | TBD | Captured fields, first/last policy and clear behavior frozen |
| `OPEN-DDR-008` | ECC registers/layout | Offsets/semantics TBD | Data/check layout, response, counters and injection frozen |
| `OPEN-DDR-009` | Rank-enable write policy | Conflict | Live-vs-locked conflict resolved by owner |
| `OPEN-DDR-010` | Page size/topology | Conflict | Fixed column width and programmable page-size contract reconciled |
| `OPEN-DDR-011` | Timing rounding | TBD | Conversion from physical time to controller ticks frozen |
| `OPEN-DDR-012` | SoC address/capacity/IRQ | Platform assumption | Approved integration map and overlap check |
| `OPEN-DDR-013` | Coherency/IOMMU/security | Missing | Approved platform profile or explicit unsupported decision |
| `OPEN-DDR-014` | Reference firmware and error tests | Missing | Versioned source plus expected PASS/FAIL evidence |

## 19. Release Gate Matrix

### 19.1 Current planning baseline

| Gate | Evidence result | Disposition | Reason/exit |
|---|---|---|---|
| `SW-G0 Provenance` | `INCONCLUSIVE` | `OPEN` | Primary HLS hashed; normative register/integration/firmware sources missing |
| `SW-G1 Schema/Config` | `NOT_RUN` | `OPEN` | Revision-1 config contract planned; controller schema unavailable |
| `SW-G2 Core` | `NOT_RUN` | `OPEN` | No DDR storage implementation yet |
| `SW-G3 TLM` | `NOT_RUN` | `OPEN` | No DDR target/negative matrix yet |
| `SW-G4 Async` | `N/A` for Revision 1 | `N/A` | Pre-initialized memory aperture has no async engine |
| `SW-G5 Dataflow` | `NOT_RUN` | `OPEN` | Storage/CPU/DMA visibility tests not run |
| `SW-G6 Environment` | `NOT_RUN` | `OPEN` | Sparse/preload backend not implemented |
| `SW-G7 Concurrency` | `NOT_RUN` | `OPEN` | Serialized interconnect stress not run |
| `SW-G8 Firmware` | `NOT_RUN` | `OPEN` | CPU/DMA guest evidence not run |
| `SW-G9 Platform` | `NOT_RUN` | `OPEN` | FX1 integration not changed |
| `SW-G10 Distribution` | `NOT_RUN` | `OPEN` | Target/install/consumer flow absent |
| `SW-G11 Quality` | `NOT_RUN` | `OPEN` | Warnings/sanitizers/performance not run |

Current release decision: `NO-GO` — plan only.

### 19.2 Gate applicability notes

- Register-schema portions of `SW-G1` are `N/A` only for the explicitly named
  Revision-1 profile; configuration and platform address-map checks remain
  applicable.
- FIFO/streaming portions of `SW-G5` are `N/A` for Revision 1, but committed
  memory visibility and CPU/DMA conservation are applicable.
- Async/IRQ gates become applicable immediately when controller initialization,
  ECC, reset sequencing or power management enters the profile.

## 20. Risk Register

| ID | Risk | Impact | Mitigation |
|---|---|---|---|
| `R-DDR-001` | Weekly spec change invalidates behavior | ABI/test churn | Hash, diff, traceability and re-open affected gates |
| `R-DDR-002` | Pre-initialized RAM is mistaken for full DDR controller | False firmware confidence | Named profile, capability report and explicit unsupported list |
| `R-DDR-003` | Dense logical DDR consumes excessive host RSS | Platform fails to elaborate/run | Sparse page backing and RSS gate |
| `R-DDR-004` | Sparse backing conflicts with DMI pointer lifetime | Stale/corrupt fast path | Deny DMI in Revision 1 |
| `R-DDR-005` | Current router loses port/QoS identity | Cannot model four-port arbitration | Restrict profile; require future extension/interconnect work |
| `R-DDR-006` | Annotated-delay requests arrive out of logical order | Shared-memory causality error | Strict zero incoming-delay boundary in Revision 1 |
| `R-DDR-007` | Reset content behavior is invented | Boot/error path divergence | Explicit content policy; no hidden reset default |
| `R-DDR-008` | Existing platform base/size are mistaken for hardware facts | Later address-map migration | Mark `COMPAT`, platform-owned, and require integration-spec exit |
| `R-DDR-009` | CPU atomic behavior assumed to be target-owned | Multi-hart lost updates | Audit CPU/interconnect lock; run contention test for applicable platform |
| `R-DDR-010` | Test probe ignores timing | Vacuous timing PASS | DDR-specific timed probe and mutation control |
| `R-DDR-011` | Preload path or file silently falls back | Non-reproducible boot | Required resource fails before simulation; log effective choice |
| `R-DDR-012` | Unrelated dirty worktree changes are overwritten | User data loss | Touch only DDR/planned integration files; inspect diff before every commit |

## 21. Performance and Observability

The model shall report, with hierarchical instance identity:

- configured logical capacity;
- active profile and spec hash;
- current/peak allocated pages and host-backed bytes;
- accepted/rejected normal and debug transactions by reason;
- bytes read/written through normal and debug paths;
- optional timing estimate totals, clearly labeled estimated;
- seed and preload identity when applicable.

Hot-path logs are off by default. Metrics/tracing must not change behavior.

Performance acceptance shall compare:

- simulated time progressed;
- host elapsed time;
- peak RSS;
- boot/software progress;
- CPU/DMA transfer throughput;
- generic `memory_tlm` compatibility baseline.

Thresholds are set from measured project baselines, not from an arbitrary MIPS
or GB/s claim.

## 22. Definition of Done

### 22.1 Revision 1 done

`VP_PREINIT_RAM` is complete only when:

- all Phase 0–5 gates are satisfied;
- feature and limitation matrices are accurate;
- required configuration has no hidden hardware default;
- storage/TLM/debug/DMI-negative/concurrency tests pass;
- CPU and DMA observe the same committed bytes through the FX1 platform;
- current platform boots/smokes with guest-visible PASS/FAIL;
- strict warnings, Debug/Release and supported sanitizers pass;
- performance/RSS evidence meets approved thresholds;
- install/export and clean consumer flow pass;
- no required gate remains open for the Revision-1 claim.

### 22.2 Full controller done

A full controller model cannot be marked complete merely because Revision 1 is
complete. It additionally requires:

- Phase 6–8 entry evidence and implementation;
- canonical register and integration contracts;
- production/reference firmware initialization and error paths;
- IRQ, reset, power, ECC and data-visibility evidence;
- narrowed or removed deviations for every unsupported controller feature.

## 23. Immediate Next Actions

1. Review and approve the `VP_PREINIT_RAM` claim boundary.
2. Confirm that `0x8000_0000/256 MiB/zero-initialized/10 ns` is accepted only as
   the current platform compatibility profile.
3. Create Phase-0 contract/traceability/limitation/decision documents.
4. Add the opt-in component skeleton and typed configuration.
5. Implement/test the pure sparse storage before any SystemC wrapper.
6. Implement the complete TLM negative matrix before FX1 platform integration.
7. Replace the current FX1 RAM only after the standalone gate passes.
8. Request the missing register, firmware, integration and IRQ artifacts for
   future controller-profile promotion.

## 24. Status Table

| Phase | Status | Evidence |
|---|---|---|
| Plan baseline | `COMPLETE` | This document plus source/repository audit |
| Phase 0 | `PARTIAL` | HLS and repository baseline reviewed; normative sources still missing |
| Phase 1 | `NOT_STARTED` | No component/build files yet |
| Phase 2 | `NOT_STARTED` | No DDR storage core yet |
| Phase 3 | `NOT_STARTED` | No DDR TLM target yet |
| Phase 4 | `NOT_STARTED` | FX1 still uses generic `memory_tlm` |
| Phase 5 | `NOT_STARTED` | No DDR performance/firmware evidence |
| Phase 6 | `BLOCKED_BY_SPEC` | Register/init/IRQ contract not frozen |
| Phase 7 | `BLOCKED_BY_SPEC` | Reset/power/ECC/DFI integration not frozen |
| Phase 8 | `NOT_STARTED` | Distribution/signoff depends on implemented profile |

## 25. Plan Maintenance Rule

This document is a controlled plan, not a historical dump. When a decision is
approved, move it from open to frozen with source/owner/date. When a feature is
implemented, update its status only with test evidence. Do not delete old
deviations or conflicts; close them with a resolution and the spec/model version
in which the resolution took effect.
