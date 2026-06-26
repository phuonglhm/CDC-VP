# PWM IP TLM Model

## Overview

This project implements a simplified **OpenTitan-style PWM IP TLM model** using **SystemC** and **TLM-2.0**.

The PWM IP is modeled as a **TLM target**. A SystemC testbench acts as a simple CPU master and accesses the PWM registers through memory-mapped TLM transactions.

The model focuses on the main PWM behavior:

* Register read/write through TLM
* Counter enable/disable
* PWM channel enable/disable
* Duty cycle control
* Phase delay
* Output inversion
* PWM waveform generation
* VCD waveform tracing

This model currently implements **channel 0 waveform generation**. The register map includes the OpenTitan PWM register layout for 6 channels, but only channel 0 is actively driven to `pwm_out`.

---

## Architecture

```text
+------------------+
|   Testbench      |
| (CPU Initiator)  |
+---------+--------+
          |
          | TLM Transactions
          |
          v
+------------------+
|      PWM IP      |
|  Target Socket   |
|  Register Bank   |
|  PWM Engine      |
+---------+--------+
          |
          v
       pwm_out
```

---

## OpenTitan PWM Concept

The PWM period is not configured by a separate `PERIOD` register.

Instead, it is derived from the `CFG` register:

```text
CFG[31]    = CNTR_EN
CFG[30:27] = DC_RESN
CFG[26:0]  = CLK_DIV
```

The PWM period is calculated as:

```text
period = 2^(DC_RESN + 1) * (CLK_DIV + 1)
```

Example:

```text
DC_RESN = 7
CLK_DIV = 0

period = 2^(7 + 1) * (0 + 1)
       = 256
```

---

## Register Map

| Register        | Offset | Description                                    |
| --------------- | ------ | ---------------------------------------------- |
| `ALERT_TEST`    | `0x00` | Alert test register                            |
| `REGWEN`        | `0x04` | Register write enable                          |
| `CFG`           | `0x08` | Counter enable, duty resolution, clock divider |
| `PWM_EN`        | `0x0C` | PWM output enable register                     |
| `INVERT`        | `0x10` | PWM output inversion register                  |
| `PWM_PARAM_0`   | `0x14` | Channel 0 phase delay and mode configuration   |
| `PWM_PARAM_1`   | `0x18` | Channel 1 parameter register                   |
| `PWM_PARAM_2`   | `0x1C` | Channel 2 parameter register                   |
| `PWM_PARAM_3`   | `0x20` | Channel 3 parameter register                   |
| `PWM_PARAM_4`   | `0x24` | Channel 4 parameter register                   |
| `PWM_PARAM_5`   | `0x28` | Channel 5 parameter register                   |
| `DUTY_CYCLE_0`  | `0x2C` | Channel 0 duty cycle register                  |
| `DUTY_CYCLE_1`  | `0x30` | Channel 1 duty cycle register                  |
| `DUTY_CYCLE_2`  | `0x34` | Channel 2 duty cycle register                  |
| `DUTY_CYCLE_3`  | `0x38` | Channel 3 duty cycle register                  |
| `DUTY_CYCLE_4`  | `0x3C` | Channel 4 duty cycle register                  |
| `DUTY_CYCLE_5`  | `0x40` | Channel 5 duty cycle register                  |
| `BLINK_PARAM_0` | `0x44` | Channel 0 blink parameter register             |
| `BLINK_PARAM_1` | `0x48` | Channel 1 blink parameter register             |
| `BLINK_PARAM_2` | `0x4C` | Channel 2 blink parameter register             |
| `BLINK_PARAM_3` | `0x50` | Channel 3 blink parameter register             |
| `BLINK_PARAM_4` | `0x54` | Channel 4 blink parameter register             |
| `BLINK_PARAM_5` | `0x58` | Channel 5 blink parameter register             |

Important note:

```text
0x30 is DUTY_CYCLE_1, not PERIOD.
```

This model follows the OpenTitan PWM register layout, so there is no custom `PERIOD` register.

---

## Register Details

### CFG Register

Offset:

```text
0x08
```

Format:

```text
bit 31     CNTR_EN
bit 30:27  DC_RESN
bit 26:0   CLK_DIV
```

Example configuration:

