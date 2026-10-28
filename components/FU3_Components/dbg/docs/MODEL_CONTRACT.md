# FU3 DBG functional contract

## Sources and scope

Specification priority for FU3 (leader decision, 2026-10-20): FU3 HAS v2.0 and FTEL_FU3_Common_Spec v2.0 first, the IP specification only where both are silent. DBG has no HDS.

- HAS 3.1: DBG is a JTAG2AXI4 bridge used to access all registers of the system, IEEE 1149.1 JTAG, instructions CMD_ADDR, CMD_DATA, CMD_BYPASS, CMD_STATUS and CMD_IDCODE with the address, data, bypass, status and IDCODE registers.
- HAS 2 and Common Diagram: RISC-V CPU debug through the DBG subsystem. HAS Table 11-2, 10.2 and Common Interrupt Mapping: `debug_req_i` of CVA6 and Ibex comes from DBG.
- HAS Table 7-1 and Common BUS INTF: DBG is an AXI4 master on SYSBUS_1, 32-bit address and data, one outstanding, INCR.
- Common MemoryMap: debug module control aperture 0x1A11_0000-0x1A11_0FFF ("Debug subsystem only"), Ibex DmHaltAddr/DmExceptionAddr 0x0002_F000/F008, CVA6 HaltAddress/ExceptionAddress 0x0001_F000/F008.
- HAS 6.5.3: debug reset resets the whole system except DBG. HAS 5.1: TCLK is the JTAG clock.
- RISC-V External Debug Support 0.13.2 for the Debug Module, the debug standard of both cores (DebugEn = 1).

CPU0 is CVA6 (hart 0) and CPU1 is Ibex (hart 1).
The RTL team's IP_Feature_Spec sheet DBG (obtained directly from the RTL team, not an issued specification) is used only for the IDCODE value. Its RISC-V DTM instructions (dtmcs, dmi) are not used, because HAS 3.1 defines the JTAG instruction set.
The source documents are private and are not included in this package.

The CPU side (halt, step, dpc/dcsr, triggers) is the `DebugHart` interface. Connecting the CVA6/Ibex wrappers to it needs an ISS debug mode.

## Structure

- `fu3_dbg::Jtag2Axi`: JTAG TAP at scan level and the AXI4 master `axi`. `scan_ir()` and `scan_dr()` stand for a full Shift/Update sequence, TCK and TAP states are not modeled.
- `fu3_dbg::DebugModule`: target socket `socket` for the 4 KiB DM window, two harts, abstract commands and the `ndmreset` output. No System Bus Access: the bridge provides system access.
- `fu3_dbg::DebugHart`: what the DM needs from a hart (halt request level, resume, halted state, register access).

## JTAG2AXI4 bridge (HAS 3.1)

| IR | Instruction | DR length | Behavior |
| --- | --- | --- | --- |
| 0x01 (after Test-Logic-Reset) | CMD_IDCODE | 32 | `FU3_DBG_IDCODE` 0x0000_0DB3 |
| 0x02 | CMD_ADDR | 32 | Capture: current address. Update: load the address |
| 0x03 | CMD_DATA | 34 | {read[33], write[32], data[31:0]}. Capture: data of the last read. Update: write data, or read, at the address, then address += 4. Neither bit: no access |
| 0x04 | CMD_STATUS | 32 | [0] busy (always 0), [1] SLVERR, [2] DECERR, [3] misaligned address. Sticky, Update clears the bits written as 1 |
| 0x1F and others | CMD_BYPASS | 1 | Captures 0 |

- Each access is one 32-bit beat with `fu3_bus::Access` and finishes inside Update-DR.
- `TLM_ADDRESS_ERROR_RESPONSE` sets DECERR, any other error sets SLVERR (including accesses the bus access policy denies). A misaligned address sets the misaligned bit and starts no access. After an error the address does not advance.
- Test-Logic-Reset selects CMD_IDCODE and clears address, read data and status.

## Debug Module (Debug Spec 3)

Bus window: local offsets 0x000-0xFFF, DMI register n at offset 4 x n (0x000-0x1FC). Other offsets inside the window read 0 and ignore writes. Only aligned 32-bit accesses: other lengths return `TLM_BURST_ERROR_RESPONSE`, misaligned or out-of-window addresses return `TLM_ADDRESS_ERROR_RESPONSE`. Reads have no side effect, so `transport_dbg` uses the same decoding.

