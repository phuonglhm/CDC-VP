# CPU Integration Strategy — primary + backup decision (Step 2)

## Decision

- **Primary CPU: Bremen riscv-vp** (`CDC_CPU_BACKEND=riscv_vp`).
- **Backup / reference CPU: mariusmm RISC-V-TLM** (`CDC_CPU_BACKEND=riscv_tlm`).

Both are integrated behind the common `cpu_base` interface and the **same firmware runs on either**
by changing one CMake flag — validated with `fw/hello_baremetal_riscv` on both backends.

## Rationale

| Criterion | Bremen riscv-vp (primary) | mariusmm RISC-V-TLM (backup) |
|---|---|---|
| ISA | RV32/RV64, IMAC + FD (softfloat), Zicsr | RV32/RV64 IMAC |
| Privilege / MMU | M/S/U + Sv32/39/48 MMU | M-mode (limited S/U) |
| Linux-capable | Yes (designed for it) | No |
| Host speed (this bench) | ~8.2 MIPS @ 10ms quantum | ~4.1 MIPS (fixed model) |
| Quantum-keeper | Runtime-tunable (clear speedup) | Compile-time only (fixed 10ns/instr) |
| TLM bus shape | 1 combined socket (`CombinedMemoryInterface`) | split instr/data sockets |
| Interrupts | CLINT/PLIC infra in upstream | TLM `irq_line_socket` (cause payload) |
| Code size / build | Larger (softfloat + core-common + boost) | Small, self-contained (+ spdlog) |
| Known quirks | quantum ≥ cycle_time; needs bus_lock/clint set | CSRRS skips write when rd==x0 |

**Why Bremen primary:** it is the only candidate that is Linux/MMU-capable, faster, and exposes a
runtime quantum-keeper — matching the roadmap target (Linux bring-up, performance tuning).

**Why mariusmm backup:** small, easy to read and modify, quick to bring up, and useful as a
cross-check / reference model for bare-metal and RTOS work. Kept as the second `cpu_base` wrapper so
the abstraction stays honest and we have a fallback if a Bremen-specific issue blocks progress.

**Not selected now:** NikosMouzakitis (reference/learning only); DBT-RISE/TGC (only if SMP/perf later
demands a DBT path — both current candidates are single-core for this phase).

## Consequences / follow-ups

- Default `CDC_CPU_BACKEND` stays `riscv_tlm` in the preset for fast iteration; switch to `riscv_vp`
  for Linux/perf work.
- Bremen IRQ/timer (real CLINT) is not yet wired in our platform (Gate 2 was done on mariusmm) — do it
  on Bremen next, reusing Bremen's CLINT instead of the stub.
- Enable DMI forwarding in `bus_router` to unlock the quantum-keeper's full speedup (see
  [cpu_benchmark_results.md](cpu_benchmark_results.md)).
- Licensing: mariusmm is GPL-3.0, Bremen is MIT — both link into the platform; review distribution
  terms before any external release (GPL is the stricter constraint).

See [cpu_benchmark_results.md](cpu_benchmark_results.md) for the measured data.
