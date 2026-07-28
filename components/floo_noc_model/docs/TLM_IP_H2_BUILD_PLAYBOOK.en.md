# Direction-2 IP Build Playbook (Cycle Micro-Architecture) — General Framework

> **This is a GENERAL framework for ANY IP going Direction 2**, not just the NPU.
> Once an IP qualifies for Direction 2 per `TLM_IP_DIRECTION_GUIDE` (verdict `H2_FULL`),
> follow this from zero until it runs inside **CDC-VP** and is packaged into a binary SDK.
>
> **The NPU (SAURIA) is only the WORKED EXAMPLE that fills the skeleton.** Your IP will
> have **different internal blocks (P3), op-mapping (P6), register contract (P9), and run
> style (P10)** than the NPU — follow the **GENERAL** part of each step and use the
> **NPU example** only as a pattern. Do not copy the NPU's structure onto a different IP.
>
> **Integration target = CDC-VP** (SystemC/TLM-2.0 + CMake, RISC-V SoC, CLINT/PLIC).
> **FVP/LISA/simgen is dropped.**
>
> **Reference template (absolute paths on this machine — read alongside as you work):**
> - Core cycle-level model:
>   `/home/duyptt_HW/Documents/work/fx1/hw/tlm/MP1_V1.1/v4_model/` (SAURIA NPU) —
>   **reused via the `SAURIA_NPU_ROOT` variable, NOT copied into CDC-VP**.
> - CDC-VP integration component:
>   `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/npu_tlm_v4_model/`
>   (`include/npu_tlm_v4_model.h`, `include/npu_tlm_v4_regmap.h`,
>   `src/npu_tlm_v4_model.cpp`, `CMakeLists.txt`, `tests/`).
> - Platform assembly:
>   `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/platforms/VP_FX1_Full_SoC/`
>   (`src/vp_fx1_full_soc_top.{h,cpp}`, `configs/default.yaml`, `src/main.cpp`).
> - Packaging:
>   `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/cmake/modules/CdcPortable.cmake`,
>   `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/tools/pack_fx1_sdk.sh`.
> - Packaged SDK + the copy shipped to another project:
>   `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/out/vp_fx1_sdk_<hash>.tar.gz`,
>   `/home/duyptt_HW/Desktop/VP_INTER/upgit/fx1/` (vp/ + sw/).
>
> Notation `<ip>` = lowercase IP name; `<IP>` = uppercase; `<MODEL_ROOT>` = the CMake
> variable pointing at the core model (like `SAURIA_NPU_ROOT`).
> Each step: **GENERAL** (any IP) · **NPU example** (how the NPU fills it) · **Done when**
> · **Pitfalls**. **Do not skip steps.** Bottom-up: verify each block before integrating.

---

## Phase map (skeleton)

```
P0  Prep & lock decisions              → micro-arch/RTL spec, golden, register contract
P1  Golden reference + test vectors    → the "correct" source to compare against
P2  Types & interface layer            → <ip>_types.h
P3  Model submodules (bottom-up)       → decompose the micro-arch → clocked sc_modules + TB
P4  Top-level integration              → <ip>_top.h (wiring, host mux, sc_trace)
P5  Instrumentation                    → performance counters (+ RO regs)
P6  Driver/config layer (if needed)    → high-level op → register fields + data layout
P7  Verification suite                 → per-block + top + stress + spec + vs RTL
P8  Standalone build system            → Makefile, run modes
──────────────────── CDC-VP integration ────────────────────
P9  Register contract                  → <ip>_tlm_regmap.h (per the platform convention)
P10 TLM wrapper component              → <ip>_tlm_model.{h,cpp} (sockets + gated clock + worker)
P11 CMake component (static lib)       → CMakeLists.txt, install/EXPORT
P12 Platform assembly + build + package → bind bus/PLIC, CdcPortable, pack SDK
P13 System validation                  → RISC-V firmware on the CDC-VP exe, vs H1 & RTL
P14 Coexistence (gated clock) + maintenance → keep it in the platform, enable option
```

