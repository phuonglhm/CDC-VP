# FU3 CPU configuration contract

## Sources and scope

FU3 HAS v2.0: Table 8-1 (CVA6 configuration), Table 8-3 (Ibex configuration), Table 10-1 (memory map), Table 11-2 (CVA6 interrupt inputs), Section 10.2 (Ibex interrupts) and Section 14.2 (boot sequence).
The source documents are private and are not included in this package.

Both CPUs are the Bremen RV32 ISS behind `cdc::cpu::cva6_wrapper`. The VP models instruction-set behavior, traps, interrupts and bus traffic. It does not model pipelines, caches, branch prediction, lockstep or timing of either core.

## CVA6

| Item | Value | Source |
| --- | --- | --- |
| Reset PC | 0x0001_0000 (CVA6_ISRAM) | Section 9, 14.2: Ibex sets boot_addr_i |
| Reset | Held after construction and released by the boot controller | Section 14.2 |
| Hart ID | 0 (PLIC target 0) | Table 11-1/11-2 |
| ISA | RV32IMC, M-mode only (`misa` without A, F, D, S, U) | Table 8-1 |
| MMU | None | Table 8-1 MmuPresent = 0 |
| Interrupts | cause 3 `ipi_i` (SYSCSR), 7 `time_irq_i` (TIMER0), 11 `irq_i[0]` (PLIC target 0) | Table 11-2 |
| Memory | CVA6_ISRAM 0x0001_0000 64 KiB (top 4 KiB debug code), DSRAM 0x0100_0000 64 KiB | Table 10-1 |

## Ibex

| Item | Value | Source |
| --- | --- | --- |
| Reset PC | 0x0000_0000 (boot ROM, Ibex only) | Section 9 |
| Reset | Runs from simulation start | Section 14.2 |
| Hart ID | 1 | Not specified: VP choice, pending mentor decision |
| ISA | RV32IMC with M and U modes (`misa` without A, F, N, S) | Table 8-3: RV32I, RV32MFast, Zca, RV32BNone |
| Memory | ROM 0x0000_0000 4 KiB, IBEX_ISRAM 0x0002_0000 64 KiB (top 4 KiB debug code) | Table 10-1 |

## Bus behavior (both CPUs)

- One initiator socket per CPU (`has_unified_bus() = true`), matching one AXI4 master per CPU on the FU3 bus.
- Every fetch/data transaction carries `fu3_bus::Access` (instruction flag, beat size).
- Fetches are aligned 4-byte words. A 2-byte-aligned PC fetches the word below and, only for a 32-bit instruction, the next word and a 16-bit instruction never fetches past its own word.
- Bus errors become access-fault traps: mcause 1 (fetch), 5 (load), 7 (store).

## Not provided by the ISS (open)

| Feature | Spec | Effect today |
| --- | --- | --- |
| Bitmanip (CVA6) | Table 8-1 RVB = 1 | Zb* instructions are illegal |
| Zcb (Ibex) | Table 8-3 RV32ZcaZcb | Zcb instructions are illegal |
| Ibex `irq_fast_i[14:0]` (cause 16-30) | Section 10.2 | Not deliverable: WDT0 is NMI, MAILBOX_IBEX, CRM, AES, DMA0, UART0, SPI, GPIO, TIMER2/3 use fast IRQs |
| Ibex `irq_nm_i` (NMI from WDT0) | Section 10.2 | Not deliverable |
| Ibex vectored-only `mtvec` | Ibex core | ISS supports vectored mode (base + 4*cause) and it also accepts direct mode, which Ibex hardwires off |
| PMP/ePMP, 16 regions, MMWP/RLB | Table 8-3 | CSRs exist, accesses are not checked and the bus enforces ROM, staging and DSRAM policy |
| Debug (HaltAddress, ExceptionAddress, debug_req_i) | Tables 8-1, 8-3, 11-2 | Not modeled (task 9) |
| Clock frequency | CRM/PLL | Wrapper runs 1000 instructions per 10 us |

Patching `third_party/riscv-vp` for the Ibex interrupt items is pending a mentor decision.
