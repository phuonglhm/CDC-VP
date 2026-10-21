# FU3 CPU configuration - CPU0 (CVA6) and CPU1 (Ibex)

FU3 configuration of the two RISC-V CPUs: CPU0 is CVA6, the application CPU, and CPU1 is Ibex, the boot and system-control CPU.
Both CPUs reuse the existing `cdc::cpu::cva6_wrapper` (Bremen RV32 ISS) in `cpu_models/cva6_vp` and this component only supplies the FU3 memory map, reset PCs, hart IDs, enabled ISA extensions and interrupt causes. See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for spec mapping and the features the ISS does not provide yet.

## Layout

- `include/fu3_cpu/cva6_config.h`: CPU0. `fu3::cva6_cpu_config()`, `kCva6DisableExtensions`, CVA6_ISRAM/DSRAM map, HaltAddress/ExceptionAddress, CVA6 interrupt causes.
- `include/fu3_cpu/ibex_config.h`: CPU1. `fu3::ibex_cpu_config()`, `kIbexDisableExtensions`, ROM/IBEX_ISRAM map, Debug Module addresses.

Both live in namespace `cdc::cpu::fu3`.

## Connect to the bus

```cpp
#include <fu3_cpu/cva6_config.h>
#include <fu3_cpu/ibex_config.h>
using namespace cdc::cpu;
cva6_wrapper cva6{
    "cva6", 
    fu3::cva6_cpu_config(), 
    fu3::kCva6DisableExtensions};

cva6_wrapper ibex{
    "ibex", 
    fu3::ibex_cpu_config(), 
    fu3::kIbexDisableExtensions};

cva6.data_bus().bind(fabric.initiator("CVA6"));   // one port per CPU (has_unified_bus())
ibex.data_bus().bind(fabric.initiator("IBEX"));

cva6.set_reset(true);        // CVA6 waits until Ibex has staged its image
ibex.load_elf("rom.elf");    // loader uses transport_dbg through the bus

/* Boot controller: fabric.finish_staging(); 
                    cva6.set_reset(false); */
cva6.set_irq(fu3::kCauseTimer, level);   // 3 software, 7 timer, 11 external
```

Every wrapper transaction carries `fu3_bus::Access` and fetches are aligned 4-byte words, split when the PC is only 2-byte aligned (RVC).

## Build

Header-only INTERFACE target `fu3::cpu`, linking `cdc_cpu_cva6`. It is built from the repository root, because the wrapper needs the top-level paths:

```bash
cmake -S /path/to/CDC-VP -B build -DSYSTEMC_HOME=/path/to/systemc-install -DCDC_BUILD_FU3_SOC=ON
cmake --build build --target cdc_cpu_cva6
```

There is no installed package and no `tests/` directory: the CPU harness used to validate this configuration stays outside the repository by project decision. Results are recorded in [VALIDATION.md](docs/VALIDATION.md).
