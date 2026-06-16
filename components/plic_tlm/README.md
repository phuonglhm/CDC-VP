# plic_tlm

Basic RISC-V **PLIC** (Platform-Level Interrupt Controller), single hart / M-mode
(`cdc::components::plic_tlm`).

Aggregates `num_sources` external interrupt lines (`irq_in[i]`, PLIC source id = `i + 1`) and drives
the CPU's machine **external** interrupt (MEIP, cause 11) via `cpu_base::set_irq`.

## Register map (offsets from base, typically `0x0C000000`)

| Offset | Name | Description |
|-------:|------|-------------|
| `0x000000 + 4*id` | priority[id] | source priority (> threshold = eligible) |
| `0x001000` | pending | pending bitfield (read) |
| `0x002000` | enable | enable bitfield, context 0 |
| `0x200000` | threshold | priority threshold, context 0 |
| `0x200004` | claim/complete | read = claim (returns highest-priority source id); write = complete |

Level-sensitive gateway: `claim` latches a source so it is not re-raised until the matching
`complete`; if the line is still high after complete, it re-asserts.

## Usage

```cpp
cdc::components::plic_tlm plic("plic", cpu, /*num_sources=*/1);  // cpu is cdc::cpu::cpu_base&
bus.add_target(0x0C000000, 0x400000).bind(plic.socket);
peripheral.irq_out(line);  plic.irq_in[0](line);   // source id 1
```

See [docs/interrupt_modeling_policy.md](../../docs/interrupt_modeling_policy.md).
