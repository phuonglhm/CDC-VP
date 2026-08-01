# noc_soc

The CDC-VP peripheral set on a FlooNoC interconnect instead of `bus_router`,
with a detailed cycle-stepped backend and a calibrated fast backend.

It is a separate platform on purpose. The peripherals and their addresses are
the same as `VP_FX1_Full_SoC` (see `docs/peripheral_memory_map.md`), so the
difference between the two runs is the interconnect and nothing else.

## Build and run

Behind its own option, because a cycle-accurate interconnect is slow:

```sh
export CC=/usr/bin/gcc CXX=/usr/bin/g++ PATH=/usr/bin:/bin:$PATH
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCDC_BUILD_NOC_SOC=ON
cmake --build build --target noc_soc --parallel
./build/platforms/noc_soc/noc_soc \
  --mode survey --noc-timing detailed --sim-us 200
```

With real firmware, which is what the platform exists to run:

```sh
export PATH=/opt/toolchains/riscv-none-elf/bin:$PATH
make -C fw/dma_riscv clean
make -C fw/dma_riscv EXTRA_CFLAGS=-DDMA_BASE=0x10060000u
./build/platforms/noc_soc/noc_soc \
  --mode firmware --noc-timing detailed \
  --fw fw/dma_riscv/dma_test.elf --sim-us 2000
```

For long runs select the Step 11 backend:

```sh
./build/platforms/noc_soc/noc_soc \
  --mode firmware --noc-timing fast \
  --fw fw/dma_riscv/dma_test.elf --sim-us 2000
```

Both must print `DMA PASS`. `--noc-timing` defaults to `detailed` for command
compatibility, but scripts should name it explicitly. These are the **manual**
commands. The automated regression
(`platforms/noc_soc/tests/run_firmware_regression.sh`) does not use them: it
builds the firmware in a private copy of its sources under its own log
directory, so it needs no in-source `make clean`, leaves the working tree
untouched, and two runs cannot collide. Build by hand like this only when
driving the platform yourself.

Two things about that build line:

- The `DMA_BASE` override is required. `fw/dma_riscv` defaults to the
  `VP_FX1_Full_SoC` address; here DMA0 is at `0x1006_0000` per
  `docs/peripheral_memory_map.md`.
- `make clean` first. The firmware Makefile depends only on its sources, so
  changing `EXTRA_CFLAGS` alone will not retrigger the link and you will silently
  run a stale ELF against the wrong DMA base. The automated regression sidesteps
  this by building in a fresh copy.
- `--sim-us 2000`, not 500. The workload needs about 563 µs of modelled time
  since the wrapper began spending the caller's annotated delay; the margin is
  there to stop a hang, not to assert a performance figure.

## Portable package

Build the ignored `out/noc_soc` bundle with:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head

cmake --build build --target noc_soc_package --parallel
```

The package contains the executable, `configs/`, the SystemC shared-library
chain and all applicable licence/provenance records. The executable has exactly
`RPATH=$ORIGIN`; it must not retain `/opt/systemc` or a build-directory
fallback. A self-contained smoke run is:

```bash
cd /tmp
env -u LD_LIBRARY_PATH \
  /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/out/noc_soc/noc_soc \
  -c /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/out/noc_soc/configs/default.yaml \
  --mode survey --sim-us 20
