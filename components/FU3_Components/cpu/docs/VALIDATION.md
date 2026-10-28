# FU3 CPU configuration validation

## Plan T07 clock and boot (2026-10-23), in repository

| ctest | What runs | Result |
| --- | --- | --- |
| `fu3_cpu_clock` | wrapper, CVA6 profile, loop of single-cycle instructions: one instruction per period at 50 ns and after a change to 2.5 ns; no instruction while the clock is stopped, runs again after restart; with a 10 ns memory latency one instruction per 60 ns (latency counted once); mcycle = minstret across every change | PASS |
| `fu3_soc_cpu_cva6`, `fu3_soc_cpu_ibex` | T05/T06 firmware, now booted by the platform: Ibex released by the CRM after power-on (20 MHz OSC); CVA6 released by the Ibex stub `tests/fw/ibex_handoff.S` through CRM FCLK_SW after it sets the PLL to 400 MHz | PASS (59 / 70 checks) |
| `fu3_soc_cpu_*_q10`, `fu3_soc_boot_q100` | the same firmware with a 10 ns / 100 ns global quantum | PASS |
| `fu3_soc_boot` | Ibex boot firmware `tests/fw/t07_boot_tests.c` over three boots (see `platforms/fu3_soc`) | PASS |

## Plan T06 Debug Mode (2026-10-23), in repository

| ctest | What runs | Checks | Result |
| --- | --- | --- | --- |
| `fu3_soc_cpu_cva6` | T05 firmware + `tests/fw/fw_debug.{S,c}` (debug code linked at the HaltAddress) | 59 (T05 37 + 22 debug, ids 78-99): HaltAddress fetch outside Debug Mode faults, dcsr/DRET illegal outside Debug Mode, tinfo/tselect WARL/tdata1 read-only outside Debug Mode, haltreq from M (cause, prv, dpc, mcause untouched), ebreakm, single step, execute trigger (cause, dpc, hit bit, target runs once after resume), exception in Debug Mode (exception entry, mcause untouched), interrupt held off in Debug Mode then taken, dscratch1 and WFI in Debug Mode | PASS |
| `fu3_soc_cpu_ibex` | same, Ibex | 70 (T05 45 + 22 shared + 3: ebreaku and haltreq from U-mode, DRET back to U) | PASS |
| `fu3_cpu_debug_{cva6,ibex}` | wrapper with the FU3 options and a small memory, the Debug Module view | halt request, abstract GPR/CSR/FPR access (x0, unknown regno, not halted), resume at dpc, two single steps, PMP bypass of the DM aperture (Ibex MMWP), reset while halted, request held across reset (dpc = reset PC) | PASS |

Mutation check (script and logs kept with the T06 report): 20 mutants of the ISS and the
wrapper (DM aperture PMP bypass, dcsr.prv, exception entry, interrupts and WFI
in Debug Mode, step, DRET/debug CSR/tdata1 legality, reset of Debug Mode and
of the request, DRET privilege and step arming, ebreaku, trigger match and hit
bit, x0, halted state after resume, `Access::debug` on fetches always/never)
and one of the platform (hart order of `fu3_soc_top::debug_harts()`, through
which the firmware harness drives the debug request): 21 in all, each one
makes at least one of the tests above fail.

Two faults of the T05 firmware harness surfaced with the T06 code and are
fixed: `fw_run_user` did not preserve s0-s11 across the U-mode excursion (a
caller value kept in s0 came back with the U-mode code's value), and
`fw_raise` could wait forever for an ack the interrupt handler had already
consumed (layout-dependent race). Both show up as false failures, not false
passes.

Not covered: dcsr.stepie, triggers in U-mode, data accesses with
`Access::debug` (the test debug code touches only DSRAM), WFI fetched while a
request is already pending.

## Plan T05 (2026-10-23), in repository

Firmware runs on each ISS inside the assembled platform (`platforms/fu3_soc`,
FU3 bus, ROM/ISRAM/DSRAM models), built with xPack riscv-none-elf-gcc 15.2.0:

| ctest | Firmware | Checks | Result |
| --- | --- | --- | --- |
| `fu3_soc_cpu_cva6` | `tests/fw/cva6_tests.c`, `-march=rv32imfc_zicsr_zifencei -mabi=ilp32f` | 37: misa/mhartid, M, FS gating (FP op, FLW, fflags), F arithmetic and flags, RMM, fcsr/frm/fflags, D/A illegal, MPP WARL, cycle/mcycle, ecall/ebreak, misaligned load/store, decode/policy access faults, NX fetch, WFI wake, vectored mtvec, locked PMP, unimplemented PMP entries | PASS |
| `fu3_soc_cpu_ibex` | `tests/fw/ibex_tests.c` + ROM boot stub, `-march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32` | 45: reset PC 0x80, mtvec 0x1, mseccfg 0x6, PMP reset entries, misa/mhartid, mtvec WARL, mcycle, F illegal, Zcb (8 instructions, 2 illegal), fast/external priority, WFI wake on fast, PMP fault cases, misaligned split, MPP WARL, U-mode isolation (M CSR, ROM, MRET, WFI with TW) and return, MPRV cleared by MRET, RLB, sticky MMWP, MML | PASS |
| `fu3_cpu_reset_{cva6,ibex}_{fetch,load,store,wfi,irq}` | wrapper with the FU3 options and a latency target | reset pulse during a fetch, load, store or WFI restarts at the reset vector with no commit from the old instruction; interrupt levels held across reset are pending after release | PASS |
| `fu3_iss_edges` | ISS unit checks (no bus) | FP CSR privilege/FS/dirty, FLW/FSW gating, RMM and reserved rm, cycle without U, xRET/WFI privilege, MPP WARL, MPRV, mcycle/mcycleh/inhibit/reset | PASS |
| `fu3_iss_mixed_backends` | link test | fork and shared riscv_vp in one binary | PASS |

Every exception in the firmware must be declared (`EXPECT_TRAP`, `RUN_USER`);
any other exception stops the run as FAIL with its mcause, mepc and mtval, so a
legal instruction that traps cannot pass unnoticed.

Interrupt inputs are driven by the test harness through a DSRAM mailbox until
the interrupt sources are integrated (T08/T09); the mailbox is a test
convention, not an ABI. Mutation checks: disabling the fast-interrupt priority
or the Ibex PMP makes `fu3_soc_cpu_ibex` fail; reverting each fix of the
T04/T05 review (CSR privilege, MRET from U, RMM, mcycle write, reset in the CPU
thread, IRQ re-apply, register-name namespace) makes the corresponding test fail.

Regression of the shared wrapper: `riscv_cpu_eval --fw fw/hello_baremetal_riscv/hello.elf`
prints "Hello from RISC-V" with the default (upstream-equivalent) options.

## Earlier evidence (v2.0, outside the repository)

Earlier results came from a harness outside the repository on the Common v2.0
map (Ubuntu/WSL). They are superseded by the in-repository tests above.
