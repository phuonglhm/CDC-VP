# FU3 ADC Controller - SystemC/TLM VP

A digital ADC controller model based on the supplied HDS: 32-bit APB CSRs,
16 channels, 1-16 samples, 12-bit data, individual sample storage and averaging.
The FSM runs on the ADC clock and receives `D/EOC` from an external hardmacro.

Source: `FTEL_CDC_ADC_Controller_Hardware_Design_Specification.docx.pdf`, 22 pages.
SHA-256: `e8772c7f86a4106b1f556ac5e1368d31ce450b6d748dcfb7e94d8230ad876c58`.
The FSM diagram on page 10 and timing diagram on page 11 were also reviewed.
The HDS contains revision and template inconsistencies; its guideline text,
UART examples and secure-boot appendix are not treated as ADC requirements.

## Build and test

Requirements: Linux/WSL, C++17, CMake and SystemC 2.3.4.

```sh
cmake -S components/FU3_Components/ADC -B build/fu3-adc \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/fu3-adc -j4
ctest --test-dir build/fu3-adc --output-on-failure
```

In PowerShell, prefix each command with `wsl`.

Run Debug, Release and UBSan regressions, coverage collection and JSON/XML reporting:

```sh
bash components/FU3_Components/ADC/scripts/run_full.sh
```

## Integration

CMake target: `fu3::adc`; header: `fu3/adc/adc.h`; class: `fu3::adc::Adc`.
Consumers can use `add_subdirectory(...)`, link `fu3::adc` and bind every port below.
The component currently uses the standalone build described above and is not
connected to the repository root build. Source files, CMake files and READMEs
outside the ADC directory remain unchanged. The existing `components/adc_tlm`
component has a different programming model and is not replaced.

| Interface | Direction relative to controller | Function |
| --- | --- | --- |
| `socket` | TLM target | Local offsets 0x00..0x58, little-endian, 4-byte aligned |
| `adc_clk` | Input | Platform-provided 10 MHz ADC clock; FSM advances on rising edges |
| `reset_n` | Input | Active-low reset with asynchronous assertion |
| `data_in`, `eoc` | Input | Hardmacro D[11:0] and end-of-conversion indication |
| `sampclk`, `cs` | Output | Sampling clock and input MUX enable |
| `pwd`, `macro_reset` | Output | Hardmacro power-down and active-high reset |
| `samp_sel`, `channel` | Output | Sampling mode and M[3:0] |

The existing FU3 bus defines target `ADC` at `0x500A0000`, with size `0x10000`,
on the `SYSBUS_1 -> PERIBUS_1` path.
Bind it with `fabric.target("ADC").bind(adc.socket)`.
Integration tests use the existing `fu3::bus_system` implementation.
No IRQ or DMA ports are provided because the HDS does not define them.

## Registers

| Offset | Register | Access | Reset | Contents |
| --- | --- | --- | --- | --- |
| 0x00 | VERSION | RO | 0x00010000 | Major 1, minor 0 |
| 0x04 | ADC_CTRL | RW | 0 | ADC_EN[0] |
| 0x08 | ADC_CFG | RW | 0x10 | SAMP_SEL[4], CHANNEL_SEL[3:0] |
| 0x0C | ADC_STATUS | RO | 0 | DONE[0]; READY[1] and BUSY[2] are VP policy |
| 0x10 | ADC_DATA | RO | 0 | 12-bit averaged result |
| 0x14 | ADC_TIMING | RW | 0x20040C00 | Setup[31:24], high[23:16], low[15:8] |
| 0x18 | ADC_SAMPLE_CFG | RW | 1 | SAMPLE_COUNT[4:0] |
| 0x1C + 4*n | ADC_SAMPLE_n | RO | 0 | 12-bit sample, n=0..15 |

Effective setup/high/low timing is clamped to at least 32/4/12 ADC cycles;
the effective sample count is clamped to 1..16. CSR reads return the masked
programmed values rather than the clamped values. Configuration is latched
when the FSM leaves POWER_DOWN. DONE remains asserted until disable.
The programming sequence and policies not fully specified by the HDS are
documented in [spec-decisions.md](docs/spec-decisions.md).

## Verification and scope

[testing.md](docs/testing.md) describes regression commands and test data.
[hds-verification-matrix.md](docs/hds-verification-matrix.md) maps requirements to tests.
[hds-clarification-request.md](docs/hds-clarification-request.md) lists questions for the IP owner.

The model does not simulate analog voltages, noise, a SAR transfer function,
pin-level APB, clock-divider RTL or metastability/CDC. The D/EOC sources in the
tests provide digital stimulus only. Passing tests do not establish
RTL equivalence or hardware sign-off.
