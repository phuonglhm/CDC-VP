# FX1 SoC — Clock Manager (clkmgr) TLM Model

A SystemC/TLM-2.0 functional model of the OpenTitan Clock Manager (clkmgr) IP used in the FX1 SoC (ARM Corstone-1000 based design).

This model provides register-level clock control behavior for software verification in a TLM simulation environment.

---

## Overview

Controls:
- External clock switching (internal ↔ external)
- Clock gating for peripheral domains
- Software clock disable hints (AES/HMAC/KMAC/OTBN)
- Basic life-cycle-aware debug gating

Not cycle-accurate.

---

## Scope

### Implemented
- EXTCLK_CTRL / EXTCLK_STATUS (mubi4)
- EXTCLK_CTRL_REGWEN (write lock)
- CLK_ENABLES (4-bit gating)
- CLK_HINTS / CLK_HINTS_STATUS (idle-aware)
- AES/HMAC/KMAC/OTBN idle tracking
- Life-cycle state (Prod/Test/Dev/Rma)

---

## Key Behavior

### External clock (mubi4)
- 0x6 = True, 0x9 = False
- Immediate status acknowledgment
- Invalid transitions ignored

### REGWEN
- 0x1 = unlocked, 0x0 = locked (rw0c)
- Lock is permanent until reset

### CLK_ENABLES
Bitmask:
- bit0 IO_DIV4
- bit1 IO_DIV2
- bit2 IO
- bit3 USB

### CLK_HINTS
- Clock gating only occurs if target block is idle
- Idle set via set_*_idle() APIs

### Life cycle
- Only Test/Dev/Rma allow external clock effect
- Prod ignores switching effects

---

## Register Map

- 0x4  EXTCLK_CTRL_REGWEN
- 0x8  EXTCLK_CTRL
- 0xC  EXTCLK_STATUS
- 0x18 CLK_ENABLES
- 0x1C CLK_HINTS
- 0x20 CLK_HINTS_STATUS

---

## Build

```bash
make
./clkmgr_sim
```

---

## Structure

clkmgr_tlm/
├── include/clkmgr.h
├── src/clkmgr.cpp
├── tests/test_clkmgr_tlm.cpp
├── main.cpp
├── Makefile
└── CMakeLists.txt

---

## References
- OpenTitan clkmgr: https://opentitan.org/book/hw/ip/clkmgr/
- SystemC 2.3.3
