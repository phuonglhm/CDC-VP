# FlooNoC Metrics Dashboard Implementation

Status: D0-D6 v1 complete  
Date: 2026-08-05  
Roadmap: `NOC_METRICS_DASHBOARD_ROADMAP.md`

## 1. Outcome

The production FlooNoC SystemC datapath now supports this end-to-end flow:

```text
detailed noc_soc or drainable noc_benchmark
        -> floo-noc-metrics-v1 JSON
        -> terminal dashboard
        -> topology/load sweep
        -> constrained winner
```

The report measures an interconnect, not an NPU. It exposes transaction
latency distributions, useful payload throughput, both physical request and
response meshes, directed-link utilisation, stalls, FIFO occupancy, traffic
attribution, fairness and saturation behaviour.

Area, power and energy remain `unavailable`. No placeholder is used.

## 2. Mandatory build environment

Run this before every build:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

$CC -dumpfullversion
$CXX --version | head -n 1
```

Only add the RISC-V toolchain afterward when building firmware:

```bash
source tools/third_party/setup_env.sh
```

## 3. Implemented phases

| Phase | Result |
|---|---|
| D0 | Versioned JSON schema, canonical `North/East/South/West/Eject` port map and `[M]/[D]/[A]/[S]` source contract |
| D1 | One passive production counter per router on both physical meshes, including explicit input/output FIFO occupancy |
| D2 | Exact integer latency histogram, P50/P95/P99, bytes, manager/target/class attribution and manager-to-target matrix |
| D3 | Warm-up, passive counter reset, modeled measurement window, injection stop, drain, conservation and deterministic repeat |
| D4 | Atomic `--noc-metrics` JSON, provenance stamping and dependency-free ASCII dashboard |
| D5 | Quiet, hotspot, contention and fairness workloads; seed, read ratio, burst size, source gap and 1-8 concurrent workers |
| D6 | Bounded topology/load sweep, per-run evidence, CSV/aggregate JSON, constraints, objectives and winner rejection rules |
| D7 | Deferred until calibrated RTL synthesis and power evidence exists |

The synthetic benchmark uses the production
`cdc::components::noc_interconnect` detailed backend. It is not an analytical
replacement for the NoC.

## 4. Important measurement distinction

Two cycle domains are intentionally separate:

- modeled measurement cycles are elapsed SystemC time divided by the 1 ns NoC
  period; throughput and directed-link utilisation use this denominator;
- router `counted_cycles` are cycles in which the gated mesh clock actually
  ran; FIFO occupancy means and clock-active ratio use this value.

Using mesh-active cycles as the throughput denominator made quiet traffic look
identical to a zero-gap hotspot. The D3 regression explicitly detects that
mistake.

The firmware platform is different from the synthetic benchmark:

- `noc_soc --mode firmware` measures a real FreeRTOS workload, but the CPU and
  interrupts were not stopped at the fixed end time, so the JSON always says
  `drained: false`; an instantaneously idle mesh is not a completed drain;
- `noc_benchmark` owns every issuer, stops injection, waits for wrapper and both
  meshes to drain, checks injected/ejected flit conservation, then writes
  `drained: true`.

Only a detailed, complete, drained benchmark row is eligible for a DSE winner.

## 5. Main files

```text
components/floo_noc_model/
  include/floo_noc_model/
    noc_counters.hpp
    noc_metrics.hpp
  docs/
    NOC_METRICS_DASHBOARD_ROADMAP.md
    NOC_METRICS_DASHBOARD_IMPLEMENTATION.md

platforms/noc_soc/
  metrics/metrics_schema_v1.json
  src/noc_benchmark.cpp
  tests/
    run_metrics_negative_controls.sh
    run_metrics_regression.sh
    test_noc_dashboard.py
    test_noc_sweep.py

tools/
  noc_dashboard.py
  noc_metrics_stamp.py
  noc_sweep.py
```

`noc_soc_top.cpp` owns the real FreeRTOS traffic attribution and firmware JSON
path. `noc_benchmark.cpp` owns the drainable D3-D6 path.

## 6. Build and run

Build the platform and benchmark:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head -n 1

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCDC_BUILD_TESTS=ON
cmake --build build --target noc_soc noc_benchmark --parallel
```

Generate one complete synthetic measurement:

```bash
./build/platforms/noc_soc/noc_benchmark \
  --workload fairness \
  --topology 4x4 \
  --transactions 64 \
  --warmup-transactions 2 \
  --workers-per-manager 8 \
  --injection-gap-cycles 0 \
  --read-percent 50 \
  --burst-bytes 8 \
  --seed 17 \
  --noc-metrics /tmp/floo_noc_metrics.json

python3 tools/noc_dashboard.py /tmp/floo_noc_metrics.json
```

### 6.1 Live FreeRTOS CLI dashboard

The interactive `noc_dashboard` command reuses the same JSON schema and the
same renderer; it does not maintain a second firmware-format report:

