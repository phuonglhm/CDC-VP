# CRM specification traceability

Source: FU3 CRM Specification V1.0 and, where it is silent, FU3 HAS v2.0, reviewed 2026-10-20. Checks live in `tests/bench.h`. `behavior.cpp` binds directly and `integration.cpp` uses the FU3 bus. The bench uses a 200 ns hold and a 1 us startup delay.

| Source | Behavior | Verification |
| --- | --- | --- |
| CRM spec 4.1, 5 (Startup) | System released first, DONE, then CVA6 released, debug still held | Output order and DONE after PRSTN and after a global reset |
| CRM spec 3.8 | Offsets, reset values, RW masks of all 19 registers | Values after startup, all-ones and all-zeros writes |
| CRM spec 3.5 | 32-bit word access, PSTRB ignored, 4 KiB decode | Undefined offset reads 0, offset 0x1000 returns error and 0, byte access refused, partial PSTRB writes the full word, debug read/write |
| CRM spec 3.7.1-3.7.2 | Bus clock path and divider, no PLL | Divider only with both selectors on the branch, div 1/2/5 give 10/5/2 MHz, bypass, PLL registers have no effect |
| CRM spec 3.8.3-3.8.4 | PLL lock sources | Powered down, counter lock, PLL LOCK source without a PLL, lken |
| CRM spec 3.7.3, 3.8.8-3.8.16 | Gates and per-IP resets drive outputs | clk_gate_0/1 and sw_rst_n_0/1/2 outputs |
| CRM spec 3.8.10, 3.8.17 | FCLK_SW and CPU_DEBUG controlled by software | CVA6 hold/release, debug release |
| CRM spec 3.3, 4.3, 4.4 | Watchdog and CPU reset request recovery | System and CVA6 held, system released first, CVA6 after the delay, debug untouched, gates kept, one service per edge, live SW_RESET_REQ |
| CRM spec 4.5 | System reset request | Only the system domain pulses |
| CRM spec 3.3 priority | Highest live request served | Watchdog and system together give one recovery with CVA6 |
| CRM spec 3.3, 3.7.3 | MRV gate suspends service, live status | Pending request served after re-enable, dropped request not served |
| CRM spec 3.1.3, 4.6 | Global request holds the CRM in reset | Every reset held, DONE cleared, writes ignored, startup again after release |
| CRM spec 3.1 (POR) | PRSTN resets everything | Resets held, writes ignored, held request across PRSTN is not an edge |
| CRM spec 3.8.20, HAS Table 11-1, 10.2 | CRM.int drives `irq` | irq high and low |
| HAS memory map | CRM at 0x5001_0000 on PERIBUS_1 | Integration bench from the IBEX initiator through the FU3 bus |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run the behavior bench via find_package |

Not covered: the MCPU instruction set and ROM, reset filter and readiness qualifier timing, synchronizer latency, the effect of gates and resets on other IPs (platform), and the open questions in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
