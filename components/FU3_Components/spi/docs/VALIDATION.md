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

The historical complete local transcript is `build/validation-gap-fix.log`, after
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

## Corner regression expansion (2026-10-20)

Final execution results on the same WSL/GNU C++ 15.2.0/SystemC 2.3.4 environment:

| Configuration | Result |
|---|---|
| Static component regression | 18/18 passed |
| Static installed consumers | 8/8 passed |
| Shared component regression | 18/18 passed |
| Shared installed consumers | 8/8 passed |
| Total final executions | 52/52 passed |

All report zero failures. Final transcript: ignored
`build/validation-corners-final.log`. Configuration/installation and earlier
execution evidence: `build/validation-corners.log`. The shared corner consumer
resolves installed `libfu3_spi.so` and shared `libsystemc.so.2.3` without missing
dependencies. No compiler warnings or errors were reported. These executions
repeat scenario tests across configurations; they are not a specification coverage
percentage. No production source or public interface changed in this test expansion.

`spi_corner_tests` adds seven named scenarios using only public MMIO and pins:

- `slave_formats`: all 32 FSZ encodings, four clock modes and both bit orders,
  with two distinct frames under one select. An independent external master
  supplies MOSI and samples MISO. FSZ 0..2 clamp to four bits; widths through
  32 bits are checked for truncation, zero-extension, FIFO pop timing and order.
  Repeat at FIFO depths 16, 32 and 64 (512 frames per depth).
- `fifo_flush`: writes during an active TX drain, retention after acknowledgement,
  repeated flush requests, hardware clear against a simultaneous software set,
  a rewrite during acknowledgement return, RX flush while receiving, a same-tick
  RX drain/push collision, and ordered tail retention after cancelling RXFLUSH.
  Repeat at depths 16/32/64. The drain/push result tests the stated VP ordering;
  it does not resolve unspecified RTL simultaneous-event semantics.
- `event_priority`: first-edge UDR, deselection EOT and frame-end OVR collide with
  ISR W1C. Hardware events must survive; a later clear must succeed. Full-FIFO
  overrun must preserve all older received words.
- `abort_states`: EN clear, MSTR clear and system reset during IDLE, SETUP,
  SHIFT, GAP, HOLD and CS_IDLE, identified from pin timing rather than private
  state. Check select/SCK/output enables, absence of new EOT on abort, retention
  of completed RX/unstarted TX, and the reset profile. System reset is issued
  with APB quiescent as required by the HDS.
- `dma_boundaries`: RX and TX thresholds 0/1/DEPTH, threshold loss during REQ,
  ACK-held WAIT despite restored data/space, return-to-zero rearming, DMACR
  cancellation and EN cancellation. These exercise request/acknowledge peers,
  not the production DMA controller.
- `reset_phases`: three APB reset pulses after commit but before response,
  exactly one TXDR side effect, subsequent access recovery, continued serial
  work through APB-only reset, partial-slave-frame system reset and clean restart.
  APB periods 1/10/13/80 ns are tested against SYS=10 ns, including a non-integer
  ratio. This is functional transport/reset coverage, not physical CDC sign-off.
- `master_delays`: DIV 0/1/2/65535 and selected DLY 0/1/255 profiles. An independent
  MOSI observer checks two ordered four-bit frames; RX readback, setup, inter-frame,
  hold and minimum idle intervals and absence of premature EOT are checked.
  Assertions describe the published functional interval model, not RTL pipelines.

The integration suite now also runs two initiator threads: CPU word writes and
DMA-stand-in FIXED bursts contend for SPI0 while SPI2 transfers through the other
peripheral bridge. Eight received frames must appear exactly once with each
origin's order preserved, without asserting a global CPU-versus-DMA order. SPI2's
two frames must also remain intact. Integration now reports 123 checks.

Installed consumers compile both the original behavior suite and the new corner
suite, link only the installed `fu3::spi` target, and execute eight tests per
configuration. Component builds execute 18 tests. Each corner scenario has a
100 ms simulation watchdog and a 30-second CTest timeout.

The initial TX-flush stimulus wrote into a still-full FIFO before the flush reader
had started, causing the expected TXOVF. Repair F01 waited three system periods
before refilling; it was a fixture correction on the first attempt. No SPI
production implementation change was needed. No defect reached three attempts.

## Limits of the evidence

Passing these tests establishes functional scenarios, not complete HDS checklist
coverage. Untested or incompletely covered areas include the full DIV/DLY
cross-product, every CPU/DMA/flush interleaving, every hardware-set/W1C collision,
exhaustive reset phase sweeps and arbitrary clock-ratio/phase combinations. The
slave width gap is covered by the new independent full-format matrix. The model
does not claim exact RTL CDC latency, physical timing or native
Windows DLL support. Idle system ticks are still simulated; full-platform runtime
has not been benchmarked.

Serial loopback and a programmed external-master stimulus are fake peers, not a
commercial SPI VIP. The interrupt receiver and DMA initiator are also fixtures.
No CPU firmware execution, actual PLIC claim/complete, IOSEL pad mapping or full
boot sequence has been verified. SPI2/3 DMA channel assignments remain unspecified
by the reviewed HAS and are not invented here.
Production AP_DMA still needs translation of its metadata to the bus Access
contract before its real data path can be connected; the added concurrent DMA
stand-in test does not close that separate integration issue.

Generated evidence stays under ignored `build/`. Private PDF content is excluded
from the component. See [traceability](TRACEABILITY.md) for requirement mapping.
