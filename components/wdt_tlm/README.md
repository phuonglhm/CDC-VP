# TODO: update this file to reflect current structure
# ARM Watchdog Module (SP805) Virtual Platform

This project provides a SystemC TLM-2.0 Loosely Timed (LT) model of the ARM Watchdog Module (SP805), along with a testbench to verify its functional behavior.

## Overview

The SP805 Watchdog module is a 32-bit down counter that generates an interrupt on the first timeout and a reset signal on the second timeout if the interrupt remains unserviced. It also features a lock register to prevent accidental overwrites of its configuration by runaway software.

This implementation accurately captures:
- Register mapping and behavior (Load, Value, Control, IntClr, RIS, MIS, Lock, and ID registers).
- The two-stage timeout sequence (Interrupt -> Reset).
- Access protection via the `WdogLock` register.
- TLM-2.0 `b_transport` interface for memory-mapped register access.

## Directory Structure

```text
watchdog_1st_LT/
├── Makefile       # Build script for the project
├── main.cpp       # Top-level module binding the DUT and testbench
├── test.cpp       # Testbench implementation
├── test.h         # Testbench class definition
wdt_tlm.cpp   # SP805 Watchdog TLM model implementation
wdt_tlm.h     # SP805 Watchdog TLM model class definition
```

## Requirements

- SystemC library (compatible with TLM-2.0)
- A C++17 compatible compiler (e.g., GCC or Clang)
- `pkg-config` (optional, used in Makefile to locate SystemC)

## Building and Running

To build the project, simply run:
```bash
make
```

To run the simulation and tests:
```bash
make run
```
This will execute the functional tests and generate a `wdt_tlm_wave.vcd` waveform file.

## Testbench Details

The included testbench (`test.cpp`) performs a series of automated checks:
1. **First Timeout:** Verifies that the interrupt (`WDOGINT`) is asserted and the reset is not asserted on the first counter expiration.
2. **Interrupt Clear:** Tests that writing to the `WdogIntClr` register correctly clears the pending interrupt and reloads the counter.
3. **Reset Generation:** Tests that a second timeout while an interrupt is pending correctly asserts the reset signal (`WDOGRES`) if the reset enable (`RESEN`) bit is set.
4. **Lock Mechanism:** Verifies that writing `0x1ACCE551` unlocks the registers and that any other write locks them, preventing accidental modifications to crucial settings.
5. **Peripheral IDs:** Verifies that the read-only Peripheral and PrimeCell Identification registers return the expected hardcoded values.
