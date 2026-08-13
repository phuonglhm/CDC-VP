# `riscv_vpp_compiler_vp` memory map and host-I/O contract

This is the reference for anyone building an image for this package. It
describes the map **once**; the authoritative machine-readable copy is
`sdk/include/compiler_vp/host_io_map.h`, which the platform, the startup code,
the linker script and the examples all compile against. If this document and
that header ever disagree, the header is right and this document is a defect.

---

## 1. The machine

One architectural RV32GCV hart. Scalar and vector execution are parts of the
**same** hart: they share the program counter, the general-purpose registers,
the vector registers, the CSRs, the privilege and trap state, `mhartid`, and one
TLM memory path. There is no scalar-to-vector MMIO command, no second hart, no
vector doorbell and no independent vector memory master.

| | |
|---|---|
| architecture | `rv32gcv_zvl512b` |
| ABI | `ilp32d` |
| XLEN | 32 |
| RVV | 1.0 |
| VLEN | 512 bits |
| ELEN | 64 bits |
| `vlenb` | 64 |
| vector registers | 32 |
| harts | 1 |
| execution profile | bare-metal / freestanding |

`--version` prints all of it, reading `vlenb` from a constructed hart's own CSR
rather than dividing VLEN by eight, so the number shown is the number firmware
would read.

**What this model is.** A functional instruction-set model with loosely-timed
TLM. It is not pipeline- or cycle-accurate and it is not a named
microarchitecture — not Rocket, BOOM, CV32E40P or VexRiscv. It models no TPU
pipeline timing, no memory bandwidth and no NoC latency, and no such number may
be published from it.

---

## 2. Regions

| Region | Base | Size | Notes |
|---|---|---|---|
| program/data RAM | `0x8000_0000` | 64 MiB by default | base fixed, size set by `--ram-size` |
| host I/O | `0x000F_0000` | 4 KiB | **simulator-only**; see §3 |
| everything else | — | — | unmapped; an access faults |

The RAM base is the TPU_V3 global-RAM base, so an image built for this package
links at an address the full SoC also maps. Only the size is configurable, and
the shipped linker script sizes its region from the same header the platform
compiles against.

**Unmapped is not "reads zero".** The decoder refuses the access with a TLM
address error, and the hart turns that into an access fault whose cause follows
what the access was for — instruction fetch, load, or store/AMO (decision
record D13). An image that runs off its map traps; it does not quietly read
zeros and carry on.

### Refusals at load time

An image is checked against this map before anything is written to memory, and
rejected with exit code 3 and a diagnostic naming the problem:

- not an ELF, truncated, or a segment running past the end of the file;
- ELFCLASS64 — an RV64 image on an RV32 hart;
- a machine other than RISC-V, or big-endian data;
- `e_type` other than `ET_EXEC`: nothing here relocates, so a PIE or a
  relocatable object has no load address;
- a float ABI other than `ilp32d`, read from `e_flags`. A soft-float or
  single-float image passes floating-point arguments in different registers, so
  it would run and produce wrong numbers rather than crash;
- an architecture string that is not `rv32*`, or one guaranteeing a minimum
  vector length **above** 512 bits. A minimum *below* 512 is accepted:
  strip-mined RVV code asks `vsetvli` how many elements it may process, so it is
  VLEN-agnostic upwards;
- a `PT_LOAD` segment outside RAM, overlapping the host-I/O window, or
  overlapping another segment;
- an entry point outside RAM.

A scalar-only architecture string is accepted and reported — checking scalar
code generation is half of what this package is for. A missing
`.riscv.attributes` section is also accepted; the checks that need the string
are skipped and the report says the string was not declared, rather than
implying they passed.

---

## 3. The host-I/O window

**This is not a TPU_V3 architectural peripheral.** It exists so a freestanding
image can print and exit without a libc, a UART model or an `ecall` handler. It
must not appear in full-SoC firmware, and the TPU_V3 address map does not
reserve it. It sits at `0x000F_0000` because that space is unmapped in the
TPU_V3 map: `boot_rom` ends at `0x0001_0000`, `global_control` at
`0x0002_0000`, and the next architectural region is `global_ram` at
`0x8000_0000`.

All registers are 32 bits and must be accessed as aligned words. A byte or
halfword access is refused with an address error — a narrow store to a control
register is almost always a mistake in the image, and answering it as though the
register were memory would leave three quarters of a value behind.

