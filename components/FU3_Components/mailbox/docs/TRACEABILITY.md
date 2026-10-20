# MAILBOX specification traceability

Source: FU3 HAS v2.0 and, where HAS is silent, the RTL team's IP_Feature_Spec sheet MAILBOX V1.0 (temporary reference), reviewed 2026-10-20. Checks live in `tests/bench.h` and `behavior.cpp` binds directly and `integration.cpp` uses the FU3 bus.

| Source | Behavior | Verification |
| --- | --- | --- |
| HAS v2.0 items 45-46, memory map | Two endpoints: MAILBOX_CVA6 0x5002_0000 (PERIBUS_1), MAILBOX_IBEX 0x4404_0000 (PERIBUS_0) | Integration bench: CVA6 initiator to MAILBOX_CVA6 and IBEX initiator to MAILBOX_IBEX through the FU3 bus |
| HAS v2.0 10.2 | `irq_mailbox_ibex` -> Ibex `irq_fast_i[14]` | `irq_ibex` follows Ibex IRQ_PENDING and INTMAP routing is tested in the PLIC package |
| Sheet F003, F004 | Two FIFOs, both directions | Three words each way, in order and each port's own receive FIFO stays empty |
| Sheet F005, F006 | 32-bit data, depth 32 | FULL after 32 words, drain in order |
| Sheet F010, F013, F014 | EMPTY/FULL, write-when-full and read-when-empty errors with PSLVERR | Error responses, FIFO_WR_ERROR / FIFO_RD_ERROR set on the accessing port only, rejected word not stored, empty read returns 0 |
| Sheet F011, F012 | WR_THRESHOLDS / RD_THRESHOLDS comparisons | Receive: active above the threshold and send: active at or below it |
| Sheet F015, F016 | Four sticky IRQ events, W1C, rising-edge threshold events | Edge only once while active and new edge after falling and W1C of 0, of another bit, of the bit and threshold write as an edge |
| Sheet IRQ_ENABLE / IRQ_PENDING | Pending = status & enable and IRQ output | Enable masking keeps status, clears pending and the output |
| Sheet register table | Reset values, RW widths | Power-on and after-reset state of both ports and 32-bit thresholds, IRQ_ENABLE [31:28] |
| VP choice | Invalid accesses -> PSLVERR | WO read, RO writes, unmapped offsets, byte/halfword, partial PSTRB |
| VP choice | Debug read peeks, debug write ignored | Head word visible, not popped, no error flag |
| VP choice | Reset clears everything, accesses during reset ignored | Reset with data and a pending IRQ |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run the behavior bench via find_package |

Not covered: firmware on the CPUs, CDC timing, separate per-port resets, and the open questions in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
