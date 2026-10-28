# VP_FU3_V1.0 — CSR reference

The register definitions the VP models implement, as C/C++ headers copied
from the CDC-VP model sources at this package's revision. Base addresses: use
`../memory_map.md` (the integrated map is authoritative; a base macro in a
header is the model's default). Offsets, fields and reset values: the headers
and the model documents named below. FU3_VP_ABI_1 (`../FU3_VP_ABI_1.md`,
`../include/fu3_vp_abi.h`) adds the SYSCSR extension and the access profile.

| IP (bus targets) | Header | Notes |
| --- | --- | --- |
| CRM | `fu3_crm/registers.h` | CRM CSR v2; UART3-7 bits 25-29 are the VP extension (ABI_1 §9) |
| SYSCSR | `fu3_syscsr/registers.h` + `fu3_vp_abi.h` | SYSCSR v1 (0x00-0xFF) and FU3_VP_ABI_1 (0x100-0x123) |
| AP_PLIC + INTMAP | `fu3_plic/registers.h` | PLIC context 0 = CVA6; INTMAP enables live in SYSCSR (ABI_1) |
| TIMER0-3 | `fu3_timer/registers.h` | interrupt acknowledged by a CMP write (Q03-d) |
| WDT0/1 | `fu3_wdt/registers.h` | WDT HDS V1.2, lock key `0x1ACCE551` |
| UART0-7 | `fu3_uart/registers.h` | 16550-style byte registers |
| SPI0-3 | `fu3_spi/registers.h` | |
| I2C | `fu3_i2c/registers.h` | no FIFO/DMA/10-bit/general call (r1_gaps) |
| GPIOA/B/C | `fu3/gpio/registers.h` | |
| AP_QSPIC + QSPI_XIP | `fu3/qspi/registers.h` | memory-mapped mode for XIP (`CCR` mode 3) |
| AP_HBCTRL + HYPERBUS | `fu3_hyperram/registers.h` | provisional VP ABI v1 (Q07-a); initialise before any HYPERBUS access |
| AES | `fu3/aes/registers.h` | no interrupt (Q03-f) |
| ADC | `fu3_adc_registers.md` | offsets of `fu3::adc::reg` (the model header is not a register header) |
| RNG | `fu3_rng/registers.h` | |
| MAILBOX_CVA6/IBEX | `fu3_mailbox/registers.h` | |
| AP_DMA | `fu3_ap_dma_registers.md` | channel `n` at `0x100 * n`, globals at `0x1000` |
| IBEX_DBG (DM) + JTAG2AXI | `fu3_dbg/registers.h` | Debug Spec 0.13.2 DMI registers, VP JTAG IR/DR profile |
