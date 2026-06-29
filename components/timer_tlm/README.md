# Timer Model & Testbench

A SystemC/TLM-2.0 model of a programmable peripheral timer.

---

## Files

| File | Description |
|---|---|
| `include/timer.h` | Timer module declaration, register offsets, control bit masks |
| `src/timer.cpp` | Timer implementation — register access and countdown thread |
| `tests/test_timer_tlm.cpp` | Testbench — programs the timer and runs the simulation |

---

## Timer Registers

| Offset | Name | Description |
|---|---|---|
| `0x00` | `RELOAD` | Reload value |
| `0x04` | `VALUE` | Current counter (read-only) |
| `0x08` | `CTRL` | Enable / mode / IRQ gate |
| `0x0C` | `INTSTATUS` | Interrupt pending flag (write `0x1` to clear) |

**CTRL bits:**
- `bit 0` — ENABLE
- `bit 1` — EXT_EN — pause counter when `extin` signal is LOW
- `bit 2` — EXT_CLK — use `extin` rising edges as the clock source
- `bit 3` — INTR_EN — gate IRQ output; `INTSTATUS` still sets without this

---

## Testbench — 11 Test Cases

| # | Name | What it checks |
|---|---|---|
| 1 | Basic interrupt | `RELOAD=5`, ENABLE + INTR_EN; waits for `posedge` on IRQ, verifies VALUE reloads, clears interrupt |
| 2 | INTR_EN=0 | IRQ output stays low but `INTSTATUS` still sets when counter expires |
| 3 | Disable mid-count | Writes `CTRL=0` partway through; reads VALUE twice and confirms it stopped changing |
| 4 | RELOAD sets VALUE | Writing RELOAD immediately updates the VALUE register |
| 5 | CTRL read-back | Writes a CTRL value and reads it back to confirm register retention |
| 6 | Reset mid-count | Pulses `reset_n` low during a count; confirms CTRL and VALUE both clear to 0 |
| 7 | EXT_EN pause | Sets EXT_EN, drives `extin` LOW; confirms timer does not count, then releases and waits for IRQ |
| 8 | EXT_CLK | Sets EXT_CLK, manually pulses `extin` 4 times; confirms IRQ fires after 4 edges with `RELOAD=3` |
| 9 | Multiple interrupts | Clears and re-arms 3 consecutive interrupts with `RELOAD=2` |
| 10 | Bad data length | Sends a non-32-bit TLM transaction; expects `TLM_GENERIC_ERROR_RESPONSE` |
| 11 | Bad address | Writes to an unmapped offset `0xFF`; expects `TLM_ADDRESS_ERROR_RESPONSE` |

---

## Build

```bash
make
./test_timer_tlm
```

---

## Expected Output

```
=== TEST 1: basic interrupt ===
PASS: basic interrupt fired
PASS: VALUE reloaded to 5
PASS: interrupt cleared
...
=== TEST 11: bad address ===
PASS: bad address returns TLM_ADDRESS_ERROR_RESPONSE

=== All tests done ===
```

---

## Requirements

- SystemC 2.3.x
- TLM-2.0
- C++17
