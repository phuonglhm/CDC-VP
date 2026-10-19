# HDS-to-test verification matrix

Test names below omit the `fu3_adc_` prefix. Each scenario also checks VP
payload and driver behavior. Only assertions whose expected values follow
explicit HDS requirements contribute to `SPEC_CHECKS`.
Machine-readable metadata is in [hds-requirements.json](../tests/hds-requirements.json).

| ID | Pages | Requirement or scope | Tests | Basis and limitations |
| --- | --- | --- | --- | --- |
| ADC-01 | 13-18 | 23 CSRs, VERSION, reset, RO/RW and reserved fields | registers, bus_registers | Walking-bit checks for every field; ignoring writes implements RO access |
| ADC-02 | 7-9,14 | 16 channels, 12-bit D and mode pin | channels, bus_channels | Distinct samples for channels 0..15; analog test mode remains unverified |
| ADC-03 | 15 | Effective sample count 1..16 | counts, bus_counts | Sweep all encodings 0..31; unused-slot behavior is policy |
| ADC-04 | 11-12,15 | Minimum and programmed timing | timing_min, timing_programmed, timing_max | 0/min-1/min/min+1/255; measure output pulses; state overhead is policy |
| ADC-05 | 10,15-18 | Raw samples and average | averaging, randomized | 0/4095, boundaries around 2048, maximum sum 65520, fractional averages and 160 sequences with a fixed seed |
| ADC-06 | 10,18-19 | Sticky DONE, disable and restart | lifecycle, bus_lifecycle | Repeated reads/enables and long-to-short sequences; retention is policy |
| ADC-07 | 18 | Capture configuration at sequence start | snapshot, bus_snapshot | Write channel/count/timing during HIGH; verify the current and next sequences |
| ADC-08 | 12 | Active-low reset clears state/data | reset, reset_phases, reset_transport, clock_pause | Reset in every enabled state, with stopped clock, during MMIO and with short pulses |
| ADC-09 | 9-11 | WAIT_EOC and capture | eoc_wait, eoc_policy | Missing/late/early/held EOC; edge-versus-level semantics remain unresolved |
| ADC-10 | 13,18 | Status bits | lifecycle | DONE[0] is explicit; READY/BUSY bit assignment is policy |
| ADC-11 | 10 | Disable during conversion | disable_phases | Check every enabled state and recovery; missing HDS transitions require policy |
| ADC-12 | 13 | APB/TLM contract | protocol, debug | No aliasing of 64-bit addresses; length/streaming/BE/null/command errors, debug and DMI; not pin-level APB verification |
| ADC-13 | 18 | Base address, window and bus path | bus_bus | Existing FU3 bus, four-word burst, subword/unmapped errors |
| ADC-14 | 12 | Clock and reset synchronization | clock_pause | Externally supplied clock; divider/CDC RTL remains unverified |
| ADC-15 | 8-12 | Analog hardmacro and ISAMPCLK | None | Outside scope; the separate hardmacro specification is unavailable |

ADC-09/10/11/14/15 retain their unresolved or incompletely verified status
even when the regression passes. Line and branch coverage measures executed
code and does not replace HDS requirement coverage.
