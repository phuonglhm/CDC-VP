# rtc_tlm

Memory-mapped TLM-2.0 real-time clock, modelled after the ARM PL031 RTC.

A free-running 32-bit counter advances one LSB every `tick_period` (default
1 Hz, i.e. one count per simulated second) while counting is enabled. When the
counter matches the alarm register the raw interrupt latches, and an unmasked
alarm drives `irq_out` to the PLIC.

## Register Map

| Offset | Name | Access | Description |
|---:|---|---|---|
| `0x00` | DR   | R     | Current counter value (free-running). |
| `0x04` | MR   | R/W   | Match/alarm value. Alarm fires when `DR == MR`. |
| `0x08` | LR   | R/W   | Load value; a write seeds `DR` immediately. |
| `0x0C` | CR   | R/W   | bit0 EN (PL031 RTCEN): write-once, set-only enable. |
| `0x10` | IMSC | R/W   | bit0: alarm interrupt mask (1 = enabled). |
| `0x14` | RIS  | R     | bit0: raw alarm interrupt status. |
| `0x18` | MIS  | R     | bit0: masked status (`RIS & IMSC`). |
| `0x1C` | ICR  | W1C   | bit0: write 1 to clear the alarm. |

`irq_out` is asserted while `MIS.bit0` is set (`RIS & IMSC`).

Accesses must be 32-bit and 4-byte aligned within `0x00..0x1C`; anything else
returns `TLM_ADDRESS_ERROR_RESPONSE`. `transport_dbg` provides side-effect-free
backdoor access.

## Behaviour Notes

- The counter wraps at 2^32 (intentional free-run), matching the PL031 32-bit
  up-counter.
- `CR.EN` (PL031 RTCEN) is write-once: software can start the RTC but cannot stop
  it by writing 0. Only `reset_n` clears it. This matches real PL031 firmware
  expectations (the RTC is a persistent time-of-day source).
- `LR` (load) seeds the counter immediately and works regardless of enable state.
  Reading `LR` returns the last loaded base value, not the live counter; read the
  live time-of-day from `DR`.
- The alarm latches in `RIS` on the tick where `DR` reaches `MR` (edge), and is
  cleared by `ICR` (W1C) — consistent with the QEMU PL031 model.
- Reset is active-low (`reset_n`); holding it low freezes and clears the model.
- `tick_period` decouples the simulated-time meaning of one count from the
  register model, so unit tests can run the clock fast (the test uses 1 ns).

## Usage

```cpp
#include <rtc_tlm.h>

cdc::components::rtc_tlm rtc("rtc"); // real 1 Hz time-of-day clock
sc_core::sc_signal<bool> rtc_irq;

bus.add_target(RTC0_BASE, 0x1000).bind(rtc.socket);
rtc.reset_n(reset_n);
rtc.irq_out(rtc_irq);
plic.irq_in[source_id - 1](rtc_irq);
```
