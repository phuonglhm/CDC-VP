# VP_FU3_V1.0 — what R1 does not cover

The VP models the R1 IP set as the specs and the recorded decisions define it.
The items below are open: a spec or RTL decision is missing, the IP has not
been received, or the VP deliberately abstracts. Each has an owner in
FU3_VP_ABI_1 §9/§10; firmware written against the VP must not rely on the VP
behaviour of an open item.

## IP not received (R2)

| IP | Map | Interrupt | Plan |
| --- | --- | --- | --- |
| EDHOC/OSCORE | `0x3101_0000` window disabled | PLIC ID 20 / Ibex fast[3] tied low | T24/T25 |
| PWM0, PWM1 | windows disabled | PLIC IDs 29/30 / Ibex fast[14] tied low | T24/T26 |
| CAN | window disabled | PLIC ID 31 / Ibex fast[10] tied low | T24/T27 |

## Features of R1 IP still open

| Item | VP state | Decision |
| --- | --- | --- |
| AES-256 and ADC interrupts | the models have no interrupt output; PLIC IDs 3/17 tied low | Q03-f: assert/clear rule needed |
| GPIO0_WAKE | raw OR of the banks' power events, no latch/clear | Q06-c |
| Timer input capture (HAS: at least 5 channels from GPIO) | not modelled: no CSR/status defined | Q06-f |
| I2C FIFO, DMA, 10-bit addressing, general call | not in the model | Q07-d |
| Pad select from SYSCSR `csr_io_cfg` | die pad <-> field mapping missing; the VP sets pad selects host side (`--pad`) | Q06-b |
| MAILBOX_CVA6 and HyperRAM controller interrupts | outputs unconnected: no PLIC ID in Common v2.1 | Q03-g / spec |
| QSPI, SPI2, SPI3 DMA requests | unconnected: no DMA0 channel in Common v2.1 | spec |
| CVA6 RVB | required by the spec, subset undecided: not offered | Q01-a |

## VP extensions not (yet) in RTL (FU3_VP_ABI_1 §9)

SYSCSR `0x100-0x123` (ABI version, BOOT_STATUS with DEBUG_EN, RESET_CAUSE,
INTMAP enables, SWIRQ); CRM gate/reset bits 25-29 for UART3-7; VP PLL (the CRM
wrapper has no analog PLL); watchdog/software/debug reset requests acted on by
the CRM (no CRM ROM handler in RTL); DBG reset independent of the CVA6 hold
(CRM 3.1.2: follows the FCLK path); Ibex PMP reset/final encodings (proposal,
not RTL confirmed); debug-partition blob (VP placeholder; the real blob is the
DBG/DM team's).

## Agreements still needed from FW/SW (FU3_VP_ABI_1 §10)

UART boot frame parity ("8P1" names no parity; proposal 8E1) and baud;
image format and UART download protocol; boot failure behaviour; ROM layout.

## Simulation profile

- TLM loosely timed with a global quantum (1 us default): instruction accurate,
  not cycle accurate; an input change reaches a CPU at most one quantum late.
- Debug: abstract commands only (no program buffer); the JTAG side is the VP's
  IR/DR profile, not the standard DTM; no external JTAG port, no OpenOCD.
- GDB: execute breakpoints only (no watchpoints); the whole SoC stands still
  while every attached hart is stopped; a stop is reported up to 10 us of
  simulated time later when another attached hart runs.
- Images are preloaded test images (`--rom/--elf/--load/--flash`), not a boot
  from external media.
- No general hold of a block's reset release until the bus has drained: R1
  models end an access caught by a reset before the release (tested for every
  gated target).
- Not modelled: reset synchronizer latency while the clock runs, minimum
  reset width.

## Behaviour firmware must expect

- After a software or watchdog reset the CRM keeps its registers: a CRM
  interrupt still asserted is pending at the PLIC.
- The PLIC keeps a pending bit captured before its source was reset: a claim
  can return an ID whose device no longer has a cause.
- Console bytes sent before the UART is initialised are lost (no flow control).
- An access to a gated block, or one held by its `SW_RST_N` bit, is a bus error.

## Outside FU3

`isp_register_bank_test` (ISP, not FU3) fails to link in the CDC-VP tree since
the baseline; it is not part of this package.
