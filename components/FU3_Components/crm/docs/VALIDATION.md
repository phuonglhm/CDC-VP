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

- Behavior: 388 checks, zero failures.
- Integration: 388 checks, zero failures.

The integration bench uses the production FU3 bus with only the CRM window enabled and drives it from the IBEX initiator with `fu3_bus::ScopedAccess`, so requests pass SYSBUS_1, PERIBUS_1 and the A2P bridge before reaching local offsets.

Mutation checks, each making both benches fail: CVA6 released without the startup delay, system request served before watchdog, MRV gate ignored, decode window 64 KiB instead of 4 KiB, divider Fin / n, CPU_DEBUG reset value 1, global request not resetting the registers.
