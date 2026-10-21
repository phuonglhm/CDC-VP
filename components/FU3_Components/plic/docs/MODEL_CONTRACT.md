# FU3 AP_PLIC functional contract

## Sources and scope

Specification priority for FU3 (leader decision, 2026-10-20): FU3 HAS v2.0 and FTEL_FU3_Common_Spec v2.0 first, the IP HDS only where both are silent.
Common Spec v2.0 sheet "PLIC Connections" defines the PLIC: pulp `rv_plic` `plic_top` with N_SOURCE = 23, N_TARGET = 1, MAX_PRIO = 7, level gateways (`le_i` = 0), the register layout, the APB access contract and the required threshold/completion fixes.
Common Spec v2.0 "Interrupt Mapping" and HAS v2.0 Table 11-1 give the Source IDs, Table 11-2 routes target 0 to CPU0 (CVA6) `irq_i[0]`, and both memory maps place AP_PLIC at 0x4010_0000 on PERIBUS_1.
HAS v2.0 section 10.2 defines the CPU1 (Ibex) INTMAP (see [INTMAP (Ibex)](#intmap-ibex)).
The PLIC HDS v1.1 (`ftel_plic`) describes a different register layout and is not used.
The source documents are private and are not included in this package.

## Bus interface

- APB4 through AP_PLIC, local offsets from the bus and the window is 64 KiB.
- Only aligned 32-bit accesses with all byte enables (Common v2.0 "APB access contract"). Other lengths return `TLM_BURST_ERROR_RESPONSE`, misaligned or out-of-window addresses return `TLM_ADDRESS_ERROR_RESPONSE`.
- Unmapped offsets and writes with a partial PSTRB return `TLM_GENERIC_ERROR_RESPONSE` (PSLVERR) and change nothing.
- `transport_dbg` reads have no side effect: reading CLAIM returns the ID a claim would return without claiming.

## Registers

| Offset | Register | Access | Behavior |
| --- | --- | --- | --- |
| 0x0000 | Source 0 priority | RO | Reads 0, writes ignored |
| 0x0004-0x005C | Priority ID 1-23 (`4 * ID`) | RW [2:0] | 0 disables the source, 1-7 |
| 0x1000 | Pending | RO | Bit n = Source ID n, bit 0 = 0. Writes ignored |
| 0x2000 | Enable (context 0) | RW | Bit n enables Source ID n, writable mask 0x00FF_FFFE |
| 0x3000 | Threshold (context 0) | RW [2:0] | An interrupt needs priority > threshold. **Offset is a VP choice** |
| 0x3004 | Claim/complete (context 0) | R/C, W/C | Read claims the best ID (0 = none), writing an ID completes it. **Offset is a VP choice** |

All RW registers reset to 0.

## Interrupt behavior

- Element k of `irq_sources` is Source ID k+1 (`irq_sources_i[n-1]` -> ID n).
- Level gateway: a high input sets the pending bit while the source is not in service. The bit stays set until claimed, even if the input falls first. FU3 timers raise their IRQ for one tick only, so latching is required. One outstanding event per source.
- Claim (functional read of CLAIM): selects the pending, enabled source whose priority is strictly greater than the threshold and ties go to the lowest ID. The comparison is explicit, so threshold 7 masks every source (Common v2.0 "Threshold corner case"). Claim clears the pending bit and puts the source in service.
- Complete (write of CLAIM): releases the written ID if it is 1-23 and in service. Other values, including 0 and 24-31, are ignored (Common v2.0 "Completion validation"). A source whose input is still high pends again at once.
- Priority 0 never interrupts.
- `irq` (`eip_targets_o[0]`) is a level: high while a claimable source exists.
- `reset_n` low clears all registers and in-service state, deasserts `irq`, and ignores functional writes. The model is untimed: gateways react to input events, not to a sampling clock.

## Open questions

| Topic | VP choice |
| --- | --- |
| Threshold and claim/complete offsets | Common v2.0 and HAS v2.0: "TBD (compact-map offset)". VP uses 0x3000 and 0x3004 until RTL fixes the compact map. HDS v1.1 offsets 0x20/0x24 collide with priority ID 8/9 in this layout |
| Write to the RO pending register | Ignored with an OK response |
| Edge sources | Not supported: FU3 uses `le_i` = 0 for every source |
| Source synchronization (IRQ_ADAPT) | Not modeled: Common v2.0 requires it outside the PLIC |

## INTMAP (Ibex)

`fu3_plic::Intmap` (`intmap.h`) is the stateless CPU1 (Ibex) interrupt map from FU3 HAS v2.0 section 10.2. Each output is the OR of its sources and all nets are active-high levels and nothing is inverted or latched.

| Ibex input (`irq[k]`) | Cause | Sources |
| --- | --- | --- |
| irq_nm_i (`kOutNmi`) | 31 | WDT0 |
| irq_fast_i[14] | 30 | MAILBOX_IBEX |
| irq_fast_i[13] | 29 | CRM |
| irq_fast_i[12] | 28 | AES |
| irq_fast_i[11] | 27 | EDHOC OSCORE |
| irq_fast_i[10] | 26 | RNG |
| irq_fast_i[9] | 25 | DMA0 |
| irq_fast_i[8] | 24 | QSPI |
| irq_fast_i[7] | 23 | UART0 |
| irq_fast_i[6] | 22 | SPI0 |
| irq_fast_i[5] | 21 | GPIO0_WAKE |
| irq_fast_i[4] | 20 | UART1, UART2 |
| irq_fast_i[3] | 19 | SPI1, SPI2, SPI3 |
| irq_fast_i[2] | 18 | I2C0 |
| irq_fast_i[1] | 17 | GPIOA, GPIOB, GPIOC |
| irq_fast_i[0] | 16 | TIMER2, TIMER3 |
| irq_external_i (`kOutExternal`) | 11 | ADC, AP_HBCTRL |
| irq_software_i (`kOutSoftware`) | 3 | SYSCSR software IRQ (Ibex bit) |
| irq_timer_i (`kOutTimer`) | 7 | TIMER1 |

`kIbexCause[k]` gives the cause of output k, so the platform can forward each output with `ibex.set_irq(kIbexCause[k], level)`.
The causes 16-31 need the pending Ibex Fast-IRQ/NMI support in the ISS. `debug_req_i` (DBG) is a direct wire, not part of this model.

| INTMAP topic | VP choice |
| --- | --- |
| AP_HBCTRL IRQ | HAS lists it on irq_external_i, but the PLIC map and the block list give it no source and kept as in section 10.2 |
| WDT0 on irq_nm_i | Section 10.2 maps it and section 10 intro still says WDT0 routing is TBD. Kept as in section 10.2 |
| TIMER1 fan-out | TIMER1 drives both PLIC ID 6 (CVA6) and Ibex irq_timer_i, as both tables say |
| Common Spec v2.0 | INTMAP and the WDT0 route are TBD there. HAS v2.0 section 10.2 is kept (leader decision, 2026-10-20) |
