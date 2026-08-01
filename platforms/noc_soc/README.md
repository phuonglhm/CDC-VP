# noc_soc

The CDC-VP peripheral set on a cycle-accurate FlooNoC interconnect instead of
`bus_router`.

It is a separate platform on purpose. The peripherals and their addresses are
the same as `VP_FX1_Full_SoC` (see `docs/peripheral_memory_map.md`), so the
difference between the two runs is the interconnect and nothing else.

## Build and run

Behind its own option, because a cycle-accurate interconnect is slow:

```sh
export CC=/usr/bin/gcc CXX=/usr/bin/g++ PATH=/usr/bin:/bin:$PATH
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCDC_BUILD_NOC_SOC=ON
cmake --build build --target noc_soc --parallel
./build/platforms/noc_soc/noc_soc --mode survey --sim-us 200
```

With real firmware, which is what the platform exists to run:

```sh
export PATH=/opt/toolchains/riscv-none-elf/bin:$PATH
make -C fw/dma_riscv clean
make -C fw/dma_riscv EXTRA_CFLAGS=-DDMA_BASE=0x10060000u
./build/platforms/noc_soc/noc_soc \
  --mode firmware --fw fw/dma_riscv/dma_test.elf --sim-us 2000
```

It must print `DMA PASS`. These are the **manual** commands. The automated regression
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

- **One transaction in flight per upstream port** — this is the *wrapper's*
  limit, not the RTL's. `noc_interconnect` keeps one waiter and one `port_busy`
  bit per port. The frozen `MaxUniqueIds = 1` branch of `floo_meta_buffer.sv`
  has read and write metadata FIFOs of depth `MaxTxns = 32` and imposes no such
  limit.
- **One AXI ID per upstream port** — but note what `MaxUniqueIds = 1` actually
  constrains. It governs the *downstream reissued* IDs: the chimney reissues all
  non-atomic traffic under one ID and keeps response metadata in a plain
  in-order FIFO with no ID matching, so responses must come back in request
  order. That the wrapper also presents a single *input* ID upstream is wrapper
  policy, not something the parameter forces.
- **`b_transport` spends simulated time** rather than annotating `delay`. A
  caller relying on temporal decoupling will find its quantum consumed.

## Not wired yet

- **ISP0 and VPU0**, reserved in the memory map and unbound here, as in
  `VP_FX1_Full_SoC`.
- **NPU0**, which needs the optional SAURIA build.
- **PMU0**, whose ~20-signal power-sequencing environment is not reproduced.
Not on this list any more: the DMA's descriptor traffic. `fw/dma_riscv` programs
a real transfer and reaches `DMA PASS` over the mesh, so CPU and DMA do contend
for the same links. A pre-A-3 directed run observed **+9 cycles**; the current
synthetic survey can also show no positive delta at this light load because the
wrapper serializes each port. Step 10.3 owns a controlled, scoreboard-driven
contention workload.

Do not read that as "a 4x4 FlooNoC cannot congest". It is a result for *this
wrapper* with three masters on *this* workload, and the dominant reason is the
one-transaction-per-port limit in `noc_interconnect` described above — not the
RTL. Generating real congestion means giving the wrapper concurrent outstanding
transactions, which is a wrapper change, and only then deciding whether
`MaxUniqueIds > 1` is also required. The two are separate decisions.

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
- **`b_transport` spends simulated time** rather than annotating `delay`, so a
  caller relying on temporal decoupling will find its quantum consumed.

## Why it is slow

`bus_router` does no work per simulated cycle because it does not simulate
cycles. This advances a clock and evaluates every router in the mesh on every
edge. Wrapper-idle cycles are skipped only when direct mesh/chimney quiescence
also holds. Any cycle carrying traffic costs real work.

For a platform that must also boot firmware at speed, the usual answer is two
modes: this one to calibrate, and an approximately-timed model for long runs.
This platform is the calibration reference.
