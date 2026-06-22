# CDC-VP clkmgr Register Sanity Firmware

This bare-metal RV32 firmware verifies the `clkmgr_tlm` register path inside
`clkmgr_platform`.

The firmware uses the temporary placeholder base address requested for this
bring-up:

| Region | Base |
|---|---:|
| UART0 | `0x1000_0000` |
| CLKMGR | `0x1006_0000` |
| RAM | `0x8000_0000` |

The clkmgr register offsets come from the OpenTitan Clock Manager register
documentation. The checks focus on the subset currently modeled by
`components/clkmgr_tlm`: `EXTCLK_CTRL_REGWEN`, `EXTCLK_CTRL`, `EXTCLK_STATUS`,
`CLK_ENABLES`, `CLK_HINTS`, `CLK_HINTS_STATUS`, `RECOV_ERR_CODE`, and
`FATAL_ERR_CODE`. The firmware also reads the remaining documented offsets as a
smoke test, but it does not fail on their reset values because the current TLM
model returns zero for unimplemented registers.

## Build

From the repository root:

```bash
make -C fw/clkmgr_test_riscv
```

This produces:

```text
fw/clkmgr_test_riscv/clkmgr_test.elf
fw/clkmgr_test_riscv/clkmgr_test.dis
```

## Expected Output

A successful simulation prints register read/write traces and ends with:

```text
CLKMGR TEST PASS
```
