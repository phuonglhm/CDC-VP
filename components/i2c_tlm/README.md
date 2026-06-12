# FX1 I2C SystemC/TLM Model

## Model Overview

This project implements a simplified I2C controller model using SystemC and TLM-2.0. The model is inspired by the OpenTitan I2C peripheral and supports both Host (Master) and Target (Slave) operating modes.

The model exposes a memory-mapped register interface through a TLM target socket. Software accesses the peripheral by reading and writing registers, while internal FIFOs and state machines manage I2C transactions.

### Implemented Components

- Register interface via TLM `b_transport`
- Host mode controller
- Target mode controller
- Interrupt generation
- FMT FIFO (Host command queue)
- RX FIFO (Host receive queue)
- ACQ FIFO (Target receive queue)
- TX FIFO (Target transmit queue)
- Host and Target state machines

---

## Supported Features

### Host Mode

- Enable/disable host controller
- START and STOP commands
- Address and data transmission
- Read transactions
- RX FIFO data storage
- Command completion interrupt

### Target Mode

- Configurable target address
- Address matching
- Reception of master write transactions
- Transmission of data through TX FIFO
- Acquisition of received bytes through ACQ FIFO
- FIFO threshold and overflow interrupts

---

## Project Structure

```text
.
├── i2c.h
├── i2c.cpp
├── main.cpp
└── Makefile
```

---

## Build Instructions

### Prerequisites

- SystemC library
- C++17 compatible compiler
- GNU Make

### Build

```bash
make
```

This generates the executable:

```text
i2c_test
```

---

## Run Instructions

Execute the simulation:

```bash
./i2c_test
```

The testbench automatically performs host-mode and target-mode verification tests.

---

## Expected Functionality

The simulation demonstrates:

1. Host controller enable and configuration
2. Host write transaction using FDATA commands
3. Interrupt generation on transaction completion
4. Target address configuration
5. Target reception of master write transactions
6. Reading received bytes from ACQ FIFO
7. Target transmission of bytes from TX FIFO
8. FIFO status monitoring

Example output:

```text
=== FX1 I2C Model Test ===

--- Test 1: Enable host mode ---
CTRL = 0x1

--- Test 4: Host write transaction ---
Sending: START + ADDR=0x50
Sending: DATA=0x33
Sending: STOP

--- Test 11: Target read transaction ---
Master received 2 bytes:
0xCD
0xEF

=== All tests complete ===
```

---

## Current Limitations

The model is functional but not cycle-accurate.

Not implemented:

- SDA/SCL signal-level modeling
- ACK/NACK handling
- Clock stretching
- Repeated START conditions
- Multi-master arbitration
- Bus timing and delays
- Full OpenTitan register coverage

Target transactions are currently simulated through direct calls to:

```cpp
receive_transaction(...)
```

rather than through a complete I2C bus model.
