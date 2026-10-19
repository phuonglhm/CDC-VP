# FU3_Components

- [AES-256](AES-256/README.md): AES-128/AES-256 single-block SystemC/TLM,
  INIT/NEXT polling, functional byte-addressed CSR profile and FU3 bus test.
- [QSPI](QSPI/README.md): AP_QSPIC register interface, QSPI controller,
  A2H_QSPI functional adapter and XIP, with SystemC/TLM spec regression.
- [GPIO_IO](GPIO_IO/README.md): GPIOA/B/C SystemC/TLM, per-bank power events,
  digital IO_PAD function selection and regression tests. IOSEL/WAKE system
  registers await their additional specification.
- [cpu](cpu/README.md): FU3 configuration of CVA6 and Ibex on the cva6_vp wrapper (memory map, reset PCs, hart IDs, ISA, interrupt causes).
- [syscsr](syscsr/README.md): SYSCSR register model per FU3_SYSCSR_v1 with FU3 bus tests.
- [plic](plic/README.md): AP_PLIC (32 sources, 1 target, level/edge, claim/complete) per PLIC HDS V1.0 with FU3 bus tests.
