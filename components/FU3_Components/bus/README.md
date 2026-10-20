# FU3 bus - SystemC / TLM-2.0

Functional bus for the FU3 virtual platform, using C++17 and SystemC 2.3.4.
It provides routing, arbitration, peripheral transfers, validation and basic
access control. CPU, memory, CSR and loader implementations belong to the platform.
Fake IPs are test fixtures only.

## Directory layout

```text
include/fu3_bus/           Public API and transaction metadata
src/                      Implementation and FU3 address map
tests/                    Configuration, behavior, contention and contract tests
tests/support/            Fake masters and sparse targets
tests/installed_consumer/  Installed package tests
tests/coexist/             FX1/FU3 link test
cmake/                    CMake package configuration
docs/                     Model contract and validation results
scripts/                  Optional local helpers, ignored by Git
build/                    Generated files, ignored by Git
```

## Topology

```mermaid
flowchart LR
  M[CVA6 / IBEX / DBG / DMA0] --> S1[SYSBUS_1]
  S1 --> MEM[ROM / ISRAM / DSRAM / IBEX_DBG / HYPERBUS]
  S1 --> S0[SYSBUS_0]
  S0 --> AXI[AES / QSPI_XIP / EDHOC_OSCORE]
  S0 --> A0[A2P_PP0]
  S0 --> A1[A2P_PP1]
  A0 --> P0[PERIBUS_0 APB4]
  A1 --> P1[PERIBUS_1 APB4]
```

Figures 4-1 and 7-1 place both A2P bridges behind SYSBUS_0. Addresses follow
HAS v2.0 pages 46-47. The PERIBUS_1 SYS column conflicts with the figures;
the agreed topology follows the figures while preserving the published addresses.

`BusConfig::fu3()` enables four initiators and 36 targets. Bind every enabled
target before `sc_start`; unused initiators may remain unbound. Disable missing
targets before constructing the fabric; their addresses become unmapped.
Configuration is fixed during elaboration. Names, ranges, overlaps, widths and
paths are validated. Regions use `[base, base+size)`. Intermediate routers keep
global addresses; leaf routers translate to local offsets. Payload fields are
restored on return, including exceptions.

## Integration

```cpp
#include <fu3_bus/bus_system.h>
auto cfg = fu3_bus::BusConfig::fu3();
fu3_bus::BusSystem fabric{"fabric", cfg};
ibex.socket.bind(fabric.initiator("IBEX"));
cva6.socket.bind(fabric.initiator("CVA6"));
// Native AP_DMA uses fu3_bus::DmaAdapter; see the integration contract below.
fabric.target("IBEX_ISRAM").bind(ibex_sram.socket);
fabric.target("SYSCSR").bind(syscsr.socket);
// Bind all other enabled targets, or disable them in cfg before construction.
// The boot controller calls finish_staging() before releasing CVA6 reset.
```

Call `b_transport` from an `SC_THREAD`, not an `SC_METHOD` or directly from
`sc_main`. Executables define `int sc_main(int, char**)`; the library defines
neither `main` nor `sc_main`. See `tests/installed_consumer/main.cpp` for an example.

Loaders may use `transport_dbg` to access memory without simulated delay. This
intentional backdoor bypasses access policy and must not replace CPU traffic.
Tests verify image-byte loading and readback in ISRAM. The library does not
execute firmware or parse ELF files.

## CPU/DMA transaction contract

Default FU3 ports `CVA6`, `IBEX`, `DBG`, `DMA0` require `Access` on every
functional transaction. Missing metadata returns GENERIC_ERROR; the fabric never
silently guesses instruction/data or beat width. `transport_dbg` is for loaders.

```cpp
// tx already has command, global address, data, length and streaming_width >= length.
// DMA: five halfword FIFO beats, length = 10 bytes.
{
    fu3_bus::ScopedAccess request(tx, 2, fu3_bus::Burst::Fixed);
    dma.socket->b_transport(tx, delay);
}
// CPU instruction fetch: read, length=4, address aligned to four bytes.
{
    fu3_bus::ScopedAccess fetch(tx, 4, fu3_bus::Burst::Increment, true);
    cpu.socket->b_transport(tx, delay);
}
```

- Beat width is 1/2/4 bytes, naturally aligned; length must be a multiple of it.
  INCR advances the address; FIXED holds it while advancing data/byte-enable position.
- DMA accepts at most 16 beats. CPU/DBG have a configurable VP guard of 256 beats
  (`max_beats`), not a burst-length value established by the HAS. WRAP, atomics
  and streaming wrap are unsupported and must not be silently substituted.
