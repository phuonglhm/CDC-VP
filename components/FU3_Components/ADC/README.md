# FU3 ADC Controller - SystemC/TLM

Digital controller for an external 12-bit ADC hardmacro: 16 channels, 1-16
samples per sequence, raw sample storage and an averaged result.

## Specification

Specification baseline: **ADC Controller HDS V2.0 - April 3, 2026**,
according to the revision history on PDF page 4 ("Update overall specification").
Source: `FTEL_CDC_ADC_Controller_Hardware_Design_Specification.docx.pdf`, 22 pages.
SHA-256: `71d34fa641a92b679f37ab9dea1699d598260dc8c18084bdf17e03fbfe8ab8ef`.

The cover still displays V1.0 / March 20, 2026 and the repeated header says
FU2 V5.7; these inconsistent labels do not identify the selected baseline.
The ADC IP `VERSION` CSR is `0x00010000` (major 1, minor 0), as specified
on pages 14 and 18; the HDS document revision and IP version are distinct.

Reviewed again on **2026-10-21**, including the FSM on page 10 and waveform
on page 11. The digital behavior follows the register tables and controller
FSM. No confirmed digital functional defect was found in this review.
See [spec-decisions.md](docs/spec-decisions.md) for exact timing, STATUS
definitions, specification gaps and the TLM abstraction boundary.

## Build and test

Use Linux/WSL, C++17, CMake and SystemC 2.3.4. Run from the repository root:

```sh
cmake -S components/FU3_Components/ADC -B build/fu3-adc \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/fu3-adc -j4
ctest --test-dir build/fu3-adc --output-on-failure
```

Full Debug, Release and UBSan regression, with coverage and JSON/XML evidence:

```sh
bash components/FU3_Components/ADC/scripts/run_full.sh
```

In PowerShell, use `wsl --exec` before these commands.
Test coverage, requirement mapping and results are described in
[testing.md](docs/testing.md).

## Integration

Target: `fu3::adc`; header: `fu3/adc/adc.h`; class: `fu3::adc::Adc`.
Use `add_subdirectory(...)`, link the target and bind every port below.
ADC currently builds independently of the repository root build.
The separate `components/adc_tlm` model has a different programming model.

| Interface | Direction | Purpose |
| --- | --- | --- |
| `socket` | TLM target | Local offsets 0x00-0x58; aligned 32-bit little-endian accesses |
| `adc_clk` | Input | Platform-provided nominal 10 MHz clock |
| `reset_n` | Input | Active-low reset; asynchronous assertion |
| `data_in`, `eoc` | Input | External hardmacro D[11:0] and conversion-complete indication |
| `sampclk`, `cs` | Output | Sampling clock and input MUX enable |
| `pwd`, `macro_reset` | Output | Active-high hardmacro power-down and reset |
| `samp_sel`, `channel` | Output | Sampling mode and M[3:0] |

The FU3 bus maps ADC at `0x500A0000`, window `0x10000`, through
`SYSBUS_1 -> PERIBUS_1`. Bind with `fabric.target("ADC").bind(adc.socket)`.
Integration tests use the existing `fu3::bus_system` target.
No IRQ or DMA interface is defined by the HDS.

## Registers and programming

| Offset | Register | Access | Reset | Fields |
| --- | --- | --- | --- | --- |
| 0x00 | VERSION | RO | 0x00010000 | Major 1, minor 0 |
| 0x04 | ADC_CTRL | RW | 0 | ADC_EN[0] |
| 0x08 | ADC_CFG | RW | 0x10 | SAMP_SEL[4], CHANNEL_SEL[3:0] |
| 0x0C | ADC_STATUS | RO | 0 | DONE[0]; READY[1]/BUSY[2] are VP ABI choices |
| 0x10 | ADC_DATA | RO | 0 | 12-bit average |
| 0x14 | ADC_TIMING | RW | 0x20040C00 | Setup[31:24], high[23:16], low[15:8] |
| 0x18 | ADC_SAMPLE_CFG | RW | 1 | SAMPLE_COUNT[4:0] |
| 0x1C + 4*n | ADC_SAMPLE_n | RO | 0 | 12-bit raw sample, n=0..15 |

1. Disable ADC and wait until `STATUS[2:0] == 0`.
2. Configure mode/channel, timing and sample count; normal mode is SAMP_SEL=1.
3. Enable ADC. Configuration is captured when the sequence starts.
4. Poll DONE[0] with a software timeout, then read DATA and individual samples.
5. Disable and wait for idle before the next configuration/start.

Effective timing minima are 32/4/12 ADC cycles; effective sample count is
clamped to 1-16. Reads return the masked programmed values. DONE remains set
until disable. The average uses unsigned integer division.

## Folder contents

`src/` and `include/` implement the model; `tests/` contains the regression
and machine-readable HDS mapping; `scripts/` runs and reports verification.
`docs/` contains only specification decisions and testing instructions.
Generated binaries, logs, coverage and reports belong under root `build/`.

The model does not simulate analog behavior, pin-level APB, a clock divider,
metastability or a multistage reset synchronizer. Passing VP tests does not
establish RTL equivalence or complete hardware sign-off.
