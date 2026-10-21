# SYSCSR validation - 2026-10-21

Environment: Ubuntu under WSL, GNU C++ 15.2.0, C++17, SystemC 2.3.4 shared installation.
Built with `-Wall -Wextra -Wpedantic`: no warnings.

| Check | Result |
| --- | --- |
| Static library: behavior and FU3 bus integration | 2/2 CTest tests passed |
| Installed static package consumer | 1/1 passed |
| Shared library: behavior and FU3 bus integration | 2/2 passed |
| Installed shared package consumer | 1/1 passed |

Total: 6/6 CTest executions passed.

- Behavior: 449 checks, zero failures.
- Integration: 446 checks, zero failures. The three malformed-access checks run only in the direct bench, because the FU3 bus rejects them first.
- Each installed consumer repeats the behavior bench through the exported package.

The integration bench uses the production FU3 bus with only the SYSCSR window enabled and drives it from the IBEX initiator with `fu3_bus::ScopedAccess`, so requests pass SYSBUS_1, SYSBUS_0, AP_PP0 and the A2P bridge before reaching local offsets.

Mutation checks: a wrong reset value (left_pad_ctrl_0), a wrong RW mask (osc_pad_ctrl) and live straps instead of the reset-release capture each made both benches fail.