```text
UART RX command -> FreeRTOS CLI -> private UART TX record
                -> noc_soc live counter snapshot
                -> atomic floo-noc-metrics-v1 JSON publish
                -> tools/noc_cli.py -> tools/noc_dashboard.py
```

Terminal 1:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head -n 1

cmake --build build --target noc_soc --parallel

source tools/third_party/setup_env.sh
make -C fw/freertos_noc_soc NOC_STEP=128 check

./build/platforms/noc_soc/noc_soc \
  --mode firmware \
  --noc-timing detailed \
  --fw fw/freertos_noc_soc/freertos_noc_soc.elf \
  --sim-us 100000 \
  --noc-metrics /tmp/noc_cli_live.json \
  --uart0-socket 5555 \
  --uart0-wait
```

Terminal 2:

```bash
python3 tools/noc_cli.py \
  --port 5555 \
  --metrics /tmp/noc_cli_live.json
```

Wait for `FreeRTOS NoC CLI ready` and enter `noc_dashboard`. The private record
is removed from user-visible output. JSON publication uses temporary-file plus
rename semantics, and the host client waits for a new file identity before
calling the renderer. This preserves one canonical dashboard implementation.

The snapshot describes the firmware activity accumulated from reset to the
request. It is deliberately `drained: false`, has no warm-up exclusion and is
not eligible to win a DSE sweep. Detailed mode and `--noc-metrics` are required.
Fast mode emits an explicit unavailable diagnostic because it cannot provide
measured physical-mesh counters.

Run the bounded D3-D5 regression:

```bash
NOC_BENCHMARK_BIN=build/platforms/noc_soc/noc_benchmark \
NOC_DASHBOARD_TOOL=tools/noc_dashboard.py \
platforms/noc_soc/tests/run_metrics_regression.sh
```

Run the D3-D6 mutation gate:

```bash
NOC_INTERCONNECT_LIBRARY=build/components/floo_noc_model/libnoc_interconnect.a \
NOC_DASHBOARD_TOOL=tools/noc_dashboard.py \
NOC_SWEEP_TOOL=tools/noc_sweep.py \
platforms/noc_soc/tests/run_metrics_negative_controls.sh
```

Run a DSE sweep:

```bash
python3 tools/noc_sweep.py \
  --benchmark build/platforms/noc_soc/noc_benchmark \
  --output-dir /tmp/floo_noc_sweep \
  --topologies 2x2,3x3,4x4 \
  --workers 1,4,8 \
  --gaps 0 \
  --workload fairness \
  --transactions 64 \
  --objective max-bandwidth
```

Outputs:

```text
/tmp/floo_noc_sweep/cfgNNN.metrics.json
/tmp/floo_noc_sweep/cfgNNN.log
/tmp/floo_noc_sweep/sweep.json
/tmp/floo_noc_sweep/sweep.csv
```

Available objectives:

```text
min-p99
max-bandwidth
min-peak-util
max-fairness
```

Available constraints:

```text
--max-p99 CYCLES
--min-bandwidth GB_PER_S
--max-link-util PERCENT
```

## 7. Verification evidence

The following gates pass with GCC/G++ 11.5 and SystemC 2.3.4:

- production counter, histogram, mesh and observer unit tests;
- exact router and mesh RTL cross-checks after counter attachment;
- dashboard corrupt/partial/schema/port/source-label rejection;
- D3 warm-up exclusion, reset neutrality, drain, offered/delivered
  reconciliation, flit conservation and same-seed determinism;
- D5 quiet/hotspot/contention/fairness cases;
- a repeatable saturation knee: increasing workers from 1 to 8 raises P99
  substantially while throughput scales sub-linearly;
- D6 failed, constraint-rejected and undrained rows cannot win;
- changing the objective selects the directed expected row;
- all 15 minimum roadmap mutations are detected for their intended reason;
- four additional workload mutations separately prove that
  quiet/hotspot/contention/fairness checks are behaviour-sensitive;
- the full component registry passes 51 detected and zero missed, and the
  D3-D6 platform metrics registry passes 10 detected and zero missed;
- the host-assisted CLI protocol test covers every TCP split of the private
  record, back-to-back requests, atomic metrics publication and exact
  UART/dashboard output ordering;
- Step 12.8 accepts the dashboard command and a dedicated mutation proves that
  omitting its private host request is detected.

The normal CTest subset is:

```bash
ctest --test-dir /tmp/cdc_vp_noc_metrics \
  --output-on-failure -R 'noc_soc_metrics|noc_soc_sweep|noc_soc_cli_dashboard'
```

## 8. Remaining optional extensions

These are not blockers for D0-D6 v1:

- MMIO-heavy, uniform-random and permutation traffic families;
- explicit endpoint-placement CLI beyond the deterministic placement policy;
- calibrated area, power and energy from named RTL/tool/library evidence;
- RTL-significant FIFO/flit/VC/routing sweeps after each configuration gains
  corresponding SystemC support, RTL identity and cross-check coverage.

Fast mode remains for long functional firmware runs. It must not be used for
measured utilisation, stalls, occupancy, contention or DSE selection.
