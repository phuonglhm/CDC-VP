# FU3_Components

- [AES-256](AES-256/README.md): AES-128/AES-256 single-block SystemC/TLM,
  INIT/NEXT polling, functional byte-addressed CSR profile and FU3 bus test.
- [QSPI](QSPI/README.md): AP_QSPIC register interface, QSPI controller,
  A2H_QSPI functional adapter and XIP, with SystemC/TLM spec regression.
- [GPIO](GPIO/README.md): 32-bit GPIO core with TLM and signal-level APB, interrupts, pad
  configuration and raw power events per GPIO HDS V1.0; includes three-bank
  integration with IO_PAD, per-bank/aggregate wake and integration tests.
- [IO_PAD](IO_PAD/README.md): standalone digital IOSEL/package routing and test
  mode per IO_PAD HDS V1.0; no internal clock, reset or register bank.
- [cpu](cpu/README.md): FU3 configuration of CPU0 (CVA6) and CPU1 (Ibex) on the cva6_vp wrapper (memory map, reset PCs, hart IDs, ISA, interrupt causes, debug addresses) per HAS v2.0 and Common Spec v2.0.
- [syscsr](syscsr/README.md): SYSCSR register model per HAS v2.0, Common Spec v2.0 and FU3_SYSCSR_v1 (boot straps captured at reset release), with FU3 bus tests.
- [plic](plic/README.md): AP_PLIC (pulp rv_plic: 23 level sources, 1 target, priorities 1-7, claim/complete) per Common Spec v2.0 and the CPU1 (Ibex) INTMAP per HAS v2.0 section 10.2, with FU3 bus tests.
- [mailbox](mailbox/README.md): MAILBOX_CVA6/MAILBOX_IBEX (port 0 = CPU0, port 1 = CPU1, two 32-word FIFOs, thresholds, W1C IRQs, per-port reset) per HAS v2.0 and Mailbox HDS V1, with FU3 bus tests.
- [crm](crm/README.md): CRM per HAS v2.0 and FU3 CRM Specification V1.1 (registers, PLL and clock selection, per-IP gates and resets, Ibex/CVA6 reset control, startup and HAS 6.5 system resets), with FU3 bus tests.
- [dbg](dbg/README.md): DBG per HAS v2.0 and Common Spec v2.0 (JTAG2AXI4 bridge with CMD_IDCODE/ADDR/DATA/STATUS/BYPASS on the DBG port, RISC-V Debug 0.13.2 Debug Module for CPU0/CPU1 at 0x1A11_0000), with FU3 bus tests.
