# SPI validation

Validation environment: WSL/Linux, GNU C++ 15.2.0, C++17 and SystemC 2.3.4.
The tests use real `sc_main` entry points. Reproduction commands are in the
[README](../README.md#build-and-validate).

## Recorded results (2026-10-17)

| Configuration | CTest result |
|---|---|
| Static: behavior at depth 16/32/64 and integration | 4/4 passed |
| Static installed consumer | 1/1 passed |
| Shared: behavior at depth 16/32/64 and integration | 4/4 passed |
| Shared installed consumer | 1/1 passed |
| Total | 10/10 executions passed |

Behavior runs report 15,141 / 15,351 / 15,773 checks for depths 16 / 32 / 64,
respectively, with zero failures. Integration reports 84 checks and zero failures.
Installed consumers repeat the depth-16 behavior suite. Counts include transport
and polling assertions; they are not counts of unique requirements or a coverage
percentage.

The latest complete local transcript is `build/validation-gap-fix.log`, after
correcting GAP FIFO consumption and MSTR-abort SCK output. The initial run remains
in `build/validation.log`. No compiler warnings
or errors were reported in the final run. `ldd` on the shared installed consumer
resolves installed `libfu3_spi.so` and shared `libsystemc.so.2.3`; no dependency is
missing. The installed consumer links only `fu3::spi`, without the bus library.

An initial slave test failed because its external-master stimulus changed MOSI
at the trailing sample edge. The stimulus was corrected to retain MOSI after that
edge; all mode/bit-order regressions passed with the corrected hold interval.

## Test design

`spi_behavior` runs direct functional TLM register accesses and pin-level serial
stimulus. The same suite also runs at FIFO depths 32 and 64. It covers:

- Register defaults and reserved decode; configuration byte strobes; TXDR's
  ignored strobes; overflow/underflow and ISR W1C lane qualification.
- All 32 FSZ encodings, both bit orders and four clock modes in master mode,
  two back-to-back loopback frames for each combination (512 frames per depth).
- An independent external-master stimulus for slave mode, all clock modes and
  both bit orders, with MOSI held across both sample-edge possibilities.
- Slave preload/first-edge pop, late TX writes, underrun zeros, RX overrun and
  partial-frame discard on deselect.
- RX-full master backpressure, ordered recovery, manual multi-select, TXONLY,
  abort without EOT, master output enables independent of EN and FIFO flush.
- Nonzero IFD in all four clock modes: queued TX level/vacancy during GAP,
  ordered normal resume, flush preventing an extra transmitted frame, and abort
  retaining an unstarted word. CPOL-high MSTR abort is checked for both CPHA
  values, including SCK zero, deselection, output enables and absence of EOT.
- IRQ threshold edge cases, masking and level flags that W1C cannot suppress.
- DMA request retention after a threshold stops being true, acknowledgement,
  WAIT until acknowledge falls, rearm and disable.
- APB reset retaining registers/FIFO, reset during a launched request with one
  side effect and dropped response, recovery, system reset and debug RX read pops.

`spi_integration` connects all four controllers to the actual FU3 BusSystem,
across both PERIBUS paths, using four different system periods. Independent golden
base constants check the published windows. Serial loopback transfers and fake
PLIC/Ibex receivers verify data and interrupt fan-out. A DMA0 initiator performs
four-word fixed-address TXDR/RXDR bursts and follows request/acknowledge ordering.
No production DMA engine is required by this test.

The installed-consumer test compiles the behavior suite with only the exported
`fu3::spi` target. It validates public headers and transitive SystemC linkage.
CTest timeouts are 30 seconds; simulation watchdogs bound the behavior and
integration suites to 20 ms and 1 ms respectively.

## Limits of the evidence

Passing these tests establishes functional scenarios, not complete HDS checklist
coverage. Untested or incompletely covered areas include all extreme DIV/DLY
combinations, repeated flush races, every hardware-set/W1C collision, reset phase
sweeps, slave frame widths other than eight bits, and arbitrary asynchronous clock
ratios. The model does not claim exact RTL CDC latency, physical timing or native
Windows DLL support. Idle system ticks are still simulated; full-platform runtime
has not been benchmarked.

Serial loopback and a programmed external-master stimulus are fake peers, not a
commercial SPI VIP. The interrupt receiver and DMA initiator are also fixtures.
No CPU firmware execution, actual PLIC claim/complete, IOSEL pad mapping or full
boot sequence has been verified. SPI2/3 DMA channel assignments remain unspecified
by the reviewed HAS and are not invented here.

Generated evidence stays under ignored `build/`. Private PDF content is excluded
from the component. See [traceability](TRACEABILITY.md) for requirement mapping.
