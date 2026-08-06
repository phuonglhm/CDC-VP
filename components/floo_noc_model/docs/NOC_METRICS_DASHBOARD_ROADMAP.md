# FlooNoC Metrics Dashboard and Design-Space Exploration Roadmap

Status: D0-D6 v1 implemented and regression-qualified (2026-08-05)  
Target: `components/floo_noc_model` integrated in `platforms/noc_soc`  
Primary measurement backend: detailed, cycle-stepped SystemC  
Long-run functional backend: fast, approximately timed SystemC  

Implementation report:
`NOC_METRICS_DASHBOARD_IMPLEMENTATION.md`.

The firmware report remains a diagnostic fixed-window observation because a
running CPU cannot be stopped and drained at an arbitrary boundary. The
drainable `noc_benchmark` executable owns D3-D6 sign-off and is the only source
whose rows may be selected by `noc_sweep.py`.

## 1. Objective

Build a terminal dashboard for the FlooNoC model with the same high-level
workflow as the existing NPU design-space report:

```text
Input configuration and workload
        -> real SystemC measurement
        -> metric dashboard
        -> design-space sweep
        -> hardware recommendation
```

The visual style may follow the NPU report, but the content must represent an
interconnect rather than a compute accelerator. The primary NoC questions are:

- What latency distribution does each traffic class observe?
- How much payload bandwidth and transaction throughput is delivered?
- Which router, directed link, port or FIFO is the bottleneck?
- How much latency is caused by contention?
- At what offered load does the network saturate?
- Are managers and flows treated fairly?
- Which supported topology, placement, clock and buffering configuration best
  meets the stated constraints?

The dashboard must keep measured, derived, analytic and static values visibly
separate. It must not present area, power, energy or fast-mode estimates as
cycle-stepped measurements.

## 2. Mandatory build environment

