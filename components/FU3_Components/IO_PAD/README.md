# FU3 IO_PAD

Standalone SystemC model of digital package routing and IOSEL, based on
`FTEL_CDC_IP_IO_PAD_Hardware_Design_Specification.pdf`, **V1.0, 16 pages**,
supplied and reviewed on **2026-10-21**. SHA-256:
`a473c96b2f01bb5d760c22e3f6b5aaf44c72fe9dacd5972666fcd699f18f20a8`.

Link `fu3::io_pad` and include `fu3/io_pad/io_pad.h`. The public types are
`fu3::io_pad::IoPad`, `IoSel`, `Pin` and `pin_info`. This component has no GPIO,
TLM, clock or reset dependency. `GPIO` retains the former
`fu3/gpio/io_pad.h` include and `fu3::gpio` type aliases for existing clients.

| HDS reference | Digital behavior |
| --- | --- |
| Sections 6.1–6.2, p9 | Combinational operation; package position 1 routes POR input in normal mode; no internal clock/reset state |
| Section 6.3, p9 | Configurable pins use select 0 for primary and 1 for alternate; codes 2/3 return zero data and receiver paths |
| Section 6.4, p9 | Data and requested OE are separate; requested OE does not establish physical pad tri-state |
| Section 6.5, p10 | Test mode follows package position 5; test select is positions 5 AND 11; normal routing is suppressed while test inputs remain available |
| Table 7-2, p11 | Package map covers all 80 positions, including fixed functions, input-only pads, reserved positions and 24 GPIO connections |
| Sections 3/8, pp8/11–12 | No bus or internal register bank; IOSEL software controls must come from external system registers |

Vectors use package position minus one. `iosel0` covers logical positions
1–40, and `iosel1` covers 41–80; these logical names do not approve physical
side/partition control mapping. Bind all `select`, pad, function-data,
function-OE and routed-input ports, then allow SystemC delta cycles to settle.
Fixed-function pins ignore select. Supply/analog positions and reserved
positions do not receive digital routing. There is no implicit output loopback.

Run from the repository root in Linux/WSL:

```sh
cmake -S components/FU3_Components/IO_PAD -B build/fu3-io-pad-standalone \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug \
  -DFU3_IO_PAD_BUILD_TESTS=ON
cmake --build build/fu3-io-pad-standalone -j4
ctest --test-dir build/fu3-io-pad-standalone --output-on-failure
```

The standalone tests check all 80 positions, four selects, primary/alternate
data and requested enables, test-mode suppression/recovery and 1,800 seeded
random vectors against an independent pin table, without any GPIO instance.
Additional metadata and isolation scenarios verify every GPIO bank/bit/select
mapping and change each pin's five source signals while checking all other
positions. Delta-cycle testing adds nonzero startup, every old/new select
pair, independent source changes, decoder input ordering and two-instance
isolation without advancing simulation time. API checks cover out-of-range
metadata and rejection of an invalid logical partition.

The independent IO_PAD review on **2026-10-21** passed **6/6** scenarios in
Debug, Release and UBSan; the combined GPIO/IO_PAD UBSan suite passed
**32/32**. Model line coverage reached **94/94**, and tests detected all
eight selected deliberately incorrect implementation copies. See the
[IO_PAD verification report](docs/verification.md) for requirement evidence,
coverage limits, commands and local logs, and the
[integration test instructions](../GPIO/docs/testing.md).

Wake belongs to [GPIO](../GPIO/README.md): its `fu3::gpio::Subsystem`
connects the selected pad receiver to GPIO input, exports each bank's power
event and ORs the three bank outputs into aggregate wake. The integration
test `fu3_gpio_wake_iosel` verifies this path for every bonded GPIO and select
code, test mode and simultaneous banks.

Foundry pad cells, analog/supply behavior, pulls, drive strength, physical
tri-state and electrical control polarity require PDK/wrapper models. The
six GPIO PADCFG bits remain exposed by the subsystem, without an invented
mapping to these external controls. Section 2's contradictory introductory
wording and sections 9–11's template gaps remain document issues; concrete
sections 6–7 define the digital behavior used here.
