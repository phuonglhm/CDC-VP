# AP_PLIC specification traceability

Source: private FU3 HAS v2.0 and FTEL_FU3_Common_Spec v2.0, reviewed 2026-10-20. Checks live in `tests/bench.h` and `behavior.cpp` binds directly and `integration.cpp` uses the FU3 bus.

| Source | Behavior | Verification |
| --- | --- | --- |
| Common v2.0 PLIC Connections (N_SOURCE, MAX_PRIO) | 23 sources, priority [2:0] per ID at `4 * ID` | Reset values, priority write-ones reads 7 for ID 1-23 |
| Common v2.0 register layout | Source 0 priority reads 0, pending bitmap 0x1000 RO, enable 0x2000 mask 0x00FF_FFFE | ID 0 write ignored, pending write ignored, enable write-ones read-back |
| Common v2.0 / HAS Table 11-1 | ID n <- irq_sources_i[n-1], pending bit n = ID n | Pending bitmap after TIMER1 rises, claims return the expected IDs, claim with nothing pending returns 0 |
| Common v2.0 gateway handshake | Level gateway, one outstanding event, claim clears pending, re-pend after complete | Claim/complete/re-pend and short level pulse latched until claim |
| Common v2.0 arbitration | Priority > threshold, ties to lowest ID, priority 0 never interrupts | Highest priority first, ties to lowest ID, enable masking, equal priority masked |
| Common v2.0 threshold corner case | Threshold 7 masks priority 7 | Threshold 7 check |
| Common v2.0 completion validation | Only IDs 1-23 in service complete | DMA0, 0, 24 and 31 do not release a claimed QSPI |
| Common v2.0 APB access contract | Full aligned words, PSLVERR for unmapped offsets and partial PSTRB | Priority ID 24, 0x1004, 0x3008 and partial PSTRB return errors, halfword and misaligned rejected |
| HAS Table 11-2 | TIMER1 -> ID 6 -> CPU0 (CVA6) irq_i[0] | Out-of-repository CVA6 firmware test: one claim of ID 6, no re-entry |
| VP choice | Threshold 0x3000, claim/complete 0x3004 | All claim and threshold checks use these offsets |
| VP choice | Debug read of CLAIM does not claim | transport_dbg returns the ID and a following functional read claims it |
| VP choice | Reset clears registers and in-service state and writes ignored during reset | Reset sequence check |
| HAS v2.0 / Common v2.0 memory map | AP_PLIC at 0x4010_0000 on PERIBUS_1 | Integration bench through the actual FU3 bus from the CVA6 port |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run the behavior bench via find_package |
| HAS v2.0 10.2 | CPU1 (Ibex) INTMAP: 26 source nets ORed onto irq_nm_i, irq_fast_i[14:0], irq_external_i, irq_software_i, irq_timer_i | `tests/intmap.cpp`: each source alone raises only its HAS output and OR keeps an output high while any source stays high, all high and cause table |

Not covered: real interrupt peers, IRQ_ADAPT synchronization, and the open questions in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
