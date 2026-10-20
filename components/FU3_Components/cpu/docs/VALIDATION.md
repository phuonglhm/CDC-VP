# FU3 CPU configuration validation - 2026-10-19

Environment: Ubuntu under WSL, GNU C++ 15.2.0, C++17, SystemC 2.3.4, riscv64-unknown-elf-gcc for `-march=rv32imc_zicsr -mabi=ilp32` firmware.

## Repository build

`cmake -S . -DCDC_BUILD_FU3_SOC=ON -DCDC_BUILD_CPU_EVAL=ON` configures, including `fu3::cpu`, `cdc_cpu_cva6` and `riscv_cpu_eval` build.

## riscv_cpu_eval regression

`hello`, `timer_irq` and `uart_test` firmware give the same output as before the single-port bus change.

## CVA6 + Ibex on the FU3 bus (harness outside the repository)

The harness instantiates the production FU3 bus with ROM, CVA6_ISRAM, IBEX_ISRAM and DSRAM enabled, both CPUs with this configuration, and a boot controller that calls `finish_staging()` and releases CVA6 once Ibex reports staging done.
Ibex boot ROM and CVA6 image are assembly firmware.

| Check | Result |
| --- | --- |
| Ibex `mhartid` = 1, `misa` = I M C U | OK |
| Ibex stages the CVA6 image into CVA6_ISRAM | OK |
| Ibex write to CVA6_ISRAM after staging closes -> mcause 7 | OK |
| CVA6 `mhartid` = 0, `misa` = I M C | OK |
| CVA6 ROM read -> mcause 5 | OK |
| CVA6 DSRAM fetch -> mcause 1 | OK |
| CVA6 sb/sh/sb to DSRAM -> 0x22334411 | OK |
| CVA6 RVC fetch: 32-bit instruction at 0x10022 crossing a word | OK |
| Both CPUs complete | OK |

12/12 checks passed and CVA6 released at 11 us, simulation ended at 14 us.

Mutation check: with the fetch split removed (4-byte fetch at the PC), CVA6 stops at its first 2-byte-aligned instruction and the harness fails.
