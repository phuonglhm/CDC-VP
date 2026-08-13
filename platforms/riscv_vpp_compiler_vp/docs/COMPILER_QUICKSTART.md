# Quickstart

From an unpacked bundle to your own RV32GCV image, and what to do when it does
not work.

Nothing here needs an installer, a source tree, `LD_LIBRARY_PATH`, or a
separately installed simulator. The only external thing you need is a RISC-V
cross toolchain, and only for building images — not for running the bundled
ones.

---

## 1. Run the two bundled examples

```bash
./riscv_vpp_compiler_vp --elf examples/scalar_hello/scalar_hello.elf
./riscv_vpp_compiler_vp --elf examples/rvv_vector_add/rvv_vector_add.elf
```

Expect exit 0, and:

```
Hello from RISC-V VP++ RV32GCV        RVV=1.0
XLEN=32                               VLEN=512
hart_id=0                             vlenb=64
SCALAR HELLO: PASS                    VECTOR ADD: PASS
```

Then a traffic report and `status : PASS`.

If these fail, stop and read §6 — nothing you build will work either.

---

## 2. See what machine you have

```bash
./riscv_vpp_compiler_vp --version
```

The frozen contract is `-march=rv32gcv_zvl512b -mabi=ilp32d`. `docs/ISA_ABI_CONTRACT.md`
says what that guarantees and, more importantly, what it does not — read the
"not guaranteed" section before quoting any number from this package.

---

## 3. Build your own image

The SDK is the same startup code, linker script and Makefile that produced the
bundled examples, not a cleaned-up copy of them.

```bash
cd sdk
make TOOLDIR=/path/to/riscv-none-elf/bin
```

That rebuilds both examples in place. To write your own, copy an example:

```bash
cp -r examples/scalar_hello examples/my_test
$EDITOR examples/my_test/main.c
```

then add it to `sdk/Makefile` next to the two that are there, or compile it
directly with the documented commands:

```bash
riscv-none-elf-gcc \
    -march=rv32gcv_zvl512b -mabi=ilp32d \
    -O2 -std=c11 \
    -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-common \
    -Icommon -Iinclude \
    -static -T common/link.ld -Wl,--build-id=none -Wl,--gc-sections \
    -o my_test.elf \
    common/crt0.S common/host_io.c examples/my_test/main.c
```

Run it:

```bash
cd .. && ./riscv_vpp_compiler_vp --elf sdk/my_test.elf
```

### What every image needs

- **`#include "host_io.h"`** for `hio_puts`, `hio_put_u32`, `hio_put_hex32`,
  `hio_exit`, and the CSR readers.
- **`int main(void)`**, whose return value becomes the exit status. Return 0 for
  a pass; the platform prints any other value as `the image reported check N`.
- **Nothing from libc.** There is none. `printf`, `malloc`, `memcmp` and friends
  are not there. `memset` and `memcpy` are provided by `host_io.c`, because the
  compiler emits calls to them whether or not you write any.

### Auto-vectorization

It is on at `-O2` for this `-march`, which is usually what you want. The scalar
example disables it (`-fno-tree-vectorize -fno-tree-slp-vectorize`) because it
exists to prove the scalar path, and `make verify` fails the build if a vector
instruction appears in it.

---

## 4. Checking what your code actually did

```bash
./riscv_vpp_compiler_vp --elf my_test.elf --trace 100
```

logs the first 100 bus transactions to stderr, each labelled as an
executable-segment read, a data access, host I/O, or an unmapped refusal.

The end-of-run report is often enough on its own:

```
TLM traffic (every access; no DMI, no ISS cache)
  executable-segment R          1584 transactions (1584 read, 0 write, 6147 bytes)
  RAM data                       137 transactions (4 read, 133 write, 539 bytes)
  host I/O                        80 transactions (7 read, 73 write, 320 bytes)
  unmapped, refused                0
  DMI requests (refused)           0
run
  instructions  : 1457
  fetches on TLM: 1584  (1.09 per retired instruction)
```

Every memory access your program makes is a real bus transaction — there is no
direct pointer into memory and no instruction cache in the way — so these counts
are the traffic, not a sample of it. `unmapped, refused` above zero means your
program went somewhere it should not have.

`--dump-signature <file>` writes the range between the `begin_signature` and
`end_signature` symbols as one hex word per line, the riscv-arch-test
convention, if you want to diff results against another simulator.

---

## 5. Watchdogs

Every run is bounded. None of the bounds has an "unlimited" value, on purpose: a
non-terminating image must fail your build rather than hang it.

```bash
--max-instructions 100000000     # default
--timeout 1s                     # default; ns/us/ms/s accepted
--wall-timeout 600               # default, in wall-clock seconds
```

`--config configs/tight_watchdog.yaml` pulls them all in far enough that a
runaway image stops in under a second of wall clock. Reach for it the day your
code generation produces an infinite loop.

Exit code 4 means one of them fired, and the message says which. See
`docs/MEMORY_MAP.md` §5 for why there is more than one.

---

## 6. When something goes wrong

The exit code tells you where to look.

| Code | Meaning | Where the problem is |
|---|---|---|
| 0 | the image ran and reported success | — |
| 1 | the image ran and reported failure, or trapped | your program |
| 2 | usage error | the command line or the config file |
| 3 | the image was rejected before loading | how the image was built |
| 4 | a watchdog expired, or the guest stopped making progress | your program, usually a loop |
| 5 | a model or integration defect | this package — please report it |
| 6 | declared bus traffic did not reach the bus | this package — please report it |

Every refusal names the field it refused on. A few common ones:

**"declares the soft-float ABI ... the frozen contract for this package is
ilp32d"** — you built without `-mabi=ilp32d`. The two pass floating-point
arguments in different registers, so the image would run and produce wrong
numbers; it is refused rather than warned about.

**"segment ... is outside the RAM window"** — you did not link with
`-T common/link.ld`, or you edited its origin. RAM is at `0x8000_0000`.

**"overlaps the simulator-only host-I/O window"** — you linked something over
`0x000F_0000`. That window is the console and the exit register, not memory.

**"is an ELF64 image"** — you used a `riscv64-*` toolchain, or omitted
`-march=rv32...`.

**"the guest has taken 1024 consecutive faulting accesses"** — your image
started executing memory that was never written. Usually a wrong entry point, or
a linker script whose first section is not the startup code. Check
`readelf -h` against `readelf -lW`.

**Trapped, `mcause 0x2`** — illegal instruction. If it is a vector instruction,
check that your startup enabled `mstatus.VS`; the shipped `crt0.S` does.

**Trapped, `mcause 0x5` or `0x7`** — load or store access fault: an address
outside both mapped regions. The traffic report's `unmapped, refused` count and
`--trace` will show you which address.

---

## 7. Reporting a result

Include `BUILD_MANIFEST.json`, or at minimum the `build_revision` and
`effective_source` fields from it. The upstream ISS is a base revision *plus* a
patch series, so naming a single revision describes source that was never
compiled.

And repeat the accuracy caveat with any number: this is a functional model.
Instruction counts and results are meaningful. Cycles, bandwidth and latency are
not.