**IP-independent** steps (use the skeleton verbatim): P1, P2, P4, P8, P11, P12, P13, P14.
**IP-dependent** steps (must be designed for YOUR IP): **P3, P5, P6, P9, P10**.

---

## P0 — Prep & lock decisions

- **GENERAL:**
  1. Obtain the **micro-arch spec / RTL** (not just the register map): block diagram, FSM,
     pipeline, internal memory, host interface, valid/ready, per-stage latency.
  2. Lock the **SW-visible register contract** — match the platform convention + the IP's
     Direction-1 model. CDC-VP uses **physical addresses**. See P9.
  3. Lock the **parameters** (datapath size, memory/FIFO depths, pipeline stages, types).
  4. **DECISION #1 — output source:** is the model datapath authoritative, or a golden
     overwrite? Recommend: datapath is the output, golden only to *compare*.
  5. **DECISION #2 — timing level:** cycle-approximate or aiming at RTL-signed CA.
  6. Lock the **list of numbers to measure** → drives P5 + the RO registers in P9.
- **NPU example:** a GEMM accelerator contract (SRC/DST/WEIGHTS + WIDTH/HEIGHT/K),
  int8×int8→int32, output currently golden-overridden.
- **Done when:** micro-arch spec, register contract, parameter table, two decisions.
- **Pitfalls:** mistaking a register spec for a micro-arch spec → request more (`NEEDS_INFO`).

---

## P1 — Golden reference + test vectors

- **GENERAL:** a plain golden model (C/C++/Python) correct in function, timing-agnostic; a
  generator for random inputs + expected outputs + corner cases; a standard data-file format.
- **NPU example:** `generate_*_test.py`, `tb_data*/` (GEMM random/unity/sparse/spec-int8).
- **Pitfalls:** golden vs model layout mismatch → false failures.

---

## P2 — Types & interface foundation

- **GENERAL:** system parameters + arithmetic types; the **address map**; **signal-safe
  templated vector containers** for `sc_signal<>` (wrap `std::array`, with `operator==`,
  `operator<<`, `sc_trace`).
- **NPU example:** `sauria_types.h` (act/wei/psum_vector_t, host_data_t, sc_trace).
- **Pitfalls:** missing `sc_trace`/`operator==` for custom types → `sc_signal` errors.

---

## P3 — Model each submodule (bottom-up, one TB per block)

