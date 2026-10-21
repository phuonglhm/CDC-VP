# FU3 bus validation

## Fabric integration upgrade (2026-10-20)

Revalidated the fabric after fixing native AP_DMA metadata interoperability,
PLIC claim side effects on narrow accesses, and preserved FIXED footprint
checks. Added functional ingress quiescence for coordinated platform reset.

| Current build / use case | Passed CTest executions |
|---|---:|
| Static core plus production-IP scenarios | 10/10 |
| Static installed consumers | 5/5 |
| Shared core plus production-IP scenarios | 10/10 |
| Shared installed consumers | 5/5 |
| FX1/FU3 coexistence | 1/1 |
| Total | 31/31 |

These counts repeat scenarios across configurations; they are not 31 distinct
specification requirements. Environment: WSL/Linux, GNU C++ 15.2.0, C++17,
SystemC 2.3.4, Release. Local transcript: ignored
`build/system-integration-final.log` in this component. The shared installed
safety executable resolves the installed bus library and shared SystemC.
The production-IP scenarios are also rerun after strengthening the post-reset
DMA check to compare a nonzero source pattern; transcript:
`build/real-ip-final-recheck.log` in this component.
After restoring DMA0 to its Git version, the optional adapter was moved into
`bus/include/fu3_bus/dma_adapter.h` and the four production-IP scenarios were
rechecked for static/shared builds. Transcript:
`build/bus-owned-adapter-recheck.log`. Only bus sources are changed.

The common-build component suite also passes **199/199**: Timer 2, WDT 2,
UART 6, SPI 18, I2C 2, RNG 2, HyperRAM 2, PLIC 2, INTMAP 1, SYSCSR 2,
QSPI 44, ADC 26, AES 35, GPIO/IOSEL 19 and DMA0 36. Log: ignored
`build/fu3-review/bus-ip-upgrade-tests.log` at repository root. These separate
component tests share the upgraded bus build; they are not one full-SoC run.
The build retains two overloaded-virtual warnings in SystemC/tlm_utils headers.

The four new production-IP scenarios use actual DMA, SPI0, UART2, Timer1, PLIC
and existing V1.1 WDT0 models. See [integration validation](INTEGRATION_VALIDATION.md)
for exact coverage, fixture assignments and remaining platform responsibilities.
No CPU firmware boot or WDT V1.2 migration is claimed.

## Historical audit before the upgrade

The following audit records the defects as they existed before this upgrade.
Its three reproduced issues are resolved by the contracts and tests above;
its missing full-platform boot/security evidence remains applicable.

## Fresh bus and IP audit (2026-10-20)

Re-extracted and reviewed the current FU3 HAS v2.0 BUS sections and memory map,
and visually inspected Figures 4-1 and 7-1. Existing topology, APB-version,
strobe and FIFO decisions remain the basis of the VP contract.

The current checkout passed static bus regression (5/5), static installed
consumers (4/4), shared regression (5/5), shared installed consumers (4/4),
and FX1/FU3 coexistence (1/1): **19/19 fresh bus executions**. Environment:
WSL/Linux, GNU C++ 15.2.0, C++17, installed SystemC 2.3.4, Release.
Transcript: ignored `build/review-20261020.log` within this component.

An additional common-build IP suite passed **185/185 tests**: Timer 2, WDT 2,
UART 6, SPI 4, I2C 2, RNG 2, HyperRAM 2, PLIC 2, INTMAP 1, SYSCSR 2,
QSPI 44, ADC 26, AES 35, GPIO/IOSEL 19 and DMA0 36. Its static libraries and
bus-enabled integration tests use the same current bus build. These are separate
test executables, not a full SoC simulation. DMA uses native fake memory peers;
GPIO/IOSEL tests do not exercise the real FU3 fabric. The broad build reports two
overloaded-virtual warnings in SystemC/tlm_utils headers, without build errors.

Focused local probes also verified independent downstream progress across the
two A2P bridges and serialization within one bridge. They reproduced three open
issues; passing ordinary regressions does not close these findings:

- Production AP_DMA emits its own TransferExtension but no bus Access extension.
  Direct default DMA0 binding returns GENERIC_ERROR before memory access. A
  metadata adapter is needed for actual DMA/fabric integration; fake DMA tests
  already attach Access and therefore do not detect this incompatibility.