```

Step 10.4 also runs the full firmware regression against this packaged binary,
not only the in-tree executable.

The reproducible distribution sign-off is one command:

```bash
./platforms/noc_soc/tests/run_packaging_regression.sh
```

The same runner is registered as `noc_soc_packaging_regression` in CTest and
labelled `packaging;distribution`. It performs a fresh Release build and clean
install, builds the standalone installed consumer, validates the FlooNoC header
manifest/SPDX identifiers, byte-compares the development and binary-package
licence/provenance files, asserts exact `RPATH=$ORIGIN`, proves with `ldd` that
SystemC resolves beside the executable, runs the packaged config with
`LD_LIBRARY_PATH` unset, and invokes the full firmware/survey regression against
the packaged binary.

Each run overrides `CDC_PACKAGE_ROOT` with its private temporary directory.
The default remains `out/`, so manual `noc_soc_package` usage is unchanged, but
parallel CI/developer jobs cannot delete or overwrite one another's artifact.
Set `PACKAGING_KEEP_ARTIFACTS=1` when the successful evidence tree should be
retained.

## What is on it

A RISC-V CPU (`cdc::cpu::riscv_vp_cpu`), CLINT, PLIC with all 31 sources wired,
16 MiB RAM, a 64 KiB boot ROM, and the peripheral set from
`docs/peripheral_memory_map.md` at its official addresses: UART x2, I2C x2,
SPI x2, TIMER x2, WDT, PWM, DMA, TRNG, CMU, DMIC, OTP, QSPI (+NOR flash), RTC,
ADC, GPIO.

`--mode` is mandatory. `survey` rejects `--fw`; `firmware` requires
`--fw <elf>`. The two modes deliberately have mutually exclusive ownership:

- survey mode owns the synthetic RAM scratch page, peripheral walk and DMA0
  experiment;
- firmware mode gives RAM contents and DMA0 to firmware and emits no synthetic
  TLM transaction from the probe port.

`--noc-timing detailed|fast` is independent of traffic ownership:

- `detailed` drives the signed SystemC chimney/mesh blocks cycle by cycle and is
  the calibration reference;
- `fast` preserves functional target effects and annotates a no-contention
  placement/burst estimate. It does not model contention, router back-pressure,
  locks, FIFO occupancy or clock-gating activity. It requires downstream
  targets to annotate their latency; a target that calls `wait()` inside
  `b_transport` is not compatible with this LT path.

## Floorplan

This is the part a flat bus has no equivalent for. On `bus_router` every
peripheral is equidistant; here placement decides latency, so it is a design
input rather than an implementation detail.

```text
       x=0            x=1            x=2            x=3
 y=3   dma *          cmu0 dmic0     rtc0 adc0      probe *
                                     gpio0
 y=2   trng0          timer1         i2c1 spi1      uart1
 y=1   ram            dma0-regs      wdt0 pwm0      timer0
 y=0   cpu *          uart0 clint    i2c0 spi0      bootrom plic
                                                    otp0 qspi0

 * = an AXI manager port, not just a target
```

**No target may share a node with a manager.** `floo_router` defaults to
`NoLoopback = 1`, which ties the Eject-input to Eject-output crossbar leg to
zero: a flit addressed to the node that injected it can never be delivered and
wedges that port for good. The platform hit this the first time the boot ROM
was placed on the CPU's node — it simply hung. `noc_interconnect` now refuses
such a placement at construction with a message rather than hanging.

CLINT sits next to the CPU because every trap entry touches it. RAM is one hop
from the CPU because it carries the fetch traffic. The boot ROM and the secure
blocks share the far corner of the CPU's row.

**Three managers share the mesh**: the CPU, the DMA, and the latency probe.
Each is on its own node, so their traffic shares links — which is the
experiment this platform exists for and something a flat bus cannot show.

## What it measures

Each peripheral gets one register read from the probe port at (3,3):

```text
  block   node    hops  network   total
  uart1   (3,2)   1     10 cyc    20 ns
  timer0  (3,1)   2     15 cyc    15 ns
  wdt0    (2,1)   3     19 cyc    19 ns
  i2c0    (2,0)   4     23 cyc    23 ns
  uart0   (1,0)   5     26 cyc    36 ns
```

The network column is a straight line in hop count — about **4 cycles per hop**
plus a fixed cost. A round trip crosses each hop twice, so that is two cycles
per hop per direction: one for the router's input spill register and one for
its output spill register, both `Depth = 2`, which is what every generated
FlooNoC router has.

With the CPU fetching concurrently the numbers pick up a cycle or two of jitter
against the clean line. That is real contention for shared links, not noise.

Read the two columns separately. `i2c0` and `spi0` share node (2,0) and cost
the network the same, but their totals are 23 ns and 43 ns because the SPI
model is slower internally. **Only the network column moves when you change the
floorplan.**

## The CPU

With firmware, this is the number the platform exists to produce:

```text
  CPU pc 0x8000047a, retired 10726 instructions in 570683 ns
  53.2056 ns per instruction, fetching from RAM over the mesh
  (measured over the 570683 ns the CPU was retiring; it then idled until 2000683 ns)
