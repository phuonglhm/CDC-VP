# MAILBOX specification traceability

Source: FU3 HAS v2.0, FTEL_FU3_Common_Spec v2.0 and the Mailbox HDS V1, reviewed 2026-10-20. Checks live in `tests/bench.h` and `behavior.cpp` binds directly and `integration.cpp` uses the FU3 bus.

| Source | Behavior | Verification |
| --- | --- | --- |
| HAS v2.0 items 45-46, memory maps | Two endpoints: MAILBOX_CVA6 0x5002_0000 (PERIBUS_1), MAILBOX_IBEX 0x4404_0000 (PERIBUS_0) | Integration bench: CVA6 initiator to MAILBOX_CVA6 and IBEX initiator to MAILBOX_IBEX through the FU3 bus |
| HAS v2.0 10.2 | `irq_mailbox_ibex` -> Ibex `irq_fast_i[14]` | `irq_ibex` follows Ibex IRQ_PENDING and INTMAP routing is tested in the PLIC package |
| HDS 1.1, 2 | Two FIFOs, port 0 -> port 1 and port 1 -> port 0 | Three words each way, in order and each port's own receive FIFO stays empty |
| HDS 5 | 32-bit data, FIFO_DEPTH 32 | FULL after 32 words, drain in order |
| HDS 7.1.3, 8.4.3, 8.4.4 | EMPTY/FULL, write-when-full and read-when-empty errors with PSLVERR | Error responses, FIFO_WR_ERROR / FIFO_RD_ERROR set on the accessing port only, rejected word not stored, empty read returns 0 |
| HDS 7.1.4, 7.1.5 | WR_THRESHOLDS / RD_THRESHOLDS comparisons | Receive: active above the threshold and send: active at or below it |
| HDS 7.1.6, 8.4.1, 8.4.2 | Four sticky IRQ events, W1C, rising-edge threshold events | Edge only once while active and new edge after falling and W1C of 0, of another bit, of the bit and threshold write as an edge |
| HDS 7.1.7, 7.1.8 | Pending = status & enable and IRQ output | Enable masking keeps status, clears pending and the output |
| HDS 7.1 register table | Reset values, RW widths | Power-on and after-reset state of both ports and 32-bit thresholds, IRQ_ENABLE [31:28] |
| HDS 8.5 | Invalid accesses -> PSLVERR | WO read, RO writes, IRQ_PENDING write, unmapped offsets, byte/halfword, partial PSTRB |
| HDS 6.1, 6.2, 8.6 | One reset per port, the other port keeps working | Ibex-only reset keeps CVA6 registers and IRQ, empties both FIFOs, Ibex reads 0, words written during the peer reset are kept, no event on release, then CVA6-only reset |
| VP choice | Debug read peeks, debug write ignored | Head word visible, not popped, no error flag |
| VP choice | Accesses to a port in reset ignored | Reset of both ports with data and a pending IRQ |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run the behavior bench via find_package |

Not covered: firmware on the CPUs, CDC timing, and the open questions in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
