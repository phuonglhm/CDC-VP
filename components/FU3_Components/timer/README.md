# FU3 Timer0/1/2/3 - SystemC/TLM

Common Spec v2.0 takes precedence over older endpoint restrictions. See the
[updated model contract](docs/MODEL_CONTRACT.md) for byte strobes and remaining VP decisions.

Functional models of the Timer HDS counter, control, compare and raw interrupts,
with four-instance integration for the FU3 VP. Source comments and diagnostics
are in English. See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for exact behavior,
VP choices and the Timer-specific full-word access exception.

## Layout

- `include/fu3_timer/registers.h`: C-compatible firmware register definitions.
- `include/fu3_timer/timer.h`: single Timer and state-transition model.
- `include/fu3_timer/timer_bank.h`: four independent Timer instances.
- `tests/`: behavior and actual FU3 bus integration, with fake masters/IRQ receivers.
- `tests/consumer/`: validation through the installed CMake package.

## Connect to the bus

```cpp
fu3_timer::TimerBank timers{"timers"};
for (unsigned i=0; i<4; ++i) {
    auto& timer = *timers.timers[i];
    bus.target("TIMER" + std::to_string(i)).bind(timer.socket);
    timer.reset_n(reset_n);
    timer.clock_enabled(clock_enabled[i]);
    timer.overflow_irq(overflow[i]);
    timer.compare_irq(compare[i]);
    timer.irq(timer_irq[i]);
}
```

Bind all three output ports, including unused raw outputs to sink signals.
Each bus target receives a local offset. The Timer does not decode global base
addresses itself. Bind or disable all other bus targets before elaboration.
The default 10-ns period is configurable, not a published FU3 frequency.

For periodic compare interrupts, write CMP with a nonzero threshold, optionally
write TIMER, then set CTRL.ENABLE. CMP writes clear TIMER. An interrupt clears
through counter state at the next tick; software must not write a nonexistent
EOI/W1C register. Gating or repeated writes can extend a raw interrupt level.

## Build and test

From this directory in Linux/WSL, with a compatible SystemC installation:

```bash
cmake -S . -B build/local -DSYSTEMC_HOME=/path/to/systemc-install -DBUILD_SHARED_LIBS=ON
cmake --build build/local --parallel 4
ctest --test-dir build/local --output-on-failure
cmake --install build/local --prefix "$PWD/build/local/install"
cmake -S tests/consumer -B build/local/consumer -DCMAKE_PREFIX_PATH="$PWD/build/local/install;/path/to/systemc-install"
cmake --build build/local/consumer --parallel 4
ctest --test-dir build/local/consumer --output-on-failure
```

Use a separate build directory and BUILD_SHARED_LIBS=OFF to verify static linkage.
Set BUILD_TESTING=OFF for a standalone library build without the sibling bus.
Public target: `fu3::timer`; installed package: `fu3-timer`. Executables define
`sc_main`; the library links SystemC::systemc publicly and defines no entry point.
No parent build or existing bus implementation is modified by this component.

## Verification evidence

See [validation results](docs/VALIDATION.md) and
[HDS requirement mapping](docs/TRACEABILITY.md).
