# FU3 CRM functional contract

## Sources and scope

Specification priority for FU3 (leader decision, 2026-10-20): FU3 HAS v2.0 and FTEL_FU3_Common_Spec v2.0 first, the IP specification only where both are silent.
HAS v2.0 defines the CRM at 0x5001_0000 on PERIBUS_1 (64 KiB), the PLL with the 20 MHz OSC as reference (3.6, 5.1), separate Ibex and CVA6 resets with Ibex released first and CVA6 held until Ibex hands off (5.2, 6.2, 14.2), one gate per clock line (6.1), the software resets (6.5) and `irq_crm` on PLIC Source ID 1 and the Ibex INTMAP.
The FU3 CRM Specification V1.1 (October 17, 2026) and CSR_CRM_fu3_v2 give the register map, the clock paths (3.7), the bus interface (3.5) and the startup sequence (4.1).
CPU0 is CVA6 and CPU1 is Ibex.
The source documents are private and are not included in this package.

The MRV controller and its ROM are not executed. A SystemC thread performs the startup writes of section 4.1 and the HAS 6.5 reset behavior.

## Bus interface

- APB4, local offsets from the bus, 64 KiB window, decoded range 0x0000-0x0FFF (section 3.5).
- Only aligned 32-bit accesses. Other lengths return `TLM_BURST_ERROR_RESPONSE` and misaligned addresses return `TLM_ADDRESS_ERROR_RESPONSE`.
- PSTRB is ignored: every write updates the full word (section 3.5).
- Offsets 0x1000 and above return `TLM_GENERIC_ERROR_RESPONSE` (PSLVERR) with read data 0. Undefined offsets inside the decoded range, including the removed 0x004C and 0x0050, read 0 and ignore writes.
- Writes while the CRM is in reset (PRSTN low or global request high) are ignored, except through `transport_dbg`.

## Registers

| Offset | Register | Access | Reset | Behavior |
| --- | --- | --- | --- | --- |
| 0x0000 | VERSION | RO | 0x0001_0000 | major 1, minor 0 |
| 0x0010 | PLL | RW [13:0], RO [14] | 0x0000_0231 | n_div [13:8], m_div [7:2], od [1:0]. lock [14] reads 1 when lken = 1 or the PLL is powered up |
| 0x0018 | PLL_CTRL | RW [14:0] | 0x0000_5949 | oe, count, lock_type, lken, bypass, powerdown |
| 0x001C | CLK_0 | RW [0] | 0 | 0 OSC, 1 PLL |
| 0x0020 | CLK_1 | RW [0] | 0 | 0 OSC, 1 CLK_0 branch |
| 0x0024 | CLK | RW [9:0] | 0 | Bus divider: 0 bypass, n -> branch / (2n) |
| 0x0028 | CLK_GATE_0 | RW [8:0] | 0x0000_01FF | rom, isram, dsram, aes_256, qspi, hyperbus, edhoc_oscore, dma, dbg |
| 0x002C | CLK_GATE_1 | RW [24:0] | 0x01FF_FFFF | mrv, wdt0, wdt1, adc, rng, syscsr, plic, gpioc, gpiob, gpioa, i2c, uart0-2, spi0-3, timer0-3, mailbox_ibex, mailbox_cva6, ap_hbctrl |
| 0x0030 | IBEX_SW | RW [0] | 0 | rst_n of CPU1 (Ibex) |
| 0x0034 | FCLK_SW | RW [0] | 0 | rst_n of CPU0 (CVA6) |
| 0x0038, 0x003C, 0x0040 | ACLK_SW, PCLK_0_SW, PCLK_1_SW | RW [0] | 1 | rst_n of the AXI bus, PERIBUS_0, PERIBUS_1 (stored only) |
| 0x0044 | SW_RST_N_0 | RW [7:0] | 0xFF | rom, isram, dsram, aes_256, qspi, dma, edhoc_oscore, hyperbus |
| 0x0048 | SW_RST_N_1 | RW [24:0] | 0x01FF_FFFF | pll_ctrl, then the CLK_GATE_1 order from wdt0 |
| 0x0054 | SYSTEM_CLK_SW | RW [0] | 0 | rst_n of the system domain |
| 0x0058 | SW_RESET_REQ | RO | 0 | Live levels: [0] global, [1] system, [2] watchdog, [3] sysresetreq |
| 0x005C | CRM | RW [0] | 0 | int: level of `irq` |