Run this before every build:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head
```

When firmware is involved, set up the pinned RISC-V toolchain only after the
host-compiler sanity check:

```bash
source tools/third_party/setup_env.sh
```

Do not silently use a compiler or SystemC installation from a user-local PATH.

## 3. Current factual baseline

The platform already has a reproducible Step 12.7 measurement path:

- runner:
  `platforms/noc_soc/tests/run_freertos_baseline.sh`;
- platform switch: firmware mode with `--noc-baseline`;
- timing backend: detailed, cycle-stepped mesh;
- measurement workload: level-125 FreeRTOS image;
- archived evidence:
  `platforms/noc_soc/evidence/noc_baseline.txt`;
- passive transaction attribution:
  `noc_interconnect::completion` observer;
- current topology: fixed 4x4 XY mesh;
- current managers: CPU, DMA and latency probe;
- current NoC clock period: 1 ns;
- current important placement:
  CPU `(0,0)`, DMA `(0,3)`, RAM `(0,1)`, PLIC/boot node `(3,0)`.

The current baseline already measures:

- PLIC claim latency;
- PLIC complete latency;
- CPU-to-RAM latency while DMA is idle;
- CPU-to-RAM latency while DMA is active;
- other CPU MMIO latency;
- contention delta between active and idle RAM traffic;
- DMA-issued transaction latency;
- DMA channel start-to-completion-interrupt latency;
- completed transaction count;
- total network latency cycles;
- peak outstanding transactions per manager;
- mesh clocked cycles;
- clock-gate transitions;
- mesh-quiescent/wrapper-busy cycles;
- RTOS tick jitter, explicitly labelled as a platform/RTOS metric rather than a
  NoC metric.

The baseline is diagnostic evidence, not a performance threshold or
timing-closure claim.

## 4. Main gap that must be closed first

`router_counters` already exists in:

```text
components/floo_noc_model/include/floo_noc_model/noc_counters.hpp
```

It has standalone and cross-check coverage, but it is not connected to every
router in the production mesh composition. Consequently, the current platform
must not report:

- per-link utilisation;
- per-port stalls;
- mean FIFO occupancy;
- FIFO high-water marks;
- packet or flit throughput inside the mesh;
- router/link congestion or hotspot maps.

The first implementation milestone is therefore to attach passive counters to
the real detailed mesh without changing datapath behaviour or timing.

The existing counter currently observes input occupancy. The production
dashboard also needs output-FIFO occupancy. Implement that as an explicit
passive observation path and cross-check it; do not infer output occupancy from
valid/ready alone.

## 5. Evidence classes

Every metric in JSON, CSV and terminal output must include one of these source
classes:

| Tag | Meaning | Permitted source |
|---|---|---|
| `[M]` | measured | sampled from the detailed SystemC datapath or a passive completion event |
| `[D]` | derived | arithmetic over named measured values |
| `[A]` | analytic | formula/model not directly observed in the run |
| `[S]` | static/spec | topology, configuration or verified protocol property |

Examples:

```text
[M] east accepted flits                 18420
[M] east stall cycles                    1021
[D] east utilisation                    42.6 %
[D] east stall ratio                     5.3 %
[A] estimated bisection capacity        32.0 GB/s
[S] topology                             4x4 XY
```

Rules:

1. Fast mode may report functional results and labelled no-contention latency
   estimates, but never measured router utilisation, FIFO occupancy, stalls or
   contention.
2. Host wall-clock runtime is simulation performance, not modeled hardware
   performance.
3. Target/peripheral access delay must stay excluded from NoC-only latency.
4. RTOS tick jitter must remain labelled as a platform/RTOS metric.
5. A formula must name all measured inputs from which it is derived.

## 6. Dashboard sections

### 6.1 Report header and provenance

Print and store:

- generation timestamp;
- Git revision and dirty status;
- source-patch hash when dirty;
- platform binary hash;
- firmware hash;
- metrics schema version;
- dashboard tool version;
- host compiler versions;
- SystemC version;
- build type;
- timing mode;
- random seed;
- warm-up and measurement windows.

### 6.2 Input configuration

The first dashboard table should contain:

```text
Topology
Routing policy
NoC clock period and frequency
Manager count and placement
Target placement and address region
Flit/data width
Input/output FIFO depths
Maximum outstanding requests per manager
Workload name
Read/write ratio
Burst-length distribution
Injection rate
Random seed
Warm-up time
Measurement time
```

Values that are frozen by the RTL configuration must be marked `[S]`. A field
that is not implemented must say `unavailable`, not zero.

### 6.3 Raw measured results

Report at least:

- accepted flits and packets per router input and output port;
- busy and stall cycles per port;
- input and output FIFO occupancy sum and high-water mark;
- completed transactions and completed payload bytes;
- latency histogram per traffic class;
- outstanding transaction peak per manager;
- transaction count and bytes by manager;
- transaction count and bytes by target/traffic class;
- clocked cycles and clock-gating transitions;
- measurement-window start/end cycle.

Ports must use one canonical index-to-direction mapping throughout JSON,
terminal output and tests. Direction names are labels; correctness depends on
the single shared mapping, not on their display order.

### 6.4 Derived metric dashboard

The main metric table should contain:

| Metric | Unit | Formula |
|---|---:|---|
| transaction throughput | Mtrans/s | completed transactions / measured time |
| payload bandwidth | GB/s | completed payload bytes / measured time |
| mean latency | cycles, ns | latency sum / completed transactions |
| P50/P95/P99 latency | cycles, ns | exact histogram quantiles |
| worst latency | cycles, ns | maximum measured latency |
| directed-link utilisation | % | accepted flits / counted cycles |
| port stall ratio | % | stall cycles / busy cycles |
| mean FIFO occupancy | entries | occupancy sum / counted cycles |
| FIFO high-water | entries | maximum observed occupancy |
| contention penalty | cycles, % | loaded latency minus quiet latency |
| clock active ratio | % | mesh clocked / eligible modeled cycles |
| clock-gating ratio | % | 1 minus clock active ratio |
| average hop count | hops | traffic-weighted source/target Manhattan distance |
| peak outstanding | requests | maximum measured outstanding count |
| Jain fairness | 0..1 | square of flow sum divided by flow-count times sum of squares |

Use payload bytes for useful bandwidth. Flit rate and packet rate are separate
network-activity metrics and must not be labelled payload bandwidth.

Latency classes should initially include:

- CPU to RAM, DMA idle;
- CPU to RAM, DMA active;
- CPU to PLIC claim;
- CPU to PLIC complete;
- CPU to other MMIO;
- DMA-issued transactions;
- per-manager aggregate;
- per-target aggregate.

The transaction observer currently stores count, sum, min and max. Extend it
with an exact integer-cycle histogram so P50/P95/P99 can be reproduced without
retaining every transaction record.

### 6.5 Hotspot and traffic views

Add a per-router/per-port table:

```text
Router  Port   Flits   Packets   Util%   Stall%   FIFO mean/max   Status
(0,0)   East   ...     ...       ...     ...      ...             HOT
```

Add a manager-to-target traffic matrix:

```text
Source  Destination/Class  Transactions  Bytes  Mean  P95  P99  Max
CPU     RAM
CPU     PLIC
CPU     other MMIO
DMA     RAM
```

Initial hotspot policy should be configurable. Suggested diagnostic defaults:

- warning at directed-link utilisation `>= 70%`;
- critical at directed-link utilisation `>= 85%`;
- warning when stall ratio `>= 10%`;
- warning when FIFO high-water reaches its configured depth.

These are dashboard warnings, not project acceptance thresholds, until
repeatability and correlation have been reviewed.

### 6.6 Design-space decision

For each configuration, print:

```text
Topology  Placement  Freq  MeanLat  P99Lat  BW  MaxLink  Stall  FIFOmax  Result
```

Then print the best feasible configuration for each objective:

- minimum P99 latency;
- maximum payload bandwidth;
- minimum peak-link utilisation;
- minimum contention penalty;
- maximum fairness;
- minimum modeled resource proxy, if and only if the proxy is explicitly
  labelled analytic.

A final winner is permitted only when:

- all user constraints pass;
- all required metrics were exercised;
- no counter overflow or missing bucket occurred;
- the run used detailed mode;
- the configuration is within the supported and verified parameter space.

## 7. Workloads

One FreeRTOS execution is not sufficient to characterise an interconnect.
Maintain two workload families.

### 7.1 Real software workload

Use the existing FreeRTOS concurrent workload:

- CPU RAM traffic;
- DMA traffic;
- TIMER0 interrupts through PLIC;
- CLI disabled or inactive during the measurement window;
- deterministic firmware image and modeled duration.

This answers how the current virtual SoC behaves under its real bring-up
software.

### 7.2 Controlled synthetic workloads

Add a dedicated benchmark mode with deterministic traffic:

1. quiet/no-contention reference;
2. CPU-to-RAM hotspot;
3. CPU and DMA competing for RAM;
4. MMIO-heavy traffic;
5. read-heavy and write-heavy mixes;
6. fixed and swept burst lengths;
7. uniform source/destination traffic where legal;
8. placement-sensitive permutation traffic;
9. injection-rate sweep through saturation;
10. fairness test with at least two active managers.

Every randomised workload must take and report an explicit seed. The same seed
and configuration must reproduce the same modeled counters.

The injection-rate sweep should continue beyond the knee far enough to show:

- latency growth;
- increased stall ratio;
- FIFO high-water growth;
- delivered-bandwidth saturation.

## 8. Measurement window

Use three phases:

```text
reset/boot -> warm-up -> measured window -> drain -> report
```

Requirements:

- warm-up traffic must not appear in measured counters;
- reset all measurement-only counters at the window boundary;
- do not reset functional model state merely to clear statistics;
- stop injection at the end of the window;
- drain the network before final conservation checks;
- record both offered and delivered traffic;
- reject a run that ends with retained flits, outstanding transactions or
  undrained packets;
- record the exact start/end cycle and modeled time.

Counter reset/snapshot/report actions must be passive and must not alter
ready/valid, arbitration, routing, clock-gating or endpoint state.

## 9. Machine-readable output

Do not make the dashboard parser scrape human console output. Add a versioned
machine-readable output:

```bash
./build/platforms/noc_soc/noc_soc \
  --mode firmware \
  --noc-timing detailed \
  --noc-baseline \
  --noc-metrics /tmp/noc_metrics.json \
  --fw fw/freertos_noc_soc/freertos_noc_soc.elf \
  --sim-us 45000
