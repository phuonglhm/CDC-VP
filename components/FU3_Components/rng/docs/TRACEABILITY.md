# RNG requirements and test coverage

Source pages refer to RNG HDS V1.0. Private source material is not copied into this component. Test expectations use literal CSR offsets and a known 32-bit input pattern, not values computed from the DUT state.

| Requirement | Source | Verification |
|---|---|---|
| Five exact aligned CSRs, reset values | pp.12-14 | Reset readback, reserved CONTROL bits, COMMAND read-zero |
| CONTROL/COMMAND strobe lane0 | pp.12-13 | Other-lane writes do not enable or start |
| DELAY independent byte writes | p.13 | Sparse replacement and zero-strobe no-op |
| Invalid address/RO write errors | p.11 | Unmapped read returns zero; RO writes error, including zero strobes; direct unaligned access |
| Data zero while invalid, non-consuming reads | p.14 | Before completion, repeated readback, VALID remains set |
| NEXT clears accumulation/valid | pp.14-15 | New collection, unfinished collection restart |
| RESTART resets collection | p.9 | Clear VALID, combined command exercises documented VP priority |
| Synchronous reset | p.9 | Between-edge pulse ignored, reset during collection clears state |
| IRQ from VALID or completed APB error | p.15 | Valid level, NEXT/RESTART deassertion, transient error and no sticky pending |
| No fixed output throughput guaranteed | pp.6,15 | VP schedule is separately documented; no hardware rate claim |
| FU3 base and interrupt routes | HAS memory/interrupt maps | Same tests through real bus at literal 0x44030000; fake PLIC index20 and fast10 sinks |
| Disabled and extreme DELAY cases | VP policy, HDS incomplete | Pause/resume, retained valid data, maximum delay, rearm after delay change |
| Debug backdoor | VP adapter policy | Invalid debug read does not cause IRQ |

## Not demonstrated by these tests

Physical ring-oscillator behavior, exact generator state equations and bit ordering, entropy quality, actual PLIC sampling of short IRQ pulses, RTL nextTrigger_REG edge detection, full counter wraparound, runtime clock gating, and CPU firmware boot are not verified. No unsupported health-test, FIFO, DMA or interrupt mask feature is inferred from the architecture wish list or the older trng_tlm model.

Integration tests verify fake IRQ sinks observing the source; they do not execute PLIC claim/complete transactions. Upstream bus rejection is distinct from a local RNG access error. The read/write adapter currently requires word payloads; CPU narrow-access support is not claimed.