## Clocks

- PLL (HAS 3.6): output = OSC x M / (N x OD) with OD = 1, 2, 4, 8, or OSC / N in bypass, 0 when powered down, oe = 0 or N = 0. Reset values give 60 MHz once powered up.
- The PLL branch is used only when CLK_0.sel = 1 and CLK_1.sel = 1, otherwise both clocks are OSC (section 3.7.1).
- `cpu_clock_hz()`: the selected source, never divided (FCLK shared by both CPUs). `bus_clock_hz()`: the selected source divided by 2 x CLK.div when div > 0 and the branch is selected.
- `clk_gate_0/1` and `sw_rst_n_0/1` drive output ports with the register values. The platform applies them to the IPs. The model does not block bus access to a gated IP (HAS 6.1 calls it prohibited).
- Untimed: no switch pause, no lock counter delay, no reset synchronizer latency.

## Resets

Outputs: `sys_rst_n` = SYSTEM_CLK_SW, `ibex_rst_n` = SYSTEM_CLK_SW & IBEX_SW, `cva6_rst_n` = SYSTEM_CLK_SW & FCLK_SW, `dbg_rst_n` = SYSTEM_CLK_SW, or kept high during a debug reset. All are low while the CRM is in reset.

| Event | System | CPU1 (Ibex) | CPU0 (CVA6) | DBG | CRM registers |
| --- | --- | --- | --- | --- | --- |
| PRSTN low or global request high | Held | Held | Held | Held | Reset values |
| Startup after release (spec 4.1) | Released at once | Released `startup_delay` later | Held until software writes FCLK_SW | Released with the system | SYSTEM_CLK_SW then IBEX_SW written to 1 |
| Watchdog or SYSCSR system request (HAS 6.5.1, 6.5.2) | Pulse of `hold`, then startup | Held, released by startup | Held, FCLK_SW cleared | Reset with the system | Kept, except the three reset bits |
| Debug request, ndmreset (HAS 6.5.3) | Same | Same | Same | Kept running | Same |

- `hold` defaults to 1.4 us (VP choice). `startup_delay` defaults to 1.2 us, the 24 MRV clocks at 20 MHz of section 4.2. Both are constructor arguments.
- Requests are rising edges. One pending reset is kept. If any watchdog or system edge is part of it, DBG is reset too. A level held across PRSTN or a global reset is not an edge.
- The global request is a level: while it is high the CRM stays in reset, and startup runs again after it returns low (section 3.1.3).
- Software can write IBEX_SW, FCLK_SW and SYSTEM_CLK_SW at any time, and the outputs follow the registers.

## Interrupt

`irq` = CRM.int while the CRM is out of reset. Connect it to PLIC Source ID 1 and INTMAP `kSrcCrm`.

## Differences and open questions

| Topic | HAS v2.0 (followed) | CRM Specification V1.1 / CSR v2 | Status |
| --- | --- | --- | --- |
| Watchdog, SYSCSR and debug requests | 6.5: reset the system (SYSCSR: except CRM, debug: except DBG) | ROM has no handler: requests are only status bits | VP resets the system and reruns startup. Confirm whether the CRM ROM will add these handlers |
| System request scope | 6.5.2: overall system | CSR v2: "Subsystem reset-request; CVA6 excluded" | Both CPUs reset |
| PLL | 3.6, 5.1: CRM includes a PLL | Wrapper has no analog PLL, CSR v2 CLK_0.sel = 1 is "PLL output" | PLL output computed from the registers |
| DBG reset | 14.2: debugger attaches while Ibex waits in ROM (DEBUG_EN) | 3.1.2: debug reset follows the FCLK path | DBG released with the system, so it runs while CVA6 is held |
| CLK_GATE_0 reset | - | Sheet 0x3FF, fields [8:0] | 0x1FF (fields) |
| CLK_GATE_1 / SW_RST_N_1 reset | - | Sheet 0x03FF_FFFF, fields [24:0] | 0x01FF_FFFF (fields) |
| Common WDT gate/reset | - | wdt0/wdt1 descriptions AND with a common wdt bit that is not in the map | Not modeled |
| Reset filter, readiness qualifier, synchronizers | Not described | 100 ns filter, 255 OSC cycles | Not modeled |
| LDO | 6.2: Power-Good handshake not defined | Power-ready inputs tied high | Not modeled |