```

Proposed top-level JSON structure:

```json
{
  "schema": "floo-noc-metrics-v1",
  "provenance": {},
  "configuration": {},
  "workload": {},
  "measurement_window": {},
  "transaction_metrics": {},
  "manager_metrics": [],
  "target_metrics": [],
  "router_metrics": [],
  "link_metrics": [],
  "clock_metrics": {},
  "derived_metrics": {},
  "availability": {},
  "warnings": []
}
```

Each metric entry should carry:

```json
{
  "value": 11.43,
  "unit": "cycles",
  "source": "M",
  "samples": 6041
}
```

JSON writing must be deterministic:

- stable field and array ordering;
- no locale-dependent number formatting;
- no host timestamps inside the measured-data hash;
- atomic final write;
- failure if the file cannot be created or completed.

CSV may be generated from JSON for sweep analysis. JSON remains the source of
truth.

## 10. Tool architecture

Keep measurement, rendering and design-space orchestration separate:

```text
noc_soc detailed run
        |
        +--> noc_metrics.json
                  |
                  +--> tools/noc_dashboard.py --> terminal dashboard
                  |
                  +--> tools/noc_sweep.py     --> CSV/JSON sweep summary
```

Suggested files:

```text
platforms/noc_soc/
  metrics/
    metrics_schema_v1.json
  tests/
    run_metrics_regression.sh
    run_metrics_negative_controls.sh

