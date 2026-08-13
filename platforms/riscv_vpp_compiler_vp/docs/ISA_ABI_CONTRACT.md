# The frozen ISA/ABI contract

What this package guarantees about the machine your code runs on, what it does
not, and how to check each claim yourself rather than take it from this
document.

---

## 1. The contract

```
-march=rv32gcv_zvl512b  -mabi=ilp32d
```

| | | Frozen? |
|---|---|---|
| architecture string | `rv32gcv_zvl512b` | yes |
| ABI | `ilp32d` | yes |
| XLEN | 32 | yes |
| RVV | 1.0 | yes |
| VLEN | 512 bits | yes |
| ELEN | 64 bits | yes |
| `vlenb` | 64 | yes |
| vector registers | 32 | yes |
| harts | 1 in this package | no — later platforms replicate the same hart |
| execution profile | bare-metal / freestanding | no — hosted support is a later, separately gated feature |

"Frozen" means these do not change when the same hart is replicated into a
larger system. Code generation stays `rv32gcv_zvl512b`/`ilp32d`; what changes is
how many harts exist, where memory lives and what else is on the bus. Those are
runtime and firmware concerns, not reasons to fork a compiler backend.

`rv32gcv` expands to `rv32imafdc` + `v`, plus `zicsr` and `zifencei`. `zvl512b`
guarantees a *minimum* vector length of 512 bits; this machine provides exactly
512.

---

## 2. Scalar and vector are one hart

This matters more than it sounds, because the alternative is a common
architecture and would change how you generate code.

Scalar and vector execution here are parts of the **same** architectural hart.
They share:

- the program counter;
- the general-purpose registers;
- the vector registers;
- the CSRs, including `mstatus`, `fcsr`, `vtype`, `vl` and `vstart`;
- privilege and trap state;
- `mhartid`;
- one memory path.

There is no scalar-to-vector MMIO command, no vector doorbell, no second hart
and no independent vector memory master. A vector instruction is an instruction,
sequenced like any other.

---

## 3. What "RVV 1.0" is verified to mean here

The vector extension version is not reported by any CSR, so it cannot simply be
read. The bundled `rvv_vector_add` example earns the claim from behaviour that
differs between 0.x and 1.0:

- a `vsetvl` naming a reserved `vsew` sets `vtype.vill` and writes `vl = 0`, and
  does **not** raise an illegal instruction (v-spec 1.0 §3.4.3);
- `misa.V` is set;
- `vlenb` reads 64, and one strip at `e32,m1` is 16 elements.

The example refuses to print `RVV=1.0` unless all of those hold.

Two further conformance properties were settled during Phase 2 of this project
and are carried by downstream patches recorded in `BUILD_MANIFEST.json`:

- **Index EEW=64 on RV32 is illegal.** All 32 indexed encodings with index
  EEW 64 raise an illegal instruction, and do so before counting a load or
  store, touching the bus, or dirtying `mstatus.VS` (v-spec 1.0 §18.2, §7.3).
- **A failed bus access is an *access* fault, not a page fault**, and its cause
  follows what the access was for: instruction fetch, load, or store/AMO.

---

## 4. What is *not* guaranteed

Read this section before quoting any number from this package.

**Timing.** This is a functional instruction-set model with loosely-timed TLM.
It is not pipeline- or cycle-accurate and it is not a named microarchitecture —
not Rocket, BOOM, CV32E40P or VexRiscv. It models no pipeline, no cache
hierarchy, no memory bandwidth and no interconnect latency. The simulated-time
figure exists to make the watchdogs meaningful and to order events. **Cycle
counts, throughput figures and latency figures from this package are not
measurements of anything.**

Instruction counts and functional results *are* meaningful, and are the right
things to compare between two builds of your compiler.

**Vector memory microarchitecture.** The model issues vector load and store one
element at a time. That is functionally correct and it is what makes the bus
traffic observable, but it is not a statement about how a real implementation
would move the data. Do not infer memory-system behaviour from the transaction
counts.

**Anything outside the two mapped regions.** There is no CLINT, no PLIC, no MMU,
no cache, no NoC and no accelerator. `satp` is Bare. An access outside RAM and
the host-I/O window faults.

**Hosted execution.** No libc, no syscalls, no OS. `-ffreestanding -nostdlib
-nostartfiles` is the documented profile for this handoff.

---

## 5. Checking it yourself

Do not take this document's word for any of it.

```bash
./riscv_vpp_compiler_vp --version
```

reports the ISA, ABI, XLEN, RVV version, VLEN, ELEN, `vlenb`, vector-register
count and hart count, plus the accuracy disclaimer, the host compiler, the
SystemC version, the CDC-VP revision, the upstream VP++ base revision and every
patch applied to it with its content hash. `vlenb` is read from a constructed
hart's own CSR rather than divided out of VLEN, so it is the value your code
would read.

`BUILD_MANIFEST.json` carries the same facts in machine-readable form. **Quote
the manifest, not a remembered number, when you report a result.**

The two bundled examples cross-check the machine against itself before printing
anything: the toolchain's `__riscv_xlen` against the platform's XLEN, `mhartid`
against the configured hart id, `misa` against every letter of the ISA string,
and the hart's `vlenb` against the host's view of it. A disagreement fails the
run rather than printing a plausible banner assembled from two different
machines.

For your own images, `sdk/common/host_io.h` exposes the same CSR readers
(`hio_csr_mhartid`, `hio_csr_misa`, `hio_csr_vlenb`) and
`sdk/include/compiler_vp/host_io_map.h` the identity registers to compare them
against.

---

## 6. If the contract has to change

It should not, and the package is built so that a drift is caught rather than
absorbed:

- an image declaring a float ABI other than `ilp32d` is refused at load time,
  because it would run and produce wrong numbers rather than crash;
- an image declaring a minimum vector length above 512 bits is refused, because
  this hart cannot honour the guarantee it was given;
- an image that is not `rv32*`, not ELF32, not RISC-V or not `ET_EXEC` is
  refused;
- the frozen values are compiled into the binary, written into the manifest, and
  asserted by both examples, so a change that touched only one of them fails.

A real change to the contract is a project decision, not a build flag. It
invalidates code generation, the bundled examples and any result previously
reported from this package.
