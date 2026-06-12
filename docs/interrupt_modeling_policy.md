# Interrupt Modeling Policy (Step 3)

How interrupts are modeled in cdc-vp and wired in `platforms/riscv_custom_soc`.

## Controllers and ownership

| Controller | Interrupts it owns | RISC-V cause | Component |
|---|---|---|---|
| **CLINT** (Core-Local Interruptor) | machine **timer** (MTIP), machine **software** (MSIP) | 7, 3 | [`components/clint_tlm`](../components/clint_tlm) |
| **PLIC** (Platform-Level Interrupt Controller) | machine **external** (MEIP) from peripherals | 11 | [`components/plic_tlm`](../components/plic_tlm) |

There is exactly **one CLINT and one PLIC** on the interrupt path — do not instantiate duplicates.

## How the CPU receives interrupts

Both controllers drive the CPU through the **backend-agnostic** `cpu_base::set_irq(cause, level)`
([cpu_base.h](../cpu_models/include/cdc/cpu/cpu_base.h)). Each wrapper maps it to its model:

- **Bremen (`riscv_vp`, primary):** `set_irq` → `ISS::trigger_timer_interrupt / trigger_software_interrupt /
  trigger_external_interrupt|clear_external_interrupt` (sets/clears `mip.MTIP/MSIP/MEIP`). Level-accurate.
- **mariusmm (`riscv_tlm`, backup):** `set_irq(cause,true)` injects the cause via the core's
  `irq_line_socket` (edge model; the core auto-clears `mip` after delivery). `set_irq(cause,false)` is a no-op.
  → timer/external behave as one-shot on this backend (documented approximation).

This keeps the SoC and interrupt controllers independent of any specific CPU model: the same
`riscv_custom_soc` runs on both backends.

## Address map (riscv_custom_soc)

| Region | Base | Notes |
|---|---|---|
| CLINT | `0x0200_0000` | `msip`@0x0, `mtimecmp`@0x4000, `mtime`@0xBFF8 (microsecond ticks) |
| PLIC  | `0x0C00_0000` | priority@`0x4*id`, enable@0x2000, threshold@0x200000, claim/complete@0x200004 |
| UART  | `0x1000_0000` | console |
| I2C   | `0x1001_0000` | PLIC **source id 1** (its `irq_out` → `plic.irq_in[0]`) |
| RAM   | `0x8000_0000` | firmware text/data/stack |

## Interrupt flow (the demo: `fw/soc_irq_riscv`)

- **Timer (CLINT):** firmware sets `mtvec`, enables `mie.MTIE`+`mstatus.MIE`, writes `mtimecmp =
  mtime + 100us`. CLINT asserts MTIP when `mtime >= mtimecmp` → CPU traps → handler prints "TIMER".
- **External (PLIC):** firmware enables PLIC source 1 (priority/enable/threshold) and `mie.MEIE`, then
  triggers I2C to raise its line. PLIC asserts MEIP → CPU traps → handler **claims** (gets source 1),
  clears the I2C source, prints "EXT IRQ", then **completes**. The PLIC gateway prevents re-raising a
  claimed source until completion; level re-asserts if the line is still high after complete.

## Simplifications (vs real hardware)

- Single hart / single PLIC context (M-mode); one 32-bit word of PLIC sources.
- PLIC priorities compared numerically; no preemption/nesting.
- CLINT `mtime` is derived from simulation time and is **independent** of the CPU's `time` CSR
  (we do not couple Bremen's internal CLINT to ours).
- 64-bit `mtime`/`mtimecmp` reads can tear across the lo/hi 32-bit halves; fine while the high word is 0.

## Deferred
- Full PLIC (priority thresholds per context, multi-hart, preemption); S/U-mode interrupt delegation;
  software-interrupt (MSIP) demo; coupling CLINT mtime ↔ CPU time CSR.