components/floo_noc_model/
  include/floo_noc_model/
    noc_counters.hpp
    noc_metrics.hpp

tools/
  noc_dashboard.py
  noc_sweep.py
```

Avoid third-party Python dependencies for the first version. Use the Python
standard library so the report works on the existing AlmaLinux environment.

## 11. Supported design-space knobs

### 11.1 Safe initial knobs

- endpoint placement;
- supported topology shape;
- network clock period;
- maximum outstanding requests;
- workload;
- read/write mix;
- burst length;
- injection rate;
- random seed.

The `noc_interconnect` constructor currently supports:

```text
2x2  3x3  4x4  4x2  2x4
```

The `noc_soc` platform currently hardcodes a 4x4 construction and placements.
Before a topology/placement sweep, move those values into a validated platform
configuration or an explicit compile-time dispatch. Validate that every manager
and target remains inside the mesh and that no address-map ownership changes.

### 11.2 Restricted knobs

FIFO depth, flit width, virtual-channel count, routing policy and chimney
configuration are RTL-significant parameters. They may enter DSE only when:

- the corresponding SystemC variant exists;
- the RTL configuration is identified;
- cross-checks cover that configuration;
- negative controls prove the relevant test can fail;
- the dashboard records the exact parameter set.

Do not label an arbitrary SystemC-only parameter combination as FlooNoC
hardware evidence.

### 11.3 Frequency sweep caveat

Changing the network clock converts cycle counts to modeled time, but it is not
automatically a hardware frequency/voltage study. Frequency-dependent target
timing, CDC, physical timing closure, dynamic power and voltage are not modeled
by simply changing `clock_period`.

The dashboard may report:

```text
latency cycles [M]
latency ns [D from measured cycles and configured period]
```

It must not infer voltage, area or power from frequency without a calibrated
external model.

## 12. Area, power and energy

The current project has no calibrated NoC area/power model. Version 1 must
display:

```text
Area          unavailable - pending RTL synthesis calibration
Power         unavailable - pending activity/power calibration
Energy/flit   unavailable
```

Never use zero or an undocumented placeholder in a hardware decision.

A later calibrated version may use:

- synthesis area per router configuration;
- FIFO/register-file area by depth and width;
- physical link length from floorplan;
- VCD/SAIF switching activity;
- technology/library/frequency/voltage provenance;
- calibrated router-traversal and link energy per flit.

Area/power/energy values must be tagged `[A]` until correlated against a named
RTL implementation and tool/library setup. A rank based on an uncalibrated
proxy must say that it is a relative analytic ranking only.

## 13. Implementation phases and gates

### D0 - Freeze scope and schema

Deliver:

- metric glossary with unit and source class;
- JSON schema v1;
- canonical router-port mapping;
- exact measurement-window definition;
- initial workload/configuration description.

Gate:

- every dashboard field has a source and formula;
- unavailable metrics are explicit;
- no area/power claim exists.

### D1 - Production router-counter integration

Deliver:

- one passive counter block per production router;
- input and output accepted flits/packets;
- busy/stall cycles;
- input/output occupancy sum and high-water;
- mesh-level snapshot/reset/report API.

Gate:

- existing router/mesh RTL cross-checks remain exact;
- attaching counters changes neither trace nor latency;
- flit conservation holds after drain;
- standalone counter tests and production composition tests pass.

### D2 - Transaction histogram and traffic attribution

Deliver:

- exact integer-cycle histogram;
- P50/P95/P99;
- completed bytes;
- manager/target/traffic-class buckets;
- traffic matrix.

Gate:

- bucket totals equal global transaction and byte totals;
- histogram count and sum equal existing completion totals;
- P50/P95/P99 are checked with directed sample sets;
- target delay remains excluded.

### D3 - Measurement-window control

Deliver:

- warm-up/reset/measure/drain phases;
- explicit start/end cycles;
- offered and delivered counts;
- incomplete/drain failure reporting.

Gate:

- warm-up traffic is absent from the snapshot;
- resetting statistics is behaviour-neutral;
- the final mesh is quiescent;
- repeated run with the same seed/config has identical modeled counters.

### D4 - JSON export and ASCII dashboard

Deliver:

- `--noc-metrics FILE`;
- deterministic JSON;
- `tools/noc_dashboard.py`;
- header, input, measurement, metric, hotspot and conclusion sections;
- explicit `[M]/[D]/[A]/[S]` legend.

Gate:

- schema validation passes;
- renderer handles unavailable fields;
- corrupt, partial and incompatible-schema input is rejected;
- terminal dashboard values exactly match JSON.

### D5 - Synthetic benchmark mode

Deliver:

- deterministic workload generator;
- workload selection and seed;
- injection-rate/read-write/burst controls;
- quiet, hotspot, contention and fairness cases.

Gate:

- injected and delivered traffic reconcile after drain;
- every workload has a directed behavioural negative control;
- saturation sweep shows a reproducible knee rather than only one load point.

### D6 - DSE sweep and decision table

Deliver:

- `tools/noc_sweep.py`;
- configuration matrix;
- constraints and objectives;
- CSV and aggregate JSON;
- winner and rejected-configuration explanations.

Gate:

- a failed or incomplete measurement cannot be selected;
- every row carries provenance;
- changing an objective selects the expected directed configuration;
- unsupported RTL-significant combinations are refused.

### D7 - Optional calibrated area/power

Start only when RTL synthesis/power data is available. Keep this phase outside
the v1 completion gate.

## 14. Required negative controls

At minimum, the mutation gate must demonstrate detection of:

1. accepted flit incremented without `valid && ready`;
2. stall cycle counted when `valid` is low;
3. packet count ignoring the `last` field;
4. one directed router link not connected to its counter;
5. input occupancy high-water update disabled;
6. output occupancy observation removed;
7. warm-up counters not reset;
8. one completed transaction omitted from a class bucket;
9. completed byte count using transaction count instead of payload length;
10. target delay incorrectly included in NoC latency;
11. histogram percentile off by one rank;
12. fast mode accepted as measured utilisation;
13. JSON metric source tag changed from measured to analytic or omitted;
14. sweep selecting a failed/incomplete row;
15. non-drained traffic accepted as a complete measurement.

A control counts as detected only when:

- the mutation was applied;
- the relevant test ran;
- the intended acceptance marker disappeared;
- a defect-specific failure marker appeared;
- the run did not merely fail from compilation, timeout or an unrelated trap.

### 14.1 Qualification result

The implemented gate maps the required defects as follows:

| # | Registered control |
|---|---|
| 1 | `router-counter-stall-counted-as-accepted` |
| 2 | `router-counter-invalid-counted-as-stall` |
| 3 | `router-counter-last-ignored` |
| 4 | `production-router-counter-miswired` |
| 5 | `router-counter-input-high-water-disabled` |
| 6 | `router-counter-output-occupancy-ignored` |
| 7 | `warmup-counters-not-reset` |
| 8 | `completed-transaction-omitted-from-manager-bucket` |
| 9 | `metrics-payload-bytes-count-transactions` |
| 10 | `metrics-target-delay-included` |
| 11 | `metrics-percentile-rank-off-by-one` |
| 12 | `fast-backend-used-for-measurement` |
| 13 | `measured-source-mislabeled-analytic` |
| 14 | `sweep-accepts-failed-row` |
| 15 | `sweep-accepts-undrained-row` |

Controls 1-6 and 9-11 are in the component mutation runner. Controls 7-8 and
12-15 are in `platforms/noc_soc/tests/run_metrics_negative_controls.sh`.
That platform runner also has one directed mutation for each D5 workload:
`quiet-gap-collapsed`, `hotspot-gap-expanded`, `contention-manager-removed` and
`fairness-manager-removed`.

Current result with GCC/G++ 11.5 and SystemC 2.3.4:

```text
component registry:       51 detected, 0 missed
D3-D6 metrics registry:   10 detected, 0 missed
```

## 15. Regression strategy

Use three layers:

1. Fast unit tests for counters, histograms, formulas, JSON and renderer.
2. Short detailed-mode SystemC tests for real router/mesh instrumentation.
3. A bounded end-to-end detailed `noc_soc` report for the final dashboard.

Do not place a long design-space sweep in the normal smoke suite. Register:

- one cheap deterministic dashboard regression;
- one extended detailed measurement;
- one explicit/manual DSE target.

Recommended commands after implementation:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

$CC -dumpfullversion
$CXX --version | head

cmake -S . -B /tmp/cdc_vp_noc_metrics \
  -DCDC_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/cdc_vp_noc_metrics --parallel
ctest --test-dir /tmp/cdc_vp_noc_metrics \
  --output-on-failure -R 'noc_metrics|noc_dashboard'
```

