# FU3 CPU configuration traceability

Source: private FTEL_FU3_Common_Spec v2.1 (revision 18/10/2026) and HAS v2.1,
decisions Q01/Q02 of the FU3 VP plan. Verification: firmware tests on the
assembled platform (`platforms/fu3_soc`, ctest `fu3_soc_cpu_cva6` and
`fu3_soc_cpu_ibex`, check ids in `tests/fw/*_tests.c` and `tests/fw/fw_debug.c`)
and the wrapper test `fu3_cpu_debug_{cva6,ibex}`.

| Source | Requirement | Verification |
| --- | --- | --- |
| CVA6-Config #2-#12 | RV32IMFC, M-mode only, no A/D | cva6 checks 1, 3, 4-13 |
| CVA6-Config #42/#43 | 8 PMP entries, NAPOT, lock binds M-mode | cva6 checks 40-44 |
| CVA6-Config #59 | Reset PC 0x0001_0000 | cva6 image entry (test runs) |
| Interrupt Mapping (CVA6) | time_irq_i cause 7, irq_i[0] cause 11, vectored mtvec | cva6 checks 30-32 |
| Booting #22, Q01-b | Ibex first fetch 0x80, mtvec 0x1, vectored only | ibex checks 1, 2, 12 |
| Ibex-Config #1, #13-#15, Q01-e | RV32IMC + Zcb, U-mode, no A/F/D | ibex checks 10, 13, 20-28 |
| Ibex-Config #2-#4, #9, Q01-d | 16 PMP regions, Smepmp, mseccfg RLB/MMWP reset | ibex checks 3, 4, 40-43, 60-67 |
| Interrupt Mapping (Ibex) | fast causes 16-30, priority fast[0..14] > MEI | ibex checks 30-32 |
| Q01-c | Hart IDs 0 (CVA6) and 1 (Ibex) | cva6 check 2, ibex check 11 |
| CVA6-Config #44-#46, Ibex-Config #33-#36, Q04-b | Debug Mode entry/exit, HaltAddress/ExceptionAddress, debug partition only in Debug Mode | both: checks 78-90, 95-99; `fu3_cpu_debug_*` |
| Ibex-Config #33-#36 | Debug Mode accesses to the DM aperture bypass PMP | `fu3_cpu_debug_ibex` |
| CVA6-Config #47, Q04-e | 2 triggers (CVA6), 4 (Ibex), mcontrol exact execute | both: checks 82-84, 91-94 |
| Q04-b/c | Debug control for the DM (halt, resume, step, abstract register access, reset) | `fu3_cpu_debug_{cva6,ibex}` |
| Q01-g | Core clock from the CRM, changeable, stop; latency counted once | `fu3_cpu_clock`, `fu3_soc_clock_reset` |
| Q02-b, Booting | Ibex released by the CRM, CVA6 by FCLK_SW after staging closes | `fu3_soc_clock_reset`, `fu3_soc_boot` |
| Q04-c | One DebugHart per CPU, hartsel 0 = CVA6, 1 = Ibex (`fu3_soc_top::debug_harts()`) | `fu3_soc_cpu_*` (debug request through that DebugHart) |
| Ibex U-mode | ebreaku, haltreq from U, DRET to U | ibex checks 100-102 |