- A byte read of the upper part of PLIC ID_0 consumes the claim while returning
  zero. Define and enforce safe access widths for side-effect CSRs before claiming
  generic subword support. Aligned word claim/complete remains supported.
- With optional preserve_burst enabled, ingress validates FIXED using one beat
  but Router validates using payload length. A valid repeated final-word request
  is rejected before reaching the endpoint. The default decomposed path and
  HyperRAM's supported INCR path are not affected by this specific probe.

The private detailed audit, source references, probe code and logs are in ignored
`build/fu3-review/SYSBUS_PERIBUS_A2P_REVIEW.md` at repository root. No production
bus or IP implementation was modified as part of this audit. System reset during
active traffic still needs a platform stop/drain or cancellation contract; the
bus's existing reset API requires quiescence. No CPU firmware/boot run is claimed.

## Recorded execution evidence

The retained local `build/sysbus-upgrade-final.log` records the following passing
runs. Its filesystem timestamp is 2026-10-16; this is historical evidence, not a
fresh certification of the current checkout.

| Build / use case | Passed CTest executions |
|---|---:|
| Static library regression | 5/5 |
| Static installed consumer | 4/4 |
| Shared library regression | 5/5 |
| Shared installed consumer | 4/4 |
| FX1/FU3 coexistence | 1/1 |
| Total | 19/19 |

These are repeated executions across build configurations, not 19 independent
specification requirements. The recorded environment is WSL/Linux with SystemC
2.3.4 and C++17. The retained `build/repo-shared-linkage.txt` resolves the installed
`libfu3_bus_system.so` and shared `libsystemc.so.2.3`. Generated logs are ignored
local artifacts and are not required in the repository.

## Coverage in the core test sources

| Test | Principal checks |
|---|---|
| [fu3_bus_config](../tests/bus_config_tests.cpp) | Default counts; invalid names, widths, ranges, capacities and overlapping windows; address bounds. |
| [fu3_bus_behavior](../tests/bus_behavior_tests.cpp) | Routing and local offsets; boundary rejection; APB splitting and timing; repeated strobes; downstream error/exception handling; debug image loading/readback; progress during denied traffic. |
| [fu3_bus_arbitration](../tests/bus_arbitration_tests.cpp) | FIFO service under flooding; independent output progress; queue recovery after target exceptions. |
| fu3_bus_zero_latency_fairness | The arbitration executable in zero-latency mode; runnable-master fairness. |
| [fu3_bus_contract](../tests/bus_contract_tests.cpp) | Independent golden address map; explicit metadata; fixed and incrementing footprints; narrow word adaptation; strobe handling; policy/handoff/reset guards; admission and actual downstream overlap; same-ID ordering. |
| [fu3_bus_safety](../tests/bus_safety_tests.cpp) | Preserved FIXED, protected side-effect words, quiescence/reset and exception recovery. |
| [Production IP scenarios](../tests/ip_integration/real_ip_tests.cpp) | Native DMA adapter, real DMA/SPI/UART data flow, Timer/PLIC IRQ and WDT coordinated reset. |
| [Installed consumers](../tests/installed_consumer/main.cpp) | Exported package usage and transactions over each of four target paths. |
| [Coexistence](../tests/coexist/CMakeLists.txt) | One executable links the FX1 and FU3 bus libraries together. |

CTest sets 30-second timeouts for regressions and 10-second timeouts for consumers
and coexistence. Behavioral/concurrency suites also contain simulation watchdogs.
The original core targets and traffic generators are test fixtures. The optional
production-IP suite separately exercises the real models listed above.

## Reproduction and interpretation

Use the [README build and test commands](../README.md#build-and-test), which cover
static/shared builds, installation, consumers and optional FX1 coexistence without
ignored helper scripts. Stop on any failed command and retain the failing output.

The evidence supports the functional bus contract, bounded concurrency, routing
and package integration. It does not establish cycle-accurate timing, complete
AMBA conformance, CPU firmware execution, complete PMP/security enforcement,
native Windows DLL operation or production peripheral correctness.

Timer integration has separate evidence in the
[Timer validation record](../../timer/docs/VALIDATION.md). Its checks should not be
counted as a new execution of the standalone bus regression.

For assumptions and endpoint obligations, see [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
