# CRM specification traceability

Source: FU3 HAS v2.0 and FU3 CRM Specification V1.1 with CSR_CRM_fu3_v2, reviewed 2026-10-20 and again 2026-10-23 (spec files in the FU3 VP plan workspace). Checks live in `tests/bench.h`. `behavior.cpp` binds directly and `integration.cpp` uses the FU3 bus. The bench uses a 200 ns hold and a 1 us startup delay.

| Source | Behavior | Verification |
| --- | --- | --- |
| HAS 5.2, 6.2 / CRM spec 4.1 | System and DBG released first, CPU1 (Ibex) after the delay, CPU0 (CVA6) held | Output order after PRSTN, after a global reset and after each system reset |
| HAS 14.2 | Ibex releases CVA6 through FCLK_SW | FCLK_SW = 1 releases CVA6, IBEX_SW = 0 holds only Ibex |
| CSR_CRM_fu3_v2 | Offsets, reset values, RW masks, removed 0x004C/0x0050 | Values after startup, all-ones and all-zeros writes, undefined offsets RAZ/WI |
| CRM spec 3.5 | 32-bit word access, PSTRB ignored, 4 KiB decode | Offset 0x1000 returns error and 0, no alias, partial PSTRB writes the full word, debug read/write, halfword and misaligned rejected |
| HAS 3.6, 5.1 / CSR v2 PLL | PLL output from N, M, OD, bypass, powerdown, oe, lock | 60 MHz at reset dividers, 200 MHz, bypass, oe = 0, lken; lock after 404 OSC cycles, not before, restarted by new dividers, immediate with lock_type = 0 |
| CRM spec 3.7.4 | Divider changes only on the OSC | No warning for the supported sequence; warning for a change on the selected branch, not for the same value |
| CRM spec 3.2, 3.1.1 | PRSTN filter 100 ns, readiness 255 OSC cycles | Held through filter + qualification after power-on and after PRSTN; short low pulse and short release glitch rejected; assertion only after the filter span; no qualification after a global reset |
| CRM spec 3.7.1-3.7.2 | Branch only with both selectors, bus divider, CPU undivided | CLK_0 alone has no effect, divider 3 gives bus / 6 with CPU unchanged |
| HAS 6.1 / CSR v2 | Gates and per-IP resets drive outputs | clk_gate_0/1 and sw_rst_n_0/1 outputs |
| CSR v2 SYSTEM_CLK_SW | System reset gates both CPUs and DBG | SYSTEM_CLK_SW = 0 holds all, 1 restores |
| HAS 6.5.1, 6.5.2 | Watchdog and SYSCSR requests reset the system and DBG, CRM registers kept, boot reruns | One pulse per output, FCLK_SW cleared, CLK kept, falling request does nothing |
| HAS 6.5.3 | Debug request resets the system but not DBG | DBG never falls |
| VP choice | Debug + watchdog together give one reset including DBG | Simultaneous requests check |
| CRM spec 3.1.3 | Global request holds the CRM in reset | Every reset held, registers reset, writes ignored, status visible, startup after release |
| CRM spec 3.1 (POR) | PRSTN resets everything | Resets held, writes ignored, held request across PRSTN is not an edge |
| CSR v2 CRM.int, HAS Table 11-1 | CRM.int drives `irq` | irq high and low |
| HAS memory map | CRM at 0x5001_0000 on PERIBUS_1 | Integration bench from the IBEX initiator through the FU3 bus |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run the behavior bench via find_package |

Not covered: the MRV instruction set and ROM, synchronizer latency, the effect of gates and resets on other IPs (platform), and the open questions in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
