# OTP Memory + Controller TLM Model

## Overview

This project implements a simplified **One-Time Programmable (OTP) Memory + Controller** model using **SystemC** and **TLM-2.0**.

The OTP controller is modeled as a **TLM target**. A SystemC testbench acts as a CPU-like initiator and accesses the OTP controller through memory-mapped register transactions.

The model is inspired by the **OpenTitan OTP Controller specification**, but it is simplified for educational and verification purposes.

The model demonstrates the basic functionality of an OTP peripheral, including:

* Memory-mapped register access
* OTP read operation
* OTP program operation
* One-time programmable behavior
* Illegal `1 -> 0` programming detection
* Partition-based write lock
* Partition-based read lock
* Status and error reporting
* Interrupt generation
* SystemC/TLM testbench verification

---

## Project Structure

```text
otp_tlm/
├── otp.h
├── otp.cpp
├── tb.cpp
├── Makefile
├── CMakeLists.txt
└── README.md
```

| File             | Description                                                                                              |
| ---------------- | -------------------------------------------------------------------------------------------------------- |
| `otp.h`          | Header file for the OTP TLM model                                                                        |
| `otp.cpp`        | Implementation of OTP memory, controller registers, read/program behavior, lock mechanism, and interrupt |
| `tb.cpp`         | SystemC/TLM testbench acting as a CPU initiator                                                          |
| `Makefile`       | Build script using `make`                                                                                |
| `CMakeLists.txt` | Build script using CMake                                                                                 |
| `README.md`      | Project documentation                                                                                    |

---

## Background

### OTP Memory

**OTP** stands for **One-Time Programmable**.

OTP memory is a non-volatile memory type where each bit can only be programmed once. In this model, the erased value is assumed to be `0`, and the programmed value is `1`.

Therefore, only this transition is allowed:

```text
0 -> 1
```

The following transition is illegal:

```text
1 -> 0
```

OTP memory is commonly used to store permanent chip information such as:

* Chip ID
* Serial number
* Boot configuration
* Secure boot key hash
* Debug enable/disable configuration
* Lifecycle state
* Calibration data
* Hardware feature configuration

---

## OTP Controller

The OTP controller sits between the CPU/system bus and the OTP memory array.

```text
CPU / Testbench
      |
      | TLM Transaction
      v
OTP Controller
      |
      v
OTP Memory Array
```

The controller is responsible for:

* Receiving read/program commands
* Checking address validity
* Enforcing OTP one-time programming rule
* Managing read/write locks
* Reporting operation status
* Reporting error conditions
* Generating interrupt requests

The CPU or software should not access the OTP memory array directly. All accesses must go through the controller.

---

## Architecture

```text
+----------------------+
|      Testbench       |
|  TLM Initiator       |
+----------+-----------+
           |
           | TLM read/write transaction
           v
+----------------------+
|    OTP Controller    |
|                      |
|  Register Interface  |
|  Read FSM            |
|  Program FSM         |
|  Lock Control        |
|  Error/Status Logic  |
|  Interrupt Logic     |
+----------+-----------+
           |
           v
+----------------------+
|    OTP Memory Array  |
|  32-bit word storage |
+----------------------+
```

---

## Model Features

The current model supports:

```text
- 32-bit OTP word array
- TLM target socket
- Memory-mapped register interface
- Read command
- Program command
- One-time programmable rule
- Illegal 1 -> 0 detection
- Partition-based write lock
- Partition-based read lock
- Busy/done/error status
- Error code register
- Interrupt enable
- Interrupt pending state
- IRQ output
```

The model does not yet implement:

```text
- ECC
- Digest lock
- Secret partition scrambling
- Lifecycle controller interface
- Key derivation interface
- Background integrity check
- Buffered partition outputs
```

These features can be added later if a more complete OpenTitan-like OTP controller model is required.

---

## Register Map

| Offset | Register         | Access | Description                   |
| -----: | ---------------- | ------ | ----------------------------- |
| `0x00` | `CTRL`           | R/W    | Control register              |
| `0x04` | `STATUS`         | R      | Status register               |
| `0x08` | `ADDR`           | R/W    | OTP word address              |
| `0x0C` | `WDATA`          | R/W    | Write/program data            |
| `0x10` | `RDATA`          | R      | Read data                     |
| `0x14` | `LOCK`           | R/W    | Write lock register           |
| `0x18` | `READ_LOCK`      | R/W    | Read lock register            |
| `0x1C` | `ERR_STATUS`     | R/W1C  | Error status register         |
| `0x20` | `INTR_ENABLE`    | R/W    | Interrupt enable              |
| `0x24` | `INTR_STATE`     | R/W1C  | Interrupt pending state       |
| `0x28` | `SIZE_WORDS`     | R      | OTP memory size in words      |
| `0x2C` | `PARTITION_SIZE` | R      | Number of words per partition |

