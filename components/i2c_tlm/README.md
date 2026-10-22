# FX1 I2C SystemC/TLM Model

## Overview

This project implements a simplified I2C controller model using SystemC and TLM-2.0.

The model is inspired by the OpenTitan I2C peripheral and supports both Host (Master) and Target (Slave) operating modes.

The peripheral exposes a memory-mapped register interface through a TLM target socket. Software accesses the I2C controller through register reads and writes while internal FIFOs and state machines manage I2C transactions.

---

## Functional host mode (CSI-2 M5a)

`i2c(name, i2c_config)` selects the behaviour; the default `i2c_config` keeps
the legacy model unchanged (mini_soc, riscv_custom_soc, noc_soc, i2c_platform).
With `i2c_config::functional_host = true` (fx1_soc) the host executes the FMT
FIFO on a bus with SCL timing (`scl_hz`, default 400 kHz) against devices
attached with `attach(addr7, i2c_device*)` (`include/i2c_device.h`, one call
per bus event):

* START: 1 bit time; each byte: 9 bit times (8 data + ACK); STOP: 1 bit time
* repeated START (FDATA.START on a later command of the same transaction)
* READB: n bytes (0 = 256), the last NACKed unless FDATA.RCONT (bit 11)
* RX FIFO full: the clock stretches until software reads RDATA
* NACK of an address or data byte: INTR_STATE.nak (bit 4), STOP at once, the
  rest of the transaction discarded up to its STOP (or a FIFO_CTRL FMT reset);
  FDATA.NAKOK (bit 12) lets a NACKed byte continue
* CMD_COMPLETE after each STOP; STATUS.HOSTIDLE = no transaction running and
  the FMT FIFO empty
* CTRL.enablehost = 0 pauses the host between transactions: a transaction
  already started runs to its STOP, the queue is kept and resumes when the host
  is enabled again (FDATA writes are ignored while disabled, so the STOP of a
  started transaction must be queued before disabling); FIFO_CTRL FMT reset
  drops the queue; with a transaction open it also aborts it (M5b): the byte
  on the bus completes (a READB stops after its current byte), then STOP at
  once, the host is idle; no NAK or CMD_COMPLETE is reported for it. An abort
  that arrives during the STOP bit itself (also the STOP after a NACK) wins:
  nothing is discarded from the emptied queue, no completion is reported; a
  byte read that completes after the abort is not put in the RX FIFO
* `irq` = (INTR_STATE & INTR_ENABLE) != 0, from one process
* register accesses copy at most 4 bytes; console traces only in legacy mode
  or with `i2c_config::verbose`

Test: `tests/test_i2c_functional.cpp` (ctest `i2c_tlm_functional`); the legacy
behaviour stays covered by `test_i2c_tlm`.

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
├── include/
│   └── i2c.h
├── src/
│   └── i2c.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── test_i2c_tlm.cpp
├── CMakeLists.txt
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
