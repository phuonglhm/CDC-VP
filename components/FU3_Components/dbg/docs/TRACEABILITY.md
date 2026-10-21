# DBG specification traceability

Source: FU3 HAS v2.0 and RISC-V External Debug Support 0.13.2, reviewed 2026-10-20. Checks live in `tests/bench.h`. Every DM access goes through the JTAG DTM. `behavior.cpp` connects SBA to a bench memory and `integration.cpp` uses the DBG master port of the FU3 bus. Harts are bench fakes implementing `DebugHart`.

| Source | Behavior | Verification |
| --- | --- | --- |
| Debug Spec 6.1.2 | IR values, IDCODE after Test-Logic-Reset, BYPASS for others | DR lengths for IDCODE, DMI, BYPASS 0x00/0x1F/0x05 and IDCODE bit 0 |
| Debug Spec 6.1.4-6.1.5 | dtmcs fields, 41-bit dmi, result of the previous access | version, abits, dmistat and previous dmcontrol read returned by the next scan and dmihardreset |
| Debug Spec 3.12.2 dmactive | DM inactive at reset, dmactive 0 resets it | Writes ignored before activation and reset of dmcontrol, data0, cmderr, sbcs, ndmreset, halt requests |
| Debug Spec 3.12.1-3.12.2 | dmstatus, havereset, ackhavereset | Per-hart havereset, platform hart reset |
| Debug Spec 3.5, 3.12.2 | haltreq, resumereq, resumeack, haltsum0 | Halt each hart, haltsum0, resumereq ignored with haltreq, resume, resumeack, resumereq on a running hart |
| Debug Spec 3.6.1.1, 3.12.6 | Access Register, cmderr rules | GPR and CSR read/write on the selected hart, transfer 0, aarsize 64, postexec, cmdtype 1, FPR, running hart, cmderr W1C and blocking |
| Debug Spec 3.12.2 hartsel | 20-bit hartsel, nonexistent harts | Hart 5 nonexistent and cmderr 4 and 0xFFFFF read back |
| Debug Spec 3.12.2 ndmreset, HAS 6.5.3 | ndmreset output, havereset on all harts | Output high/low, havereset on both harts |
| Debug Spec 3.10, HAS Table 7-1 | SBA, 32-bit only | Write, autoincrement burst, readonaddr/readondata block read, sbaccess 8-bit -> 4, blocking while sberror, W1C, misaligned -> 3, unmapped -> 2, slave error -> 7 |
| HAS memory map, bus policy | DBG reaches DSRAM and boot ROM is Ibex-only | Integration: SBA through the DBG port and ROM read -> sberror 7 |
| Packaging | Public SystemC dependency and install/export | Static/shared consumers run the behavior bench via find_package |

Not covered: real CVA6/Ibex harts (phase 2), triggers and single step, DEBUG_EN, and the open questions in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).
