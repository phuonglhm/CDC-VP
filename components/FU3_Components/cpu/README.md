# FU3 CPU configuration - CPU0 (CVA6) and CPU1 (Ibex)

FU3 configuration of the two RISC-V CPUs per Common Spec v2.1: CPU0 is CVA6,
the application CPU (RV32IMFC, M-mode), and CPU1 is Ibex, the boot and
system-control CPU (RV32IMC + Zcb, U-mode, PMP/Smepmp). Both run on
`cdc::cpu::cva6_wrapper` over the FU3 ISS fork (`cpu_models/fu3_iss`); this
component supplies the memory map, reset PCs, hart IDs, ISA, interrupt causes
and per-core ISS options. See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for
the values, their sources and what is not modelled.

## Layout

- `include/fu3_cpu/cva6_config.h`: CPU0. `fu3::cva6_cpu_config()`,
  `kCva6DisableExtensions`, `fu3::cva6_core_options_fu3()`, CVA6_ISRAM/DSRAM
  map, HaltAddress/ExceptionAddress, interrupt causes.
- `include/fu3_cpu/ibex_config.h`: CPU1. `fu3::ibex_cpu_config()`,
  `kIbexDisableExtensions`, `fu3::ibex_core_options()` (fast interrupts,
  vectored mtvec, Zcb, PMP/Smepmp reset profile), ROM/IBEX_ISRAM map, Debug
  Module addresses.

Both live in namespace `cdc::cpu::fu3`.

## Connect to the bus

```cpp
#include <fu3_cpu/cva6_config.h>
#include <fu3_cpu/ibex_config.h>
using namespace cdc::cpu;
cva6_wrapper cva6{"cva6", fu3::cva6_cpu_config(), fu3::kCva6DisableExtensions,
                  fu3::cva6_core_options_fu3()};
cva6_wrapper ibex{"ibex", fu3::ibex_cpu_config(), fu3::kIbexDisableExtensions,
                  fu3::ibex_core_options()};

cva6.data_bus().bind(fabric.initiator("CVA6"));   // one port per CPU (has_unified_bus())
ibex.data_bus().bind(fabric.initiator("IBEX"));

cva6.set_reset(true);                    // released at handoff (CRM, plan T07)
cva6.set_irq(fu3::kCauseTimer, level);   // 3 software, 7 timer, 11 external
ibex.set_irq(fu3::kCauseFast0 + i, level); // irq_fast_i[i]
```

Every wrapper transaction carries `fu3_bus::Access`; fetches are aligned
4-byte words, split when the PC is only 2-byte aligned (RVC).

## Build and test

Header-only INTERFACE target `fu3::cpu`, linking `cdc_cpu_cva6`; built from the
repository root with `-DCDC_BUILD_FU3_SOC=ON`. The CPU tests are firmware
running on the platform: `platforms/fu3_soc` ctest `fu3_soc_cpu_cva6` and
`fu3_soc_cpu_ibex`; this directory adds wrapper tests with the FU3 options
(`fu3_cpu_reset_*`, `fu3_cpu_debug_*`). See [VALIDATION.md](docs/VALIDATION.md).

## Debug Mode

Both CPUs implement Debug Mode (debug spec 0.13.2, execution based) with the
v2.1 HaltAddress/ExceptionAddress; the Debug Module drives it through
`set_debug_request()`, `debug_halted()`, `debug_resume()` and
`debug_access_register()` (see the integration contract in
[MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md)).
