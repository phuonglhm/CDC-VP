# OpenTitan-like OTP Memory Controller TLM Model

## Overview

This component implements a simplified **OTP Memory Controller TLM model** using **SystemC** and **TLM-2.0**.

The model is inspired by the **OpenTitan OTP Controller** register structure and behavior. It provides an OpenTitan-like memory-mapped register interface for direct OTP read, program, digest, status/error reporting, interrupt handling, read lock, and simplified partition lock behavior.

This is not a full OpenTitan RTL implementation. It is a functional TLM abstraction intended for learning, early platform integration, and firmware access testing.

---

## Project Structure

```text
otp_tlm/
├── include/
│   └── otp.h
├── src/
│   └── otp.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── tb.cpp
├── Makefile
├── CMakeLists.txt
└── README.md
```

| File                         | Description                       |
| ---------------------------- | --------------------------------- |
| `include/otp.h`              | Header file for the OTP TLM model |
| `src/otp.cpp`                | OTP controller implementation     |
| `tests/tb.cpp`               | SystemC/TLM testbench             |
| `tests/CMakeLists.txt`       | CMake test target                 |
| `Makefile`                   | Build script using `make`         |
| `CMakeLists.txt`             | Build script using CMake          |
| `README.md`                  | Documentation                     |

---

## Background

OTP means **One-Time Programmable**.

An OTP memory is a non-volatile memory where each bit can only be programmed once. In this model, the erased value is assumed to be `0`, and the programmed value is `1`.

Allowed transition:

```text
0 -> 1
```

Illegal transition:

```text
1 -> 0
```

Typical OTP use cases include:

```text
- Chip ID
- Serial number
- Boot configuration
- Secure boot key hash
- Debug lock configuration
- Calibration data
- Hardware feature configuration
- Lifecycle state
```

---

## Architecture

```text
+----------------------+
|      Testbench       |
|   TLM Initiator      |
+----------+-----------+
           |
           | TLM read/write transactions
           v
+----------------------+
|   OTP Controller     |
|                      |
| - Register Interface |
| - Direct Access IF   |
| - Read Control       |
| - Program Control    |
| - Digest Stub        |
| - Read Lock Control  |
| - Error Logic        |
| - Interrupt Logic    |
+----------+-----------+
           |
           v
+----------------------+
|   OTP Memory Array   |
|  32-bit word storage |
+----------------------+
```

The testbench accesses the OTP controller through a TLM initiator socket. The OTP controller is modeled as a TLM target.

---

## Model Features

The current model supports:

```text
- SystemC/TLM-2.0 target socket
- OpenTitan-like OTP register map
- Direct Access Interface, DAI
- DAI read command
- DAI write/program command
- DAI digest command, simplified
- 32-bit OTP memory array
- OTP programming rule: only 0 -> 1 is allowed
- Illegal 1 -> 0 detection
- Read lock registers
- Digest-based write lock behavior, simplified
- STATUS register
- PARTITION_STATUS_0 register
- ERR_CODE_0..ERR_CODE_23 registers
- INTR_STATE / INTR_ENABLE / INTR_TEST
- ALERT_TEST stub
- CHECK_TRIGGER stub
- Integrity/consistency check period registers
- IRQ output
```

The model does not implement the following OpenTitan features fully:

```text
- Real ECC
- Real digest algorithm
- Real secret partition scrambling
- Lifecycle controller interface
- Key derivation interface
- Background integrity checker
- Hardware alerts
- Full OpenTitan partition policy
- Security-hardened FSM encoding
```

---

## Interfaces

### TLM Target Socket

```cpp
tlm_utils::simple_target_socket<otp> socket;
```

The socket receives memory-mapped read/write transactions from a testbench or platform bus.

### Interrupt Output

```cpp
sc_core::sc_out<bool> irq_out;
```

The interrupt is asserted when:

```text
- An OTP operation completes
- An OTP error occurs
```

IRQ is active only when the corresponding interrupt bit is enabled in `INTR_ENABLE`.

---

## OTP Memory Organization

Default configuration:

```text
OTP size        = 256 words
Word width      = 32 bits
Partition size  = 64 words
Partitions      = 4
```

Partition mapping:

