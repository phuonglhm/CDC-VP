# FU3 AP_PLIC functional contract

## Sources and scope

PLIC HDS V1.0 (ftel_plic) defines parameters (Table 5-1), the APB4 interface (Table 7.1), source mapping (Table 7-2), the single target (Table 7-3) and the register map (Table 8.1).
FU3 HAS v2.0 Table 11-1 lists Source IDs, Table 11-2 routes target 0 to CVA6 `irq_i[0]`, and the memory map places AP_PLIC at 0x4010_0000 on PERIBUS_1.
The source documents are private and are not included in this package.

HDS sections 6.x (functions) and 8.1 (register fields) are still empty templates. Where the HDS is silent, the model follows the RISC-V PLIC and the RoaLogic PLIC, whose parameter set (SOURCES, TARGETS, PRIORITIES, MAX_PENDING_COUNT, HAS_THRESHOLD, HAS_CONFIG_REG) and CONFIG/EL/PRIORITY registers match this IP. Each such choice is listed below.

## Parameters

SOURCES = 32,
TARGETS = 1,
PRIORITIES = 8,
MAX_PENDING_COUNT = 8,
HAS_THRESHOLD = 1,
HAS_CONFIG_REG = 1 (HDS defaults).

## Bus interface

- APB4, 32-bit data, zero wait states, no slave error (HDS 7.1). Local offsets from the bus and the window is 64 KiB.
- Aligned 1/2/4-byte accesses; byte enables (PSTRB) select written lanes. Other lengths return `TLM_BURST_ERROR_RESPONSE`; misaligned or out-of-window addresses return `TLM_ADDRESS_ERROR_RESPONSE` (VP transport checks only).
- Unmapped offsets read 0 and ignore writes with an OK response (no PSLVERR).
- `transport_dbg` reads have no side effect: reading ID_0 returns the ID a claim would return without claiming.

## Registers

| Offset | Register | Access | Behavior |
| --- | --- | --- | --- |
| 0x00 | CONFIG_0 | RO | 0x0001_0020: [15:0] SOURCES = 32, [31:16] TARGETS = 1 |
| 0x04 | CONFIG_1 | RO | 0x0108_0008: [15:0] PRIORITIES = 8, [23:16] MAX_PENDING_COUNT = 8, [24] HAS_THRESHOLD |
| 0x08 | EL | RW | Bit k configures Source ID k+1: 0 level, 1 rising edge |
| 0x0C-0x18 | PRIORITY_0..3 | RW | 4-bit field per ID, 8 IDs per register and [2:0] writable (0..7) |
| 0x1C | IE_0 | RW | Bit k enables Source ID k+1 for target 0 |
| 0x20 | THRESHOLD_0 | RW | [2:0] and an interrupt needs priority > threshold |
| 0x24 | ID_0 | R/C, W/C | Read claims the best ID (0 = none) and writing an ID completes it |

All RW registers reset to 0.

## Interrupt behavior

- Source k of `irq_sources` is Source ID k+1 (HDS 7.2). ID 0 means no interrupt.
- Level gateway: a high input sets a latched pending bit while the source is not in service. The bit stays set until claimed, even if the input falls first (RISC-V PLIC behavior). On complete, a still-high input pends again at once. FU3 timers raise their IRQ for one tick only, so latching is required.
- Edge gateway: each rising edge increments a per-source counter, saturating at MAX_PENDING_COUNT = 8. The source is pending while the counter is nonzero and it is not in service and each claim consumes one edge. Edges arriving while in service are kept.
- Claim (functional read of ID_0): selects the pending, enabled source with the highest priority strictly above THRESHOLD_0 and ties go to the lowest ID. The source becomes in service and stops pending until completed.
- Complete (write of ID_0): releases the written ID if it is in service and other values are ignored.
- Priority 0 never interrupts.
- `irq` (o_plic_irq[0]) is a level: high while a claimable source exists.
- `reset_n` low clears all registers, in-service state and edge counters, deasserts `irq`, and ignores functional writes. The model is untimed: gateways react to input events, not to a sampling clock.

## Open questions (HDS)

| Topic | VP choice |
| --- | --- |
| PRIORITY field width and packing | 4 bits per ID, [2:0] used (HDS: 8 IDs per register, 8 levels) |
| CONFIG_0/1 field layout | As in the Registers table |
| EL bit polarity | 1 = edge (HDS Table 7-2 lists le_i = 0 for level sources) |
| Level pulse dropped before claim | Latched until claim (RISC-V PLIC) |
| Completing an unclaimed ID | Ignored |
| Source synchronization | Not modeled and HDS requires synchronous sources |
| Sources 22-23 (SPI2, SPI3) | Present in HAS Table 11-1, absent from HDS Table 7-2 and the model supports all 32 inputs |
