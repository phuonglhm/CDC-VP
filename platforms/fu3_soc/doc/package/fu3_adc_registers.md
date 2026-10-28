# ADC registers (VP_FU3_V1.0)

From `components/FU3_Components/ADC/include/fu3/adc/adc.h` (`fu3::adc::reg`)
at this package's revision. Base `0x500A_0000`, 64 KiB window, 32-bit
registers. Field details and the decisions taken from the ADC HDS:
`components/FU3_Components/ADC/docs/spec-decisions.md` (copied as
`fu3_adc_spec_decisions.md`).

| Offset | Register |
| --- | --- |
| 0x00 | VERSION (reset `0x0001_0000`) |
| 0x04 | CTRL |
| 0x08 | CFG |
| 0x0C | STATUS: bit 0 DONE (HDS), bit 1 READY and bit 2 BUSY (VP ABI) |
| 0x10 | DATA |
| 0x14 | TIMING |
| 0x18 | SAMPLE_CFG |
| 0x1C + 4 x n | SAMPLE n |

The ADC has no interrupt output in R1 (PLIC ID 17 tied low, Q03-f): poll STATUS.