- Check the entire footprint before the first beat. FIXED near an aperture end
  is valid when one beat fits. An INCR request crossing a target is rejected whole.
- `Burst` describes functional address progression, not AXI AxBURST encoding.
  Ingress emits single incrementing operations; fixed-address requests become
  repeated singles. DBG rejects fixed progression and non-word beat sizes.
- Word interfaces from Table 7-1 receive aligned 4-byte operations. Narrow accesses
  use lane enables and read extraction without RMW; other endpoints retain exact
  beat sizes. QSPI receives single operations with one outstanding maximum.
  Byte enables repeat over payload bytes and survive beat boundaries.
- No read-modify-write on CSR. Targets must honor byte enables. `Strobe = Full` is confirmed
  by the user to mean full byte-write-strobe support, including partial/sparse
  writes, not mandatory all-byte writes. IP adapters must satisfy this contract.
  Both PERIBUS_0 and PERIBUS_1 use APB4: the user requires matching versions,
  using the explicit APB4 version of PERIBUS_1 for both. This clarifies the PDF
  label on PERIBUS_0. Transfer-size adaptation remains a separate question.
- Timer0/1/2/3 are an endpoint exception: their Timer HDS excludes partial
  writes. The bus preserves byte enables; the Timer VP adapter accepts aligned
  full-word accesses and rejects partial requests without read-modify-write.
  See the [Timer contract](../timer/docs/MODEL_CONTRACT.md) for adapter diagnostics.
- Default AP_PLIC claim/complete at local 0x24 requires a full aligned word
  with all lanes enabled. `strict_word_offsets` protects side-effect words before
  dispatch; other PLIC configuration words retain narrow access support.