| Partition | Word Address Range | Byte Address Range |
| --------: | -----------------: | -----------------: |
|         0 |           `0 - 63` |    `0x000 - 0x0FC` |
|         1 |         `64 - 127` |    `0x100 - 0x1FC` |
|         2 |        `128 - 191` |    `0x200 - 0x2FC` |
|         3 |        `192 - 255` |    `0x300 - 0x3FC` |

The DAI address is treated as a **byte address**. Internally, the model converts it to a word address:

```cpp
word_addr = byte_addr >> 2;
```

---

## Register Map

The model uses an OpenTitan-like OTP controller register map.

|          Offset | Register                   | Access | Description                         |
| --------------: | -------------------------- | ------ | ----------------------------------- |
|         `0x000` | `INTR_STATE`               | R/W1C  | Interrupt pending state             |
|         `0x004` | `INTR_ENABLE`              | R/W    | Interrupt enable                    |
|         `0x008` | `INTR_TEST`                | W      | Force interrupt for testing         |
|         `0x00C` | `ALERT_TEST`               | W      | Alert test stub                     |
|         `0x010` | `STATUS`                   | R      | Controller status                   |
|         `0x014` | `PARTITION_STATUS_0`       | R      | Simplified partition status         |
| `0x018 - 0x074` | `ERR_CODE_0..23`           | R      | Error code registers                |
|         `0x078` | `DIRECT_ACCESS_REGWEN`     | R/W0C  | DAI register write enable           |
|         `0x07C` | `DIRECT_ACCESS_CMD`        | W      | DAI command                         |
|         `0x080` | `DIRECT_ACCESS_ADDRESS`    | R/W    | DAI byte address                    |
|         `0x084` | `DIRECT_ACCESS_WDATA_0`    | R/W    | DAI write data word 0               |
|         `0x088` | `DIRECT_ACCESS_WDATA_1`    | R/W    | DAI write data word 1               |
|         `0x08C` | `DIRECT_ACCESS_RDATA_0`    | R      | DAI read data word 0                |
|         `0x090` | `DIRECT_ACCESS_RDATA_1`    | R      | DAI read data word 1                |
|         `0x094` | `CHECK_TRIGGER_REGWEN`     | R/W0C  | Check trigger write enable          |
|         `0x098` | `CHECK_TRIGGER`            | W      | Trigger integrity/consistency check |
|         `0x09C` | `CHECK_REGWEN`             | R/W0C  | Check config write enable           |
|         `0x0A0` | `CHECK_TIMEOUT`            | R/W    | Check timeout value                 |
|         `0x0A4` | `INTEGRITY_CHECK_PERIOD`   | R/W    | Integrity check period              |
|         `0x0A8` | `CONSISTENCY_CHECK_PERIOD` | R/W    | Consistency check period            |
| `0x0AC - 0x0E4` | `*_READ_LOCK`              | R/W    | Runtime read lock registers         |
|        `0x0E8+` | Digest registers           | R/W    | Simplified digest storage           |
|         `0x200` | `MODEL_SIZE_WORDS`         | R      | Model-only OTP size                 |
|         `0x204` | `MODEL_PARTITION_SIZE`     | R      | Model-only partition size           |

The registers at `0x200` and `0x204` are debug/model information registers. They are not OpenTitan architectural registers.

---

## Interrupt Registers

### INTR_STATE

Offset:

```text
0x000
```

| Bit | Name                 | Description         |
| --: | -------------------- | ------------------- |
|   0 | `OTP_OPERATION_DONE` | Operation completed |
|   1 | `OTP_ERROR`          | Error occurred      |

`INTR_STATE` uses write-one-to-clear behavior.

Example:

```cpp
write32(REG_INTR_STATE, 0x3);
```

This clears both interrupt bits.

### INTR_ENABLE

Offset:

```text
0x004
```

| Bit | Name                    | Description            |
| --: | ----------------------- | ---------------------- |
|   0 | `OTP_OPERATION_DONE_EN` | Enable done interrupt  |
|   1 | `OTP_ERROR_EN`          | Enable error interrupt |

IRQ logic:

```cpp
irq_out = (INTR_STATE & INTR_ENABLE) != 0;
```