The exact CMake option must follow the repository's current top-level build
contract when D0 starts; do not copy an obsolete option from this roadmap
without checking `CMakeLists.txt`.

## 16. Dashboard sketch

```text
FlooNoC SystemC Virtual Platform - Network Metrics Report
Input -> Measure (detailed SystemC) -> Dashboard -> Hardware Decision

[1] INPUT
Topology       4x4 XY                 [S]
Clock          1.00 GHz              [S]
Managers       CPU, DMA, probe        [S]
Workload       FreeRTOS concurrent    [S]
Window         warmup ... / measure ...

[2] MEASUREMENT
Transactions   ...
Payload bytes  ...
Flits          ...
Packets        ...
Drained        yes

[3] METRIC DASHBOARD
Metric                    Value     Unit       Source
Mean CPU->RAM latency      ...       cycles     [M]
P99 CPU->RAM latency       ...       cycles     [M]
Payload bandwidth          ...       GB/s       [D]
Peak directed-link util    ...       %          [D]
Peak stall ratio           ...       %          [D]
FIFO high-water            ...       entries    [M]
Clock-gating ratio         ...       %          [D]
Area                       unavailable           -
Power                      unavailable           -

[4] HOTSPOTS
Router  Port  Util  Stall  FIFO mean/max  Status
...

[5] HARDWARE DECISION
Config ... PASS/FAIL reason ...
Winner ... only if all constraints and evidence requirements pass

[6] CONCLUSIONS
- observed bottleneck
- contention source
- recommended supported configuration
- unavailable/unmodelled quantities
```

