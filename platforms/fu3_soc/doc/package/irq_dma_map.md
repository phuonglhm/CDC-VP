# VP_FU3_V1.0 — interrupt and DMA map (R1)

Source: Common Spec v2.1 *Interrupt Mapping*, *PLIC Configuration/Connection*
and *DMA Mapping* sheets, as integrated (`fu3_peripherals`, `fu3_plic`) and
checked by `fu3_soc_topology`, `fu3_soc_peripherals` and `fu3_soc_irq_reset`.

## CPU interrupt inputs

| CPU | Input | mcause | Source |
| --- | --- | --- | --- |
| CVA6 | `irq_i[0]` (MEIP) | 11 | PLIC target 0 (IDs 1-31 below) |
| CVA6 | `time_irq_i` (MTIP) | 7 | TIMER0 (through the pending latch, acknowledged by a CMP write) |
| CVA6 | `ipi_i` (MSIP) | 3 | SYSCSR SWIRQ bit 0 (FU3_VP_ABI_1 §3) |
| Ibex | `irq_fast_i[0..14]` | 16-30 | INTMAP fast groups (below) |
| Ibex | `irq_external_i` (MEIP) | 11 | INTMAP: TIMER2, TIMER3, WDT1, MAILBOX_IBEX |
| Ibex | `irq_software_i` (MSIP) | 3 | INTMAP: SYSCSR SWIRQ bit 1 |
| Ibex | `irq_timer_i` (MTIP) | 7 | INTMAP: TIMER1 |
| Ibex | `irq_nm_i` | 31 | no source (tied low) |

Ibex priority: NMI > fast[0..14] > MEI > MSI > MTI. Every INTMAP route is
disabled at reset; firmware enables it in SYSCSR `INTMAP_EN0/EN1`
(FU3_VP_ABI_1 §3, bit = INTMAP source index).

## PLIC sources (CVA6) and INTMAP routes (Ibex)

| PLIC ID | Source | INTMAP source index -> Ibex input | R1 status |
| --- | --- | --- | --- |
| 1 | CRM | 1 -> fast[1] | integrated |
| 2 | UART0 | 8 -> fast[8] | integrated |
| 3 | AES-256 | 4 -> fast[4] | **tied low**: the model has no interrupt (Q03-f) |
| 4 | QSPI (AP_QSPIC) | 7 -> fast[7] | integrated |
| 5 | DMA0 | 6 -> fast[6] | integrated |
| 6 | TIMER1 | 33 -> irq_timer_i | integrated |
| 7 | TIMER2 | 28 -> irq_external_i | integrated |
| 8 | TIMER3 | 29 -> irq_external_i | integrated |
| 9 | SPI0 | 16 -> fast[9] | integrated |
| 10 | UART1 | 9 -> fast[8] | integrated |
| 11 | UART2 | 10 -> fast[8] | integrated |
| 12 | GPIOA | 21 -> fast[11] | integrated |
| 13 | GPIOB | 22 -> fast[11] | integrated |
| 14 | GPIOC | 23 -> fast[11] | integrated |
| 15 | I2C | 24 -> fast[12] | integrated |
| 16 | SPI1 | 17 -> fast[9] | integrated |
| 17 | ADC | 25 -> fast[13] | **tied low**: the model has no interrupt (Q03-f) |
| 18 | WDT1 warning | 30 -> irq_external_i | integrated |
| 19 | GPIO0_WAKE | 2 -> fast[2] | provisional: raw OR of the three banks' power events, no latch/clear (Q06-c) |
| 20 | EDHOC/OSCORE | 3 -> fast[3] | **R2, not received** (tied low) |
| 21 | RNG | 5 -> fast[5] | integrated |
| 22 | SPI2 | 18 -> fast[9] | integrated |
| 23 | SPI3 | 19 -> fast[9] | integrated |
| 24-28 | UART3-UART7 | 11-15 -> fast[8] | integrated |
| 29, 30 | PWM0, PWM1 | 26, 27 -> fast[14] | **R2, not received** (tied low) |
| 31 | CAN | 20 -> fast[10] | **R2, not received** (tied low) |
| — | WDT0 warning | 0 -> fast[0] | Ibex only |
| — | MAILBOX_IBEX | 31 -> irq_external_i | Ibex only (Q03-g) |
| — | SYSCSR SWIRQ (Ibex) | 32 -> irq_software_i | Ibex only |
| — | MAILBOX_CVA6 | — | **unconnected**: no PLIC ID in Common v2.1 (Q03-g) |
| — | HyperRAM controller | — | **unconnected**: no PLIC ID in Common v2.1 |

Pulse sources: the timers raise a one-tick pulse; the platform latches it
until software writes the timer's CMP register (decision Q03-d). PLIC gateway:
a source captured as pending stays pending until claimed, even if the source
itself is reset meanwhile; a pulse that ends while the PLIC clock is gated is
lost, a level still high when the clock returns is captured.

## Watchdog reset

WDT0 and WDT1 reset requests are ORed into the CRM watchdog input: a watchdog
reset of the system and DBG domains (`clock_reset.md`).

## DMA0 channels (Common v2.1 DMA Mapping, DMA ID 0)

| Channel | Request | Endpoint data register |
| --- | --- | --- |
| 0 | SPI0 receive | SPI0 RXDR |
| 1 | SPI0 transmit | SPI0 TXDR |
| 2 | UART1 receive | UART1 RBR |
| 3 | UART1 transmit | UART1 THR |
| 4 | UART2 receive | UART2 RBR |
| 5 | UART2 transmit | UART2 THR |
| 6 | SPI1 receive | SPI1 RXDR |
| 7 | SPI1 transmit | SPI1 TXDR |

A channel selects its peripheral through `CH_PERIPHERAL_CONFIG` (channel
offset 0x20): `READ_ID` [4:0] for a receive channel (the endpoint is read),
`WRITE_ID` [20:16] for a transmit channel (the endpoint is written); VP IDs
SPI0 = 1, UART1 = 2, UART2 = 3, SPI1 = 4 (AP_DMA `registers.md`). SPI2/SPI3 and QSPI have no DMA channel (their request outputs
are unconnected). DMA0 access profile (FU3_VP_ABI_1 §7): data memories, QSPI
XIP read, and the data registers of its endpoints in their direction only;
anything else ends the channel with a slave error.