---

## STATUS Register

Offset:

```text
0x010
```

The current model uses simplified status bits.

| Bit | Name          | Description                       |
| --: | ------------- | --------------------------------- |
|   0 | `DAI_IDLE`    | Direct Access Interface is idle   |
|   1 | `DAI_ERROR`   | DAI operation error               |
|   2 | `CHECK_ERROR` | Integrity/consistency check error |
|   3 | `FSM_ERROR`   | FSM/state error                   |

Example:

```text
STATUS = 0x1
```

Meaning:

```text
DAI_IDLE = 1
No error
```

Example:

```text
STATUS = 0x3
```

Meaning:

```text
DAI_IDLE  = 1
DAI_ERROR = 1
```

---

## Error Codes

The model exposes `ERR_CODE_0` to `ERR_CODE_23`.

The main testbench uses `ERR_CODE_0`.

|  Code | Name                          | Description                                           |
| ----: | ----------------------------- | ----------------------------------------------------- |
| `0x0` | `ERR_NO_ERROR`                | No error                                              |
| `0x1` | `ERR_MACRO_ERROR`             | Generic OTP macro error                               |
| `0x2` | `ERR_MACRO_ECC_CORR_ERROR`    | Correctable ECC error, stub                           |
| `0x3` | `ERR_MACRO_ECC_UNCORR_ERROR`  | Uncorrectable ECC error, stub                         |
| `0x4` | `ERR_MACRO_WRITE_BLANK_ERROR` | Illegal programming operation                         |
| `0x5` | `ERR_ACCESS_ERROR`            | Invalid access, locked access, or out-of-range access |
| `0x6` | `ERR_CHECK_FAIL_ERROR`        | Integrity/consistency check failure                   |
| `0x7` | `ERR_FSM_STATE_ERROR`         | FSM state error                                       |

---

## Direct Access Interface

The Direct Access Interface, DAI, is used for direct OTP read, program, and digest operations.

### DAI Command Register

Offset:

```text
0x07C
```

| Value | Command       | Description                                   |
| ----: | ------------- | --------------------------------------------- |
| `0x1` | Read          | Read OTP data                                 |
| `0x2` | Write/Program | Program OTP data                              |
| `0x4` | Digest        | Generate simplified digest and lock partition |

---

## DAI Read Flow

```text
1. Write byte address to DIRECT_ACCESS_ADDRESS.
2. Write 0x1 to DIRECT_ACCESS_CMD.
3. Controller checks address range.
4. Controller checks read lock.
5. Controller reads OTP memory.
6. Data is returned in DIRECT_ACCESS_RDATA_0.
7. Operation done interrupt is generated.
```

Example:

```cpp
write32(REG_DIRECT_ACCESS_ADDRESS, 0x000);
write32(REG_DIRECT_ACCESS_CMD, 0x1);
data = read32(REG_DIRECT_ACCESS_RDATA_0);
```

---

## DAI Write/Program Flow

```text
1. Write data to DIRECT_ACCESS_WDATA_0.
2. Optionally write data to DIRECT_ACCESS_WDATA_1.
3. Write byte address to DIRECT_ACCESS_ADDRESS.
4. Write 0x2 to DIRECT_ACCESS_CMD.
5. Controller checks address range.
6. Controller checks digest-based write lock.
7. Controller checks OTP programming rule.
8. If valid, data is programmed.
9. Operation done interrupt is generated.
```

Example:

```cpp
write32(REG_DIRECT_ACCESS_WDATA_0, 0x12345678);
write32(REG_DIRECT_ACCESS_WDATA_1, 0x00000000);
write32(REG_DIRECT_ACCESS_ADDRESS, 0x000);
write32(REG_DIRECT_ACCESS_CMD, 0x2);
```

---

## OTP Programming Rule

The model assumes:

```text
erased bit    = 0
programmed bit = 1
```

Only `0 -> 1` is allowed.

Illegal condition:

```cpp
if ((old_value & ~new_value) != 0) {
    error = ERR_MACRO_WRITE_BLANK_ERROR;
}
```

Program operation:

```cpp
mem[word_addr] = old_value | new_value;
```

Example:

```text
Old value = 0x12345678
New value = 0x00000000
```

