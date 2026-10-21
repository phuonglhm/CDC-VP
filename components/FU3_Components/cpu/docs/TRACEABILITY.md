# FU3 CPU configuration traceability

Source: private FU3 HAS v2.0 and FTEL_FU3_Common_Spec v2.0, reviewed 2026-10-20. CPU0 is CVA6 and CPU1 is Ibex. Verification ran with an out-of-repository harness (CVA6 + Ibex + FU3 bus + four memories) and the `riscv_cpu_eval` platform regression.

| Source | Requirement | Verification |
| --- | --- | --- |
| Table 8-1 | CVA6 RV32IMC, M-mode only | Harness reads `misa` = I, M, C |
| Table 8-3 | Ibex RV32IMC with U-mode | Harness reads `misa` = I, M, C, U |
| Table 11-1/11-2 | CPU0 (CVA6) is hart 0 | Harness reads `mhartid` = 0 |
| Leader decision | CPU1 (Ibex) is hart 1 | Harness reads `mhartid` = 1 |
| Common CVA6-Config #45-#46, Ibex-Config #33-#36 | Debug addresses | Constants compiled into the harness, values reviewed against the sheets |
| Section 9 | Ibex resets to ROM 0x0000_0000 | Ibex executes the harness boot ROM |
| Section 9, 14.2 | CVA6 held in reset, starts at 0x0001_0000 after staging | Released by the harness boot controller after `finish_staging()` and runs the staged image |
| Section 9 | Ibex writes CVA6_ISRAM only while staging | Write before close succeeds and write after close traps with mcause 7 |
| Section 9 | CVA6 denied ROM | CVA6 ROM load traps with mcause 5 |
| Section 9 | DSRAM shared and non-executable | CVA6 fetch from DSRAM traps with mcause 1 |
| Table 7-1, bus contract | One AXI4 master per CPU, `Access` on every request | Both CPUs run on the actual FU3 bus |
| Bus contract | Word fetch and RVC at 2-byte-aligned PC | 16-bit at aligned PC, 32-bit at PC%4==2 crossing a word, 16-bit at PC%4==2 |
| Bus contract | Narrow stores use lane enables | sb/sh/sb to DSRAM reads back 0x22334411 |
| Table 11-2 | CVA6 cause 7 (timer) | riscv_cpu_eval `timer_irq` firmware |
| Table 11-2 | CVA6 causes 7 and 11 on FU3 peers | Harness outside the repository: TIMER0 -> cause 7 (`fw/timer_irq_riscv`, PLATFORM=fu3), TIMER1 -> PLIC ID 6 -> cause 11 (`fw/fu3_plic_irq`) and cause 3 not exercised (SYSCSR v1 has no software IRQ) |

Not covered: the open ISS features in [MODEL_CONTRACT.md](MODEL_CONTRACT.md) and real PLIC/INTMAP, CRM and debug peers.
