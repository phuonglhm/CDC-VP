# DBG specification traceability

Source: FU3 HAS v2.0, FTEL_FU3_Common_Spec v2.0 and RISC-V External Debug Support 0.13.2, reviewed 2026-10-21. Checks live in `tests/bench.h`. `behavior.cpp` routes the bridge directly to the DM and a memory, `integration.cpp` uses the FU3 bus.

| Source | Behavior | Verification |
| --- | --- | --- |
| HAS 3.1 (CMD_IDCODE, IEEE 1149.1) | IDCODE after Test-Logic-Reset, bit 0 = 1 | DR length and value |
| HAS 3.1 (CMD_BYPASS) | 1-bit bypass for CMD_BYPASS and unused IRs | Three IR values |
| HAS 3.1 (CMD_ADDR, address register) | 32-bit address, captured and loaded | Capture after load, cleared by Test-Logic-Reset |
| HAS 3.1 (CMD_DATA, data register) | 32-bit AXI read and write at the address, auto-increment | DSRAM write and read back, block write and block read, scan without read/write does not access |
| HAS 3.1 (CMD_STATUS, status register) | Sticky error bits, W1C | Misaligned, unmapped (DECERR), slave error (SLVERR), W1C clear |
| HAS Table 7-1, Common BUS INTF | AXI4 master on the DBG port, 32-bit beats | Integration bench: bridge on the DBG initiator, DSRAM through SYSBUS_1, boot ROM denied (SLVERR) |
| Common MemoryMap 0x1A11_0000 | DM control aperture | All DM checks go through the bridge into the IBEX_DBG window, DMI register n at 4 x n, upper window reads 0 |
| HAS v2, Table 11-2, 10.2 | debug_req_i of both CPUs from DBG | Haltreq drives the selected hart only |
| Debug Spec 3.12.1-3.12.2 | dmcontrol / dmstatus | Activation, inactive DM ignores writes, version 2, authenticated |
| Debug Spec 3.12.2 | havereset, ackhavereset | Power-on, platform reset, ndmreset |
| Debug Spec 3.5 | Halt, resume, resumeack, haltsum0 | Halt one hart, both harts, resume ignored with haltreq, resume of a running hart |
| Debug Spec 3.6.1.1 | Access Register | GPR and CSR read/write, unsupported size/postexec/cmdtype, FPR rejected, running hart |
| Debug Spec 3.12.2 | hartsel 20 bits, nonexistent hart | hart 5 nonexistent, hartsello/hi |
| HAS 6.5.3 | ndmreset to CRM | Output follows dmcontrol.ndmreset and sets havereset |
| Debug Spec 3.12.2 | dmactive = 0 resets the DM | dmcontrol, ndmreset, halt requests, data0, cmderr, sbcs 0 |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run the behavior bench via find_package |

Not covered: TAP state sequencing and TCK timing, real CVA6/Ibex harts (ISS debug mode), and the open questions in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