```c
CFG = (1u << 31) | (7u << 27) | 0;
```

This means:

```text
CNTR_EN = 1
DC_RESN = 7
CLK_DIV = 0
period  = 256
```

---

### PWM_EN Register

Offset:

```text
0x0C
```

Each bit enables one PWM channel.

```text
bit 0 = enable channel 0
bit 1 = enable channel 1
...
bit 5 = enable channel 5
```

Example:

```c
PWM_EN = 0x1;
```

This enables channel 0.

---

### INVERT Register

Offset:

```text
0x10
```

Each bit controls polarity inversion for one channel.

```text
bit 0 = invert channel 0
bit 1 = invert channel 1
...
bit 5 = invert channel 5
```

Example:

```c
INVERT = 0x1;
```

This inverts channel 0 output.

---

### PWM_PARAM_0 Register

Offset:

```text
0x14
```

For this simplified model, only the phase delay field is used:

```text
PWM_PARAM_0[15:0] = PHASE_DELAY
```

The phase delay is represented as a 16-bit fraction of the PWM period.

Example:

```text
0x0000 = 0%
0x4000 = 25%
0x8000 = 50%
0xFFFF = almost 100%
```

For 20% phase delay:

```c
PWM_PARAM_0 = 0x00003333;
```

---

### DUTY_CYCLE_0 Register

Offset:

```text
0x2C
```

Format:

```text
bit 31:16 = B
bit 15:0  = A
```

In normal PWM mode, this simplified model uses only field `A`.

Duty encoding:

```text
0x0000 = 0%
0x4000 = 25%
0x8000 = 50%
0xC000 = 75%
0xFFFF = almost 100%
```

Examples:

```c
DUTY_CYCLE_0 = 0x00000000; // 0%
DUTY_CYCLE_0 = 0x00006666; // about 40%
DUTY_CYCLE_0 = 0x00008000; // 50%
DUTY_CYCLE_0 = 0x0000C000; // 75%
DUTY_CYCLE_0 = 0x0000FFFF; // almost 100%
```

---

## Project Structure

```text
pwm_tlm/
├── include/
│   └── pwm.h
├── src/
│   └── pwm.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── tb.cpp
├── CMakeLists.txt
├── Makefile
├── README.md
└── wave.vcd
```

`wave.vcd` is generated after running the simulation.

---

## File Description

### include/pwm.h

Contains:

* PWM module declaration
* TLM target socket
* PWM output port
* OpenTitan-style register offset definitions
* Internal register storage
* PWM thread declaration

### src/pwm.cpp

Contains:

* PWM constructor
* Register reset values
* TLM transaction handling through `b_transport`
* Register read/write behavior
* PWM period calculation from `CFG`
* Channel 0 waveform generation

### tests/tb.cpp

Contains:

* SystemC testbench
* TLM initiator socket
* Register read/write helper functions
* PWM verification test cases

### Makefile

Contains:

* Build rules
* SystemC include path
* SystemC library path auto-detection
* Simulation run target
* Clean target

---

## Prerequisites

Install SystemC 2.3.4.

Default installation path used by this project:

```text
/opt/systemc-2.3.4
```

Check SystemC installation:

```bash
ls -l /opt/systemc-2.3.4/include
ls -l /opt/systemc-2.3.4/lib64/libsystemc.so*
ls -l /opt/systemc-2.3.4/lib/libsystemc.so*
ls -l /opt/systemc-2.3.4/lib-linux64/libsystemc.so*
```

Depending on the machine, `libsystemc.so` may be located in:

```text
/opt/systemc-2.3.4/lib
/opt/systemc-2.3.4/lib-linux64
/opt/systemc-2.3.4/lib64
/opt/arm/fastmodels/SystemC/Accellera/SystemC/dynlib/Linux64_GCC-*
```

The Makefile can auto-detect these common locations.

---

## Build Instructions

Clean previous build:

```bash
make clean
```

Compile:

```bash
make
```

Run simulation:

```bash
make run
```

Or run directly:

```bash
./pwm_tb
```

Expected build command example:

```text
g++ -std=c++17 -g -O0 -Wall -I/opt/systemc-2.3.4/include -Iinclude -o pwm_tb tests/tb.cpp src/pwm.cpp /opt/systemc-2.3.4/lib64/libsystemc.so.2.3.4 -Wl,-rpath,/opt/systemc-2.3.4/lib64/
```

