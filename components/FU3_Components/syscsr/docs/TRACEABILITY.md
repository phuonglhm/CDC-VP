# SYSCSR specification traceability

Source: private FU3 HAS v2.0, FTEL_FU3_Common_Spec v2.0 and FU3_SYSCSR_v1.xlsx (sheet SYSCSR), reviewed 2026-10-21. Checks live in `tests/bench.h` and `behavior.cpp` binds directly and `integration.cpp` uses the FU3 bus. The expected-register table in the bench was generated from the spreadsheet, not from the model source.

| Source | Behavior | Verification |
| --- | --- | --- |
| Configuration table | APB, 32-bit data, 16-bit address, 4 write strobes | Out-of-window, misaligned and length-3 accesses rejected |
| Register table, reset column | Reset value of all 51 registers | Reset-value read of every register |
| Register table, type column | RW/RO masks of all fields, reserved bits read zero | Write all-ones and all-zeros to every register, read back |
| Write Strobe = 4 | Partial writes by byte lane | Byte, halfword and sparse-PSTRB writes and halfword read of lane 2 |
| HAS 11 / syscsr_status | Straps captured at reset release, test bits live, RO | Pins follow while in reset, captured at release, later pin changes ignored, test_mode live, write ignored |
| syscsr_sys_ctrl | sw_reset_req / sw_global_rst_req levels to CRM, not self-clearing | Outputs follow bits 0/4 and jtagmux bits stored only |
| syscsr_cpu_ctrl / cpu_systick | Reserved, read zero, write ignored | Part of the mask check |
| pad_ctrl_8 reset | Complement of pad_ctrl_5.dr | Reset values of all four sides |
| VP choice | Writes ignored while reset_n low and reset restores values and clears requests | Reset sequence check |
| VP choice | Unmapped offset reads 0 and ignores writes | 0x0084 read, write, read |
| HAS v2.0 memory map | SYSCSR at 0x4401_0000 on PERIBUS_0 | Integration bench through the actual FU3 bus from the IBEX port |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run the behavior bench via find_package |

Not covered: firmware execution, real CRM/IO_PAD peers, and the open questions in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
