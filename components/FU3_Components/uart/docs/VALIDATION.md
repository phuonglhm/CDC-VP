# UART validation

Environment: WSL/Linux, GNU C++ 15.2.0, C++17, installed SystemC 2.3.4.
Executables use `sc_main`; exported library consumers link only `fu3::uart`.
Reproduction commands are in [README.md](../README.md#build-and-validate).

## Test structure

- `uart_behavior`: every combination of 5-8 data bits, stop selection, and
  none/odd/even/mark/space parity. TX is sampled independently at bit centers;
  an external RX stimulus constructs frames independently of production helpers.
  Checks cover framing, parity/break/overrun, FIFO head errors, false starts,
  majority voting, divisor selection, FIFO thresholds and flush, THRE/TEMT,
  timeout, modem/loopback, auto-flow, DMA and synchronous reset.
- The behavior suite is repeated at depth 32/64 and oversampling 8/16.
- `uart_boundaries`: back-to-back frames without added idle gaps, per-entry error
  acknowledgement, simultaneous interrupt causes, THRE sole-source clear,
  zero-divisor behavior, two concurrent MMIO callers, reset with queued requests,
  a reset pulse missing a clock edge, and malformed transfers consuming delay.
- `uart_integration`: all three UARTs on the real FU3 bus, independent golden
  addresses on both PERIBUS paths, serialized loopback, fake PLIC/Ibex routes,
  and a fake DMA initiator executing fixed-address byte bursts. UART0 uses the
  HAS 20 MHz boot source and integer divisor 65 with an explicitly chosen 8E1 frame.
- Installed consumers compile and execute the behavior suite without linking the
  bus library, checking public headers and transitive SystemC dependencies.

The fixtures are not a commercial UART VIP. No CPU firmware or boot ROM is
executed. Interrupt fan-out checks do not exercise real PLIC claim/complete.
The simulated DMA peer is not a production DMA controller.

## Execution records (2026-10-19)

The initial static run passed all four behavior configurations but failed the
integration DMA threshold expectation. The fixture had left FCR extended mode
enabled and therefore selected threshold 16 rather than 4. Repair T01 in
[REVIEW.md](REVIEW.md) corrected the programming sequence under DLAB. The next
static/shared and installed-consumer runs passed. Post-build spec rereading then
added the boundary suite; final results are recorded below.

| Build / consumer | Result |
|---|---|
| Static: four behavior configurations, boundaries, integration | 6/6 passed |
| Static installed consumer | 1/1 passed |
| Shared: four behavior configurations, boundaries, integration | 6/6 passed |
| Shared installed consumer | 1/1 passed |
| Total | 14/14 executions passed |

Each depth-64 behavior run reports 3,661 checks; depth-32 reports 3,430 checks.
The boundary suite reports 127 checks and integration 63 checks. All report zero
failures. The final build transcript contains no compiler warnings or errors.
The shared consumer resolves the installed `libfu3_uart.so` and shared
`libsystemc.so.2.3`; `ldd` reports no missing dependency.

Final transcript: ignored `build/validation-final.log`. Earlier transcripts:
`build/validation.log` and `build/validation-repair1.log`. Counts include transport
and repeated assertions; they are not unique requirements or a coverage percentage.

## Remaining limits

The source does not fully specify reset constants, IER/IIR encodings, timeout
duration, debug side effects, FCR resize behavior, DMA-clear timing or simultaneous
FIFO-event priority. The selected compatibility/VP rules are explicit in
MODEL_CONTRACT.md; tests validate those rules without turning them into HDS facts.

RS485 firmware control remains undefined. Auto-flow and 8x selection are available
as integration configuration, not invented CSR bits. RATIO and prefilter SIZE
have no sufficiently defined behavior to reproduce separately. The majority
voter is functional and does not model metastability or analog line conditioning.

Clock ticks are simulated while idle. Long full-platform boot runtime has not been
benchmarked. Maximum divisor, randomized baud mismatch, exhaustive noise shapes,
every reset phase and all same-tick RX/FIFO/IRQ races are not exhaustively covered.
Native Windows DLL linking is not validated by these Linux/WSL builds.

Private specifications, extracts, generated images and build artifacts remain
ignored. Only the new UART component is part of the source change.
