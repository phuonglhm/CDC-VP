# AP_PLIC specification traceability

Source: private PLIC HDS V1.0 and FU3 HAS v2.0, reviewed 2026-10-19. Checks live in `tests/bench.h` and `behavior.cpp` binds directly and `integration.cpp` uses the FU3 bus.

| Source | Behavior | Verification |
| --- | --- | --- |
| HDS Table 5-1 | SOURCES 32, TARGETS 1, PRIORITIES 8, MAX_PENDING_COUNT 8, HAS_THRESHOLD 1 | CONFIG_0/1 read-back and CONFIG writes ignored |
| HDS Table 8.1 | Register offsets and access types | Reset values, write all-ones/all-zeros masks |
| HDS 7.1 | Zero wait state, no slave error | Unmapped offset reads 0, write ignored, OK response |
| HDS 7.1 (PSTRB) | Byte write enables | Byte write to PRIORITY_1 lane 2 and sparse PSTRB lanes 0 and 3 |
| HDS 7.2 | ID n <- irq_sources_i[n-1], ID 0 = none | Claims return the expected IDs and claim with nothing pending returns 0 |
| HDS 7.2 (le_i) | Level and edge triggering | Level claim/complete/re-pend and short level pulse latched until claim and edge pulses queued after the input falls |
| HAS Table 11-1/11-2 | TIMER1 -> ID 6 -> CVA6 irq_i[0] | Out-of-repository CVA6 firmware test: one claim of ID 6, no re-entry |
| HDS Table 5-1 | MAX_PENDING_COUNT edge queue | Ten pulses give eight claims |
| HDS 2 (features 4-9) | Priority arbitration, enable, threshold, claim/complete | Highest priority first, ties to lowest ID, IE masking, priority must exceed threshold and priority 0 never interrupts |
| HDS 7.3 | One target, level output o_plic_irq[0] | `irq` follows claimable state, drops on claim and on reset |
| VP choice | Wrong-ID complete ignored | DMA0, 0 and 33 do not release a claimed QSPI |
| VP choice | Debug read of ID_0 does not claim | transport_dbg returns the ID and a following functional read claims it |
| VP choice | Reset clears registers and in-service state and writes ignored during reset | Reset sequence check |
| HAS v2.0 memory map | AP_PLIC at 0x4010_0000 on PERIBUS_1 | Integration bench through the actual FU3 bus from the CVA6 port |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run the behavior bench via find_package |
| HAS v2.0 10.2 | Ibex INTMAP: 26 source nets ORed onto irq_nm_i, irq_fast_i[14:0], irq_external_i, irq_software_i, irq_timer_i | `tests/intmap.cpp`: each source alone raises only its HAS output and OR keeps an output high while any source stays high, all high and cause table |

Not covered: firmware execution on CVA6, real interrupt peers, and the open questions in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
