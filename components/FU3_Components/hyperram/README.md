# FU3 HyperRAM behavioral VP

This component models the firmware-facing HyperRAM controller, its AP_HBCTRL configuration interface, and a 32-MiB S80KS2564 memory backing store. It preserves whole TLM bursts and exercises configuration, data flow, reset, and observable errors. It is not a pin-level HyperBus PHY or a cycle-accurate AXI implementation.

**The controller CSR ABI is provisional VP ABI v1, not the hardware register map.** The supplied controller specification v0.13 and CSR proposal v0.1 do not assign CSR offsets/bits/reset values; the CSR proposal references an unavailable parent v0.14. The model makes those missing choices explicit in [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md). Do not reuse the driver offsets on hardware without reconciliation.

## Build

```sh
cmake -S components/FU3_Components/hyperram -B build/fu3-hyperram \
  -DSYSTEMC_HOME=/path/to/systemc -DCMAKE_BUILD_TYPE=Release
cmake --build build/fu3-hyperram --parallel
ctest --test-dir build/fu3-hyperram --output-on-failure
cmake --install build/fu3-hyperram --prefix /path/to/install
```

Use `BUILD_SHARED_LIBS=ON` for shared libraries. Consumers use `find_package(fu3-hyperram CONFIG REQUIRED)` and link `fu3::hyperram`; the exported package also requires `fu3-bus` and SystemC. Executables provide `sc_main`.

## Platform wiring

Call `Controller::configure_bus(config)` before constructing `BusSystem`. It enables opt-in whole-burst forwarding for HYPERBUS, disables subword widening at that endpoint, and allows eight aggregate target requests. The controller independently bounds admission at four reads and four writes; master ingress limits still apply.

Bind `bus.target("HYPERBUS")` to `controller.memory` and `bus.target("AP_HBCTRL")` to `controller.csr`. The architecture windows are `0x60000000..0x61FFFFFF` and `0x500B0000..0x500BFFFF`. Direct socket callers use local offsets. Memory requests carry `fu3_bus::Access` with 4-byte INCR beats and IDs 0..63. No DMI is exposed.

Bind active-low `reset_n` (controller only), `device_reset_n` (device plus controller invalidation), and level-high `irq`. Route IRQ in the platform according to its interrupt wiring; this component does not instantiate PLIC or CPU models. Configuration defaults to a 5-ns core period and the conservative Industrial Plus 1-us tCSM limit; the platform may select the 4-us profile explicitly.

The virtual device supports ID0/ID1/CR0/CR1 service, normal operation, linear accesses, fixed/variable latency, and byte masks. Delay codes are virtual settings only. Low-power modes, physical capture/calibration, raw AXI channel framing, and actual firmware boot are not implemented. `Fault` is a test-only payload extension for refresh indication and failures after a chosen number of words, not a firmware register.