This tries to change some `1` bits back to `0`, so it is illegal.

Expected error:

```text
ERR_CODE_0 = 0x4
```

---

## Read Lock Behavior

Read lock registers are located from:

```text
0x0AC - 0x0E4
```

Simplified behavior:

```text
1 = readable
0 = read-locked
```

Once a read lock register is cleared to `0`, it remains locked during simulation.

Example:

```cpp
write32(REG_VENDOR_TEST_READ_LOCK, 0);
```

This locks reads from partition 0 in the current model.

After that, reading from word 0 causes:

```text
ERR_CODE_0 = 0x5
```

---

## Digest Behavior

The model implements a simplified digest command.

DAI digest command:

```text
DIRECT_ACCESS_CMD = 0x4
```

Digest behavior in this model:

```text
1. Convert DIRECT_ACCESS_ADDRESS to a word address.
2. Detect the target partition.
3. Write fake non-zero digest values into digest registers.
4. Mark partition as digest-locked.
5. Further writes to that partition are blocked.
```

The fake digest values are:

```cpp
digest_regs[2 * partition]     = 0xD1650000 | partition;
digest_regs[2 * partition + 1] = 0xA5A50000 | partition;
```

A non-zero digest means the partition is write-locked in this simplified model.

---

## PARTITION_STATUS_0

Offset:

```text
0x014
```

The model uses simplified partition status bits.

```text
bit p      = partition p has non-zero digest / is digest-locked
bit p + 16 = partition p has error
```

Example:

```text
PARTITION_STATUS_0 = 0x2
```

Meaning:

```text
bit 1 = 1
partition 1 is digest-locked
```

---

## CHECK_TRIGGER

Offset:

```text
0x098
```

The model provides a simplified check trigger.

| Bit | Meaning                   |
| --: | ------------------------- |
|   0 | Trigger integrity check   |
|   1 | Trigger consistency check |
|  31 | Inject check failure      |

Example pass:

```cpp
write32(REG_CHECK_TRIGGER, 0x3);
```

This triggers both integrity and consistency checks and passes.

Example fail injection:

```cpp
write32(REG_CHECK_TRIGGER, 0x80000000);
```

This injects a check failure.

Expected error:

```text
ERR_CODE_0 = 0x6
```

---

## Timing Model

The model uses approximate simulation timing.

| Operation         |    Delay |
| ----------------- | -------: |
| Register access   |  `10 ns` |
| DAI read          | `100 ns` |
| DAI write/program |   `1 us` |
| DAI digest        | `500 ns` |
| Check trigger     | `200 ns` |

These values are not physical OTP timing values. They are used only for TLM simulation.

---

## Testbench

The testbench in `tb.cpp` verifies the following cases:

```text
Test 1: DAI write word0 = 0x12345678
Test 2: DAI read word0
Test 3: Illegal program 1 -> 0
Test 4: Read lock partition 0
Test 5: Digest locks partition 1 for write
Test 6: CHECK_TRIGGER pass
Test 7: Address out of range
```

Expected results:

```text
- Valid DAI write passes
- Valid DAI read returns correct data
- Illegal 1 -> 0 programming returns ERR_CODE_0 = 0x4
- Read lock returns ERR_CODE_0 = 0x5
- Digest lock blocks further writes to the partition
- CHECK_TRIGGER pass returns no error
- Out-of-range access returns ERR_CODE_0 = 0x5
```

---

## Example Output

Example successful output:

