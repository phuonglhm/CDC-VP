# FU3_Components

- [AES-256](AES-256/README.md): AES-128/AES-256 single-block SystemC/TLM,
  INIT/NEXT polling, functional byte-addressed CSR profile and FU3 bus test.
- [QSPI](QSPI/README.md): AP_QSPIC register interface, QSPI controller,
  A2H_QSPI functional adapter and XIP, with SystemC/TLM spec regression.
- [GPIO_IO](GPIO_IO/README.md): GPIOA/B/C SystemC/TLM, per-bank power events,
  digital IO_PAD function selection and regression tests. IOSEL/WAKE system
  registers await their additional specification.
- [cpu](cpu/README.md): FU3 configuration of CPU0 (CVA6) and CPU1 (Ibex) on the cva6_vp wrapper (memory map, reset PCs, hart IDs, ISA, interrupt causes, debug addresses) per HAS v2.0 and Common Spec v2.0.
- [syscsr](syscsr/README.md): SYSCSR register model per HAS v2.0, Common Spec v2.0 and FU3_SYSCSR_v1 (boot straps captured at reset release), with FU3 bus tests.
- [plic](plic/README.md): AP_PLIC (pulp rv_plic: 23 level sources, 1 target, priorities 1-7, claim/complete) per Common Spec v2.0 and the CPU1 (Ibex) INTMAP per HAS v2.0 section 10.2, with FU3 bus tests.
- [mailbox](mailbox/README.md): MAILBOX_CVA6/MAILBOX_IBEX (port 0 = CPU0, port 1 = CPU1, two 32-word FIFOs, thresholds, W1C IRQs, per-port reset) per HAS v2.0 and Mailbox HDS V1, with FU3 bus tests.
- [crm](crm/README.md): CRM per HAS v2.0 and FU3 CRM Specification V1.1 (registers, PLL and clock selection, per-IP gates and resets, Ibex/CVA6 reset control, startup and HAS 6.5 system resets), with FU3 bus tests.
- [dbg](dbg/README.md): DBG phase 1 (JTAG DTM, RISC-V Debug 0.13.2 Debug Module for two harts, 32-bit system bus access on the DBG port) per HAS v2.0, with FU3 bus tests.
