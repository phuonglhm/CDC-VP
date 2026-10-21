# DBG validation - 2026-10-21

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

- Behavior: 110 checks, zero failures.
- Integration: 111 checks, zero failures. The extra check reads the boot ROM, which only the bus rejects for the DBG master.

The integration bench uses the production FU3 bus with only the DSRAM, ROM and IBEX_DBG windows enabled. The bridge `axi` socket is bound to the DBG initiator and attaches `fu3_bus::Access` itself, and the DM is the IBEX_DBG target, so every DM access also crosses SYSBUS_1.

Mutation checks, each making both benches fail: no address auto-increment, every CMD_DATA scan reading, CMD_STATUS not cleared, DECERR reported as SLVERR, Test-Logic-Reset selecting BYPASS, DM register at offset n instead of 4 x n.
