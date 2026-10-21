# FU3 WDT0/1 - SystemC/TLM

Behavioral watchdog model based on Common Spec v2.0 (highest authority) and
the latest WDT HDS V1.2. Two independent watchdogs use 64-KiB windows at
0x44050000 and 0x44080000 on PERIBUS_0. WDT1 warning goes to PLIC ID 18;
WDT0 Ibex routing/reset escalation remains provisional in Common.

Each Watchdog binds `socket`, `preset_n`, `wdogres_n`, `irq` and `reset_request`.
Optional `wdogpause` publishes the software-selected DBGEN pause. The obsolete
V1.1 `i_wdt_other_unlock` binding is optional and ignored: no SYSCSR unlock
register is required or invented. Firmware writes WD_LOCK at local 0x1000:
0x1ACCE551 unlocks; other nonempty writes lock; reads return 0 unlocked/1 locked.
Program LOAD and CONTROL, then lock. Service by unlocking and writing LOAD or
INT_CLR. Common Full strobes are applied internally without bus RMW.

Programming reset restores LOCK/LOAD/CONTROL; watchdog-only reset clears
runtime state and reloads retained LOAD. First expiry warns; second clears
the warning and requests reset when RESEN is set. The platform owns recovery
scope and actual IRQ/reset wiring. No cycle-accurate CDC or CPU boot is claimed.

```bash
cmake -S . -B build -DSYSTEMC_HOME=/path/to/systemc -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Use BUILD_SHARED_LIBS=ON for shared output. The installed target is `fu3::wdt`.
See [model contract](docs/MODEL_CONTRACT.md), [traceability](docs/TRACEABILITY.md)
and the bus [joint validation](../bus/docs/VALIDATION.md).
