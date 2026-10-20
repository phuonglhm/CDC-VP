# FU3 CRM functional contract

## Sources and scope

The IP specification is the FU3 CRM Specification V1.0 (October 9, 2026). It defines the register map (section 3.8), the clock paths (3.7), the reset behavior (3.1, 3.3) and the embedded firmware sequences (4).
FU3 HAS v2.0 is used only where the CRM specification is silent: the 64 KiB window at 0x5001_0000 on PERIBUS_1 and the `irq_crm` routes (PLIC Source ID 1, Ibex `irq_fast_i[13]`).
The source documents are private and are not included in this package.

The Mini RISC-V controller (MCPU) and its ROM are not executed. A SystemC thread performs the same register writes as the firmware in section 4, in the same order.

## Bus interface

- APB4, local offsets from the bus, 64 KiB window, decoded range 0x0000-0x0FFF (section 3.5).
- Only aligned 32-bit accesses. Other lengths return `TLM_BURST_ERROR_RESPONSE` and misaligned addresses return `TLM_ADDRESS_ERROR_RESPONSE` (VP transport checks, section 3.5 places them outside the programming contract).
- PSTRB is ignored: every write updates the full word, as the asynchronous bridge does not carry byte masks (section 3.5).
- Offsets 0x1000 and above return `TLM_GENERIC_ERROR_RESPONSE` (PSLVERR) with read data 0. Undefined offsets inside the decoded range read 0 and ignore writes.
- Writes while the CRM is in reset (PRSTN low or global request high) are ignored, except through `transport_dbg`.

## Registers

| Offset | Register | Access | Reset | Behavior |
| --- | --- | --- | --- | --- |
| 0x0000 | VERSION | RO | 0x0001_0000 | major 1, minor 0 |
| 0x0010 | PLL | RW [13:0], RO [14] | 0x0000_0231 | n_div, m_div, od stored. lock [14] reads 1 when lken = 1, or when lock_type = 1 and powerdown = 0 |
| 0x0018 | PLL_CTRL | RW [14:0] | 0x0000_5949 | oe, count, lock_type, lken, bypass, powerdown stored |
| 0x001C | CLK_0 | RW [0] | 0 | Branch selector: OSC or divided branch |
| 0x0020 | CLK_1 | RW [0] | 0 | Final selector: OSC or branch |
| 0x0024 | CLK | RW [9:0] | 0 | Bus divider: 0 bypass, n -> Fin / (2n) |
| 0x0028 | CLK_GATE_0 | RW [12:0] | 0x1FFF | mrv, rom, isram, dsram, aes_256, qspi, rng, wdt, adc, fclk, dbg, dma, edhoc_oscore |
| 0x002C | CLK_GATE_1 | RW [14:0] | 0x7FFF | timer0-3, spi0-1, uart0-2, i2c, gpioa-c, plic, syscsr |
| 0x0030 | FCLK_SW | RW [0] | 0 | rst_n of CVA6 |
| 0x0034, 0x0038, 0x003C | ACLK_SW, PCLK_0_SW, PCLK_1_SW | RW [0] | 1 | rst_n of the AXI bus, PERIBUS_0, PERIBUS_1 |
| 0x0040 | SW_RST_N_0 | RW [7:0] | 0xFF | rom, isram, dsram, aes_256, qspi, wdt, adc, pll_ctrl |
| 0x0044 | SW_RST_N_1 | RW [3:0] | 0xF | timer0-3 |
| 0x0048 | SW_RST_N_2 | RW [13:0] | 0x3FFF | spi0-1, uart0-2, i2c, rng, dma, gpioa-c, plic, syscsr, edhoc_oscore |
| 0x004C | CPU_DEBUG | RW [0] | 0 | rst_n of the CVA6 debug subsystem |
| 0x0050 | SYSTEM_CLK_SW | RW [0] | 0 | rst_n of the system clock domain |
| 0x0054 | SW_RESET_REQ | RO | 0 | Live levels: [0] global, [1] system, [2] watchdog, [3] sysresetreq (CPU reset request) |
| 0x0058 | CRM | RW [0] | 0 | int: level of `irq` |

