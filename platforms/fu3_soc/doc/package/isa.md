# VP_FU3_V1.0 — CPU and ISA profile (R1)

Both CPUs run on the FU3 fork of the Bremen RISC-V ISS (`cpu_models/fu3_iss`)
behind the CDC-VP CPU wrapper, configured from Common Spec v2.1 *CVA6-Config*
and *Ibex-Config* (`components/FU3_Components/cpu/include/fu3_cpu/`). The model
is instruction accurate; pipeline timing is not modelled (one cycle per
instruction plus bus latency, loosely timed).

| | CVA6 (CPU0, hart 0) | Ibex (CPU1, hart 1) |
| --- | --- | --- |
| Role | application CPU | boot/control CPU |
| ISA | RV32IMFC + Zicsr, Zifencei (C = Zca + Zcf) | RV32IMC + Zicsr, Zifencei, Zcb (C = Zca + Zcb; Zcb without Zbb: `c.sext.b/h`, `c.zext.h` illegal) |
| Not implemented | A, D, B (RVB = 1 required by the spec, subset undecided: do not use Zb*), S, U, MMU, caches | A, F, D, B, S |
| Float ABI | ilp32 or ilp32f (single precision) | ilp32 (soft float) |
| Privilege | M only | M and U |
| Reset PC | `0x0001_0000` (CVA6_ISRAM), held in reset until CRM `FCLK_SW[0]` | `0x0000_0080` (ROM, boot_addr 0 + 0x80) |
| mtvec at reset | direct/vectored (WARL) | `0x0000_0001`, vectored only (256-byte aligned base) |
| Interrupts | MEI (PLIC), MTI (TIMER0), MSI (SWIRQ) | 15 fast + MEI, MSI, MTI through INTMAP; NMI tied low |
| PMP | 8 regions, NAPOT, all OFF at reset (M-mode: only locked entries restrict) | 16 regions + Smepmp; reset profile and final W^X profile in FU3_VP_ABI_1 §5 (encodings not RTL confirmed) |
| Debug | Debug Mode, halt `0x0004_0000`, exception `0x0004_0008`; 2 execute triggers | Debug Mode, halt `0x0006_0000`, exception `0x0006_0008`; 4 execute triggers |
| Misaligned load/store | trap (cause 4/6) | split into aligned accesses (Ibex LSU) |
| Clock | CRM FCLK (OSC 20 MHz after reset, 400 MHz with the PLL) | same FCLK |

Notes:

- Triggers are `mcontrol` execute-address, exact match, M/U-mode; load/store
  triggers are not offered (decision Q04-e), so GDB has no watchpoints.
- The loader refuses an ELF whose code in a CPU's region uses an extension that
  CPU lacks (ELF float ABI, RVE flag, `.riscv.attributes` arch string).
- Debug code in the debug partitions comes from the Boot ROM (blob owned by the
  DBG/DM team; the VP ships a placeholder, FU3_VP_ABI_1 §6).
- Build flags used by the example firmware: CVA6 `-march=rv32imfc_zicsr_zifencei
  -mabi=ilp32f`, Ibex `-march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32`.
