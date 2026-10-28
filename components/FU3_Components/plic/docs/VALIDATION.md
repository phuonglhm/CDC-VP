# AP_PLIC validation

## 2026-10-23 - Common v2.1 (platform build, fu3_soc T08)

Environment: AlmaLinux 9, GCC 11.5.0, SystemC 2.3.4, Release build of the CDC-VP tree (`build-fu3-t07/root`).

- Behavior: 708 checks, zero failures.
- Integration (FU3 bus): 702 checks, zero failures.
- INTMAP (v2.1 routes and enables): 1408 checks, zero failures.

The packaging run below (`tests/run_all.sh`) has not been repeated for v2.1.

## 2026-10-20 - Common v2.0 (historical)

Environment: Ubuntu under WSL, GNU C++ 15.2.0, C++17, SystemC 2.3.4 shared installation.
Built with `-Wall -Wextra -Wpedantic`: no warnings.
Command: `tests/run_all.sh`.

| Check | Result |
| --- | --- |
| Static library: behavior, FU3 bus integration and INTMAP | 3/3 CTest tests passed |
| Installed static package consumer | 1/1 passed |
| Shared library: behavior, FU3 bus integration and INTMAP | 3/3 passed |
| Installed shared package consumer | 1/1 passed |

Total: 8/8 CTest executions passed.

- Behavior: 556 checks, zero failures.
- Integration: 553 checks, zero failures. The three malformed-access checks run only in the direct bench, because the FU3 bus rejects them first.
- INTMAP: 1047 checks, zero failures.

The integration bench uses the production FU3 bus with only the AP_PLIC window enabled and drives it from the CVA6 initiator with `fu3_bus::ScopedAccess`, so requests pass SYSBUS_1, AP_PP1 and the A2P bridge before reaching local offsets.

Mutation checks, each making both benches fail: `>=` instead of `>` in the threshold comparison, enable bit 0 writable, partial PSTRB accepted, no re-pend on complete.

INTMAP mutation checks, each failing `intmap_tests`: UART2 dropped from irq_fast_i[4], TIMER2 instead of TIMER1 on irq_timer_i, CRM moved to irq_fast_i[12].

End-to-end (harness outside the repository): CPU0 (CVA6) firmware `fu3_plic_irq` on the FU3 bus with the Timer models and this PLIC takes TIMER1 through Source ID 6 as one machine external interrupt (mcause 11), claims ID 6 once and completes it.
