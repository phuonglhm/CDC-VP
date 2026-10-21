# FU3 Timer functional contract

## Common Spec baseline (2026-10-21)

The supplied FU3 Common Spec v2.0 workbook is the highest authority, followed
by FU3 HAS v2.0 and the latest IP detail where higher-level documents are silent.
See the bus [rebaseline decisions](../../bus/docs/COMMON_REBASELINE.md) for exact
cross-IP choices, source conflicts and platform responsibilities. Earlier test
results below retain their historical source baselines; current joint results
are recorded in the bus validation document.

## Sources and scope

Timer HDS V1.0, pp.7-15 defines the IP behavior. FU3 HAS v2.0 pp.46-47,
49, 52-53 defines system windows and interrupt routes. The source PDFs are
private local documents and are not included in this package.

This model verifies software-visible behavior, data flow and interrupt flow.
It does not model APB pin handshakes or RTL pipeline latency. Functional ticks
preserve the state-transition rules that affect counter values and IRQs.

## Registers and state transitions

All three registers are 32-bit RW, little-endian at the TLM interface, reset zero:

| Offset | Register | Write effect |
|---|---|---|
| 0x00 | TIMER | Load counter; overrides all other counter updates |
| 0x04 | TIMER_CTRL | Store all 32 bits; old control still governs this tick |
| 0x08 | CMP | Store compare value and clear counter |
| 0x0C | Reserved | No register write; reads return zero |

TIMER_CTRL[0] enables counting; [5:3] is PRESCALER. Other bits are retained and
participate in the internal cycle-counter threshold comparison. They must not be
masked away. Reads have no side effects. There is no interrupt status/mask/clear
register and no reload register.

On each enabled clock tick, decisions use old state:

1. Internal cycle counter becomes zero if old cycle >= old TIMER_CTRL; otherwise
   it increments, even when timer counting is disabled.
2. TIMER write loads data; otherwise CMP write clears TIMER; otherwise overflow
   or compare equality clears TIMER; otherwise an enabled count condition adds 1.
3. CTRL/CMP writes commit their new values.

The count condition is ENABLE and either PRESCALER=0 or PRESCALER equals the
old internal cycle counter's low three bits. This is NOT division by prescaler+1.
For CTRL=9 from zero cycle phase, increments occur on ticks 2,10,12,20. For CTRL=73
with the same prescaler field, they occur on ticks 2,10,18 within the first 20 ticks.

Overflow is TIMER==0xFFFFFFFF. Compare is CMP!=0 and TIMER==CMP. Neither depends
on ENABLE. Conditions clear TIMER at the next tick unless overridden by a write.
CMP=0 suppresses compare only. A disabled timer can still interrupt and clear.
Free-running operation uses CMP=0; a nonzero CMP provides periodic compare/clear
behavior. No extra mode bits are invented from the broader HAS feature summary.

## VP timing, reset and clock gating

Each Timer has a configurable positive period (default 10 ns, a VP parameter).
The first tick occurs one period after simulation start when reset is already
released, or one period after reset release. Ticks continue on that phase grid.
Reset is active-low and asynchronous: it clears all state and IRQ conditions and
cancels a pending transaction with GENERIC_ERROR. A single SystemC method drives
IRQ outputs, avoiding multiple-writer signal errors.

clock_enabled=false freezes all state, including internal phase state and raw IRQ
conditions; the tick grid continues. Re-enabling resumes at the next grid tick.
This is a platform gating contract, not a new register. Reset works while gated.
Access during reset or gating is diagnosed with GENERIC_ERROR; the model does not
simulate an indefinite bus deadlock. Runtime period changes are not supported.

Blocking transport consumes incoming delay once, validates the request, and
serializes admission. A valid access samples/commits at the next processing tick.
Reads return pre-edge state; writes take the documented priority over that tick.
A request arriving at exactly the tick time follows SystemC process scheduling;
testbenches should issue off-edge when requiring a specific sampling order.
No APB setup/access cycles are added here: the existing bus models bridge timing.
The wait for a functional sampling tick is a VP synchronization convention, not
an inserted PREADY wait state. Rejected malformed requests consume one period.

## Access adapter and explicit strobe exception

Common Full strobes apply at the model boundary. Counter/control/compare
merge enabled bytes at the functional edge; zero strobes do not write. Raw IRQ
behavior and write priority remain unchanged. See the shared rebaseline decisions.

Common Full strobes apply at the model boundary. Counter/control/compare
merge enabled bytes at the functional edge; zero strobes do not write. Raw IRQ
behavior and write priority remain unchanged. See the shared rebaseline decisions.

These adapter diagnostics are not a claim that the HDS IP asserts PSLVERR.
The HDS reports PSLVERR=0 for native accesses, including reserved offsets.
The adapter rejects unaligned calls even though low address bits are ignored
by the described RTL. Upper-bit aliasing IS preserved: register decode uses
local address & 0xC throughout the routed 64-KiB aperture.

transport_dbg supports word read-only inspection without time or side
effects, including during reset/gating. Debug writes are rejected. DMI is disabled.
Blocking transport must run in SC_THREAD; each in-flight call owns its payload.

## Four-instance integration

TimerBank creates four independent TIMER_CNT=1 IPs, each with its own period,
reset input and gate input. This is not a packed TIMER_CNT=4 IP aperture.
The 16-bit local window is an SoC integration choice; the generic IP's default
APB_ADDR_WIDTH=12 is not treated as a requirement to add four new address windows.

| Instance | Base | Bus | CVA6 route | Ibex route |
|---|---|---|---|---|
| TIMER0 | 0x44060000 | PERIBUS_0 | time_irq_i | Not specified |
| TIMER1 | 0x44070000 | PERIBUS_0 | PLIC ID 6 | irq_timer_i |
| TIMER2 | 0x50060000 | PERIBUS_1 | PLIC ID 7 | irq_fast_i[0], OR with TIMER3 |
| TIMER3 | 0x50070000 | PERIBUS_1 | PLIC ID 8 | irq_fast_i[0], OR with TIMER2 |

Each Timer exposes overflow_irq and compare_irq directly. Its convenience irq
output is their OR, the chosen mapping to the HAS's single irq_timerN. It adds
no sticky latch, mask or acknowledge. PLIC and INTMAP routing remains platform
wiring; integration tests use fake receivers with these routes. A receiver must
capture raw levels as required; polling them occasionally is not sufficient to
guarantee observation. Integration here does not prove real PLIC capture or CPU
interrupt service. There is no DMA request, external event clock, PWM or capture.

## Verification boundary

Behavior tests include hand-derived counter sequences and prescaler phase examples,
write priorities, IRQ timing, reset, gating, aliases and strict access diagnostics.
Integration tests use the actual FU3 bus with only the four Timer windows enabled,
a fake MMIO master, and fake interrupt receivers. They test register isolation,
repeated interrupts and continued peer operation during clock gating.
These are driver-style TLM transactions, not executed CPU firmware. Existing
components/timer_tlm is not reused because its register map/behavior differs.
