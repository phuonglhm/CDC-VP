# FU3 CRM - SystemC/TLM

Functional model of the FU3 Clock Reset Module per FU3 HAS v2.0 and the FU3 CRM Specification V1.1 (CSR_CRM_fu3_v2): register map, PLL and the CPU/bus clock selection and divider, per-IP clock gates and software resets, separate CPU1 (Ibex) and CPU0 (CVA6) resets, the startup sequence and the system resets required by HAS section 6.5.
The CRM ROM is not executed: a SystemC thread performs the startup writes and the HAS reset behavior. See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for exact behavior and the open points.

## Layout

- `include/fu3_crm/registers.h`: C-compatible base, offsets and bits.
- `include/fu3_crm/crm.h`: the `fu3_crm::Crm` model.
- `tests/`: direct behavior and actual FU3 bus integration (shared `bench.h`), `run_all.sh`.
- `tests/consumer/`: validation through the installed CMake package.

## Connect to the bus

```cpp
#include <fu3_crm/crm.h>
fu3_crm::Crm crm{"crm"};
bus.target("CRM").bind(crm.socket);             // PERIBUS_1, 0x5001_0000, local offsets
crm.prst_n(prst_n);                             // PRSTN pad
crm.wdt_reset_req(wdt_reset_req);               // WDT0.reset_request | WDT1.reset_request
crm.cpu_reset_req(ndmreset);                    // DBG ndmreset, tie low until DBG exists
crm.sw_reset_req(syscsr_sw_reset_req);          // SYSCSR outputs
crm.sw_global_rst_req(syscsr_sw_global_rst_req);
crm.sys_rst_n(sys_rst_n);                       // -> buses, memories, peripherals
crm.ibex_rst_n(ibex_rst_n);                     // -> CPU1 (Ibex) reset
crm.cva6_rst_n(cva6_rst_n);                     // -> CPU0 (CVA6) reset
crm.dbg_rst_n(dbg_rst_n);                       // -> DBG
crm.irq(irq_crm);                               // -> PLIC Source ID 1, INTMAP kSrcCrm
crm.clk_gate_0(gate0); crm.clk_gate_1(gate1);   // sc_signal<std::uint32_t>
crm.sw_rst_n_0(rst0);  crm.sw_rst_n_1(rst1);
```

After PRSTN the CRM releases the system and DBG, then CPU1 (Ibex) 1.2 us later. CPU0 (CVA6) stays in reset until Ibex software writes 1 to FCLK_SW.
`crm.cpu_clock_hz()` and `crm.bus_clock_hz()` give the current clocks.

## Build and test

Run everything (static/shared builds, both benches, installed consumers):

```bash
tests/run_all.sh        # SYSTEMC_HOME defaults to /usr/local/systemc-2.3.4
```

Set `BUILD_TESTING=OFF` for a standalone library build without the sibling bus.
Public target: `fu3::crm`, installed package: `fu3-crm`.

## Verification evidence

See [validation results](docs/VALIDATION.md) and [specification traceability](docs/TRACEABILITY.md).