```

**About 53.2 ns per instruction**, every fetch crossing the mesh, measured over
the window in which the CPU was actually retiring. Firmware ends in `wfi`, so
charging the whole run to its instructions would make the same workload look
slower the longer the simulation is left running.

This is the post-A-3 measurement from 2026-07-31: `noc_interconnect` drives the
timed per-node chimneys through AXI AW/W/AR/B/R handshakes. The earlier
52.506 ns figure used the abstract endpoint path.

**This figure was 30.8 ns until 2026-07-30, and the change is a correction, not
a regression.** The same 10,726 instructions now take 563 µs of modelled time
instead of 331 µs, because the wrapper stopped discarding the caller's annotated
`delay`. A temporally decoupled ISS runs ahead and passes the time it has
already consumed; the old wrapper threw that away, so each transaction began
earlier than the CPU believed and the CPU's own execution time was never spent
at all. The old number measured the network and nothing else. The new one
includes both.

Anything quoting ~30 ns per instruction predates that fix, including the
calibration baseline in the fast-mode plan.

Step 11 ran the same Release binary, ELF and 2 ms firmware window on the same
AlmaLinux host:

```text
timing     host wall   retired   modeled active time   ns/instruction
detailed   8.12 s      10726     570 us                53.1419
fast       0.08 s      10726     570 us                53.1419
```

That is about **101x** in this one measurement. It is not a portable performance
guarantee: host load, compiler, build type and measurement window all matter.
The modeled-time agreement is the accuracy result; host wall time is the
speed result. Fast firmware also passes in an assert-enabled build, proving its
nondecreasing delay annotation satisfies the Bremen quantum keeper.

In survey mode the platform preloads a `jal x0, 0` spin loop into the first 8 KiB
of both RAM and the boot ROM, so the CPU runs from `0x80000000` and reports about
**21 ns per instruction**. That is deliberately lower than the firmware figure:
a four-byte loop is pure fetch, while real firmware also loads, stores, and
touches peripherals. Use the no-firmware number to compare floorplans, not to
quote a CPU cost.

The preload happens in the constructor rather than `start_of_simulation`, because
the ISS may fetch on the first delta and a CPU that reaches zeroed RAM first
takes a trap it never recovers from. If execution ever does leave mapped RAM the
report says so explicitly rather than printing a per-instruction figure that
would be measuring a trap storm.

The boot ROM image and any ELF are loaded backdoor, straight into the memory
model. That deliberately does not cross the NoC: a firmware image is not traffic
the design would ever carry, and charging it to the interconnect would corrupt
every number above.

**Synthetic survey traffic never runs in firmware mode.** The firmware report
must show zero synthetic transactions, zero synthetic RAM writes and zero
synthetic DMA-register writes. Survey mode keeps its RAM accesses inside the
final 4 KiB page (`[0x80ff_f000, 0x8100_0000)`), which is an enforced reserved
region rather than a convention: every little-endian RISC-V ELF32 `PT_LOAD`
range is checked using `p_memsz` before simulation starts, and an overlap is
rejected with both the exact ELF range and reserved range. ELF64 is rejected
because this platform's CPU is RV32.

The survey used to write at `kRamBase` and `kRamBase + 0x100`, inside live
firmware `.text`. The ISS decoded the overwritten instructions and trapped,
making an ownership bug look like a NoC store-path defect. Explicit modes remove
that interference path; the reserved-page check prevents a future ELF from
silently claiming survey memory.

## Reading the report honestly

- A peripheral that refuses the probed register is marked, and its latency is
  still reported: a refusal travels the network both ways just like data, so
  the measurement is valid. It is the register choice that was wrong.
- Register offsets and access widths are per IP because the models disagree.
  `spi_tlm` rejects anything wider than two bytes and `trng_tlm`'s first valid
  register is at `0x100`. The NoC carries whichever `AxSIZE` an access needs,
  so a narrow read stays narrow all the way to the target — it is not padded to
  the bus width.
- The first access is a warm-up and is not measured; the mesh holds its reset
  for a few cycles after time zero.

## Accuracy

Twelve cross-check runners sign the selected timing blocks against frozen
FlooNoC revision `9a6972a`, cycle for cycle: route selection, FIFO, arbiter,
router, AXI sizing, chimney request/response content and timing, `NoRoB`,
manager-side response unpacking, and inter-node mesh timing. See
`components/floo_noc_model/docs/STATUS.md` for the evidence and the negative
controls behind each.

**Read those as isolated proofs, not as a signed platform.** Since A-3 this
platform runs `noc_interconnect` over the complete signal-driven `axi_noc` and
drives timed chimney AW/W/AR/B/R boundaries cycle by cycle. Every selected
hardware block is signed individually, but the TLM adapters and their
composition have no monolithic RTL counterpart. Step 10.3 covers that unsigned
layer with bounded three-manager scoreboarding, watchdogs and mutation controls.

Three behaviours to keep straight, because two of them are often misattributed:

- **Bounded concurrent calls per upstream port** — Step 10.5 gives
  `noc_interconnect` per-transaction request slots and separate FIFO B/R
  completion owners. The default and hard bound are 32, matching the frozen
  metadata depth; a platform may configure a smaller bound.
- **One AXI ID per upstream port** — but note what `MaxUniqueIds = 1` actually
  constrains. It governs the *downstream reissued* IDs: the chimney reissues all
  non-atomic traffic under one ID and keeps response metadata in a plain
  in-order FIFO with no ID matching, so responses must come back in request
  order. That the wrapper also presents a single *input* ID upstream is wrapper
  policy, not something the parameter forces.
- **Timing contract depends on the selected backend.** Detailed
  `b_transport` spends simulated time and returns zero delay. Fast preserves
  the incoming annotation and adds its estimate without waiting.

## Not wired yet

- **ISP0 and VPU0**, reserved in the memory map and unbound here, as in
  `VP_FX1_Full_SoC`.
- **NPU0**, which needs the optional SAURIA build.
- **PMU0**, whose ~20-signal power-sequencing environment is not reproduced.
Not on this list any more: the DMA's descriptor traffic. `fw/dma_riscv` programs
a real transfer and reaches `DMA PASS` over the mesh, so CPU and DMA do contend
for the same links. A pre-A-3 directed run observed **+9 cycles**; the current
synthetic survey can also show no positive delta at this light load because its
CPU, DMA and probe each make blocking calls from one process and therefore do
not saturate Step 10.5's same-port capacity. Step 10.3 owns a controlled,
scoreboard-driven cross-port contention workload; the component's
`test_noc_interconnect_concurrency` owns same-port saturation.

Do not read that as "a 4x4 FlooNoC cannot congest". It is a result for *this
wrapper with three masters on *this* workload, not the RTL's capacity.
Step 10.5 deliberately retains `MaxUniqueIds = 1`: no current SoC requirement
needs the unverified downstream `id_queue`/out-of-order matching branch.

## Known gaps

- ~~The firmware run is not automated.~~ It is, as of 2026-07-30:
  `tests/run_firmware_regression.sh`, registered as
  `noc_soc_firmware_regression` under `CDC_BUILD_TESTS`. It runs both explicit
  modes, proves zero synthetic ownership in firmware mode, checks invalid mode
  combinations, and relocates a real ELF into the reserved page to verify the
  exact conflicting `PT_LOAD` range is rejected before the internal SoC is
  constructed.
- ~~Clock gating is not proven.~~ Closed by Step 10.3: the gate requires both
  wrapper idle and direct mesh/chimney quiescence, with mutation controls for
  output-FIFO occupancy, packet locks and the two-predicate decision.
- Detailed mode still intentionally spends time, so Bremen's assert-enabled
  quantum keeper rejects that backend's decreasing local annotation at
  `mem.h:65`. Fast mode is the supported temporal-decoupling path and passes
  the same assert-enabled firmware workload.
- Fast timing is calibrated for no contention only. It must not be used to
  answer congestion, back-pressure, router-lock or clock-gating questions.

## Why it is slow

`bus_router` does no work per simulated cycle because it does not simulate
cycles. This advances a clock and evaluates every router in the mesh on every
edge. Wrapper-idle cycles are skipped only when direct mesh/chimney quiescence
also holds. Any cycle carrying traffic costs real work.

Step 11 now provides both modes: detailed for calibration and interconnect
analysis, fast for long firmware runs. `test_noc_interconnect_fast` keeps their
no-contention latency relationship within a hard one-cycle tolerance.