## Clocks

- The CPU clock is always the oscillator (section 3.7.1). The wrapper has no PLL, so the PLL registers do not change any clock.
- `bus_clock_hz()`: OSC unless CLK_1.sel = 1 and CLK_0.sel = 1, then OSC / (2 x CLK.div), or OSC when div = 0.
- `clk_gate_0/1` and `sw_rst_n_0/1/2` drive output ports with the register values. The platform applies them to the IPs. The model does not block bus access to a gated IP.
- Untimed: no switch pause, no lock counter, no reset synchronizer latency.

## Resets and firmware

| Event | System (`sys_rst_n`) | CVA6 (`cva6_rst_n`) | Debug (`dbg_rst_n`) | Registers |
| --- | --- | --- | --- | --- |
| PRSTN low or global request high | Held | Held | Held | Reset values, DONE cleared |
| Startup after release (4.1) | Released at once | Released `startup_delay` later, after DONE | Held (CPU_DEBUG = 0) | SYSTEM_CLK_SW then FCLK_SW written to 1 |
| Watchdog or CPU reset request (4.3, 4.4) | Pulse of `hold`, released first | Held, released `startup_delay` after the system | Unchanged | Only SYSTEM_CLK_SW and FCLK_SW written, DONE set again |
| System reset request (4.5) | Pulse of `hold` | Unchanged | Unchanged | Only SYSTEM_CLK_SW written |

- `hold` defaults to 1.4 us (seven instructions at four 20 MHz cycles) and `startup_delay` to 8.65 us (the measured 173 SoC cycles of section 4.2). Both are constructor arguments.
- Requests are rising edges. One pending event is kept. The firmware then reads the live levels and serves only the highest priority: watchdog, CPU reset request, system (section 3.3). A request that has already dropped is not served. A level held across PRSTN or a global reset is not an edge.
- The global request is a level: while it is high the CRM stays in reset, and startup runs again after it returns low (section 3.1.3).
- CLK_GATE_0.mrv = 0 suspends the firmware: requests stay pending and are served when the gate is enabled again (section 3.7.3).
- Software can write FCLK_SW, SYSTEM_CLK_SW and CPU_DEBUG at any time, and the outputs follow the registers.
- `done()` reports the DONE indication.

## Interrupt

`irq` = CRM.int while the CRM is out of reset. Connect it to PLIC Source ID 1 and INTMAP `kSrcCrm`.

## Differences from HAS and open questions

| Topic | CRM Specification V1.0 (followed) | HAS v2.0 | Status |
| --- | --- | --- | --- |
| CVA6 release | CRM firmware releases CVA6 after startup and after each watchdog or CPU recovery | 14.2: Ibex releases CVA6 after staging its image | Confirm. The CVA6 can start before its image is staged |
| Ibex reset | Not described | 14.2: Ibex starts from ROM after reset | VP: Ibex follows `sys_rst_n` |
| Debug reset | CPU_DEBUG resets to 0, so DBG is held until software releases it | 14.2: boot waits in ROM while DEBUG_EN = 1 for debug | Confirm how a debugger attaches at boot |
| PLL | Registers present, no PLL in the wrapper | 3.6: one PLL instance | Confirm |
| Clock gates and resets | No bit for MAILBOX_CVA6/IBEX, SPI2/3, HyperBus/AP_HBCTRL, separate WDT0/1, separate CVA6/Ibex ISRAM | 6.1: one gate per IP | Confirm |
| DONE | Software-written indication, no register in section 3.8 | Not described | Exposed as `done()` only |
| Reset request inputs | Watchdog, processor reset request, system, global | 6.5: WDT, SYSCSR and debug reset | VP: `cpu_reset_req` <- DBG ndmreset, `sw_reset_req` <- SYSCSR sw_reset_req, `sw_global_rst_req` <- SYSCSR sw_global_rst_req |
| Readiness qualification, reset filter | 255 OSC cycles, 100 ns filter | Not described | Not modeled |
| LDO | Power-ready inputs fixed ready in the wrapper | 6.2: Power-Good handshake not defined | Not modeled |