### Block A — exit protocol (`0x000`–`0x00C`)

Bit-for-bit the first four words of the Phase 2 contract
(`fw/TPU_V3_SoC/rvv_smoke/sim_exit.h`), at the same offsets, with the same
trigger rule. Phase 4.5 extends that contract rather than inventing a second
exit address.

| Offset | Address | Access | Meaning |
|---|---|---|---|
| `0x000` | `0x000F_0000` | W | **exit kind**, and the trigger: 0 normal, 1 trapped |
| `0x004` | `0x000F_0004` | W | exit status; 0 is a pass |
| `0x008` | `0x000F_0008` | W | `mcause` on the trap path |
| `0x00C` | `0x000F_000C` | W | `mepc` on the trap path |

Write `EXIT_KIND` **last**. The platform stops the simulation when it sees a
write there, by which point status, cause and `mepc` are already in place.

`0x010`–`0x3FF` are the rest of the Phase 2 contract — fault injection, trap
signatures, D12/D13 conformance, interrupt plumbing. This platform implements
none of them. They read zero and ignore writes rather than faulting, so a Phase 2
image that strays into them fails its own checks instead of dying in a trap that
hides the reason.

### Block B — console (`0x400`–`0x404`)

| Offset | Address | Access | Meaning |
|---|---|---|---|
| `0x400` | `0x000F_0400` | W | low byte is appended to the host console |
| `0x404` | `0x000F_0404` | W | flush the current partial line |

One byte per write, so `puts` costs one TLM transaction per character. That is
slow and it is the point: the gate requires host-I/O MMIO to be observable on
TLM like every other access. The host buffers by line so guest output cannot
interleave mid-line with the platform's own messages; `crt0` flushes before
exiting so a partial line — often the one naming a failure — is not lost.

### Block C — machine identity, read-only (`0x420`–`0x44C`)

| Offset | Address | Value |
|---|---|---|
| `0x420` | `0x000F_0420` | identity, `0x4350_5056` (`"VPPC"`) |
| `0x424` | `0x000F_0424` | host-I/O ABI version, 1 |
| `0x428` | `0x000F_0428` | XLEN |
| `0x42C` | `0x000F_042C` | hart count |
| `0x430` | `0x000F_0430` | hart id, as configured by `--hart-id` |
| `0x434` | `0x000F_0434` | VLEN in bits |
| `0x438` | `0x000F_0438` | ELEN in bits |
| `0x43C` | `0x000F_043C` | `vlenb` |
| `0x440` | `0x000F_0440` | RAM base |
| `0x444` | `0x000F_0444` | RAM size |
| `0x448` | `0x000F_0448` | host-I/O base |
| `0x44C` | `0x000F_044C` | host-I/O size |

These are what the **host** believes it built. Firmware should read them and
compare against what the **hart** reports through `mhartid`, `misa` and
`vlenb`; both shipped examples do. The `vlenb` register is filled in from the
constructed hart's own CSR, not computed from VLEN/8, so the comparison is
between two independent sources rather than a value and itself.

Reading the identity register first is worth the four bytes: it distinguishes
"talking to this platform" from "talking to a probe memory that answers every
read with zero", and every other check is only meaningful once it has answered.

### Block D — measurement window (`0x460`–`0x464`)

| Offset | Address | Access | Meaning |
|---|---|---|---|
| `0x460` | `0x000F_0460` | W | non-zero opens a window with that id; zero closes it |
| `0x464` | `0x000F_0464` | W | minimum RAM **data** accesses the open window must cause |

The gate asks for evidence that vector memory traffic reaches TLM, and "the run
made a lot of transactions" is not that — a scalar loop makes a lot of
transactions too. So the firmware brackets the section it wants measured and
declares, from its own element count, how much traffic that section must
produce. The platform checks the closed window and **fails the run** (exit code
6) if the traffic did not happen.

VP++ issues vector load/store one element at a time, so the expected number is
exact rather than a guess: a loop over `N` elements doing two loads and one
store per element must cause `3N` data accesses. A model that serviced a whole
vector register from one transaction, or from a direct pointer, would move a
sixteenth of that and be caught.

Accesses counted are RAM accesses **outside any loadable executable segment** —
see §4. The ELF load itself is not counted: it uses `transport_dbg`, and folding
it in would let a large image satisfy a small loop's declaration.

---

## 4. How traffic is classified, and how far the claim goes

