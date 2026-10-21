# FU3 SYSCSR - SystemC/TLM

Functional model of the FU3 system control/status registers (`m_fpt_syscsr`):
IO pad configuration, boot/mode straps captured at reset release and live test status, software reset requests to CRM, oscillator and ROM control.
See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for exact behavior, VP choices and open specification questions.

## Layout

- `include/fu3_syscsr/registers.h`: C-compatible firmware offsets and bits.
- `include/fu3_syscsr/syscsr.h`: the `fu3_syscsr::Syscsr` model.
- `tests/`: direct behavior and actual FU3 bus integration (shared `bench.h`).
- `tests/consumer/`: validation through the installed CMake package.

## Connect to the bus

```cpp
#include <fu3_syscsr/syscsr.h>
fu3_syscsr::Syscsr syscsr{"syscsr"};
bus.target("SYSCSR").bind(syscsr.socket);     // PERIBUS_0, 0x4401_0000, local offsets
syscsr.reset_n(reset_n);
syscsr.boot(boot); 
syscsr.mode_0(mode_0); 
syscsr.mode_1(mode_1);   // IO_PAD straps (captured at reset release)
syscsr.test_mode(io_pad.test_mode); 
syscsr.test_mode_sel(io_pad.test_select);
syscsr.sw_reset_req(crm_sw_reset_req);               // level, CRM detects rising edge
syscsr.sw_global_rst_req(crm_sw_global_rst_req);
```

Bind all ports. Pad configuration registers (`*_io_cfg`, `*_pad_ctrl_*`) have no port yet; consumers such as IOSEL can call `read_register(offset)` until the pad mapping is specified. Call `b_transport` from an `SC_THREAD`.

## Build and test

Run everything (static/shared builds, both benches, installed consumers):

```bash
tests/run_all.sh        # SYSTEMC_HOME defaults to /usr/local/systemc-2.3.4
```

Or step by step from this directory in Linux/WSL, with a compatible SystemC installation:

```bash
cmake -S . -B build/local -DSYSTEMC_HOME=/path/to/systemc-install -DBUILD_SHARED_LIBS=ON
cmake --build build/local --parallel 4
ctest --test-dir build/local --output-on-failure
cmake --install build/local --prefix "$PWD/build/local/install"
cmake -S tests/consumer -B build/local/consumer -DCMAKE_PREFIX_PATH="$PWD/build/local/install;/path/to/systemc-install"
cmake --build build/local/consumer --parallel 4
ctest --test-dir build/local/consumer --output-on-failure
```

Use a separate build directory and `BUILD_SHARED_LIBS=OFF` to verify static linkage.
Set `BUILD_TESTING=OFF` for a standalone library build without the sibling bus.
Public target: `fu3::syscsr` and installed package: `fu3-syscsr`.

## Verification evidence

See [validation results](docs/VALIDATION.md) and [specification traceability](docs/TRACEABILITY.md).
