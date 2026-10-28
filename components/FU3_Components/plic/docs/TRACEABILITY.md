# AP_PLIC specification traceability

Source: private FU3 HAS v2.1 and FTEL_FU3_Common_Spec v2.1 with the platform plan T02 decisions (Q02-g, Q03-a/b/g/h). Checks live in `tests/bench.h` and `behavior.cpp` binds directly and `integration.cpp` uses the FU3 bus.

| Source | Behavior | Verification |
| --- | --- | --- |
| Common v2.1 PLIC Configuration (32 slots, MAX_PRIO) | 31 sources, priority [4:0] per ID at `4 * ID` | Reset values, priority write-ones reads 31 for ID 1-31 |
| Common v2.1 register layout | Source 0 priority reads 0, pending bitmap 0x1000 RO, enable 0x2000 mask 0xFFFF_FFFE | ID 0 write ignored, pending write ignored, enable write-ones read-back |
| Common v2.1 Interrupt Mapping | ID n <- irq_sources_i[n-1], pending bit n = ID n, IDs 24-31 provisional (Q03-h) | Pending bitmap after TIMER1 rises, claims return the expected IDs, claim with nothing pending returns 0 |
| Gateway handshake | Level gateway, one outstanding event, claim clears pending, re-pend after complete | Claim/complete/re-pend and short level pulse latched until claim |
| Arbitration | Priority > threshold, ties to lowest ID, priority 0 never interrupts | Highest priority first, ties to lowest ID, enable masking, equal priority masked |
| Threshold corner case | Threshold 31 masks priority 31 | Threshold 31 check |
| Completion validation | Only IDs 1-31 in service complete | Wrong or idle IDs do not release a claimed QSPI |
| Q02-g strobes | Byte/halfword lanes, partial PSTRB merges enabled lanes, CLAIM full word only | Partial PSTRB writes one lane, halfword reads per lane, unmapped offsets return errors |
| Q03-a | Threshold 0x3000, claim/complete 0x3004 (VP ABI) | All claim and threshold checks use these offsets |
| VP choice | Debug read of CLAIM does not claim | transport_dbg returns the ID and a following functional read claims it |
| VP choice | Reset clears registers and in-service state and writes ignored during reset | Reset sequence check |
| Memory map | AP_PLIC at 0x4010_0000 on PERIBUS_1 | Integration bench through the actual FU3 bus from the CVA6 port |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run the behavior bench via find_package |
| Common v2.1 Ibex allocation (Q03-b) | INTMAP: 34 source nets with reset-disabled route enables, ORed onto irq_fast_i[14:0], irq_external_i, irq_software_i, irq_timer_i; irq_nm_i low | `tests/intmap.cpp`: each source alone raises only its v2.1 output once enabled, nothing while disabled, OR keeps an output high while any source stays high, cause table |
| Platform (fu3_soc T08) | PLIC and INTMAP read the same source nets; PLIC -> CVA6 cause 11, INTMAP -> Ibex | `platforms/fu3_soc/tests/test_fu3_soc_peripherals.cpp` section `plic_intmap` and the per-peripheral IRQ checks |

Not covered: IRQ_ADAPT synchronization and the open questions in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