| DMI address | Register | Behavior |
| --- | --- | --- |
| 0x04, 0x05 | data0, data1 | RW, Access Register uses data0 |
| 0x10 | dmcontrol | haltreq (reads 0), resumereq, ackhavereset, hartsello/hi (20 bits), ndmreset, dmactive, hasel, hartreset and resethaltreq are 0 |
| 0x11 | dmstatus | version 2, authenticated 1, halted/running/resumeack/havereset/nonexistent of the selected hart |
| 0x12 | hartinfo | 0 |
| 0x16 | abstractcs | progbufsize 0, busy 0, cmderr (W1C), datacount 2 |
| 0x17 | command | Access Register only |
| 0x18, 0x1D | abstractauto, nextdm | 0 |
| 0x38 | sbcs | 0: no System Bus Access |
| 0x40 | haltsum0 | Bit i = hart i halted |

- After reset `dmactive` = 0: only a dmcontrol write with dmactive = 1 has an effect. Writing dmactive = 0 resets the DM (data, cmderr, hartsel, ndmreset) and clears halt requests, havereset is kept.
- haltreq is a level driven to the selected hart on every dmcontrol write. resumereq resumes the selected hart if it is halted and haltreq is 0, and sets resumeack once it runs.
- havereset is set at power-on, when ndmreset rises, and by `notify_hart_reset()`, ackhavereset clears it.
- Access Register: aarsize 2 only, regno 0x0000-0x0FFF CSR, 0x1000-0x101F GPR, passed to the hart. cmderr 2 for other cmdtype, aarsize or postexec, cmderr 4 when the selected hart is not halted or does not exist, cmderr 3 when the hart rejects the register. Commands are ignored while cmderr is nonzero.
- `ndmreset` output follows dmcontrol.ndmreset, connect it to CRM `cpu_reset_req` (HAS 6.5.3: system reset, DBG kept).

## VP choices and open questions

| Topic | VP choice | Status |
| --- | --- | --- |
| JTAG instruction codes | CMD_IDCODE 0x01, CMD_ADDR 0x02, CMD_DATA 0x03, CMD_STATUS 0x04, CMD_BYPASS 0x1F, IR 5 bits | HAS 3.1 gives names only |
| CMD_DATA / CMD_STATUS layout | Read/write flags, auto-increment by 4, sticky W1C error bits | HAS 3.1 gives names only |
| How the debugger reaches the DM | DM registers mapped in the 0x1A11_0000 window, DMI register n at 4 x n, through the bridge | Common: "Ibex Debug Module control aperture", layout not given. RTL sheet uses a RISC-V DTM (dtmcs/dmi) instead |
| One DM for both CPUs | Harts 0 (CVA6) and 1 (Ibex) in the same DM | Common names the aperture for Ibex only |
| Who may access the DM window | Not checked in the DM | Common: CVA6 blocked, Ibex "debug subsystem only". Bus access policy (task 5) |
| Debug style | Abstract commands only, no program buffer and no debug ROM | Common reserves debug code at the halt addresses (execution-based debug), the ISS has no debug mode |
| IDCODE | 0x0000_0DB3 (sheet DBG) | HAS has no value |
| DEBUG_EN / debug permission | Not modeled: always authenticated | HAS uses DEBUG_EN only as a boot gate |
| TAP state machine, TCK | Scan level only | Functional model |
| CPU halt/step/triggers | `DebugHart` interface | ISS has no debug mode (dcsr/dpc read 0, no dret) |

## DBG-domain reset and hart availability (platform integration)

- `DebugModule::reset_n` (optional, active low): while low the DM is held at its
  reset values (DMCONTROL, DATA0/1, hartsel, cmderr), halt requests are
  released, havereset is set for every hart and DMI writes are ignored. The
  platform binds it to the DBG-domain reset (watchdog, global and POR resets);
  ndmreset does not reach it.
- `Jtag2Axi::reset_registers()` clears the AXI-side registers (ADDR, read data,
  STATUS) on the DBG-domain reset; the TAP state (IR) is kept.
- `DebugHart::unavailable()` (default false): a hart that cannot be debugged now
  (held in reset) is reported `anyunavail`/`allunavail` in DMSTATUS, neither
  running nor halted, and abstract commands to it fail with cmderr HALTRESUME. A
  halt request set meanwhile halts it on its first instruction once available.
