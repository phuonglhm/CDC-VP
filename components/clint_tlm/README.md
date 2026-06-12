# clint_tlm

RISC-V **CLINT** (Core-Local Interruptor), single hart (`cdc::components::clint_tlm`).

Drives the CPU's machine **timer** (MTIP, cause 7) and **software** (MSIP, cause 3) interrupts via
`cpu_base::set_irq`, so it works with any CPU backend.

## Register map (offsets from base, typically `0x02000000`)

| Offset | Name     | Access | Description |
|-------:|----------|--------|-------------|
| `0x0000` | msip     | R/W | software interrupt pending (bit0) |
| `0x4000` | mtimecmp | R/W | 64-bit timer compare |
| `0xBFF8` | mtime    | R   | 64-bit monotonic time (microseconds, from sim time) |

When `mtime >= mtimecmp` (and `mtimecmp != 0`) the timer interrupt is asserted; writing `mtimecmp`
re-evaluates. Writing `msip` bit0 drives the software interrupt.

## Usage

```cpp
cdc::components::clint_tlm clint("clint", cpu);   // cpu is a cdc::cpu::cpu_base&
bus.add_target(0x02000000, 0x10000).bind(clint.socket);
```

See [docs/interrupt_modeling_policy.md](../../docs/interrupt_modeling_policy.md).