---

## Check SystemC C++ Standard

SystemC must be compiled with the same C++ standard used by this project.

To check the SystemC ABI symbol:

```bash
nm -D /opt/systemc-2.3.4/lib/libsystemc.so | c++filt | grep sc_api_version_2_3_4 | head
```

Example output:

```text
sc_core::sc_api_version_2_3_4_cxx201703L...
```

This means SystemC was built with C++17, so the Makefile should use:

```text
-std=c++17
```

---

## Run Output Example

Example simulation output:

```text
SystemC 2.3.4-Accellera

========== Read reset registers ==========
CFG reset = 0x38008000

========== TC1: Counter enable, PWM channel disable ==========

========== TC2: Duty 40% ==========
1160 ns counter=1 period=256 duty_raw=0x8000 duty_count=128 phase_count=0 pwm=1
1190 ns counter=4 period=256 duty_raw=0x6666 duty_count=102 phase_count=0 pwm=1
```

Explanation:

```text
DC_RESN = 7
CLK_DIV = 0
period = 2^(7 + 1) * (0 + 1) = 256
```

For 40% duty:

```text
duty_raw = 0x6666
duty_count = 0x6666 * 256 / 65536 ≈ 102
102 / 256 ≈ 39.8%
```

---

## Waveform Generation

The simulation generates:

```text
wave.vcd
```

Open with GTKWave:

```bash
gtkwave wave.vcd
```

Signal:

```text
pwm_out
```

---

## Verification Test Cases

### TC1 — Counter Enable, PWM Channel Disable

Configuration:

```text
CFG.CNTR_EN = 1
PWM_EN[0]   = 0
```

Expected result:

```text
pwm_out remains LOW
```

---

### TC2 — Duty 40%

Configuration:

```text
PWM_EN[0]      = 1
DUTY_CYCLE_0.A = 0x6666
```

Expected result:

```text
pwm_out is HIGH for about 40% of the period
pwm_out is LOW for about 60% of the period
```

---

### TC3 — Duty 75%

Configuration:

```text
DUTY_CYCLE_0.A = 0xC000
```

Expected result:

```text
pwm_out is HIGH for about 75% of the period
pwm_out is LOW for about 25% of the period
```

---

### TC4 — Phase Delay 20%

Configuration:

```text
PWM_PARAM_0.PHASE_DELAY = 0x3333
```

Expected result:

```text
PWM waveform is shifted by about 20% of the period
```

---

### TC5 — Invert Mode

Configuration:

```text
INVERT[0] = 1
```

Expected result:

```text
HIGH and LOW regions are inverted
```

---

### TC6 — Duty 0%

Configuration:

```text
DUTY_CYCLE_0.A = 0x0000
```

Expected result:

```text
pwm_out remains LOW
```

---

### TC7 — Duty Almost 100%

Configuration:

```text
DUTY_CYCLE_0.A = 0xFFFF
```

Expected result:

```text
pwm_out is HIGH for almost the entire PWM period
```

---

### TC8 — Disable Counter

Configuration:

```text
CFG.CNTR_EN = 0
```

Expected result:

```text
pwm_out returns LOW
```

---

## Current Limitations

This is a simplified model. Current limitations:

* Only channel 0 waveform is generated
* Channels 1 to 5 registers are stored but not actively driven
* Blink mode is not implemented yet
* Heartbeat mode is not implemented yet
* Interrupts are not implemented
* Alerts are modeled only as a register placeholder
* No functional coverage collection
* No UVM/SystemVerilog verification environment

---

## Future Enhancements

Potential improvements:

* Generate outputs for all 6 PWM channels
* Implement blink mode
* Implement heartbeat mode
* Add IRQ support
* Add alert behavior
* Add more self-checking testbench checks
* Add functional coverage
* Integrate into CDC-VP platform as `pwm_platform`
* Connect PWM base address `0x1005_0000` through the platform bus router

---

## References

1. Accellera Systems Initiative, SystemC 2.3.4 Language Reference Manual
2. Accellera Systems Initiative, TLM-2.0 Language Reference Manual
3. OpenTitan Project, PWM Hardware IP Specification