---

## CTRL Register

Offset:

```text
0x00
```

| Bit | Name         | Description                 |
| --: | ------------ | --------------------------- |
|   0 | `READ_START` | Start OTP read operation    |
|   1 | `PROG_START` | Start OTP program operation |
|   2 | `CLEAR_IRQ`  | Clear interrupt state       |

Example:

```cpp
CTRL = 1 << 0;  // Start read
CTRL = 1 << 1;  // Start program
CTRL = 1 << 2;  // Clear IRQ
```

---

## STATUS Register

Offset:

```text
0x04
```

| Bit | Name    | Description          |
| --: | ------- | -------------------- |
|   0 | `BUSY`  | Controller is busy   |
|   1 | `DONE`  | Operation completed  |
|   2 | `ERROR` | Error occurred       |
|   3 | `IRQ`   | Interrupt is pending |

Example status value:

```text
STATUS = 0xA
```

Binary:

```text
0xA = 1010b
```

Meaning:

```text
DONE = 1
IRQ  = 1
```

---

## Error Codes

| Code | Name                    | Description                         |
| ---: | ----------------------- | ----------------------------------- |
|    0 | `ERR_NONE`              | No error                            |
|    1 | `ERR_ADDR_OUT_OF_RANGE` | Address is outside OTP memory       |
|    2 | `ERR_WRITE_LOCKED`      | Write access is locked              |
|    3 | `ERR_READ_LOCKED`       | Read access is locked               |
|    4 | `ERR_PROGRAM_1_TO_0`    | Illegal OTP programming from 1 to 0 |
|    5 | `ERR_BUSY`              | Controller is busy                  |
|    6 | `ERR_BAD_ACCESS`        | Invalid register access             |
|    7 | `ERR_BYTE_ENABLE`       | Byte-enable access is not supported |

---

## OTP Memory Organization

The OTP memory is organized as 32-bit words.

Default configuration:

```text
OTP words       = 256
Partition size  = 64 words
Partitions      = 4
```

Partition mapping:

| Partition | Word Address Range |
| --------: | ------------------ |
|         0 | `0` to `63`        |
|         1 | `64` to `127`      |
|         2 | `128` to `191`     |
|         3 | `192` to `255`     |

---

## Write Lock

The `LOCK` register controls write access to each partition.

Each bit corresponds to one partition.

```text
LOCK[0] = lock partition 0
LOCK[1] = lock partition 1
LOCK[2] = lock partition 2
LOCK[3] = lock partition 3
```

The lock bits are sticky. Once a partition is locked, it cannot be unlocked during simulation.

Example:

```cpp
LOCK = 1 << 0;
```

This locks write access to partition 0.

After this operation, programming word address `0` to `63` will fail with:

```text
ERR_WRITE_LOCKED = 2
```

---

## Read Lock

The `READ_LOCK` register controls read access to each partition.

Each bit corresponds to one partition.

```text
READ_LOCK[0] = read-lock partition 0
READ_LOCK[1] = read-lock partition 1
READ_LOCK[2] = read-lock partition 2
READ_LOCK[3] = read-lock partition 3
```

The read lock bits are sticky.

Example:

```cpp
READ_LOCK = 1 << 0;
```

This locks read access to partition 0.

After this operation, reading word address `0` to `63` will fail with:

```text
ERR_READ_LOCKED = 3
```

---

## Read Operation Flow

```text
1. Testbench writes target word address to ADDR.
2. Testbench writes READ_START to CTRL.
3. OTP controller checks address validity.
4. OTP controller checks read lock.
5. OTP controller reads data from OTP memory array.
6. Data is stored in RDATA.
7. STATUS.DONE is set.
8. Interrupt is generated if enabled.
```

Example:

```cpp
write32(REG_ADDR, 0);
write32(REG_CTRL, CTRL_READ_START);
data = read32(REG_RDATA);
```

---

## Program Operation Flow

```text
1. Testbench writes target word address to ADDR.
2. Testbench writes program data to WDATA.
3. Testbench writes PROG_START to CTRL.
4. OTP controller checks address validity.
5. OTP controller checks write lock.
6. OTP controller checks OTP programming rule.
7. If only 0 -> 1 transition is requested, data is programmed.
8. If 1 -> 0 transition is requested, an error is reported.
9. STATUS.DONE or STATUS.ERROR is set.
10. Interrupt is generated if enabled.
```

