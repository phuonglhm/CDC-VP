# RISC-V VP++ Compiler Enablement VP

A self-contained virtual platform for RV32GCV code-generation work: one RISC-V
hart with scalar and RVV 1.0 execution, program/data RAM, and a simulator-only
console and exit protocol. Nothing else — no NoC, no accelerators, no operating
system.

Everything needed to run it is in this directory. There is no installer, no
`LD_LIBRARY_PATH` to set, no source tree to keep around, and no separately
installed simulator binary.

```
./riscv_vpp_compiler_vp --elf examples/scalar_hello/scalar_hello.elf
./riscv_vpp_compiler_vp --elf examples/rvv_vector_add/rvv_vector_add.elf
```

---

## The compiler contract

Build for exactly this, and nothing else:

```
-march=rv32gcv_zvl512b  -mabi=ilp32d
```

| | |
|---|---|
| XLEN | 32 |
| RVV | 1.0 |
| VLEN | 512 bits |
| ELEN | 64 bits |
| `vlenb` | 64 |
| harts | 1 |
| profile | bare-metal / freestanding |

`./riscv_vpp_compiler_vp --version` prints all of it, together with the exact
upstream sources the binary was built from. `vlenb` is read from a hart's own
CSR rather than derived from VLEN, so it is what your code would read.

The same values are in `BUILD_MANIFEST.json` in machine-readable form, along
with the host compiler, the SystemC version, the CDC-VP revision, and the
upstream base revision plus every patch and its hash. Quote the manifest, not a
remembered number, when you report a result.

---

## What is in the box

```
riscv_vpp_compiler_vp      the simulator
libsystemc.so*             its runtime, bundled
configs/                   ready-made configurations
examples/                  the two demonstrations, built and disassembled
sdk/                       startup, linker script, host-I/O shim, Makefile
docs/MEMORY_MAP.md         the memory map and host-I/O contract
licenses/                  everything statically linked, and its licence
BUILD_MANIFEST.json        what this bundle was built from
```

Read `docs/MEMORY_MAP.md` before writing an image. It is the reference for the
address map, the console and exit registers, the load-time refusals and the exit
codes.

---

## Building your own image

The SDK is the same startup code, linker script and Makefile that produced the
bundled examples — not a cleaned-up copy of them. Point it at your cross
toolchain and it works from inside the package:

```
cd sdk
make TOOLDIR=/path/to/riscv-none-elf/bin
```

To start from one of the examples, copy `sdk/examples/<name>/main.c` and edit
it. The three pieces every image needs are:

- `common/crt0.S` — sets `gp` and `sp`, zeroes `.bss` and `.vdata`, installs a
  trap vector, enables the FPU and vector unit, calls `main`, and reports
  `main`'s return value as the exit status;
- `common/link.ld` — links at `0x8000_0000` with separate program headers for
  text, read-only data and writable data (see `docs/MEMORY_MAP.md` §4 for why
  the split matters here);
- `common/host_io.h` — `hio_putchar`, `hio_puts`, `hio_put_u32`,
  `hio_put_hex32`, `hio_exit`, and the CSR readers the examples cross-check
  against the platform's identity registers.

There is no libc. The toolchain ships no vector multilib, so linking one would
pull scalar objects built for a different `-march`; `-ffreestanding -nostdlib
-nostartfiles` avoids the question instead of answering it badly. Hosted
`printf`, syscalls and an OS are a later feature, not an oversight.

`make verify` re-runs the checks the bundled examples had to pass: the ISA
attributes really say `rv32...v...zvl512b`, the scalar example really contains
no vector instruction, the vector example really contains `vsetvli`, a vector
load, `vadd.vv` and a vector store, and every loadable segment really sits
inside the RAM window.

---

## The two demonstrations

**`scalar_hello`** prints

```
Hello from RISC-V VP++ RV32GCV
XLEN=32
hart_id=0
SCALAR HELLO: PASS
```

Every value is read from somewhere and cross-checked against somewhere else
before it is printed — XLEN from the toolchain against XLEN from the platform,
`mhartid` from the hart against the configured hart id, `misa` against every
letter of the frozen ISA string — so the banner is a result rather than four
string literals. It is compiled with auto-vectorization off and verified by
disassembly to contain no vector instruction, so it is real scalar-path
evidence.

