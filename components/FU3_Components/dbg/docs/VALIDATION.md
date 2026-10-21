# DBG validation - 2026-10-20

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

- Behavior: 127 checks, zero failures.
- Integration: 130 checks, zero failures. The three extra checks read the boot ROM, which only the bus rejects for the DBG master.

The integration bench uses the production FU3 bus with only the DSRAM and ROM windows enabled and the DM `sba` socket is bound to the DBG initiator and attaches `fu3_bus::Access` itself.

Mutation checks, each making both benches fail: resumereq accepted with haltreq set, 8/16-bit SBA allowed, commands run while cmderr is set, hartselhi dropped, DMI scan returning the current instead of the previous result, Access Register on a running hart.
