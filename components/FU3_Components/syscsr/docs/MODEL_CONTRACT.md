# FU3 SYSCSR functional contract

## Sources and scope

Specification priority for FU3 (leader decision, 2026-10-20): FU3 HAS v2.0 and FTEL_FU3_Common_Spec v2.0 first, the IP specification only where both are silent.
HAS v2.0 and Common Spec v2.0 define the PERIBUS_0 window at 0x4401_0000 (APB, 16-bit address, 32-bit data), the boot straps MODE_1, MODE_0 and BOOT stored in SYSCSR (HAS 11), DEBUG_EN as a separate input, the per-pad `csr_io_cfg[1:0]` function select (IOPAD MAP), the SYSCSR software reset of the whole system except CRM (HAS 6.5.2) and the software interrupt to CVA6 `ipi_i` and Ibex `irq_software_i` with its offset TBD.
FU3_SYSCSR_v1.xlsx gives the register map, reset values and field types. IO_PAD HDS Table 7-2 and WDT HDS v1.1 were cross-checked. The source documents are private and are not included in this package.

The model covers software-visible register behavior and the CRM reset-request outputs. It does not model APB pin handshakes, pad electrical behavior or CRM.

## Bus interface

- Configuration: APB, 32-bit data, 16-bit address (64 KiB window), 4 write strobes, no asynchronous bus/register domain. The bus delivers local offsets.
- Accepts aligned 1/2/4-byte accesses and byte enables (PSTRB) select written lanes.
  The FU3 bus already splits bursts. Other lengths return `TLM_BURST_ERROR_RESPONSE` and misaligned or out-of-window addresses return `TLM_ADDRESS_ERROR_RESPONSE`.
- Reads have no side effects. No read-modify-write is required from the bus.
- `transport_dbg` uses the same decoding and may write while `reset_n` is low.

## Registers

All 51 registers in 0x0000-0x00D8 use the reset values and field types of the register table (only `rw` and `ro` occur). RO and reserved bits read their reset value and ignore writes.

| Offset | Register | Behavior |
| --- | --- | --- |
| 0x0000 | version | RO 0: the FU3 top ties `i_version_major/minor` to 0 |
| 0x0010-0x001C | left/bottom/right/top_io_cfg | RW [31:0], 2 bits per side-local pad 0..15 |
| 0x00BC-0x00C8 | *_io_cfg_ext | RW [15:0], side-local pads 16..23 |
| 0x0020-0x006C | *_pad_ctrl_0..4 | RW [23:0]: IE, PU, PD, SR, CS per side |
| 0x008C-0x00B8 | *_pad_ctrl_5..7 | RW [23:0]: DR (DRV[0]), OS, OD per side |
| 0x00CC-0x00D8 | *_pad_ctrl_8 | RW [23:0]: DR1 (DRV[1]) and reset is the complement of pad_ctrl_5.dr |
| 0x0070 | syscsr_status | RO: boot[0], mode_0[1], mode_1[2] captured at reset release, test_mode[3], test_mode_sel[4] live from ports |
| 0x0074 | syscsr_sys_ctrl | RW sw_reset_req[0], sw_global_rst_req[4], jtagmux_sel[8], jtagmux_block[12] |
| 0x0078, 0x007C | syscsr_cpu_ctrl, syscsr_cpu_systick | Reserved in FU3, RO 0 |
| 0x0080 | osc_pad_ctrl | RW ei[0], eo[1], pd[2], bypass[3], xdrv[5:4], reset 0x13 |
| 0x0088 | rom_ctrl | RW rom_lven[0] |

## Reset and outputs

`reset_n` is active low. While low, registers hold reset values, functional writes are ignored and the strap fields follow the pins. When `reset_n` rises they keep the pin values of that moment until the next reset. Without any reset they hold the values at simulation start. `sw_reset_req` and `sw_global_rst_req` mirror sys_ctrl bits 0 and 4 as levels. They are not self-clearing and CRM detects the rising edge.
One SystemC method drives both outputs, so they update two delta cycles after the write. jtagmux_sel/jtagmux_block are stored only.
The FU3 top leaves them open.

## VP choices and open questions

| Topic | VP choice | Status |
| --- | --- | --- |
| Strap capture | boot, mode_0, mode_1 captured when `reset_n` rises (HAS 11: "stored in SYSCSR") | HAS: sampling event TBD. The v1 table describes live pins |
| Unmapped offsets inside the window (e.g. 0x0084) | Read 0, write ignored, OK response | Pending RTL: PSLVERR or not |
| Strap pin order | Separate `boot`/`mode_0`/`mode_1` ports | IOPAD MAP: PIN_2 BOOT, PIN_3 MODE_1, PIN_4 MODE_0 |
| Pad config to IO_PAD `select[80]` | `read_register()` only, no ports | Side-local pad to package position mapping not specified |
| SYSCSR reset domain | Platform supplies `reset_n` (system reset with CRM SW_RST_N_1.syscsr) | HAS 6.5.2 is implemented by CRM: a set sw_reset_req is cleared by the system reset it causes |
| CPU software IRQ | Not modeled | HAS and Common: offset TBD, "tie low if not implemented" |
| CVA6 reset release, `boot_addr_i` | Not in SYSCSR | CRM FCLK_SW releases CVA6 (CRM V1.1), `boot_addr_i` fixed at 0x0001_0000 (Common CVA6-Config #59) |
| WDT unlock (`i_wdt_other_unlock`) | Not modeled | WDT HDS v1.1 moves the lock to SYSCSR and the v1 table has no field |
| Version inputs | Constant 0 | Final product version not assigned |
