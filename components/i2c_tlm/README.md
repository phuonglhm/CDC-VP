# FX1 I2C SystemC/TLM Model

## Overview

This project implements a simplified I2C controller model using SystemC and TLM-2.0.

The model is inspired by the OpenTitan I2C peripheral and supports both Host (Master) and Target (Slave) operating modes.

The peripheral exposes a memory-mapped register interface through a TLM target socket. Software accesses the I2C controller through register reads and writes while internal FIFOs and state machines manage I2C transactions.

---

## Implemented Components

* TLM-2.0 target socket interface
* Register read/write interface (`b_transport`)
* Host mode controller
* Target mode controller
* Interrupt generation
* Host FMT FIFO
* Host RX FIFO
* Target ACQ FIFO
* Target TX FIFO
* Host state machine
* Target state machine

---

## Supported Features

### Host Mode

* Host enable/disable control
* START command generation
* STOP command generation
* Address transmission
* Data transmission
* Read transaction support
* RX FIFO management
* Command completion interrupt

### Target Mode

* Programmable target address
* Address matching
* Master write transaction handling
* Master read transaction handling
* ACQ FIFO support
* TX FIFO support
* FIFO threshold interrupts
* FIFO overflow interrupts

---

## Project Structure

```text
components/i2c_tlm/
├── i2c.h
├── i2c.cpp
├── main.cpp
├── Makefile
└── README.md
```

---

## Requirements

The standalone model was verified using:

* SystemC 2.3.4
* GNU Make
* GCC/G++ with C++17 support

Example environment:

```text
Ubuntu 22.04
SystemC 2.3.4
GCC 11+
```

---

## Build Instructions

### Configure SystemC Location

The build does not use a hard-coded SystemC path.

Specify the SystemC installation directory through:

```bash
SYSTEMC_HOME=/path/to/systemc-2.3.4
```

Example:

```bash
SYSTEMC_HOME=/opt/systemc-2.3.4
```

### Build

```bash
make clean
make SYSTEMC_HOME=/opt/systemc-2.3.4
```

The build generates:

```text
i2c_sim
```

---

## Run Instructions

Execute:

```bash
./i2c_sim
```

The testbench automatically performs host-mode and target-mode verification.

---

## Verification Coverage

The standalone testbench verifies:

### Host Controller

* Host enable
* Register access
* START command
* STOP command
* Address transmission
* Data transmission
* Interrupt generation

### Target Controller

* Target address configuration
* Address matching
* Acquisition FIFO operation
* TX FIFO operation
* Target read transactions
* Target write transactions

### Interrupts

* Command complete interrupt
* FIFO threshold interrupt
* FIFO overflow interrupt

### FIFOs

* FMT FIFO
* RX FIFO
* ACQ FIFO
* TX FIFO

---

## Example Output

```text
=== FX1 I2C Model Test ===

--- Test 1: Enable host mode ---
CTRL = 0x1

--- Test 2: Host write transaction ---
START + ADDR=0x50
DATA=0x33
STOP

--- Test 3: Interrupt generation ---
CMD_COMPLETE interrupt detected

--- Test 4: Target write transaction ---
ACQ FIFO received data

--- Test 5: Target read transaction ---
Master received:
0xCD
0xEF

=== All tests complete ===
```

---

## Current Limitations

This model is functional but not cycle accurate.

The following features are not currently implemented:

* SDA/SCL signal-level modeling
* ACK/NACK timing behavior
* Clock stretching
* Repeated START conditions
* Multi-master arbitration
* Bus timing constraints
* Full OpenTitan register coverage

Target transactions are currently injected through:

```cpp
receive_transaction(...)
```

rather than through a complete I2C bus model.

---

## Notes

This standalone model is intended for functional verification and feature validation of the I2C controller.

For full CDC-VP integration testing, use the dedicated platform:

```text
platforms/tests/i2c_platform
```

which connects:

* RISC-V CPU
* Bus Router
* PLIC
* CLINT
* UART
* I2C TLM IP

and executes firmware-based verification on the complete platform.
