# FU3 GPIO

32-bit GPIO model with TLM and signal-level APB interfaces, plus three-bank
integration with IO_PAD, based on
`FTEL_CDC_IP_GPIO_Hardware_Design_Specification-1.pdf`, V1.0, 19 pages.
Reviewed on 2026-10-21; source SHA-256:
`82c3ebc68ff9031f0bcde028da42d3d86816750985286909230e4d1608916cb1`.

Link `fu3::gpio` and use the interface required by the platform:

| Header / type | Interface |
| --- | --- |
| `fu3/gpio/gpio.h`, `Gpio` | Clocked TLM target |
| `fu3/gpio/apb.h`, `ApbGpio<AddressWidth = 12>` | HCLK/HRESETn, PADDR/PWDATA/PWRITE/PSEL/PENABLE, PRDATA/PREADY/PSLVERR |
| `fu3/gpio/subsystem.h`, `Subsystem` | Three TLM banks, IO_PAD wiring, per-bank and aggregate wake |

All types above are in `fu3::gpio`. TLM and APB use the same digital state
machine in `src/state.cpp`. GPIO adds and links the
separate [IO_PAD](../IO_PAD/README.md) library automatically. There are only
two components and two implementation libraries: `fu3::gpio` and `fu3::io_pad`.
The core owns the register bank, three-stage input pipeline, latched interrupt,
six-bit pad-configuration outputs and raw `power_event` output. With multiple
POWEREVENT bits selected, the highest-numbered selected pin determines the
output. PADDIR and PADOUT are independent digital outputs.

## Build and test

Run from the repository root in Linux/WSL:

```sh
cmake -S components/FU3_Components/GPIO -B build/fu3-gpio-standalone \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug \
  -DFU3_GPIO_BUILD_TESTS=ON -DFU3_IO_PAD_BUILD_TESTS=ON
cmake --build build/fu3-gpio-standalone -j4
ctest --test-dir build/fu3-gpio-standalone --output-on-failure
```

The full suite has 30 scenarios: eight direct-core tests, twelve integration
tests, six APB tests and four standalone IO_PAD tests. Final clean rebuilds
passed **30/30** in both Debug and UBSan on 2026-10-21; a separate IO_PAD
build passed **4/4**. See [registers](docs/registers.md),
[timing and transport](docs/usage.md#gpio-transport),
[testing](docs/testing.md) and the detailed [verification report](docs/verification.md).
The recorded review covers 27 digital HDS requirements and four VP contracts in both
runs. Seven integration/electrical requirements remain explicitly external.

The APB fixture checks widths 6, 12 and 128, all register access rules, setup
versus access, back-to-back/held transfers, combinational PRDATA, constant
PREADY/PSLVERR, all 32 pins/four IRQ modes, reset and 1,536 seeded cycles.
Model coverage executed 315/316 executable lines; the remaining line is the
VP guard rejecting an invalid logical IOSEL partition constructor argument.
Coverage is execution evidence, not exhaustive state-space or RTL equivalence.

## Wake and IOSEL

IOSEL remains inside IO_PAD. The GPIO subsystem routes selected package inputs
to banks A/B/C and exports `wake_bank[3]`. Each bank keeps highest-selected-pin
POWEREVENT priority; aggregate `wake` ORs the three bank outputs. Unassigned
select codes on configurable pins and test mode suppress normal GPIO routing.
`fu3_gpio_wake_iosel` checks all 24 bonded GPIOs, all select codes/test modes
and simultaneous wake sources. Aggregation is a board policy, not a separate
WAKE register model.

## Specification boundaries

The IO_PAD baseline is V1.0, 16 pages, SHA-256
`a473c96b2f01bb5d760c22e3f6b5aaf44c72fe9dacd5972666fcd699f18f20a8`;
see its [README](../IO_PAD/README.md) for the PDF requirements. Physical pad
electrical behavior, PADDIR/PADCFG electrical mapping and IOSEL system registers
require external integration sources. `ApbGpio` covers the HDS pin-level digital
APB behavior; existing TLM platforms continue to use `Gpio` without APB wires.

The legacy CMake name `fu3::gpio_io` aliases `fu3::gpio`. The legacy
`fu3/gpio/io_pad.h` header aliases pad types into `fu3::gpio`; existing clients
can migrate without a separate wrapper component.
