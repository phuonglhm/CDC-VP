# PWM IP TLM Model

## Overview

This project implements a simplified Pulse Width Modulation (PWM) Intellectual Property (IP) model using **SystemC** and **TLM-2.0**.

The PWM IP is modeled as a TLM Target and supports memory-mapped register access through TLM transactions. A SystemC testbench acts as a CPU master and configures the PWM through read and write operations.

The model demonstrates the basic functionality of a PWM peripheral, including:

* PWM Enable/Disable
* Duty Cycle Control
* Period Configuration
* Phase Delay
* Output Inversion
* Waveform Generation
* VCD Trace Generation

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

## Register Map

| Register   | Address | Description               |
| ---------- | ------- | ------------------------- |
| CFG        | 0x08    | Counter control register  |
| PWM_EN     | 0x0C    | PWM enable register       |
| INVERT     | 0x10    | PWM output inversion      |
| PWM_PARAM0 | 0x14    | Phase delay configuration |
| DUTY0      | 0x2C    | Duty cycle configuration  |
| PERIOD     | 0x30    | PWM period configuration  |

---

## Project Structure

```text
pwm_tlm_complete/
├── pwm.h
├── pwm.cpp
├── tb.cpp
├── Makefile
└── wave.vcd (generated after simulation)
```

### File Description

#### pwm.h

Contains:

* PWM module declaration
* Register definitions
* Target socket declaration
* Internal register storage
* PWM output port

#### pwm.cpp

Contains:

* PWM constructor
* TLM transaction handling (`b_transport`)
* PWM waveform generation engine (`pwm_thread`)

#### tb.cpp

Contains:

* Testbench implementation
* Initiator socket
* Register access helper functions
* Verification test cases

#### Makefile

Contains:

* Build configuration
* SystemC library linkage
* Simulation execution targets

---

## Prerequisites

Install SystemC 2.3.4 or later.

Example installation path:

```text
/usr/local/systemc-2.3.4
```

Verify installation:

```bash
ls /usr/local/systemc-2.3.4
```

---

## Build Instructions

Clean previous build:

```bash
make clean
```

Compile the project:

```bash
make
```

Expected output:

```text
g++ pwm.cpp tb.cpp ...
-o pwm_sim
```

---

## Run Instructions

Execute simulation:

```bash
./pwm_sim
```

or

```bash
make run
```

Example console output:

```text
110 ns counter=1 pwm=0
120 ns counter=2 pwm=0
130 ns counter=3 pwm=1
...
```

---

## Waveform Generation

The simulation automatically generates:

```text
wave.vcd
```

Open the waveform using GTKWave:

```bash
gtkwave wave.vcd
```

Signal list:

```text
pwm_out
```

---

## Verification Test Cases

### TC1 – PWM Disable

Expected Result:

```text
PWM output remains LOW
```

### TC2 – Duty Cycle = 40%

Expected Result:

```text
40% HIGH
60% LOW
```

### TC3 – Duty Cycle = 75%

Expected Result:

```text
75% HIGH
25% LOW
```

### TC4 – Phase Delay = 20

Expected Result:

```text
PWM waveform shifted in time
```

### TC5 – Invert Mode

Expected Result:

```text
HIGH and LOW regions are inverted
```

### TC6 – Duty Cycle = 0%

Expected Result:

```text
PWM output always LOW
```

### TC7 – Duty Cycle = 100%

Expected Result:

```text
PWM output always HIGH
```

---

## References

1. Accellera Systems Initiative, *SystemC 2.3.4 Language Reference Manual*.
2. Accellera Systems Initiative, *TLM-2.0 Language Reference Manual*.
3. OpenTitan Project, *PWM Hardware IP Specification*.
4. SystemC Official Documentation.

---

## Future Enhancements

Potential improvements include:

* Interrupt (IRQ) Support
* Multi-Channel PWM
* Clock Divider
* APB-to-TLM Bridge
* Advanced PWM Modes
* Functional Coverage Collection
* UVM-Based Verification Environment

```
```