```text
[OTP] OpenTitan-like OTP TLM model created
[OTP] words=256, partition_words=64, partitions=4

[TB] Start OpenTitan-like OTP TLM test

[TB] MODEL_SIZE_WORDS     = 256
[TB] MODEL_PARTITION_SIZE = 64
[TB] STATUS               = 0x1
[TB] DIRECT_ACCESS_REGWEN = 0x1

[TB] Test 1: DAI write word0 = 0x12345678
[OTP] DAI WRITE byte_addr=0x0 word=0 wdata0=0x12345678 wdata1=0x0
[TB] STATUS     = 0x1
[TB] ERR_CODE_0 = 0x0
[TB] PASS: no error

[TB] Test 2: DAI read word0
[OTP] DAI READ byte_addr=0x0 word=0 rdata0=0x12345678 rdata1=0x0
[TB] RDATA_0    = 0x12345678
[TB] PASS: read data matched
[TB] PASS: no error

[TB] Test 3: illegal program 1 -> 0
[OTP] ERROR code=0x4
[TB] PASS: MACRO_WRITE_BLANK_ERROR detected, ERR_CODE_0=0x4

[TB] Test 4: read lock partition 0
[OTP] ERROR code=0x5
[TB] PASS: ACCESS_ERROR by read lock detected, ERR_CODE_0=0x5

[TB] Test 5: digest locks partition 1 for write
[OTP] DAI DIGEST partition=1
[TB] PARTITION_STATUS_0 = 0x2
[OTP] ERROR code=0x5
[TB] PASS: ACCESS_ERROR by digest lock detected, ERR_CODE_0=0x5

[TB] Test 6: CHECK_TRIGGER pass
[OTP] CHECK_TRIGGER value=0x3 PASS
[TB] PASS: no error

[TB] Test 7: address out of range
[OTP] ERROR code=0x5
[TB] PASS: ACCESS_ERROR by address out of range detected
```

---

## Build Using Makefile

Clean:

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

If SystemC is installed in a custom path:

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

If the runtime loader cannot find `libsystemc.so`, run one of:

```bash
LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib-linux64:$LD_LIBRARY_PATH ./build/otp_tlm
```

or:

```bash
LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib:$LD_LIBRARY_PATH ./build/otp_tlm
```

---

## Memory Map Usage in Platform

Inside the standalone model, addresses are register offsets.

Example:

```text
DIRECT_ACCESS_CMD     = 0x07C
DIRECT_ACCESS_ADDRESS = 0x080
DIRECT_ACCESS_WDATA_0 = 0x084
DIRECT_ACCESS_RDATA_0 = 0x08C
```

When integrated into a SoC platform, the platform provides a base address.

Example:

```text
OTP_BASE = 0x10070000
OTP_SIZE = 0x00001000
```

Then the CPU-visible full addresses become:

| Register                |       Full Address |
| ----------------------- | -----------------: |
| `DIRECT_ACCESS_CMD`     | `OTP_BASE + 0x07C` |
| `DIRECT_ACCESS_ADDRESS` | `OTP_BASE + 0x080` |
| `DIRECT_ACCESS_WDATA_0` | `OTP_BASE + 0x084` |
| `DIRECT_ACCESS_RDATA_0` | `OTP_BASE + 0x08C` |

The platform bus/router maps the range:

```text
0x10070000 - 0x10070FFF
```

to the OTP TLM model.

The OTP model itself should normally receive only register offsets after address decoding.

---

## Relationship to OpenTitan

This model is inspired by the OpenTitan OTP Controller.

Implemented OpenTitan-like concepts:

```text
- Bus-accessible OTP controller
- Direct Access Interface
- Direct read/program/digest commands
- Interrupt registers
- Status register
- Error code registers
- Partition status register
- Read lock registers
- Digest registers
- Check trigger registers
```

Simplified or stubbed concepts:

```text
- Digest is fake, not cryptographic
- ECC is not implemented
- Lifecycle interface is not implemented
- Key derivation interface is not implemented
- Secret scrambling is not implemented
- Background checks are not automatic
- Alert output is not connected
- Register fields are simplified
```

Therefore, the correct description is:

```text
This is an OpenTitan-like functional OTP TLM model.
```

It should not be described as:

```text
A full OpenTitan-compliant OTP Controller implementation.
```

---

## Summary

This component provides an OpenTitan-like OTP Memory Controller TLM model.

Main capabilities:

```text
- OpenTitan-like register map
- DAI read/write/digest commands
- OTP write-once behavior
- Illegal 1 -> 0 detection
- Read lock protection
- Digest-based write lock behavior
- Error reporting through ERR_CODE registers
- Interrupt signaling
- SystemC/TLM testbench verification
```

The model is suitable for:

```text
- Learning OTP controller behavior
- SystemC/TLM modeling practice
- IP-level verification
- Early SoC platform integration
- Firmware access testing through memory map
```
