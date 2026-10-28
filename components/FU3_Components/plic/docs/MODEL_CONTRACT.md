# FU3 AP_PLIC functional contract

## Sources and scope

Specification priority for FU3: FU3 HAS v2.1 and FTEL_FU3_Common_Spec v2.1 first (platform plan T02 decisions), the IP HDS only where both are silent.
Common Spec v2.1 "PLIC Configuration" defines the PLIC: pulp `rv_plic` `plic_top` with 32 slots (ID 0 reserved, sources 1-31), N_TARGET = 1, MAX_PRIO = 31, level gateways (`le_i` = 0), the register layout and the required threshold/completion fixes.
Common Spec v2.1 "Interrupt Mapping" gives the Source IDs (IDs 24-31 are a provisional allocation, decision Q03-h: UART3-7, PWM0/1, CAN) and routes target 0 to CPU0 (CVA6) `irq_i[0]`; the memory map places AP_PLIC at 0x4010_0000 on PERIBUS_1.
The same sheet proposes the CPU1 (Ibex) allocation used by the INTMAP (see [INTMAP (Ibex)](#intmap-ibex)).
The PLIC HDS v1.1 (`ftel_plic`) describes a different register layout and is not used.
The source documents are private and are not included in this package.

## Bus interface

- APB4 through AP_PLIC, local offsets from the bus and the window is 64 KiB.
- Byte, halfword and word accesses inside one word (decision Q02-g, Common v2.1 "Full" strobes): byte enables select the lanes, a write merges only the enabled lanes and a write with no enabled lane changes nothing. Lengths 0, 3 or > 4 return `TLM_BURST_ERROR_RESPONSE`, misaligned or out-of-window addresses return `TLM_ADDRESS_ERROR_RESPONSE`.
- Unmapped offsets return `TLM_GENERIC_ERROR_RESPONSE` (PSLVERR) and change nothing. CLAIM/complete has side effects and takes full words only (a narrower access is PSLVERR).
- `transport_dbg` reads have no side effect: reading CLAIM returns the ID a claim would return without claiming.

## Registers

| Offset | Register | Access | Behavior |
| --- | --- | --- | --- |
| 0x0000 | Source 0 priority | RO | Reads 0, writes ignored |
| 0x0004-0x007C | Priority ID 1-31 (`4 * ID`) | RW [4:0] | 0 disables the source, 1-31 |
| 0x1000 | Pending | RO | Bit n = Source ID n, bit 0 = 0. Writes ignored |
| 0x2000 | Enable (context 0) | RW | Bit n enables Source ID n, writable mask 0xFFFF_FFFE |
| 0x3000 | Threshold (context 0) | RW [4:0] | An interrupt needs priority > threshold. **Offset is a VP choice** |
| 0x3004 | Claim/complete (context 0) | R/C, W/C | Read claims the best ID (0 = none), writing an ID completes it. **Offset is a VP choice** |

All RW registers reset to 0.

## Interrupt behavior

- Element k of `irq_sources` is Source ID k+1 (`irq_sources_i[n-1]` -> ID n).
- Level gateway: a high input sets the pending bit while the source is not in service. The bit stays set until claimed, even if the input falls first. FU3 timers raise their IRQ for one tick only, so latching is required. One outstanding event per source.
- Claim (functional read of CLAIM): selects the pending, enabled source whose priority is strictly greater than the threshold and ties go to the lowest ID. The comparison is explicit, so threshold 31 masks every source ("Threshold corner case"). Claim clears the pending bit and puts the source in service.
- Complete (write of CLAIM): releases the written ID if it is 1-31 and in service. Other values, including 0, are ignored ("Completion validation"). A source whose input is still high pends again at once.
- Priority 0 never interrupts.
- `irq` (`eip_targets_o[0]`) is a level: high while a claimable source exists.
- `reset_n` low clears all registers and in-service state, deasserts `irq`, and ignores functional writes. The model is untimed: gateways react to input events, not to a sampling clock.

## Open questions

| Topic | VP choice |
| --- | --- |
| Threshold and claim/complete offsets | Common/HAS: "TBD (compact-map offset)". VP uses 0x3000 and 0x3004 (VP ABI, decision Q03-a) until RTL fixes the compact map. HDS v1.1 offsets 0x20/0x24 collide with priority ID 8/9 in this layout |
| Write to the RO pending register | Ignored with an OK response |
| Edge sources | Not supported: FU3 uses `le_i` = 0 for every source |
| Source synchronization (IRQ_ADAPT) | Not modeled: the Common Spec requires it outside the PLIC |
| IDs 24-31 | Provisional allocation (Q03-h): UART3-7 driven in R1, PWM0/1 and CAN low until R2 |

## INTMAP (Ibex)

`fu3_plic::Intmap` (`intmap.h`) maps peripheral IRQ levels onto the CPU1 (Ibex) inputs per the Common Spec v2.1 Interrupt Mapping, "Ibex CPU interrupt mapping - proposed FU3 source allocation" (decision Q03-b). Each output is the OR of its enabled sources; all nets are active-high levels, nothing is inverted or latched.

Every source has a route enable that resets **disabled** (Q03-b). The enable register is not defined yet (offset/layout pending), so the enables are a host parameter (`set_enable()`, `enable_all()`) for integration and tests; firmware cannot set them.

| Ibex input (`irq[k]`) | Cause | Sources |
| --- | --- | --- |
| irq_fast_i[0] | 16 | WDT0 warning (Ibex only) |
| irq_fast_i[1] | 17 | CRM |
| irq_fast_i[2] | 18 | GPIO0_WAKE |
| irq_fast_i[3] | 19 | EDHOC OSCORE |
| irq_fast_i[4] | 20 | AES-256 |
| irq_fast_i[5] | 21 | RNG |
| irq_fast_i[6] | 22 | DMA0 |
| irq_fast_i[7] | 23 | QSPI |
| irq_fast_i[8] | 24 | UART0-7 |
| irq_fast_i[9] | 25 | SPI0-3 |
| irq_fast_i[10] | 26 | CAN |
| irq_fast_i[11] | 27 | GPIOA, GPIOB, GPIOC |
| irq_fast_i[12] | 28 | I2C0 |
| irq_fast_i[13] | 29 | ADC |
| irq_fast_i[14] | 30 | PWM0, PWM1 |
| irq_external_i (`kOutExternal`) | 11 | TIMER2, TIMER3, WDT1, MAILBOX_IBEX (Q03-g) |
| irq_software_i (`kOutSoftware`) | 3 | SYSCSR software IRQ, Ibex bit (Q03-e pending: tied low) |
| irq_timer_i (`kOutTimer`) | 7 | TIMER1 |
| irq_nm_i (`kOutNmi`) | 31 | none (tied low; WDT0 escalation stays a CRM reset path) |

`kIbexCause[k]` gives the cause of output k, so the platform forwards each output with `ibex.set_irq(kIbexCause[k], level)`. `debug_req_i` (DBG) is a direct wire, not part of this model.

| INTMAP topic | VP choice |
| --- | --- |
| Route enables | Reset disabled, host-only until the enable CSR is defined (Q03-b) |
| TIMER1 fan-out | TIMER1 drives both PLIC ID 6 (CVA6) and Ibex irq_timer_i; software enables it for one owner |
| AP_HBCTRL IRQ | No source in the v2.1 table: not routed (Q03-g) |

## Clock gate (platform integration)

The clock controller (FU3 CRM gate, through the platform) drives the gate; it is enabled by default, so a stand-alone bench needs no change. `set_clock_enabled(false)` stops the gateways: no source is captured while gated. On enable the sources still high are captured; a pulse that ended while gated is lost.
