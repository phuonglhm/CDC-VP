# CPU Benchmark Results — quantum-keeper (Step 2)

Measures host simulation throughput (MIPS = simulated instructions / host second) of the
Bremen RISC-V backend, and the effect of the TLM quantum-keeper on it.

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

Re-measured 2026-06-27 (Debug build). Retired-instruction counts are exact and
reproducible; MIPS varies run-to-run (~±10%) with host load.

| Backend                  | Quantum        | Retired instr | Host time (s) | **MIPS** |
|--------------------------|----------------|--------------:|--------------:|---------:|
| Bremen (riscv_vp)        | 10 ns (sync/cycle)     |   482,765 | 0.108 | **4.48** |
| Bremen (riscv_vp)        | 1 ms                   |   482,765 | 0.060 | **7.98** |
| Bremen (riscv_vp)        | 10 ms                  |   482,765 | 0.060 | **7.99** |

## Interpretation

- **Quantum-keeper works (Bremen):** raising the quantum from sync-every-cycle to 1 ms lifts host
  throughput ~1.8× (4.48 → 7.98 MIPS); 10 ms adds nothing more (diminishing returns past ~1 ms).
  Larger quanta batch the SystemC time-sync, eliminating per-cycle `wait()`/context-switches.
- **The QK speedup is modest here because DMI is disabled** — our `bus_router` does not forward
  `get_direct_mem_ptr`, so every instruction fetch and data access is a TLM `b_transport`, and that
  transaction cost dominates. Enabling DMI (a deferred bus_router feature) would let the quantum-keeper
  show a much larger speedup, closer to Bremen's native numbers.

## Reproduce

```bash
source tools/third_party/setup_env.sh   # sets CC/CXX + RISC-V toolchain (riscv-none-elf) on PATH
make -C fw/bench_riscv                   # -> fw/bench_riscv/bench.elf

# Bremen (riscv_vp) — the quantum sweep
cmake -S . -B build-bench-vp -DCMAKE_BUILD_TYPE=Debug -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_CPU_EVAL=ON -DCDC_BUILD_CUSTOM_SOC=OFF -DCDC_BUILD_MINI_TLM=OFF -DCDC_BUILD_TESTS=OFF
cmake --build build-bench-vp --target riscv_cpu_eval -j"$(nproc)"
EXE=build-bench-vp/platforms/riscv_cpu_eval/riscv_cpu_eval
for q in 10 1000000 10000000; do
  "$EXE" --bench --sim-ms 20 --quantum "$q" --fw fw/bench_riscv/bench.elf
done
```
