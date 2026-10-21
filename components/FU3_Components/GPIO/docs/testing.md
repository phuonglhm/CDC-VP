# Testing

The suite runs on the host with SystemC; no board, FPGA or firmware is required.
GPIO/subsystem CTest names use `fu3_gpio_` followed by the suffix below.
Standalone IO_PAD names use `fu3_io_pad_`. The full suite has 30 scenarios.

## Test scenarios

| CTest suffix | Behavior checked |
| --- | --- |
| registers | Three-bank reset maps, masks, ignored RO writes, walking data/direction, pad fields, isolation |
| sampling | Exact three-stage pipeline in each bank; input independent of direction/output |
| interrupts | Three banks × 32 pins × four modes; assertion cycle, RC, level reassertion, no edge repetition; simultaneous vector, frozen status, coincident lost edge |
| wake | Three banks × 32 bits; raw assertion/deassertion, priority, board OR, IRQ independence |
| protocol | Aliases; invalid command, byte enables, lengths, streaming, null pointer; debug; delay |
| reset | Asynchronous restoration with pending IRQ; reset-time rejection; release sampling |
| concurrency | Same-bank serialization; reset abort/recovery; different banks at same edge; reset releases pending and mutex-queued callers |
| reset_pulse | A reset pulse ending before mutex release invalidates both queued writes; new accesses recover |
| iosel | Package counts, codes, fixed functions, input-only cells, test decoder and suppression |
| integration | Independent 24-pin map; data/direction, package input → PADIN/IRQ/wake, mux disconnection |
| wake_iosel | All 24 GPIO package positions, four selects and two test modes; raw bank/aggregate wake, PADIN routing, simultaneous banks and final-source disconnection |
| random | 5,400 seeded cycles; three per-pin reference machines; 48 registers and IRQ/sync/wake after every access |
| spec_register_outputs | Direct 32-bit data/direction ports; all 64 values for each of 96 PADCFG fields; neighbors and banks preserved |
| spec_input_stream | Consecutive walking/random inputs; pipeline; unsampled sub-cycle and sampled one-cycle pulses |
| spec_irq_boundaries | Empty RC/event coincidence, ignored W1C, invalid reads, pending reconfiguration, old-enable semantics, per-pin masking, disabled edges, mixed modes, opposite edges |
| spec_irq_histories | 3,072 six-cycle traces: all 64 binary input histories, mixed per-pin modes, four RC schedules and varied INTEN masks across three banks; expected pipeline samples indexed directly from stimulus history |
| spec_wake_priority | Every highest-selected pin in each bank; selected/unselected inputs; stopped clock; isolation; zero mask |
| spec_clock_reset | No pre-edge write; stopped-clock retention/raw wake; access abort; full reset without clock; held reset over edges; release pipeline |
| fu3_io_pad_truth | Independent pin table; four codes × 32 data/enable/input combinations × two test-mode states × 80 positions; recovery, POR/straps/oscillator |
| fu3_io_pad_random | 1,800 seeded vectors over 80 positions and both logical partitions; independent simultaneous changes; no GPIO or clock/reset dependency |
| spec_address_decode | Writable words except INTEN with four low-bit × four upper-address aliases per bank; aliased status RC |
| apb_registers | APB reset map and all access/mask rules, walking data/direction, independent input sampling; widths 6/12/128 |
| apb_phases | Idle/setup/access qualification, aborted setup, back-to-back/held transfers, clocked writes, combinational PRDATA and constant response pins |
| apb_interrupts | 32 pins x four modes x three address widths; qualified RC, ignored status writes, level reassertion, empty RC/event and concurrent-clear edge loss |
| apb_sampling_wake | Exact pipeline stages and raw highest-selected wake while clock stopped, including zero selection |
| apb_reset | Asynchronous reset during selected access, held reset priority, all register/output reset values and release pipeline |
| apb_random | 1,536 seeded pin-level cycles against input-history/truth-table oracle; PSEL/PENABLE/PWRITE gating, aliases, data/IRQ/wake/config |
| fu3_io_pad_metadata | Independent GPIO bank/bit/select map for every package position and specified digital/input-only/configurable/GPIO counts |
| fu3_io_pad_isolation | Change each non-test-mode pin source and select while checking all 80 positions; repeated shared test-mode entry/exit |
| spec_address_writes | 960 aliased writes across all 16 words and three banks, including 64-bit TLM aliases, ignored PADIN/INTSTATUS writes, reserved-bit masks, adjacent words and output ports |

The GPIO reference model evaluates each pin independently of the implementation.
Expected package maps are independent of `pin_info`. The direct-core fixture
observes all 32 bits per bank; the standalone IO_PAD fixture drives primary and
alternate data and requested-enable inputs independently.

## Run the suite

