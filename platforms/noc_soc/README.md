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
./build/platforms/noc_soc/noc_soc --sim-us 200
```

With real firmware, which is what the platform exists to run:

```sh
export PATH=/opt/toolchains/riscv-none-elf/bin:$PATH
make -C fw/dma_riscv clean
make -C fw/dma_riscv EXTRA_CFLAGS=-DDMA_BASE=0x10060000u
./build/platforms/noc_soc/noc_soc --fw fw/dma_riscv/dma_test.elf --sim-us 500
```

It must print `DMA PASS`. Two things about that build line:

- The `DMA_BASE` override is required. `fw/dma_riscv` defaults to the
  `VP_FX1_Full_SoC` address; here DMA0 is at `0x1006_0000` per
  `docs/peripheral_memory_map.md`.
- `make clean` first. The firmware Makefile depends only on its sources, so
  changing `EXTRA_CFLAGS` alone will not retrigger the link and you will silently
  run a stale ELF against the wrong DMA base.

## What is on it

A RISC-V CPU (`cdc::cpu::riscv_vp_cpu`), CLINT, PLIC with all 31 sources wired,
16 MiB RAM, a 64 KiB boot ROM, and the peripheral set from
`docs/peripheral_memory_map.md` at its official addresses: UART x2, I2C x2,
SPI x2, TIMER x2, WDT, PWM, DMA, TRNG, CMU, DMIC, OTP, QSPI (+NOR flash), RTC,
ADC, GPIO.

Run firmware with `--fw <elf>`. Without it the CPU still runs — see the CPU
section below for what that measures and what it does not.

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
  uart1   (3,2)   1     11 cyc    21 ns
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
  CPU pc 0x8000047a, retired 10726 instructions in 330704 ns
  30.832 ns per instruction, fetching from RAM over the mesh
  (measured over the 330704 ns the CPU was retiring; it then idled until 3000704 ns)
```

**About 30 ns per instruction**, every fetch crossing the mesh. It is stable
against `--sim-us` — 29.7 at 200 µs, 30.4 at 1000, 30.8 at 3000 — because the
figure is measured over the window in which the CPU was actually retiring.
Firmware ends in `wfi`, so charging the whole run to its instructions made the
same workload look like 29.7, 93.3 and 279.8 ns depending only on how long the
simulation was left running.

Without `--fw` the platform preloads a `jal x0, 0` spin loop into the first 8 KiB
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

**Synthetic survey traffic stays out of firmware memory.** The survey writes
only inside the final 4 KiB page of RAM (`kSurveyScratch`). It used to write at
`kRamBase` and `kRamBase + 0x100`, which is inside the firmware's `.text`: it
overwrote live instructions, the ISS decoded the debris and trapped, and the
symptom looked convincingly like a store-path bug in the interconnect. It was
not. Keep every synthetic RAM access in that page.

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

RTL-signed against frozen FlooNoC revision `9a6972a`, cycle for cycle: the
routers and their FIFOs, the wormhole arbiters, both chimney directions, the
`NoRoB` ordering rule, and inter-node timing. See
`components/floo_noc_model/docs/STATUS.md` for the evidence and the negative
controls behind each.

Not signed, and not signable: the endpoint transactors that turn a TLM payload
into AXI. They have no RTL counterpart.

Two behaviours that are the frozen configuration's real behaviour rather than
modelling shortcuts:

- **One AXI ID per upstream port**, because `MaxUniqueIds = 1` makes the
  chimney's response metadata a plain in-order FIFO. A port's transactions are
  serialised.
- **`b_transport` spends simulated time** rather than annotating `delay`. A
  caller relying on temporal decoupling will find its quantum consumed.

## Not wired yet

- **ISP0 and VPU0**, reserved in the memory map and unbound here, as in
  `VP_FX1_Full_SoC`.
- **NPU0**, which needs the optional SAURIA build.
- **PMU0**, whose ~20-signal power-sequencing environment is not reproduced.
Not on this list any more: the DMA's descriptor traffic. `fw/dma_riscv` programs
a real transfer and reaches `DMA PASS` over the mesh, so CPU and DMA do contend
for the same links. The measured answer is that it barely matters —
`MaxUniqueIds = 1` gives each manager one transaction in flight, so three
managers cannot congest a 4x4 mesh and the worst observed cost is +9 cycles. A
congestion study needs `MaxUniqueIds > 1` or many more masters.

## Known gaps

- **The firmware run is not automated.** Build and run it by hand, per the
  commands above. There is no CTest that would catch a regression in it, which is
  exactly how the `kSurveyScratch` corruption survived 28 passing component
  tests.
- **Clock gating is not proven.** `noc_interconnect::network_idle()` decides
  from its own bookkeeping, not from mesh state — `floo_mesh` does not export
  router occupancy or lock state upward. See `AI_HANDOFF_CONTEXT.md` section
  13.6b.
- **`b_transport` spends simulated time** rather than annotating `delay`, so a
  caller relying on temporal decoupling will find its quantum consumed.

## Why it is slow

`bus_router` does no work per simulated cycle because it does not simulate
cycles. This advances a clock and evaluates every router in the mesh on every
edge. Idle cycles are skipped — exact, because with no `valid` asserted
anywhere every register holds — but any cycle carrying traffic costs real work.

For a platform that must also boot firmware at speed, the usual answer is two
modes: this one to calibrate, and an approximately-timed model for long runs.
This platform is the calibration reference.