- Production AP_DMA needs `fu3_bus::DmaAdapter` to translate its native
  metadata and lane strobes; see the [integration contract](docs/MODEL_CONTRACT.md#production-ip-integration-contract-2026-10-20).
- Downstream error stops the request; completed beats are not rolled back.
  Exceptions restore payload fields and release admission, order and pipeline leases.
- Custom initiators in `BusConfig{}` default to `require_access=false` for legacy
  TLM models. Without metadata they issue linear byte blocks, not AXI bursts;
  legacy A2P accepts aligned 1/2 bytes or word-aligned 4..64-byte blocks. Do not
  use this mode as a replacement for FU3 CPU/DMA instruction/transfer semantics.

## Admission, ordering and pipelines

- CPU admission is 2 total read/write requests; DBG is 1; DMA has separate 4-read
  and 4-write gates. Excess callers wait FIFO rather than fail. A slot spans the
  complete request, including time waiting for its ordered turn.
  The CPU combined limit is a conservative VP interpretation of Table 7-1, not
  an explicit per-direction limit. HyperBus/EDHOC target capacities default to
  one because their table entries are unspecified; see MODEL_CONTRACT.md for
  the PDF evidence and selected defaults.
- Requests sharing master, direction and `Access::ordering_id` remain ordered.
  Different ordering IDs can complete independently. ID zero is the default;
  adapters must use distinct IDs only where out-of-order completion is allowed.
  This is an ordering token, not a signal-level AXI ID implementation.
- Output dispatch is FIFO per beat, confirmed by the user; there is no fixed
  master priority. Arrival order is measured at each arbiter after input delay;
  simultaneous arrivals follow scheduler enqueue order. Independent outputs do
  not share a global completion-order constraint. One-stage APs at masters and AP_SB0 release
  before downstream completion; leaf occupancy is separately bounded. SRAM/AES
  allow four pending operations, ROM/QSPI one. Custom AXI target capacity is
  `TargetConfig::outstanding`; its default is one. APB always serializes.
- Slow IPs do not retain SYSBUS dispatch ownership. Independent outputs, including
  those behind AP_SB0, can overlap. A stalled IP can exhaust its target/master
  capacity; no timeout is invented. `peak_outstanding` reports admission peaks.
- AP_PP0/1 and A2P retain serialized APB access. Stage timing is a functional LT
  approximation, not a model of independent AXI channel handshakes or RTL throughput.
- Input and downstream delays are consumed once; return delay is zero. Defaults:
  AP 2 ns (`axi_cycle_ns`), PP 10 ns (`apb_cycle_ns`), router decode 2 ns
  (`router_latency_ns`), A2P setup+access two APB cycles per beat. These are VP
  parameters, not confirmed RTL frequencies. APB timing tests include all stages.
- Functional requests yield to the kernel. Validation/policy failures consume
  router latency so an error loop does not starve a time-scheduled peer. With
  all latencies zero, fairness is only among runnable processes; an infinite
  delta-cycle loop cannot advance physical time. DMI and `nb_transport` are disabled.

## Access, boot and reset

- ROM is readable only by IBEX and never functionally writable. DBG/DMA denial
  is a conservative documented choice pending a full permission matrix.
- DSRAM rejects instruction fetch. Default ports reject missing metadata; CPU
  adapters must still identify fetches honestly. This does not replace PMP/ePMP
  or the CPU's full W^X/cacheability implementation.
- IBEX may write CVA6_ISRAM during staging. Stop issuing IBEX requests and drain
  them before `finish_staging()` and CVA6 reset release. The call throws
  `std::logic_error` without changing policy if IBEX has any pending request.
- Stop producers and call `quiesce()` in a platform SC_THREAD to close ingress
  and drain admitted work. Reset attached IPs, call `reset()`, release IP reset,
  call `resume()`, then restart producers. Debug loaders must stop separately.
  Do not call quiesce from a target callback; no hung-target timeout is invented.
- `reset()` requires all masters to be quiescent; otherwise it throws without
  changing policy. It reopens staging for reboot, does not reset external IPs or
  abort transactions, and retains diagnostic peak counters.
- Clock/reset pins, CGF, IRQ, HyperRAM readiness and peripheral CSR behavior are
  platform/IP responsibilities. The bus does not invent missing register maps.
- `transport_dbg` bypasses timing and access policy intentionally for firmware
  loading, but still rejects cross-window requests. It is not a CPU execution path.

| Condition | TLM response |
|---|---|
| Unmapped, cross-window, misalignment | ADDRESS_ERROR |
| Unsupported command, denied policy, instruction write | COMMAND_ERROR |
| Unsupported beat/count/burst/streaming | BURST_ERROR |
| Invalid byte enables | BYTE_ENABLE_ERROR |
| Missing required metadata, null data, zero length | GENERIC_ERROR |

## Build and test

Run from this directory in a Linux or WSL Bash shell. Use an installed SystemC
with a compatible compiler, C++ standard and ABI. Replace the example prefix.
No local scripts are required. Stop if any step fails.

```bash
export SYSTEMC_HOME=/absolute/path/to/systemc-install
for mode in static shared; do
    shared=OFF
    if [ "$mode" = shared ]; then shared=ON; fi
    cmake -S . -B "build/$mode" -DSYSTEMC_HOME="$SYSTEMC_HOME" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS="$shared" -DFU3_BUS_BUILD_TESTS=ON || break
    cmake --build "build/$mode" --parallel 4 || break
    ctest --test-dir "build/$mode" --output-on-failure || break
    cmake --install "build/$mode" --prefix "$PWD/build/$mode/install" || break
    cmake -S tests/installed_consumer -B "build/$mode/consumer" -DCMAKE_PREFIX_PATH="$PWD/build/$mode/install;$SYSTEMC_HOME" || break
    cmake --build "build/$mode/consumer" --parallel 4 || break
    ctest --test-dir "build/$mode/consumer" --output-on-failure || break
done
```

Each build runs six regression tests and five installed-consumer tests. Enable
`-DFU3_BUS_BUILD_IP_TESTS=ON` alongside `FU3_BUS_BUILD_TESTS` to add four
production-IP scenarios (adapter, DMA/SPI/UART, Timer/PLIC and WDT/reset). This
requires the sibling DMA0/AP_DMA, SPI, UART, Timer, WDT and PLIC sources; the
exported bus library does not depend on them. With
FX1 available at its repository location, also run the coexistence check:

```bash
cmake -S tests/coexist -B build/coexist -DSYSTEMC_HOME="$SYSTEMC_HOME" -DBUILD_SHARED_LIBS=ON
cmake --build build/coexist --parallel 4
ctest --test-dir build/coexist --output-on-failure
```

This adds one test: 23 executions for core builds, or 31 with production-IP
tests enabled in both static and shared builds. The optional `scripts/`
helpers are ignored by Git and are not required after cloning.

The public CMake target is `fu3::bus_system`, with `SystemC::systemc` as a PUBLIC
dependency. Use the imported target to retain SystemC compile/link settings.
Namespaces, include paths and package targets are separate from FX1. Native
Windows DLL support is not validated; the verified environment is WSL/Linux.

- [Model contract and integration details](docs/MODEL_CONTRACT.md)
- [Recorded validation results](docs/VALIDATION.md)
- [Production-IP integration coverage and remaining work](docs/INTEGRATION_VALIDATION.md)
- [Repository integration guide](docs/REPOSITORY_HANDOFF.md)