Build using the [README commands](../README.md#build-and-test), then run these
commands from the repository root in Linux/WSL:

```sh
# Run all 30 scenarios
ctest --test-dir build/fu3-gpio-standalone --output-on-failure

# List scenarios without running them
ctest --test-dir build/fu3-gpio-standalone -N

# Inspect an individual scenario
ctest --test-dir build/fu3-gpio-standalone -R '^fu3_gpio_interrupts$' -V

# Run the 18 spec scenarios
ctest --test-dir build/fu3-gpio-standalone -L spec --output-on-failure

# Run each fixture directly
./build/fu3-gpio-standalone/test_fu3_gpio_integration reset_pulse
./build/fu3-gpio-standalone/test_fu3_gpio_spec irq_boundaries
./build/fu3-gpio-standalone/test_fu3_gpio_apb phases
./build/fu3-gpio-standalone/io_pad/test_fu3_io_pad pad_truth
```

Rebuild after changing code or tests. A successful complete run ends with:

```text
100% tests passed, 0 tests failed out of 30
```

Failures report the scenario, simulation time and assertion message. Use `-V`
to inspect details. Each scenario has a 30-second timeout. If CTest finds no
tests, check the build directory, `FU3_GPIO_BUILD_TESTS=ON` and
`FU3_IO_PAD_BUILD_TESTS=ON`. Each standalone component has its own build
commands in the [GPIO](../README.md) and [IO_PAD](../../IO_PAD/README.md) READMEs.

Without an argument, `test_fu3_gpio_integration` runs `registers` and
`test_fu3_gpio_spec` runs `irq_boundaries`; `test_fu3_gpio_apb` runs `phases`;
`test_fu3_io_pad` runs `pad_truth`.
Each binary runs only one scenario per invocation.
Direct spec-binary arguments omit the `spec_` prefix shown in CTest names.

## Test sources and helpers

- [test_integration.cpp](../tests/test_integration.cpp): subsystem integration, twelve scenarios.
- [GPIO test_spec.cpp](../tests/test_spec.cpp): direct GPIO only, eight scenarios.
- [test_apb.cpp](../tests/test_apb.cpp): pin-level APB at widths 6/12/128, six scenarios.
- [IO_PAD test_io_pad.cpp](../../IO_PAD/tests/test_io_pad.cpp): standalone IO_PAD only, four scenarios.

The subsystem fixture provides these helpers:

| Helper | Purpose |
| --- | --- |
| `wr(bank, offset, value)` | Write a register; bank 0/1/2 means A/B/C. |
| `rd(bank, offset)` | Normal TLM read, including INTSTATUS side effects. |
| `peek(bank, offset)` | Non-destructive debug read. |
| `raw(bank, value)` | Drive GPIO inputs through the fixture wiring. |
| `tick(n)` | Wait for n rising edges and allow signals to settle. |
| `check(condition, message)` | Record an assertion result. |

For example, this checks the GPIOA output latch; `integration` also checks
routing to the package pin:

```cpp
wr(0, reg::PADOUT, 1);
check(rd(0, reg::PADOUT) == 1, "GPIOA PADOUT bit0");
```

## Optional instrumentation

Enable GCC coverage by reconfiguring the build:

```sh
cmake -S components/FU3_Components/GPIO -B build/fu3-gpio-standalone \
  -DFU3_GPIO_COVERAGE=ON
cmake --build build/fu3-gpio-standalone -j4
ctest --test-dir build/fu3-gpio-standalone --output-on-failure
cd build/fu3-gpio-standalone
gcov -b CMakeFiles/fu3_gpio.dir/src/state.cpp.gcda
gcov -b CMakeFiles/fu3_gpio.dir/src/gpio.cpp.gcda
gcov -b io_pad/CMakeFiles/fu3_io_pad.dir/src/io_pad.cpp.gcda
gcov -b CMakeFiles/fu3_gpio.dir/src/subsystem.cpp.gcda
gcov -b CMakeFiles/test_fu3_gpio_apb.dir/tests/test_apb.cpp.gcda
```

For a separate UBSan build, run from the repository root:

```sh
cmake -S components/FU3_Components/GPIO -B build/fu3-gpio-ubsan \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so \
  -DCMAKE_BUILD_TYPE=Debug -DFU3_GPIO_BUILD_TESTS=ON \
  -DFU3_GPIO_COVERAGE=OFF \
  '-DCMAKE_CXX_FLAGS=-fsanitize=undefined -fno-sanitize-recover=undefined -Wall -Wextra -Wpedantic -Werror'
cmake --build build/fu3-gpio-ubsan -j4
ctest --test-dir build/fu3-gpio-ubsan --output-on-failure
```

## Recorded review

The [verification results](verification.md) record the completed specification
review and successful Debug/UBSan runs. Temporary requirement maps and review
helper scripts were removed after verification; the functional tests remain.

To save test results locally, run from the repository root in Linux/WSL:

```sh
ctest --test-dir build/fu3-gpio-standalone --output-on-failure --output-junit debug-results.xml
ctest --test-dir build/fu3-gpio-ubsan --output-on-failure --output-junit ubsan-results.xml
```

The supplied HDS does not approve PADDIR polarity, electrical PADCFG mapping,
physical pad cells or external IOSEL/SoC registers. Their explicit external
status prevents test results from being reported as electrical/SoC sign-off.
