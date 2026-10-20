# MAILBOX validation - 2026-10-20

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

- Behavior: 428 checks, zero failures.
- Integration: 428 checks, zero failures.

The integration bench uses the production FU3 bus with only the MAILBOX_CVA6 and MAILBOX_IBEX windows enabled. The CVA6 initiator reaches MAILBOX_CVA6 through PERIBUS_1 and the IBEX initiator reaches MAILBOX_IBEX through PERIBUS_0, both with `fu3_bus::ScopedAccess`.

Mutation checks, each making both benches fail: FULL at depth + 1, `>=` instead of `>` in the receive threshold, W1C clearing every bit, EMPTY reading the send FIFO.
