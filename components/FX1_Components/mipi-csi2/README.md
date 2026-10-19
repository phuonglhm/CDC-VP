# MIPI D-PHY RX (4 data lanes + clock lane) - SystemC model

SystemC model of the `mipirxphy` receiver PHY: RTL control logic, the gate-level
digital core and PLL netlists, and the behavioral analog blocks, with the same
module hierarchy as the Verilog source.

## Contents

```
include/vl/      runtime library (4-state logic type, signal helpers, timing,
                 gate primitives, UDP evaluation, drive-strength resolution)
src/top/         top level: class mipirxphy (mipirxphy.h), I/O pad wrappers
src/rtl/         RTL control blocks (interface, tester, calibration, BIST, ...)
src/rx_core/     RX PHY core (digital netlist + analog front end); rx_cells.h
                 holds the standard cells
src/pll/         PLL (digital netlist + analog model); pll_cells.h holds the
                 standard cells
src/common/      UDP tables shared by the cell libraries
src/manual/      LP receiver model (mipi_4_rx_dphy_ns_DPHYLPRX)
src/sources.txt  list of .cpp files to compile (relative to src/)
```

## Requirements

- SystemC 2.3.3 or later (tested with 2.3.4)
- C++17 compiler (GCC, Clang)

## Build

Compile `include/vl/vl_rt.cpp`, every file listed in `src/sources.txt` and
`src/manual/*.cpp`, with these include paths:

```
-I include -I src/common -I src/rtl -I src/rx_core -I src/pll -I src/top -I src/manual
```

Example (GCC, Linux, from this folder):

```sh
SYSTEMC_HOME=/path/to/systemc
INC="-Iinclude -Isrc/common -Isrc/rtl -Isrc/rx_core -Isrc/pll -Isrc/top -Isrc/manual -I$SYSTEMC_HOME/include"
SRCS="include/vl/vl_rt.cpp $(sed 's|^|src/|' src/sources.txt) $(ls src/manual/*.cpp)"
mkdir -p obj
for f in $SRCS; do g++ -std=c++17 -O1 $INC -c "$f" -o "obj/$(basename "${f%.cpp}").o" || exit 1; done
ar rcs libmipirxphy.a obj/*.o
# link your testbench with: libmipirxphy.a -L$SYSTEMC_HOME/lib -lsystemc
```

The core netlist is large; building with `-O1` keeps compile time reasonable
(about 3 minutes on 8 cores). Use parallel compilation (`make -j`, `xargs -P`).

## Using the model

```cpp
#include "mipirxphy.h"

int sc_main(int, char**) {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);   // required, see below
    mipirxphy dut("dut");
    // bind every port, then sc_start()
}
```

- **Time resolution must be 1 fs**, set before any `sc_time` is created. The
  model uses timescales down to 1 fs.
- **Port types:** 1-bit ports are `sc_logic`, buses are `sc_lv<N>`. Inputs are
  `sc_in`, outputs are `sc_out`, and pads / supplies / analog lines are
  `sc_inout<sc_logic>`.
- **Channels:** bind inout pads to resolved channels. For the lane pads
  (`MIPIRX_DATxP/N`, `MIPIRX_CLKP/N`) use `vl::sres`, which also resolves
  drive strength. The LP receivers use strength to tell LP signalling from HS
  signalling:
  - LP driving: `vl::wr_s(pad, value, vl::ST_PULL)`
  - HS driving: plain `write()` (strong)
  - released: write `SC_LOGIC_Z`

  Other buses can be bound to `sc_signal_rv<N>`, single bits to
  `sc_signal_resolved`, and inputs to ordinary `sc_signal`.
- **Supplies:** drive `MIPIRX_VDD`, `MIPIRX_VP`, `MIPIRX_VPH`, `MIPIRX_VPL` to
  1 and `MIPIRX_VSS`, `MIPIRX_AGND` to 0. Blocks check their supplies and
  produce X/Z when they are not valid.
- **Internal state** is public and named after the Verilog nets, so it can be
  inspected with `sc_find_object()` or traced with `sc_trace()`.

## Build configuration of the model

- The netlist options `BIAS_PINS` and `regulator_pins` are enabled; they are
  required for the connections between the core, PLL and I/O blocks.
- `src/gen/mipirxphy_misc.v` includes `IODIR.v`, which was not part of the
  source package. The model uses `OUTEN = 0`, so every
  `MIPIRXPHY_CONT_DATn_oen` output is 0. If the real value differs, change the
  `n_MIPIRXPHY_CONT_DATn_oen` constants in class `mipirxphy_misc`
  (`src/top/top_cells.h`).
- Functional-only netlist mode (`VIRL_functiononly`) is not enabled.
  `specify` path delays are all zero in the source and are not modeled.
  Timing checks (`$setuphold`, `$recrem`) are not modeled; their delayed
  signals follow their inputs directly.

## Verification

The model was compared cycle by cycle against the Verilog source on a
commercial Verilog simulator over a 300 us startup sequence: power-up, reset,
PLL lock, offset calibration, termination / DDL calibration and HS clock
enable. All 131 outputs were sampled every 100 ns and match, apart from the
clock start-up difference described below.

The HS data reception path (HS bursts on the data lanes) is not covered by
that sequence.

## Known difference

When the HS clock path of the clock lane is enabled, the cml2cmos converter
model produces zero-width X glitches, and the enable synchronizer in the
clock buffer (`HSRXC.CLK_BUF`) can capture one of them. Whether it does
depends on the order in which a simulator processes events within one time
step. As a result, `rxbyteclkhs` and the derived word clocks can start one
cfg_clk cycle later than in the Verilog simulation. The period and later
behavior are the same. Calibration steps timed from these clocks
(`scan_rxN_rxhs_ddl_tune_out`) shift accordingly.

In hardware this enable is asynchronous to the clock, so a start-up latency
of plus or minus one cycle is expected behavior.
