# WDT validation

The source and test fixtures were migrated to WDT HDS V1.1 on 2026-10-20. The previous V1.0 results are superseded because they exercised the removed internal LOCK register, old status layout and old reset-pulse behavior.

The newly supplied HDS V1.2 was reviewed on 2026-10-20. It reintroduces internal
WD_LOCK, changes reset ownership/retention, and changes second-timeout status
behavior. The implementation has not yet been migrated to that contract. Results
below establish V1.1 behavior only and must not be reported as V1.2 conformance.
The private comparison and unresolved definitions are recorded in ignored
`build/wdt-review/WDT_V12_REVIEW.md` at repository root.

## V1.1 execution results (2026-10-20)

Environment: WSL/Linux, GNU C++ 15.2.0, C++17, SystemC 2.3.4, Release builds.
Executables use `sc_main`. Reproduction commands are in the component README.

| Configuration | Result |
|---|---|
| Static component: behavior and real FU3 bus integration | 2/2 passed |
| Shared component: behavior and real FU3 bus integration | 2/2 passed |
| Installed static consumer, linking only `fu3::wdt` | 1/1 passed |
| Installed shared consumer, linking only `fu3::wdt` | 1/1 passed |
| Total final executions | 6/6 passed |

Each final behavior execution reports 110 checks and zero failures; each
integration execution reports 32 checks and zero failures. The shared consumer
resolves the installed `libfu3_wdt.so` and `libsystemc.so.2.3` without missing
dependencies. No compiler warnings were reported in these builds. This is Linux
shared-library evidence; native Windows DLL behavior was not tested.

The regressions exercise the V1.1 register map, full-word access policy, SYSCSR
unlock input, reset-domain behavior, first/second timeout, sticky reset request,
debug halt, and independent WDT0/WDT1 bus routing. An independent LOAD=3 scenario
checks absence of premature IRQ/reset and exact first/second expiry after four
5 ns edges per stage. These checks validate functional deadlines, not physical CDC.

## Repairs during validation

The initial static build succeeded but both tests failed. One implementation
defect was repaired on the first attempt: the event-driven counter skipped to
the expiry edge without first applying the preceding decrements. It now sets
VALUE to zero before evaluating that expiry, preserving the N+1-edge contract.
The new boundary regression prevents this failure from being hidden by coarse
waits. Two fixture defects were also repaired on their first attempt: the locked
write scenario had left unlock asserted, and the integration bench drove each
watchdog reset from both the main thread and fake CRM. Reset signals now have a
single CRM writer. No issue reached the three-attempt limit.

Ignored evidence: `build/validation-final.log` records the static/shared build,
installation and consumer runs. `build/validation-static-boundaries.log` records
the final static run with the added boundary checks. The final script's `ldd`
command had a truncated executable name; a separate successful dependency check
is recorded in `build/validation-runtime.log`. This script typo did not affect
the builds or CTest results.

## Remaining integration limits

The tests use fake CRM/interrupt peers. They do not verify physical 2-FF CDC behavior, APB signal timing, the actual SYSCSR unlock register/key, production reset scope, PLIC claim/complete, or CPU firmware boot. The current model approximates the documented 6-7 APB wait states with a configurable fixed seven-clock TLM delay.