Example:

```cpp
write32(REG_ADDR, 0);
write32(REG_WDATA, 0x12345678);
write32(REG_CTRL, CTRL_PROG_START);
```

---

## Interrupt Behavior

The model has one interrupt output:

```cpp
sc_core::sc_out<bool> irq_out;
```

Interrupt is generated when:

```text
- an operation completes successfully
- an error occurs
```

Interrupt must be enabled through:

```text
INTR_ENABLE = 1
```

The interrupt can be cleared by writing:

```text
INTR_STATE = 1
```

or by using:

```text
CTRL.CLEAR_IRQ = 1
```

---

## Testbench

The testbench performs the following tests:

```text
Test 1: Program word 0 with 0x12345678
Test 2: Read word 0 and compare with expected data
Test 3: Try illegal 1 -> 0 programming
Test 4: Lock partition 0 and try to program word 1
Test 5: Read-lock partition 0 and try to read word 0
Test 6: Access an out-of-range address
```

Expected behavior:

```text
- Program operation passes
- Read operation returns correct data
- Illegal 1 -> 0 programming is detected
- Write lock is detected
- Read lock is detected
- Address out-of-range error is detected
```

---

## Build Using Makefile

Clean previous build:

```bash
make clean
```

Build:

```bash
make
```

Run:

```bash
make run
```

If SystemC is installed in a custom path, set `SYSTEMC_HOME`:

```bash
make clean
make SYSTEMC_HOME=/opt/systemc-2.3.4
make run SYSTEMC_HOME=/opt/systemc-2.3.4
```

---

## Build Using CMake

Configure:

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build
```

Run:

```bash
./build/otp_tlm
```

If the runtime loader cannot find `libsystemc.so`, run:

```bash
LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib-linux64:$LD_LIBRARY_PATH ./build/otp_tlm
```

or:

```bash
LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib:$LD_LIBRARY_PATH ./build/otp_tlm
```

depending on where `libsystemc.so` is installed.

---

## Example Output

Example successful simulation output:

```text
SystemC 2.3.4-Accellera

[OTP] Created OTP TLM model
[OTP] words = 256, partition_words = 64, partitions = 4

[TB] Start OTP TLM test

[TB] OTP size words      = 256
[TB] OTP partition words = 64

[TB] Test 1: program word 0 = 0x12345678
[OTP] PROGRAM addr=0 data=0x12345678
[TB] STATUS = 0xa

[TB] Test 2: read word 0
[OTP] READ addr=0 data=0x12345678
[TB] RDATA  = 0x12345678
[TB] STATUS = 0xa
[TB] PASS: read data matched

[TB] Test 3: illegal program 1 -> 0
[OTP] ERROR code=4
[TB] PASS: illegal 1 -> 0 detected

[TB] Test 4: lock partition 0
[OTP] ERROR code=2
[TB] PASS: write lock detected

[TB] Test 5: read lock partition 0
[OTP] ERROR code=3
[TB] PASS: read lock detected

[TB] Test 6: address out of range
[OTP] ERROR code=1
[TB] PASS: address out of range detected

[TB] OTP TLM test finished
```

---

## Reference Specification

This model is inspired by the OpenTitan OTP Controller concept.

Main ideas adopted from OpenTitan:

```text
- OTP controller as a bus-accessible peripheral
- OTP memory accessed through a controller
- Direct read/program access
- Logical partitions
- Access control
- Status and error reporting
- Interrupt behavior
```

However, this model is simplified and does not implement the full OpenTitan OTP controller architecture.

---

## Limitations

This is a functional TLM model for learning and early verification. It does not model low-level OTP physical behavior.

Current limitations:

```text
- No real OTP programming voltage or timing model
- No ECC implementation
- No digest calculation
- No secret scrambling
- No lifecycle state machine
- No background integrity check
- No hardware key output interface
```

---

## Future Improvements

Possible future extensions:

```text
- Add ECC check and error injection
- Add digest-based lock
- Add secret partition
- Add lifecycle controller interface
- Add key derivation interface
- Add background integrity checker
- Add VCD trace generation
- Integrate into a larger RISC-V VP platform
```

---

## Summary

This project provides a standalone SystemC/TLM OTP memory and controller model.

It supports a memory-mapped register interface, OTP read/program commands, one-time programmable behavior, partition-based access control, error reporting, and interrupt generation.

The model is suitable for:

```text
- Learning SystemC/TLM modeling
- Understanding OTP memory and controller behavior
- Building a simple SoC peripheral model
- Creating a testbench for OTP register access
- Extending toward an OpenTitan-like OTP controller
```
