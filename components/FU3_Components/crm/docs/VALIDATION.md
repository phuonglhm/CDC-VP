# CRM validation - 2026-10-20

Environment: Ubuntu under WSL, GNU C++ 15.2.0, C++17, SystemC 2.3.4 shared installation.
Built with `-Wall -Wextra -Wpedantic`: no warnings.
Command: `tests/run_all.sh`.

| Check | Result |
| --- | --- |
| Static library: behavior and FU3 bus integration | 2/2 CTest tests passed |
| Installed static package consumer | 1/1 passed |
| Shared library: behavior and FU3 bus integration | 2/2 passed |
| Installed shared package consumer | 1/1 passed |

Total: 6/6 CTest executions passed.

- Behavior: 358 checks, zero failures.
- Integration: 355 checks, zero failures. The three malformed-access checks run only in the direct bench, because the FU3 bus rejects them first.

The integration bench uses the production FU3 bus with only the CRM window enabled and drives it from the IBEX initiator with `fu3_bus::ScopedAccess`, so requests pass SYSBUS_1, PERIBUS_1 and the A2P bridge before reaching local offsets.

Mutation checks, each making both benches fail: startup also releases CVA6, system reset keeps FCLK_SW, debug reset resets DBG, watchdog reset keeps DBG, CLK_GATE_1/SW_RST_N_1 reset 0x03FF_FFFF, PLL ignores OD, CPU clock divided, Ibex reset ignores SYSTEM_CLK_SW.
