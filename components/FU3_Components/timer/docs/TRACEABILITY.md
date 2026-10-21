# Timer HDS behavior traceability

Source: private Timer HDS V1.0, reviewed 2026-10-17. Page numbers are PDF pages.

| Source | Behavior | Verification |
|---|---|---|
| pp.7,13-15 | 32-bit TIMER/CTRL/CMP; reset zero | State and TLM register checks |
| p.10 | Increment and unusual prescaler | Hand-derived CTRL=1,9,73 sequences; disabled phase test |
| p.11 | Write priorities; old control on control write | State tests for count/clear/write collisions |
| p.11 | Compare and overflow independent of ENABLE | Disabled writes assert raw IRQs; CMP=0 does not suppress overflow |
| p.11 | No sticky pending or software acknowledge | Timed IRQ hold before tick and deassertion after tick |
| p.10 | Asynchronous reset | Reset between ticks clears state and IRQ |
| pp.13-14 | Upper address aliases and reserved offset | 0x14 aliases CTRL; 0x0C read zero/write ignored |
| Common BUS INTF overrides HDS p.8 | Full byte strobes | All 16 masks preserve disabled counter bytes; narrow bus writes reach aligned word lanes |
| p.12 | Valid/reserved accesses do not report APB error | OK responses; VP malformed-request errors separately documented |
| HDS p.12; HAS pp.49,52-53 | Two raw IRQs mapped to each Timer source | OR output and fake direct/PLIC/INTMAP routes |
| HAS pp.46-47 | Four distinct windows | Actual FU3 BusSystem drives four Timer models |
| VP choice | Freeze on clock gating | Stop/resume tests; peers continue independently |
| VP choice | Reset cancels pending transport | Pending read cancellation followed by successful write |
| VP choice | Read-only debug | No-time inspection, debug write rejection |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run behavior checks via find_package |

These tests execute driver-style TLM requests, not CPU firmware instructions.
Fake receivers verify wiring and observation; real PLIC/INTMAP capture/service
requires later platform integration. Packed TIMER_CNT>1, PWM, DMA and external
clock inputs are outside this four-single-instance FU3 model.