## 17. Definition of v1 complete

Version 1 is complete only when:

- production detailed mesh counters are wired and behaviour-neutral;
- latency percentiles, payload bandwidth, per-link utilisation, stalls and
  FIFO occupancy are measured or derived from measured values;
- a traffic matrix and hotspot table are produced;
- JSON is the source of truth and the terminal dashboard is a renderer;
- the real FreeRTOS workload and controlled synthetic workloads both run;
- a bounded injection-rate sweep identifies a reproducible saturation region;
- DSE refuses unsupported configurations and incomplete results;
- all negative controls are detected for their intended reason;
- the report carries full source/build/workload provenance;
- fast-mode, RTOS, analytic and unavailable values are labelled honestly;
- area/power remain unavailable until calibrated evidence exists.

## 18. Recommended immediate next action

Start with D0 and D1 only:

1. freeze the JSON/metric names and canonical port mapping;
2. expose production router signals needed by the passive observer;
3. instantiate one `router_counters` per router in `floo_mesh`;
4. add mesh snapshot/reset/report APIs;
5. prove behaviour neutrality through existing trace cross-checks;
6. add negative controls before connecting the dashboard renderer.

Do not start the DSE script before D1-D4 are signed off. A visually complete
dashboard built on missing router/link evidence would conceal the main
measurement gap rather than close it.