Every access the hart makes arrives at the decoder: the backend installs no DMI
and no ISS-internal decode or load-store cache, and the decoder refuses
`get_direct_mem_ptr` and counts the attempt. The end-of-run report prints the
refusal count, which is expected to be zero because nothing asks.

Separating **fetch** from **data** is a weaker claim and is labelled as such.
VP++'s combined memory interface carries fetch and data on one socket and marks
neither, so the payload cannot say which it is. The decoder classifies by
address instead: a read inside a loadable executable segment is counted as
fetch, everything else in RAM as data. That is exact for any program that does
not read its own text, and wrong by exactly the number of such reads for one
that does.

This is also why the shipped linker script emits three program headers — R+X
for `.text`, R for `.rodata`, R+W for everything writable — instead of the one
RWX segment a flat bare-metal script would normally produce. With one segment
covering the whole image, every load is inside an executable segment, every data
access is counted as a fetch, and the measurement window sees only the stores.

A useful consequence: with no decode cache and no DMI, the fetch count comes out
at exactly **one per retired instruction**. The report prints the ratio, and a
run where it is not 1.00 means something is servicing instruction reads off the
bus.

---

## 5. Watchdogs

All of them are always armed and none has an "unlimited" value, because a
packaging or CI job must not be able to hang.

| Mechanism | Default | Catches |
|---|---|---|
| `--max-instructions` | 100,000,000 | a program spinning: instructions retire for ever |
| `--timeout` | 1 s simulated (`1000000000` ns) | a program stalled: time advances, nothing retires |
| fault-loop detector | 1024 consecutive refused accesses | a program that has stopped letting the kernel run |
| `--wall-timeout` | 600 s wall clock | everything else |

The first two describe the same amount of work — the ISS cycle time is 10 ns, so
a second of simulated time is 100 million cycles and roughly that many
instructions — so a run that trips one of them is genuinely stuck. *Which* one
it trips is the diagnosis: instructions exhausted with time to spare is a spin,
time exhausted with instructions to spare is a stall.

Simulated time is advanced in slices so both can be checked; the time bound is
exact because the last slice is clipped to land on it, and the instruction bound
is checked at slice boundaries, so a run may retire up to one slice past it
before stopping. The report prints the true count, not the limit.

### Why two bounds are not enough

Both are polled *between* slices, from outside `sc_start()`. A SystemC process
that never yields cannot be preempted, so if the hart stops returning to the
kernel, `sc_start()` never returns and neither bound is ever read again.

That is not an exotic failure. An image whose entry point lands on memory it
never wrote executes a zero word, takes an illegal-instruction trap, and vectors
to `mtvec` — which is still 0, because the startup code that would have set it
never ran. Fetching from 0 faults as well, and the hart loops: it retires
nothing, so the instruction bound cannot advance, and it never reaches a
quantum boundary, so simulated time cannot advance either. One mistyped load
address in a linker script produces it.

What *can* see it is the decoder, which is called on every one of those faulting
accesses. An unbroken run of refused accesses with no successful one between
them is a fault loop — a working program breaks the run with its very next
instruction fetch — so after 1024 of them the decoder throws, which unwinds the
hart's thread. The run ends with exit code 4 and a diagnostic naming the address
and the likely cause. The threshold is fixed, and generous: a program
deliberately probing its address map interleaves successful fetches of its own
probing loop, so it never gets near it.

`--wall-timeout` is the backstop for whatever that does not cover. It is a host
thread, and it is explicitly a **wall-clock** bound: *when* it fires depends on
the machine, so nothing may be concluded from the fact that it fired beyond
"this did not finish". Pass `--wall-timeout 0` to disable it, for an
interactive debugging session where a very long run is intended.

`configs/tight_watchdog.yaml` ships much smaller values for the day code
generation produces an infinite loop.

---

## 6. Exit codes

| Code | Meaning |
|---|---|
| 0 | the image ran and reported success |
| 1 | the image ran and reported failure, or trapped |
| 2 | usage error |
| 3 | the image was rejected before loading |
| 4 | a watchdog expired, or the guest stopped making progress |
| 5 | a model or integration defect (a TLM protocol error) |
| 6 | the image's declared bus traffic did not reach the bus |

They are distinct on purpose. A CI job that only knows "non-zero" cannot tell a
guest that failed its own checks from a simulator that refused the image, and
those two send whoever reads the log to different places.
