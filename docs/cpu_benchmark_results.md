# CPU Benchmark Results — quantum-keeper (Step 2)

Measures host simulation throughput (MIPS = simulated instructions / host second) of the two
wrapped RISC-V backends, and the effect of the TLM quantum-keeper on the Bremen core.

## Method

- Platform: `platforms/riscv_cpu_eval` (RV32, RAM @ 0x80000000, UART @ 0x10000000), no DMI.
- Workload: `fw/bench_riscv` — a tight, never-halting ALU loop (`acc += i*i + 7; i++`).
- Harness: `riscv_cpu_eval --bench --sim-ms <T> --quantum <ns> --fw bench.elf`.
  Runs a fixed **simulated** time `T`, measures **host wall-clock** around `sc_start`, reads the
  backend's retired-instruction counter, and reports `MIPS = instret / host_s / 1e6`.
- Quantum = TLM global quantum (`tlm_global_quantum`), set before the platform is built.
  The Bremen ISS requires quantum ≥ its cycle time (10ns), so "no batching" = 10ns (sync every cycle).
- `T = 20 ms` simulated. Single representative run per cell (host time varies ±10%).
- Host: AlmaLinux 9, GCC 11, SystemC 2.3.4, Debug build (`-g`, no `-O`).

## Results (T = 20 ms simulated)

| Backend                  | Quantum        | Retired instr | Host time (s) | **MIPS** |
|--------------------------|----------------|--------------:|--------------:|---------:|
| mariusmm (riscv_tlm)     | n/a (fixed 10ns/instr) | 2,000,000 | 0.487 | **4.10** |
| Bremen (riscv_vp)        | 10 ns (sync/cycle)     |   482,765 | 0.101 | **4.78** |
| Bremen (riscv_vp)        | 1 ms                   |   482,765 | 0.061 | **7.86** |
| Bremen (riscv_vp)        | 10 ms                  |   482,765 | 0.059 | **8.19** |

## Interpretation

- **Quantum-keeper works (Bremen):** raising the quantum from sync-every-cycle to 1 ms lifts host
  throughput ~1.7× (4.78 → 7.86 MIPS); 10 ms adds little more (diminishing returns). Larger quanta
  batch the SystemC time-sync, eliminating per-cycle `wait()`/context-switches.
- **Bremen is faster than mariusmm** at host level (8.19 vs 4.10 MIPS, ~2×) and is runtime-tunable.
- **mariusmm has no runtime quantum** — its core advances a fixed `wait(10ns)` per instruction, so
  `--quantum` does not affect it; it is a single baseline point. (Its retired-instr count is higher
  because it models 1 instr / 10ns cycle, whereas Bremen's instruction timing is multi-cycle.)
- **The QK speedup is modest here because DMI is disabled** — our `bus_router` does not forward
  `get_direct_mem_ptr`, so every instruction fetch and data access is a TLM `b_transport`, and that
  transaction cost dominates. Enabling DMI (a deferred bus_router feature) would let the quantum-keeper
  show a much larger speedup, closer to Bremen's native numbers.

## Reproduce

```bash
source tools/setup_env.sh            # or export CC/CXX + RISC-V toolchain on PATH
make -C fw/bench_riscv               # -> fw/bench_riscv/bench.elf

# mariusmm (default preset)
cmake --build --preset debug --target riscv_cpu_eval
./build/debug/platforms/riscv_cpu_eval/riscv_cpu_eval --bench --sim-ms 20 --fw fw/bench_riscv/bench.elf

# Bremen, sweep the quantum
cmake -S . -B build/bremen -G Ninja -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCDC_CPU_BACKEND=riscv_vp
cmake --build build/bremen --target riscv_cpu_eval
for q in 10 1000000 10000000; do
  ./build/bremen/platforms/riscv_cpu_eval/riscv_cpu_eval \
     --bench --sim-ms 20 --quantum $q --fw fw/bench_riscv/bench.elf
done
```
