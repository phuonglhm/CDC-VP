# Questions for the IP owner

The source ADC Controller HDS and its SHA-256 are pinned in the [README](../README.md).
This is a handoff document; these questions have not been sent externally.

1. Identify the authoritative revision: the cover says V1.0, the history includes
   V2.0 and the repeated header identifies FU2 V5.7.
2. Provide the ADC_STATUS field table: BUSY/READY bit positions, reset values
   and transition timing.
3. Specify average rounding when the sum is not divisible by the sample count:
   truncation, round-to-nearest or another rule.
4. Specify whether timing/count reads return raw programmed values or clamped values.
5. Clarify whether EOC uses edge detection or level sensing in WAIT_EOC.
   How are early pulses and held-high EOC handled? Is D captured on the edge
   that detects EOC or in the subsequent CAPTURE state?
6. Does disable immediately abort SETUP/HIGH/LOW/WAIT_EOC/CAPTURE/CHECK_CNT?
7. Are data and samples retained after disable? Does a new start clear all 16 samples?
8. Confirm the SETUP_WAIT cycle count, WAIT_EOC/CAPTURE/CHECK_CNT latency
   and CS/PWD/RST levels during SETUP_WAIT and DONE.
9. Define RO, reserved, unmapped and subword APB access behavior when
   PSLVERR/PSTRB are unsupported.
10. Provide the 80-to-10 MHz divider configuration, CSR/data CDC scheme and
    reset release synchronizer.
11. Provide HHG_F011Q7P6_JLADC12001MDP if analog/test-mode modeling or
    ISAMPCLK verification is required; the HDS currently only references it.

Current choices and their effects are documented in
[spec-decisions.md](spec-decisions.md). Passing regression tests do not resolve
these questions.