**`rvv_vector_add`** prints

```
RVV=1.0
VLEN=512
vlenb=64
elements=1024
VECTOR ADD: PASS
```

It adds two 1024-element arrays three ways — a scalar golden result, a C
version using `<riscv_vector.h>` intrinsics, and a hand-written
`vsetvli`/`vle32.v`/`vadd.vv`/`vse32.v` loop — and compares all three. That is
deliberate: intrinsics wrong and assembly right is a compiler defect, both wrong
the same way is the model, both wrong differently is the program. Making that
distinction is the reason a compiler team is given a simulator, so it is built
into the example rather than left to whoever debugs it.

`RVV=1.0` is measured, not printed from a constant. There is no CSR reporting
the vector-extension version, so the line is earned from behaviour that changed
between 0.x and 1.0: a `vsetvl` naming a reserved `vsew` must set `vtype.vill`
and write `vl = 0` *without* raising an illegal instruction.

The intrinsic loop is also bracketed by a measurement window that declares how
much bus traffic it must produce. If the vector loads and stores do not reach
the bus one element at a time, the run fails with exit code 6 rather than
passing quietly.

---

## Useful options

```
--elf <path>              the image to run
--config <path>           a configuration file (configs/)
--hart-id <n>             what mhartid reports
--ram-size <bytes>        K/KiB, M/MiB, G/GiB accepted
--max-instructions <n>    instruction-retired watchdog
--timeout <time>          simulated-time watchdog; ns/us/ms/s
--wall-timeout <seconds>  wall-clock backstop; 0 disables it
--trace [n]               log the first n bus transactions to stderr
--dump-signature <path>   write [begin_signature, end_signature) as hex words
--print-config            the machine, and what this invocation would do
--version                 the machine, and what this binary was built from
--help                    everything above, with the exit codes
```

The watchdogs are always armed and none has an "unlimited" value: a
non-terminating image must fail deterministically rather than hang your build.
`configs/tight_watchdog.yaml` pulls them in far enough that a runaway image
stops in under a second of wall clock.

There are more of them than the two obvious ones, because the two obvious ones
are checked between slices of simulation and a hart that stops returning to the
simulation kernel is never checked again. An image whose entry point lands on
memory it never wrote does exactly that — it traps, vectors to an `mtvec` its
startup code never set, and faults on the fault. The platform recognises that
from the bus and stops with exit code 4 and a diagnostic naming the address;
`--wall-timeout` is the backstop for anything else. `docs/MEMORY_MAP.md` §5 has
the details.

Every option refuses a value it does not understand rather than falling back to
a default. A simulator that quietly ran something other than what the command
line asked for is worse than no simulator when you are chasing a codegen
difference.

---

## What this model is, and is not

It is a **functional instruction-set model** with loosely-timed TLM. It executes
RV32GCV correctly and every memory access — instruction fetch, scalar load and
store, vector load and store, console and exit MMIO — is a real transaction on
the bus, with no direct pointer into memory and no instruction cache in the way.
The end-of-run report shows the counts, including one instruction fetch per
retired instruction, which is what the absence of those shortcuts looks like
from outside.

It is **not** pipeline- or cycle-accurate, and it is not a named
microarchitecture — not Rocket, BOOM, CV32E40P or VexRiscv. It models no TPU
pipeline timing, no memory bandwidth and no NoC latency. Instruction counts and
functional results from this package are meaningful; cycle counts, bandwidth
figures and latency figures are not, and must not be published from it.

The simulated-time figure in the report exists to make the watchdogs meaningful
and to order events. It is not a performance measurement.

---

## Reuse

The hart in this package is not a throwaway. It is `cdc::cpu::riscv_vp_plusplus`,
the same CPU model the TPU_V3 platform instantiates once per NEO-CORE. Code
generation stays `rv32gcv_zvl512b`/`ilp32d` when those cores are assembled; what
changes is how many harts there are, where memory lives and what else is on the
bus — runtime and firmware concerns, not reasons to fork the compiler backend.
