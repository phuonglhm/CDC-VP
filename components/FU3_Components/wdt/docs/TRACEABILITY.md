# WDT V1.2 behavior traceability

Common Spec v2.0 supplies SoC windows and WDT1 PLIC ID 18; WDT0 route remains
TBD. Latest HDS references are physical PDF pages, retained privately.

| Requirement | Evidence | Verification |
|---|---|---|
| Local internal LOCK/key/read status | V1.2 pp.14-17,24-25 | Wrong/full/partial keys, zero strobes, locked writes and independent locks |
| Split reset ownership and retention | pp.20-21 | Programming-only and watchdog-only resets; restart with retained enable |
| First/second timeout and RAW/MASK | pp.22,27-28 | Independent N+1 edge boundary; second warning clear; RESEN=0 decision |
| LOAD/CLEAR service and debug pause | pp.14,22-23 | Service one bank without affecting the other; frozen count and wdogpause |
| Independent functional clock periods | pp.19-20 | 1-us watchdog with ten 2-ns APB wait periods |
| Full strobes | Common BUS INTF, overrides HDS p.16 | Sparse LOAD merge, zero writes, malformed enables and partial-key protection |
| Two WDT instances | Common MemoryMap | Both live bus windows, independent IRQ/recovery and retained settings |

The shared rebaseline record lists unresolved reset pulse, debug pin, Version
and read-snapshot details. Tests do not establish physical CDC or CPU firmware boot.
