# WDT0/1 functional model contract

## Source and scope

Primary source: `FTEL_CDC_WDT_Hardware_Design_Specification_v1.1.docx.pdf`, 38 pages. FU3 HAS v2.0 supplies the WDT0/WDT1 address windows and interrupt routes. Private source files remain under `Secret-Docs` and are not part of this component package. HDS p.8 names APB 2.0 and explicitly excludes PSTRB, while its feature table on p.9 says APB4; the model follows the explicit full-word/no-strobe behavior and treats the bus-facing APB4 bridge as an upstream adapter.

This model supports software-visible timeout, warning, write protection, and reset-recovery flow. It does not model APB pins or physical CDC flops. The V1.1 APB-to-watchdog crossing is represented by a configurable transaction delay and one functional watchdog clock. Timing is not a cycle-accurate reproduction of the synchronizer RTL.

## Interfaces and resets

Each instance provides the TLM target socket, `preset_n`, `wdogres_n`, `i_wdt_other_unlock`, `irq`, and `reset_request`.

- `i_wdt_other_unlock=1` permits writes; `0` makes every valid APB write complete normally without modifying registers. Reads remain available while locked.
- `preset_n` is the programming-side reset. It cancels an in-flight TLM access and clears the APB-side synchronized halt request. It does not reset watchdog CSRs, counter, warning status, or an asserted reset request.
- `wdogres_n` is watchdog-side reset. Assertion restores LOAD and VALUE to `0xFFFFFFFF`, CONTROL to `RESEN=1`, clears TIMEOUT_SEEN, and deasserts the reset request and interrupt.
- Reset assertion is asynchronous in this behavioral model. Platform reset release should be synchronized to the relevant clock, as required by the HDS.
- The default functional clock period is 5 ns (200 MHz). Both HDS clocks have this frequency; separate clock-tree phase and 2-FF synchronizers are abstracted. APB access latency defaults to seven functional clock periods, representing the documented 6-7 wait states.

The WDT lock register is outside this IP. The supplied SYSCSR workbook does not yet define the corresponding software register/field, so platform wiring must provide the unlock signal explicitly until that ABI is supplied. WDT0 and WDT1 have independent input ports in the model; shared or separate SYSCSR control remains a platform decision.

## Register ABI

Offsets are local to the 64-KiB WDT window. Only the seven aligned word registers below decode. `0x1000` and every other unlisted offset return an address error, corresponding to APB PSLVERR. The FU3 bus owns the global base address.

| Offset | Register | Access | Reset / behavior |
|---|---|---|---|
| 0x1004 | WD_CNT_LD | RW | `0xFFFFFFFF`; write reloads VALUE and clears TIMEOUT_SEEN. Writing zero sets TIMEOUT_SEEN immediately. |
| 0x1008 | WD_CNT_VAL | RO | `0xFFFFFFFF`; current down-counter value. |
| 0x100C | WD_CNT_CTRL | RW | `0x00000001`; RESEN[0], INTEN[1], DBGEN[2], RESDIV[6:3], reserved[31:7]. |
| 0x1010 | WD_INT_RAW_STAT | RO | TIMEOUT_SEEN in bit 0; reserved bits read zero. |
| 0x1014 | WD_INT_MASK_STAT | RO | TIMEOUT_SEEN AND INTEN in bit 0; reserved bits read zero. |
| 0x1018 | WD_INT_CLR | WO | Any write clears TIMEOUT_SEEN and reloads VALUE; reads return zero. |
| 0x101C | WD_DBG_STAT | RO | Bit 0 reports the active debug halt; reserved bits read zero. |

There is no ID register, internal lock register, PSTRB, or programmable reset pulse width in V1.1. RESDIV is stored/read back but has no effect on `reset_request`; once asserted, that output remains high until `wdogres_n` is asserted. Writes to RO registers have no effect. Reserved CONTROL bits are ignored on write.

## Watchdog state behavior

- INTEN enables counting and the interrupt. While INTEN is zero, VALUE follows LOAD and TIMEOUT_SEEN is cleared.
- A LOAD write restarts VALUE from the new LOAD and clears TIMEOUT_SEEN. If LOAD is zero while INTEN is enabled, TIMEOUT_SEEN is set immediately and IRQ asserts; a reset request can be raised on the next watchdog edge when RESEN is enabled.
- With INTEN set and DBGEN clear, a loaded value N reaches its first timeout after N+1 eligible watchdog edges. The first timeout sets TIMEOUT_SEEN, asserts IRQ, and reloads VALUE.
- If TIMEOUT_SEEN is still set when VALUE reaches zero a second time, RESEN=1 asserts `reset_request` and freezes the counter. TIMEOUT_SEEN/IRQ remain pending. The request cannot be cleared by software after assertion; watchdog-side reset clears it.
- With RESEN=0, a later timeout reloads VALUE and leaves TIMEOUT_SEEN/IRQ asserted.
- WD_INT_CLR clears TIMEOUT_SEEN and reloads VALUE before reset has been requested. Clearing INTEN also clears TIMEOUT_SEEN and stops the counter.
- DBGEN halts the counter and suppresses timeout events. LOAD/CLEAR writes still reload while halted. `WD_DBG_STAT[0]` follows the active halt request. Programming reset clears that request without changing CONTROL.

When a reset request is already pending, it remains asserted until watchdog reset. The model preserves pending timeout state and freezes VALUE even if subsequent software writes arrive, following the HDS reset-pending priority.

## TLM adapter

The IP accepts aligned 32-bit transfers only. Following the explicit no-PSTRB statement (HDS p.8) and unsupported partial-write list (p.16), partial TLM byte enables are rejected; absent enables or all-`0xFF` enables represent a full word. No read-modify-write is used. Annotated delay is consumed exactly once. Malformed command/length/address/enable requests receive TLM adapter errors; those are not a cycle-level PSLVERR waveform. Locked writes and writes to RO registers return OK and have no effect. Debug access supports untimed side-effect-free reads only; writes are rejected. DMI and nonblocking transport are unsupported.

## FU3 integration boundary

WDT0 is at `0x44050000` and routes to the Ibex NMI path; WDT1 is at `0x44080000` and routes to PLIC source 18. The reset outputs are requests to the platform CRM. This component does not decide whether either request resets one watchdog, both watchdogs, or the whole SoC. Tests use a fake CRM and interrupt sinks; they do not establish production reset scope or execute CPU firmware.
