# FU3 DBG functional contract

## Sources and scope

The specification is FU3 HAS v2.0 together with the RISC-V External Debug Support specification 0.13.2, the debug standard that both CVA6 (HAS Table 8-1 DebugEn = 1) and Ibex implement:

- Table 4-1 item 2: DBG gives external JTAG access to the processors and to system resources through the interconnect.
- Table 7-1 and the bus: DBG is an AXI4 master on SYSBUS_1 with 32-bit word, incrementing transfers only.
- Table 11-2 and 10.2: `debug_req_i` of CVA6 and Ibex come from DBG.
- 6.5.3: debug reset resets the whole system except DBG.
- 5.1: TCLK is the JTAG clock.

HAS has no DBG register map or HDS. This model covers phase 1: the JTAG DTM, the Debug Module and system bus access. The CPU side (halt, step, dpc/dcsr, triggers) is the `DebugHart` interface and connecting the CVA6/Ibex wrappers to it is phase 2.
The RTL team's IP_Feature_Spec sheet DBG (obtained directly from the RTL team, not an issued specification) is only used for the IDCODE value, which HAS does not give and pending mentor approval.

## Structure

- `fu3_dbg::Dtm`: JTAG TAP and DTM at scan level. `scan_ir()` and `scan_dr()` stand for a full Shift/Update sequence, TCK and TAP states are not modeled.
- `fu3_dbg::DebugModule`: DMI registers, two harts (VP: hart 0 CVA6, hart 1 Ibex), abstract commands, SBA through the `sba` initiator socket, and the `ndmreset` output.
- `fu3_dbg::DebugHart`: what the DM needs from a hart (halt request level, resume, halted state, register access).

## JTAG DTM (Debug Spec 6.1)

| IR | Register | Length | Behavior |
| --- | --- | --- | --- |
| 0x01 (after Test-Logic-Reset) | IDCODE | 32 | `FU3_DBG_IDCODE` 0x0000_0DB3 |
| 0x10 | dtmcs | 32 | version 1, abits 7, dmistat 0, idle 1, dmihardreset drops the held DMI result |
| 0x11 | dmi | 41 | {address[40:34], data[33:2], op[1:0]}, op 1 read, 2 write and a scan returns the result of the previous access with op 0 |
| 0x00, 0x1F, others | BYPASS | 1 | Captures 0 |

DMI accesses finish in the same Update-DR, so `dmistat` is never busy or failed.

## Debug Module (Debug Spec 3)

| Address | Register | Behavior |
| --- | --- | --- |
| 0x04, 0x05 | data0, data1 | RW, Access Register uses data0 |
| 0x10 | dmcontrol | haltreq (reads 0), resumereq, ackhavereset, hartsello/hi (20 bits), ndmreset, dmactive, hasel, hartreset and resethaltreq are 0 |
| 0x11 | dmstatus | version 2, authenticated 1, halted/running/resumeack/havereset/nonexistent of the selected hart |
| 0x12 | hartinfo | 0 |
| 0x16 | abstractcs | progbufsize 0, busy 0, cmderr (W1C), datacount 2 |
| 0x17 | command | Access Register only |
| 0x18, 0x1D | abstractauto, nextdm | 0 |
| 0x38 | sbcs | sbversion 1, sbasize 32, only sbaccess32, sberror W1C |
| 0x39, 0x3C | sbaddress0, sbdata0 | System bus address and data |
| 0x40 | haltsum0 | Bit i = hart i halted |

- After reset `dmactive` = 0: only a dmcontrol write with dmactive = 1 has an effect. Writing dmactive = 0 resets the DM (data, cmderr, SBA state, hartsel, ndmreset) and clears halt requests, havereset is kept.
- haltreq is a level driven to the selected hart on every dmcontrol write. resumereq resumes the selected hart if it is halted and haltreq is 0, and sets resumeack once it runs.
- havereset is set at power-on, when ndmreset rises, and by `notify_hart_reset()`, ackhavereset clears it.
- Access Register: aarsize 2 only, regno 0x0000-0x0FFF CSR, 0x1000-0x101F GPR, passed to the hart. cmderr 2 for other cmdtype, aarsize or postexec, cmderr 4 when the selected hart is not halted or does not exist, cmderr 3 when the hart rejects the register. Commands are ignored while cmderr is nonzero.
- SBA: 32-bit accesses only, because the DBG bus port takes 32-bit beats. sberror 4 for another sbaccess, 3 for a misaligned address, 2 for a bus address error, 7 for any other bus error. sbreadonaddr, sbreadondata and sbautoincrement (+4) as in the spec. While sberror is nonzero, writes to sbaddress0/sbdata0 store the value but start no access. Accesses complete inside the DMI access, so sbbusy and sbbusyerror stay 0.
- `ndmreset` output follows dmcontrol.ndmreset, connect it to CRM `dbg_reset_req`.

## VP choices and open questions

| Topic | VP choice | Status |
| --- | --- | --- |
| Debug style | Abstract commands only, no program buffer and no debug ROM | HAS reserves 4 KiB debug code in each ISRAM and an Ibex DM aperture at 0x1A11_0000 (execution-based debug) and not needed by this model |
| HaltAddress / ExceptionAddress | Not used | HAS: "refer to memory map", no values |
| Hart numbering | 0 CVA6, 1 Ibex | Not specified |
| SBA access sizes | 32-bit only (HAS Table 7-1) | Sheet DBG lists 8/16/32 |
| IDCODE | 0x0000_0DB3 (sheet DBG) | HAS has no value |
| DEBUG_EN / debug permission | Not modeled: always authenticated | HAS uses DEBUG_EN only as a boot gate and the access rule in the plan is not specified |
| TAP state machine, TCK | Scan level only | Functional model |
| CPU halt/step/triggers | `DebugHart` interface, phase 2 | ISS has no debug mode (dcsr/dpc read 0, no dret) and wrapper work needed |