- **GENERAL — decomposition method (do NOT copy the NPU's blocks):** from the spec/RTL,
  decompose the IP's micro-architecture into blocks; each is a templated `sc_module` with
  `i_clk`/`i_rstn`/ports, verified standalone, built **leaves-up**. **Common block groups**
  — do the ones YOUR IP has; **not every IP has all six**:

  | Group | What to model | Which IPs have it |
  |---|---|---|
  | (a) **Compute/transform datapath** (core function) | pipeline delay line, special mechanisms | almost every compute IP |
  | (b) **Parallel / pipeline assembly** | tile many (a) units into an array/lane/channel | IPs with parallelism |
  | (c) **Data movement / feeders / buffering** | read memory, ordering/skew, FIFO, back-pressure | streaming IPs |
  | (d) **Internal memory** | SRAM/FIFO/regfile/line-buffer/double-buffer, read latency | IPs with local storage |
  | (e) **Control / FSM** | sequence the work, faithful to RTL FSM, log states | almost always |
  | (f) **Config/status register block** | host write/read, drive params, start/done handshake | almost always |

- **NPU example filling the skeleton:** (a)=PE `systolic_array/sa_processing_element.h` ·
  (b)=systolic array `sa_array.h` · (c)=feeders `data_feeder/*_feeder.h` (skew, FIFO) ·
  (d)=SRAM A/B/C `sram/sram_top.h` (double-buffer) · (e)=`control/main_controller.h`
  (`enum ctrl_state_t`, "matching context_fsm.sv") · (f)=`config/config_regs.h`.
- **Other IPs decompose very differently**, e.g.:
  - **DMA:** (b) N channels · (a) copy/transform engine · (e) descriptor FSM + arbiter · (f) regs. No systolic-style (c)/(d).
  - **Codec (VPU):** (a) parse/entropy → IDCT/transform → reconstruct → loop-filter · (d) line/reference buffers · (e) frame/slice FSM.
  - **Crypto:** (a) round datapath + key schedule · (e) mode FSM (CBC/CTR...) · (f) key/IV/data regs. Usually no (b)/(c).
- **GENERAL — each block has its own TB** (P7.1) and must **PASS before** assembly.
- **Pitfalls:** forcing the NPU structure onto an IP it doesn't fit; the (e) FSM diverges
  from RTL most easily → cross-check early.

---

## P4 — Top-level integration (core)

- **GENERAL:** `<ip>_top` declares top ports (clk, rstn, soft_reset, start, done, deadlock,
  host addr/wr/rd/wdata/wmask/rdata, runtime configs); `new` submodules + wire clock/reset +
  internal signals; `SC_METHOD` host read mux / start-reset / debug; `sc_trace` waves.
- **NPU example:** `npu_top.h`.
- **Done when:** one op runs end-to-end in a TB, asserting `o_done`.
- **Pitfalls:** mis-wiring → deadlock; add `o_deadlock` + a timeout.

> `<ip>_top` is the **pin-level interface** the TLM wrapper (P10) drives — the boundary
> between "core model" and "CDC-VP integration". This boundary is the same for every IP.

---

## P5 — Instrumentation (measurement)

- **GENERAL:** collect the **numbers that matter FOR THIS IP** (locked in P0 #6). Separate:
  1. **Measured directly** from the model (run cycles, stalls, busy, active elements, throughput).
  2. **Analytic** (DMA/overlap cost by a bandwidth formula, theory-min).
  3. Choose which to **expose to SW via RO registers** (P9).
- **NPU example:** `instrumentation/perf_counters.h` (PE utilization, mac_ops, exec/stall) +
  `eval_counters.h` (analytic DMA). RO regs: CYCLE_COUNT, BYTES_READ/WRITTEN, LAST_ERROR.
- **Other IPs:** "PE utilization" is meaningless for a DMA → measure bytes/cycle, queue
  depth; for a codec → cycles/frame; for crypto → cycles/block, throughput. **Pick numbers
  that match the IP's nature.**
- **Pitfalls:** mixing **measured** with **analytic** numbers when reporting.

---

## P6 — Driver / config layer (CONDITIONAL)

- **GENERAL:** **only do this if** the "high-level op → register values + data layout"
  mapping is **complex** (loop-limits/tiling/skew). If registers map directly to behavior
  (simple IP), **SKIP this step**. If needed: a layer descriptor; `compute_fields(desc,
  target)`; `prepare(...)` data layout; a target/profile table.
- **NPU example:** needs it — `driver/lib*_cfg.h`, `*_run.h`, `*_targets.h` compute
  incntlim/rows_active/ncontexts + skew the tensors.
- **Other IPs:** a DMA's channel config is simple, crypto loads key/IV directly → **no
  driver layer needed.**
- **Pitfalls:** the layer most prone to layout bugs; compare against the P1 golden immediately.

---

## P7 — Verification suite (all tiers)

- **GENERAL:**
  - **P7.1 Per-block TB:** each P3 submodule a TB (`sc_module` driving the DUT with an
    `SC_THREAD` + `wait(clk.pos())`, comparing to golden, PASS/FAIL, waveform).
  - **P7.2 Top functional TB:** host thread + one run cycle.
  - **P7.3 Large + performance TB:** large workload, measure the IP-specific throughput.
  - **P7.4 Stress TB:** corner cases (saturation, empty, dense, boundaries).
  - **P7.5 Spec / special-config TBs.**
  - **P7.6 RTL cross-check (MANDATORY):** function + cycle/FSM match RTL (the `RTL/` folder).
- **NPU example:** `tb_sa_array.cpp`, `tb.cpp`, `tb_32x32.cpp` (GFLOPS/sparsity),
  `tb_32x32_stress.cpp`, `tb_standalone/strided/multitile/spec_evaluate.cpp`.
- **Other IPs:** swap P7.3/P7.4 for the right metric (frames, blocks, bytes...). P7.1/P7.6
  always apply.
- **Done when:** function matches golden 100%; timing matches RTL within the P0 #2 tolerance.
- **Pitfalls:** without an RTL cross-check, cycle numbers are only "estimates", not CA.

---

## P8 — Standalone build system (verify the model alone)

- **GENERAL:** a Makefile detecting `SYSTEMC_HOME`, `-O3 -std=c++14 -I$(SYSTEMC_HOME)/include`,
  link `-lsystemc -lm -pthread`; a target per TB; `run_*`; `clean`. Run **before** entering
  CDC-VP.
- **NPU example:** `v4_model/Makefile`.
- **Done when:** `make run` and `make run_*` run green.

> From here down is **CDC-VP integration**. The core model (P2–P8) is reused via
> `<MODEL_ROOT>`, NOT copied into the CDC-VP tree. Steps P9–P14 have the **same shape** for
> every IP; only the contract/wrapper contents vary per IP.

---

## P9 — Register contract (SW-visible)

- **GENERAL:** define the register map firmware uses, per the **platform's convention for
  that IP class**. Invariants: **physical addresses**, 32-bit little-endian words, and
  **match the IP's Direction-1 model**. The minimum most IPs share: `CTRL`
  (ENABLE/START/SOFT_RESET/IRQ_EN), `STATUS` (BUSY/DONE/ERROR/IDLE, W1C), `IRQ_ENABLE`,
  `IRQ_STATUS`, + **RO counters** (CYCLE_COUNT, LAST_ERROR...), + an `enum error_code`.
- **NPU example (accelerator contract — template for compute IPs that fetch operands from
  RAM):** adds `SRC_ADDR`/`DST_ADDR`/`WEIGHTS_ADDR`/`*_SIZE_BYTES`/`WIDTH`/`HEIGHT`/`FORMAT`/
  `OP_MODE` + a private bank (K_DIMENSION, ROWS_ACTIVE...). File `npu_tlm_v4_regmap.h`.
- **Other IPs:** the accelerator contract only fits "read RAM → process → write RAM" IPs.
  Others use a different layout (codec: frame-buffer addrs; crypto: key/IV/data regs; DMA:
  src/dst/len/ctrl per channel). **Use the IP's own convention, don't copy the NPU's set.**
- **Pitfalls:** offset/bit divergence from H1 or the platform contract → swappability lost.

---

## P10 — TLM wrapper component (integration core)

- **GENERAL:** wrap the cycle-level core as a TLM-2.0 `sc_module` on the CDC-VP bus.
  **Replaces both the FVP "bridge" and "LISA"** — the control logic lives in `impl`. Skeleton:
  1. **Header** `<ip>_tlm_model.h`: an `sc_module` with
     - `tlm_utils::simple_target_socket` — MMIO slave (**always**).
     - `tlm_utils::simple_initiator_socket` — RAM DMA master (**only if the IP autonomously
       reads/writes system memory**; a pure register IP does not need it).
     - `sc_in<bool> reset_n`, `sc_out<bool> irq_out`.
     - `b_transport`, `transport_dbg`, `drive_irq`, and (**if the op is long-running**)
       `worker_thread`; a pImpl `struct impl`.
  2. **`impl`:** owns the **core** instance + `sc_signal`s wired to it + the `register_file`;
     **GATED CLOCK (mandatory):** `sc_signal core_clk` + a `clock_thread` toggling only while
     the worker consumes edges → an idle IP costs ~0 sim time.
  3. **`b_transport`:** decode offset → read/write `register_file`; on a `CTRL.START` write
     → kick the worker (or run inline if the op is short).
  4. **`worker_thread`:** validate → (DMA read if there's a master) → stage into the core →
     enable the gated clock, pulse start, **poll `done`/`deadlock` with a timeout** → read
     results → (DMA write) → update STATUS/IRQ + counters.
  5. **DECISION #1 (P0):** if datapath-authoritative, do NOT overwrite with golden.
- **NPU example:** `npu_tlm_v4_model.{h,cpp}` — has a master socket, a worker staging tensors
  + tiling, gated `core_clk`.
- **Other IPs:** an IP with no DMA (works on internal state) → **omit the initiator socket**,
  a short worker or synchronous handling in `b_transport`. But the **gated clock is still
  mandatory** if the core has its own clock.
- **Done when:** a simple TLM test writing START yields the correct result + sane counters;
  **idle consumes no sim cycles**.
- **Pitfalls:** forgetting to gate the clock → the whole platform slows while the IP is
  idle; missing timeout/`deadlock` → hang.

---

## P11 — CMake component (static lib)

- **GENERAL:** `CMakeLists.txt`: `add_library(<ip>_tlm_model STATIC ...)` + alias
  `cdc::components::<ip>_tlm_model`; `target_include_directories` PUBLIC `include/`, PRIVATE
  `${<MODEL_ROOT>}`; link `SystemC::systemc`; `install(TARGETS ... EXPORT
  cdc-components-targets ...)`; an **option `CDC_ENABLE_<IP>` (default OFF)** +
  `FATAL_ERROR` if the model is missing; `tests/`.
- **NPU example:** `npu_tlm_v4_model/CMakeLists.txt` (`CDC_ENABLE_SAURIA_NPU_V4`,
  `SAURIA_NPU_ROOT`).
- **Done when:** `-DCDC_ENABLE_<IP>=ON -D<MODEL_ROOT>=...` builds `lib<ip>_tlm_model.a`.

---

## P12 — Platform assembly + build + SDK packaging

- **GENERAL:**
  1. **Assemble in the platform top:** declare the instance (guard `#if CDC_ENABLE_<IP>`);
     address + MMIO size constants; `bus.add_target(base,size).bind(target_socket)`;
     `master_socket.bind(bus.cpu_port(port++))` (if it has DMA); `irq_out→plic.irq_in[N]`;
     `reset_n`. Bump the bus target/master counts.
  2. **Config:** add the address to `configs/*.yaml`; `main.cpp` sets `tlm_global_quantum`.
  3. **Build:** `cmake -S . -B build-soc -DCDC_ENABLE_<IP>=ON -D<MODEL_ROOT>=/path` →
     `cmake --build build-soc`.
  4. **Package:** `cdc_make_portable` ($ORIGIN + libsystemc.so) → `cdc_package_platform`
     → `out/<target>/` → `pack_fx1_sdk.sh` → `out/..._sdk_<hash>.tar.gz`.
  5. **LICENSE:** if the external model has its own license (e.g. SAURIA SHL-2.1), you
     **must ship its license + provenance in `vp/licenses/`** when the IP is enabled (the
     model code is compiled **into** the binary).
- **NPU example:** `vp_fx1_full_soc_top.cpp` (`bus.add_target(kNpu0,...).bind`,
  `npu0.master_socket.bind`, `plic.irq_in[16]`), `CdcPortable.cmake`, `pack_fx1_sdk.sh`.
- **Done when:** the exe builds clean; `out/..._sdk_<hash>.tar.gz` runs on another machine.
- **Pitfalls:** forgetting to bump `num_targets`/`num_masters`; duplicate PLIC index;
  missing `libsystemc.so` or the model license.

---

## P13 — System validation (on CDC-VP)

- **GENERAL:** RISC-V firmware programs the registers (physical addresses) + START + waits
  for DONE/IRQ + reads the result; **function** == P1 golden == the Direction-1 model;
  **timing** read from the RO counters, compared vs RTL (P0 #2 tolerance); re-run the P7
  case set through the CDC-VP path.
- **NPU example:** place A/B/C in RAM, write SRC/DST/WEIGHTS_ADDR + K, START, read CYCLE_COUNT.
- **Pitfalls:** if golden-override (P0 #1), the function check **does not** exercise the datapath.

---

## P14 — Coexistence (gated clock) + maintenance

- **GENERAL:**
  1. **The gated clock (P10) is the coexistence mechanism:** an idle IP costs ~0 → the
     Direction-2 model stays in the platform, no H1/H2 swap needed.
  2. Keep the **`CDC_ENABLE_<IP>` option** to compile it out.
  3. **Parity:** the register contract always matches the functional model.
  4. **Maintenance:** when RTL/model changes → update FSM/latency + re-run P7.6 + P13;
     record the version; keep the license/provenance.
- **Pitfalls:** forgetting the gated clock → the SoC slows when the IP is idle.

---

## Condensed checklist (paste into PR/issue)

```
[ ] P0  Micro-arch/RTL spec + register contract (matching H1) + 2 decisions
[ ] P1  Golden model + generator + test vectors
[ ] P2  <ip>_types.h (types, address map, vector container + sc_trace)
[ ] P3  Decompose the IP micro-arch → submodules bottom-up, each PASSING its TB  (do NOT copy NPU blocks)
[ ] P4  <ip>_top.h wiring + host mux + start/reset + sc_trace  (pin-level interface)
[ ] P5  counters matching the IP's nature (+ choose numbers to expose as RO regs)
[ ] P6  driver/config  (ONLY if op→config is complex; simple IPs skip)
[ ] P7  TB per-block + top + performance + stress + spec + RTL CROSS-CHECK
[ ] P8  Standalone Makefile + run modes
[ ] P9  include/<ip>_tlm_regmap.h (platform convention, physical addresses, RO counters)
[ ] P10 <ip>_tlm_model.{h,cpp} (target socket + [initiator if DMA] + reset_n/irq_out + GATED CLOCK + [worker if long op] + b_transport)
[ ] P11 CMake static lib + option CDC_ENABLE_<IP> + <MODEL_ROOT> guard + EXPORT
[ ] P12 Platform assembly (bind bus/[master]/PLIC/reset) + build-soc + CdcPortable + pack SDK + model LICENSE
[ ] P13 RISC-V firmware on the CDC-VP exe: matches golden & H1 + matches RTL
[ ] P14 Gated-clock coexistence + enable/disable option + RTL-tracking maintenance
```

## Deliverables (directory tree — block names depend on the IP)

```
<ip>_model/                       # standalone cycle-level model (P2–P8)
  <ip>_types.h
  <block folders from the IP's P3 decomposition>   # NPU: config/ control/ data_feeder/
                                                    #      systolic_array/ psm/ sram/
  <ip>_top.h
  driver/ (if needed)  instrumentation/
  tb*.cpp, generate_*_test.py, tb_data*/, RTL/
  Makefile, README.md
  # -> reused via CMake -D<MODEL_ROOT>=path/to/<ip>_model

CDC-VP/components/<ip>_tlm_model/  # integration component (template npu_tlm_v4_model/)
  include/<ip>_tlm_model.h         # sc_module: target socket [+ initiator] + reset_n + irq_out
  include/<ip>_tlm_regmap.h        # register contract
  src/<ip>_tlm_model.cpp           # impl: core + gated clock + b_transport [+ worker]
  tests/  CMakeLists.txt  README.md

CDC-VP/platforms/<PLATFORM>/       # assembly: bind bus/PLIC/reset + configs
CDC-VP/out/<...>_sdk_<hash>.tar.gz # self-contained binary SDK for other projects
```

---

## Three invariant principles (any IP, CDC-VP)
1. **The Direction-2 register contract must match the platform contract + the functional
   model** (physical addresses) — otherwise swappability is lost.
2. **RTL cross-check is mandatory** for cycle numbers to mean anything; without it they are
   only estimates.
3. **A gated clock (or equivalent)** so the cycle-level model costs ~0 when idle — this lets
   it live in the integrated platform without an H1/H2 swap.

> **Reminder:** this framework applies to every IP. The parts that **vary per IP** are **P3
> (internal blocks), P5 (metrics), P6 (driver or not), P9 (contract), P10 (DMA & worker or
> not)** — design them for your IP's nature, using the NPU only as a filled-in example.
```
