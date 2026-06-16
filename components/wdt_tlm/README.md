# wdt_tlm

SystemC/TLM-2.0 model of an ARM SP805-style watchdog timer. The component
provides a memory-mapped target socket, an interrupt output, a watchdog reset
output, and an active-low hardware reset input.

## Requirements

- SystemC **2.3.4** is required and is the only supported SystemC version for
  this project.
- A C++17 compiler.
- The repository top-level CMake configuration, which defines the imported
  `SystemC::systemc` target.

## Directory Structure

```text
components/wdt_tlm/
├── CMakeLists.txt
├── README.md
├── include/
│   └── wdt_tlm.h
├── src/
│   └── wdt_tlm.cpp
└── tests/
    ├── CMakeLists.txt
    ├── test.cpp
    ├── test.h
    └── test_wdt_tlm.cpp
```

This layout follows the same component structure used by `adc_tlm`: public
headers in `include/`, implementation files in `src/`, and CMake-driven tests
in `tests/`.

## Hardware Interface

- `target_socket`: TLM target socket for memory-mapped register access.
- `reset_n`: active-low hardware reset input.
- `irq`: interrupt output, asserted on the first watchdog timeout when
  interrupts are enabled.
- `reset_o`: watchdog-generated reset output, asserted on the second unserviced
  timeout when reset generation is enabled.

The active-low `reset_n` input resets all internal watchdog registers, counters,
interrupt flags, lock state, and output levels back to their default hardware
state.

## Register Behavior

The model implements the key SP805 watchdog registers:

- `WDOG_LOAD`
- `WDOG_VALUE`
- `WDOG_CONTROL`
- `WDOG_INTCLR`
- `WDOG_RIS`
- `WDOG_MIS`
- `WDOG_LOCK`
- Peripheral ID and PrimeCell ID registers

The watchdog follows the usual two-stage timeout behavior:

1. First timeout asserts `irq`.
2. If the interrupt remains pending and reset generation is enabled, the next
   timeout asserts `reset_o`.

## Build and Run Tests

From the repository root, configure the CMake build tree with tests enabled:

```bash
cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_BUILD_TESTS=ON
```

Build the WDT testbench:

```bash
cmake --build build/bremen --target test_wdt_tlm
```

Run the test through CTest:

```bash
ctest --test-dir build/bremen -R wdt_tlm --output-on-failure
```

You can also run the test executable directly:

```bash
./build/bremen/components/wdt_tlm/tests/test_wdt_tlm
```

## Testbench Explanation

The WDT testbench is split into two files:

- `tests/test_wdt_tlm.cpp`: top-level SystemC test executable. It instantiates
  the DUT, instantiates the testbench, creates signals, binds sockets/ports, and
  applies the initial hardware reset sequence.
- `tests/test.cpp`: functional stimulus and checks. It drives register
  transactions through a TLM initiator socket and checks `irq`, `reset_o`, and
  register readback values.

The top-level test creates these signals:

- `rst_n_sig`: active-low hardware reset input for the DUT.
- `irq_sig`: watchdog interrupt output.
- `reset_sig`: watchdog-generated reset output.

The testbench then binds:

- `tb.initiator_socket -> dut.target_socket`
- `tb.reset_n -> rst_n_sig`
- `tb.irq -> irq_sig`
- `tb.reset_i -> reset_sig`
- `dut.reset_n -> rst_n_sig`
- `dut.irq -> irq_sig`
- `dut.reset_o -> reset_sig`

After reset is released, the functional test runs five checks:

1. **First Timeout**
   - Writes `WDOG_LOAD = 5`.
   - Enables interrupt generation with `WDOG_CONTROL.INTEN`.
   - Waits long enough for the counter to expire.
   - Expects `irq == true`, `reset_o == false`, `WDOG_RIS == 1`, and
     `WDOG_MIS == 1`.

2. **Interrupt Clear**
   - Writes `WDOG_INTCLR`.
   - Expects the pending interrupt to clear.
   - Verifies `irq == false`, `WDOG_RIS == 0`, `WDOG_MIS == 0`, and the counter
     reloads from `WDOG_LOAD`.

3. **Reset Generation**
   - Lets another interrupt become pending.
   - Enables both `INTEN` and `RESEN`.
   - Waits for the next unserviced timeout.
   - Expects `reset_o == true`.
   - Verifies the counter stops while watchdog reset is asserted.

4. **Lock Mechanism**
   - Writes `WDOG_LOAD` while unlocked and confirms the write is accepted.
   - Writes an invalid lock value to lock the watchdog.
   - Confirms writes are ignored while locked.
   - Writes `0x1ACCE551` to unlock.
   - Confirms writes are accepted again.

5. **Peripheral ID Registers**
   - Reads the SP805 peripheral ID and PrimeCell ID registers.
   - Verifies each register returns the expected constant value.

The test ends by printing:

```text
[TB] Result: PASS (0 error(s))
```

or a failure count if any check fails.

## Testbench Reset Flow

The testbench binds a `sc_core::sc_signal<bool> rst_n_sig` to `wdt_tlm::reset_n`.
At the start of simulation it:

1. Drives `rst_n_sig = false` to assert reset.
2. Runs simulation for a few nanoseconds.
3. Drives `rst_n_sig = true` to deassert reset.
4. Applies normal watchdog register stimulus.

This mirrors the active-low reset style used by the current component standard.
