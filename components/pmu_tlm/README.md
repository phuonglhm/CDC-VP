# PMU (Power Manager) Model & Testbench

A SystemC/TLM-2.0 model of an OpenTitan-pwrmgr-style power manager: two
cooperating FSMs (slow/AST domain + fast domain) sequencing power-up, low-power
entry/exit, reset requests and escalation faults.

---

## Files

| File | Description |
|---|---|
| `include/pmu.h` | `Pwrmgr` module declaration, register offsets, bit masks, FSM state enums |
| `src/pmu.cpp` | Implementation — register access, slow/fast FSM threads, escalation + power-glitch monitors |
| `tests/test_pmu_tlm.cpp` | Testbench — drives the environment handshake and register flows |

Standalone test: `make` then `./test_pmu_tlm` (`make clean` to clean outputs).

---

## Registers (32-bit LE, byte enables honored; unmapped offset → address error)

| Offset | Name | Access | Reset | Description |
|---|---|---|---|---|
| `0x00` | `INTR_STATE` | RW1C | `0x0` | bit0 `WAKEUP` interrupt pending; write 1 to clear |
| `0x04` | `INTR_ENABLE` | RW | `0x0` | bit0 gates `wakeup_irq` |
| `0x08` | `INTR_TEST` | WO | — | write 1 to bit0 → sets `INTR_STATE.WAKEUP` (reads 0) |
| `0x0C` | `ALERT_TEST` | WO | — | write 1 to bit0 → sets `FAULT_STATUS.REG_INTG_ERR` (reads 0) |
| `0x10` | `CTRL_CFG_REGWEN` | RO | `0x1` | 1 = `CONTROL` writable; hardware clears it while a low-power entry is in progress and restores it on fall-through/abort |
| `0x14` | `CONTROL` | RW (gated) | `0x180` | see bit-fields below; writable mask `0x1F1`, gated by `CTRL_CFG_REGWEN` |
| `0x18` | `CFG_CDC_SYNC` | RW1C | `0x0` | write 1 to clear; sync is instantaneous in the model, reads back 0 |
| `0x1C` | `WAKEUP_EN_REGWEN` | RW0C | `0x1` | write 0 to lock `WAKEUP_EN` (sticky until reset) |
| `0x20` | `WAKEUP_EN` | RW (gated) | `0x0` | bits [5:0] enable the 6 wakeup inputs |
| `0x24` | `WAKE_STATUS` | RO | `0x0` | bits [5:0]: enabled wakeup inputs currently asserted |
| `0x28` | `RESET_EN_REGWEN` | RW0C | `0x1` | write 0 to lock `RESET_EN` (sticky until reset) |
| `0x2C` | `RESET_EN` | RW (gated) | `0x0` | bits [1:0] enable the 2 peripheral reset-request inputs |
| `0x30` | `RESET_STATUS` | RO | `0x0` | reserved — the model acts on reset requests immediately and never latches this register |
| `0x34` | `ESCALATE_RESET_STATUS` | RO | `0x0` | bit0 set on `esc_rx` or escalation-clock timeout |
| `0x38` | `WAKE_INFO_CAPTURE_DIS` | RW | `0x0` | bit0 = 1 stops recording into `WAKE_INFO` |
| `0x3C` | `WAKE_INFO` | RW1C | `0x0` | [5:0] wakeup reasons, bit6 `FALL_THROUGH`, bit7 `ABORT`; write 1 to clear |
| `0x40` | `FAULT_STATUS` | RO | `0x0` | bit0 `REG_INTG_ERR`, bit1 `ESC_TIMEOUT`, bit2 `MAIN_PD_GLITCH` |

**`CONTROL` bit-fields** (reset `0x180`):

| Bit | Name | Meaning |
|---|---|---|
| 0 | `LOW_POWER_HINT` | request low-power entry (armed when the core sleeps); hardware self-clears it on entry, fall-through and abort |
| 4 | `CORE_CLK_EN` | stored only — clock gating is not modeled |
| 5 | `IO_CLK_EN` | stored only |
| 6 | `USB_CLK_EN_LP` | stored only |
| 7 | `USB_CLK_EN_ACTIVE` | stored only |
| 8 | `MAIN_PD_N` | 1 = keep main power domain on during low power; 0 = power it down (`ast_main_pd_n` drops) |

---

## Behaviour

- **Power-up:** slow FSM `RESET → PWR_UP_AST → REQ_FAST_PWR → IDLE`; fast FSM
  `LOW_POWER → CLKS_ON → OTP_INIT → LC_INIT → STRAP → ROM_CHECK → ACTIVE`,
  gated by the environment inputs `otp_done`, `lc_done`, `rom_done`.
  `fetch_en` asserts in `ROM_CHECK` when `rom_good` (or `lc_test_state`) is set.
- **Low-power entry:** software sets `CONTROL.LOW_POWER_HINT` and the core
  asserts `core_sleeping` → `LOW_POWER_PREP`. If the hint is dropped before
  commit, entry **falls through**: `WAKE_INFO.FALL_THROUGH` + `INTR_STATE.WAKEUP`
  are set and the FSM returns to `ACTIVE`. If `flash_idle` is low, entry
  **aborts**: `WAKE_INFO.ABORT` + `INTR_STATE.WAKEUP`, back to `ACTIVE`.
- **Wakeup:** in `LOW_POWER`, any enabled `wakeups` bit re-runs the power-up
  sequence; reasons are latched into `WAKE_STATUS` / `WAKE_INFO` unless
  `WAKE_INFO_CAPTURE_DIS` is set. A plain wakeup does **not** set
  `INTR_STATE.WAKEUP` — only fall-through, abort and `INTR_TEST` do.
- **Reset requests:** enabled `rstreqs`, `sw_rst_req`, `ndmreset_req`,
  escalation or a main-power glitch trigger a reset pulse on `sys_rst_n`
  (`RESET_PREP → CLKS_ON` re-init).
- **Escalation monitor:** `esc_rx` sets `ESCALATE_RESET_STATUS`; a dead
  escalation clock (128 ticks without `esc_clk_alive`) additionally sets
  `FAULT_STATUS.ESC_TIMEOUT`.
- **Power-glitch monitor:** `main_pok` dropping while the main domain should be
  on sets `FAULT_STATUS.MAIN_PD_GLITCH` and requests a reset.
- Register access costs ~10 ns; only 32-bit transactions are accepted.

---

## In the VP_FX1 SoC

- Base `CDC_PMU0_BASE = 0x1009_0000`, interrupt `CDC_IRQ_PMU0 = 11` (PLIC,
  level = `INTR_STATE & INTR_ENABLE`).
- The SoC top drives the environment handshake at boot (POR, OTP/LC/ROM done,
  `rom_good`/`flash_idle`/`main_pok` high), so firmware sees the PMU already
  `ACTIVE`; its reset outputs are observed only, not routed to the SoC reset.
- The 6 `wakeups` and 2 `rstreqs` inputs are tied to 0 in the SoC: a committed
  low-power entry has no wired wakeup source, so exercise the fall-through /
  abort / `INTR_TEST` flows instead of a full sleep-wake round trip.
