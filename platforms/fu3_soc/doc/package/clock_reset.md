# VP_FU3_V1.0 — clock and reset contract (R1)

Sources: CRM Spec V1.1, CRM CSR v2, SYSCSR v1, HAS v2.1, Common Spec v2.1 and
the decisions in FU3_VP_ABI_1 §4. Checked by `fu3_soc_clock_reset`,
`fu3_soc_boot`, `fu3_soc_reset_in_flight`, `fu3_soc_irq_reset`.

## Power-on and boot

| Step | Time / condition |
| --- | --- |
| PRSTN asserted at power-on | 100 ns |
| CRM reset filter after PRSTN release | 100 ns (CRM spec 3.2) |
| Power-readiness qualification | 255 OSC cycles = 12.75 us (spec 3.1.1) |
| System domain released | at the end of the startup (spec 4.1) |
| Ibex released | 1.2 us later: Ibex starts 14.05 us after PRSTN rises, at `0x0000_0080` |
| CVA6 released | when software writes CRM `FCLK_SW[0] = 1`; the boot staging window closes first (accesses already issued by Ibex complete, later ones see the final policy) |

Straps (die pads 2 BOOT, 4 MODE_0, 3 MODE_1, 5 TESTMODE, 13 TEST_MODE_SEL) are
captured into FU3_VP_ABI_1 `BOOT_STATUS` at each release of the SYSCSR reset;
SYSCSR `STATUS` (0x70) reads them live. DEBUG_EN (die 6) is read live in
`BOOT_STATUS[8]`.

## Clocks

| Clock | After reset | VP PLL profile |
| --- | --- | --- |
| CPU (FCLK, both CPUs) | OSC 20 MHz | 400 MHz (PLL 20 MHz x 20 / 1, `CLK_0 = CLK_1 = 1`) |
| Bus (system and peripheral buses, timers, UART, SPI, I2C, GPIO, QSPI, AES, RNG, DMA peripheral cycle) | OSC 20 MHz / 2 x `CLK.div` path: 50 ns per bus cycle | 200 MHz (5 ns) |

- PLL lock: 404 OSC cycles (20.2 us) after power-up (CSR v2 lock counter).
- Change the bus divider only while both selectors are on the OSC (CRM spec
  3.7.4); the VP reports any other sequence.
- **RTL difference:** the integrated CRM wrapper has no analog PLL
  (FU3_VP_ABI_1 §9 #15): firmware needs an OSC fallback.
- HyperRAM requires the 200 MHz bus clock (tCSM) before its controller is
  initialised.

## Resets

| Reset | Source | Scope |
| --- | --- | --- |
| POR | PRSTN | everything, CRM registers included; RESET_CAUSE cleared to POR |
| Global | SYSCSR `sw_global_rst_req` | system + DBG domains, CRM registers |
| Software | SYSCSR `sw_reset_req` | system + DBG domains; CRM registers kept |
| Watchdog | WDT0/WDT1 reset request | system + DBG domains; CRM registers kept |
| Debug (ndmreset) | Debug Module | system domain only: DBG (DM, JTAG2AXI) kept (Q05-e) |
| CVA6 local | `FCLK_SW[0] = 0` | CVA6 only; staging not reopened |
| Per block | CRM `SW_RST_N_0/1` bit | that block |

Every system reset starts a new boot epoch: the bus drains and reopens boot
staging, Ibex boots again from ROM, CVA6 is held (`FCLK_SW` cleared). SRAM
content is kept. The cause is recorded in FU3_VP_ABI_1 `RESET_CAUSE`
(cumulative bitmap, write 1 to clear, only POR clears its state).

The CRM keeps its registers across a software or watchdog reset: a CRM
interrupt still asserted (`CRM.int`) is pending at the PLIC after the reset
(enables are back at 0); firmware clears it before enabling interrupts.

## Clock gates and block resets

`CLK_GATE_0/1` and `SW_RST_N_0/1` apply per bus target (the VP prints each
target's bits with `vp_fu3_soc --list-instances`; UART3-7 use bits 25-29, a VP
extension not in CRM CSR v2):

- A gated block stops (state kept) and resumes when its clock returns.
- An access to a gated block or one held by its `SW_RST_N` bit answers a bus
  error (`TLM_GENERIC_ERROR_RESPONSE`, decision Q02-i); the VP warns, this is a
  software error.
- An access reaching a block while its domain (system or DBG) is in reset was
  issued before the reset: aborted with a bus error, counted, not warned.
- Reset assertion is immediate; release is synchronized to the block clock: a
  release while the block is gated waits for its clock, then two bus clock
  periods.
- The CRM itself is never gated.

## Not modelled

Reset synchronizer latency while the destination clock runs; a minimum reset
width; a general hold of reset release until the bus has drained (accesses
caught by a reset end before the release for every model in R1, as tested,
because the models abort them).
