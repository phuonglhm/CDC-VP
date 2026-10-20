# FU3 WDT0/1 - SystemC/TLM

Behavioral watchdog models for software-visible timeout, warning interrupt, SYSCSR write protection and reset-recovery flow. One reusable `fu3_wdt::Watchdog` class serves both instances; `WatchdogBank` creates WDT0 and WDT1 with independent state and configurable functional clock periods.

The register map and behavior follow WDT HDS V1.1. WDT writes are controlled by the external `i_wdt_other_unlock` input; the lock register belongs to SYSCSR. The model abstracts the two clock domains using a functional watchdog period and a configurable seven-cycle APB access delay. It does not model physical CDC flops or APB waveforms. See the [model contract](docs/MODEL_CONTRACT.md) and [traceability](docs/TRACEABILITY.md).

## Integration

```cpp
#include <fu3_wdt/wdt_bank.h>
fu3_wdt::WatchdogBank watchdogs{"watchdogs"};
// Bind the existing FU3 bus targets for WDT0 and WDT1.
bus.target("WDT0").bind(watchdogs.watchdogs[0]->socket);
bus.target("WDT1").bind(watchdogs.watchdogs[1]->socket);

sc_core::sc_signal<bool> preset_n{"preset_n"};
sc_core::sc_signal<bool> wdog_reset_n{"wdog_reset_n"};
sc_core::sc_signal<bool> wdt0_unlocked{"wdt0_unlocked"};
sc_core::sc_signal<bool> wdt1_unlocked{"wdt1_unlocked"};
watchdogs.watchdogs[0]->preset_n(preset_n);
watchdogs.watchdogs[0]->wdogres_n(wdog_reset_n);
watchdogs.watchdogs[0]->i_wdt_other_unlock(wdt0_unlocked);
// Bind the corresponding reset and unlock inputs for WDT1 as well.
```

Both WDTs are on PERIBUS_0. Route WDT0 IRQ to the Ibex NMI path, WDT1 IRQ to PLIC source 18, and each `reset_request` to the platform reset controller. The supplied SYSCSR workbook does not yet describe the WDT unlock register, so the platform must provide the unlock signal until that interface is specified.

Firmware sequence: unlock the watchdog through SYSCSR, write `WD_CNT_LD`, write `WD_CNT_CTRL` with RESEN/INTEN/DBGEN as required, then lock it. Service by unlocking and writing `WD_CNT_LD` or `WD_INT_CLR`, then locking again. Constants are in [registers.h](include/fu3_wdt/registers.h).

## Build and validate

Run from this directory in Linux/WSL with CMake and a compatible SystemC installation:

```bash
cmake -S . -B build -DSYSTEMC_HOME=/absolute/path/to/systemc-install -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

For shared-library validation, repeat with a separate build directory and
`-DBUILD_SHARED_LIBS=ON`. Validate the installed package for either build:

```bash
cmake --install build --prefix "$PWD/build/install"
cmake -S tests/consumer -B build/consumer -DCMAKE_PREFIX_PATH="$PWD/build/install;/absolute/path/to/systemc-install"
cmake --build build/consumer --parallel 4
ctest --test-dir build/consumer --output-on-failure
```

The installed consumer links only `fu3::wdt` and runs the behavior regression.
See [VALIDATION.md](docs/VALIDATION.md) for execution results and remaining limits.

Consumers use `find_package(fu3-wdt CONFIG REQUIRED)` and link `fu3::wdt`. The production library has no dependency on the bus library; the integration test links both. The TLM target accepts aligned full-word accesses at offsets `0x1004..0x101C`; reserved/misaligned accesses return TLM address errors.
