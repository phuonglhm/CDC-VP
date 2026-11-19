# FX1 NPU (SAURIA v4.7): driver, jobs and fixtures

This guide is for firmware and software engineers who run work on the NPU of
the FX1 VP. It covers what is delivered, how to run it, the job format the
firmware executes, the bare-metal driver and the limits. The register-level ABI
(every offset, bit and behaviour of the model wrapper) is in
[register_abi.md](register_abi.md).

Paths below are relative to the SDK repository root:
`sw/bootloader/sources/VP_FX1_V3.0` is written `sources/`,
`sw/bootloader/test/VP_FX1_V3.0` is written `test/`.

## What is delivered

| What | Where |
|---|---|
| The NPU model (SAURIA v4.7 drop: native top + HasNpuTop) | inside `vp/bin/VP_FX1_V3.0/fx1_soc`; `fx1_soc --version` names the drop (`NPU model manifest sha256 ...`) |
| Register map, VP ABI 1.0 | `sources/include/fx1_npu/fx1_npu_regs.h` |
| Reference bare-metal driver (C11, no OS) | `sources/drivers/npu/fx1_npu.[ch]`, instruction builders `fx1_npu_build.c` |
| Job format and its executor | `sources/drivers/npu/fx1_npu_job.[ch]`, `fx1_npu_job_run.c` |
| FreeRTOS owner task and `npu` commands | `sources/app/cmd_npu.c` |
| C oracle and the `npu selftest` cases | `sources/npu_selftest/` |
| Fixture generator (host tool) | `sources/npu_jobs/npu_job_gen.c` (`make npu-jobs`) |
| Launcher (Python standard library only) | `test/npu_run.py`, driven by `test/run_vp.sh npu` |
| Job fixtures with goldens | `test/npu/<name>/` |

Platform facts:

- MMIO aperture `0x1300_0000`, 1 MiB (`FX1_NPU_VP_BASE`). This is the VP
  integration address, not the silicon one; use the macro.
- PLIC source 4 (`FX1_IRQ_NPU`), level.
- The NPU's DMA reaches DDR through the bus write guard, so CPU LR/SC and AMOs
  stay correct against it.
- `fx1_soc --no-npu` removes it: the aperture answers with a bus error, source
  4 stays low, and every `npu` command reports that there is no NPU.
- The model reads configuration knobs from the environment (`HAS_*`,
  `SAURIA_*`, `NPU_*` and others). Every `run_vp.sh npu*` mode removes them, so
  the drop's defaults are used. The banner then reports `env=none`, and the
  launcher counts a run only with that banner.

## Running

```bash
T=sw/bootloader/test/VP_FX1_V3.0
$T/run_vp.sh npu-selftest            # npu info, npu selftest, npu status: NPU SELFTEST PASS
$T/run_vp.sh npu list                # the fixtures
$T/run_vp.sh npu chain               # one fixture: NPU chain PASS: as declared (OK)
$T/run_vp.sh npu all                 # every fixture: NPU FIXTURES PASS (9/9, ...)
$T/run_vp.sh npu chain interactive 5000   # job loaded, console on 127.0.0.1:5000
$T/fx1_console.sh 5000               # second terminal: npu info, npu run, npu wait, npu status, poweroff
```

`npu-selftest` runs three cases and compares each with the C oracle:

- the GEMM bank, K=32;
- a rich chain of five instructions (conv 3x3, conv 1x1 + residual, MAX_POOL,
  AVG_POOL, ADD);
- the neo_lite register file (MAX_POOL, RESIDUAL).

`run_vp.sh npu NAME` hands the fixture to `npu_run.py`, which does five things:

1. loads `job.bin` and `window.bin` with `fx1_soc --load`;
2. types `npu info`, `npu run`, `npu wait`, `npu status` and `poweroff`;
3. dumps the result block and every output tensor;
4. compares the outputs with the goldens, byte for byte;
5. writes `summary.txt` and `summary.json` into
   `sw/bootloader/build/VP_FX1_V3.0/npu/<name>/` (or `--out DIR`).

The summary holds the outcome, the failing operation and the error registers,
the operation counts and the model cycles. It also lists the simulated and wall
time and the inputs' SHA-256.

Exit codes:

| Code | Meaning |
|---|---|
| 0 | the declared outcome, with every output equal to its golden |
| 1 | another outcome, or an output differs |
| 2 | bad arguments or fixture |
| 3 | the simulator failed or wrote no result block |

A directory with a `fixture.json` may be given instead of a name. For example,
the output of `make npu-jobs`:

```bash
$T/run_vp.sh npu sw/bootloader/build/VP_FX1_V3.0/npu_jobs/mixed
```

The output directory belongs to the launcher. It refuses a directory that
overlaps the fixture or the binaries. It also refuses a directory that is not
empty and was not made by a previous run. In a directory it made itself, it
replaces only its own files, and never writes through a link: a link at one of
its file names is removed (its target is not touched), and a marker that is a
link is refused (exit 2). `--print-command` prints the exact `fx1_soc`
command line of one fixture with its clean environment and runs nothing: it
is not a verdict. `npu all` counts a fixture only from the `summary.json` of
its own run, so `npu all --print-command` is refused (exit 2).

## The CLI's `npu` commands

One FreeRTOS task, `npu`, owns the NPU and is the only caller of the driver.
It runs at priority 2, the same as the CLI, with time slicing. The commands
post a request to it; the interrupt handler only acknowledges and wakes the
task.

| Command | Behaviour |
|---|---|
| `npu info` | Identity (VP_ID, ABI, model hash), features, queue depth, window limit, arenas, job and result addresses |
| `npu status` | The task's state (idle, running with job, operation and time, closed), VP_STATUS, ACCEPTED, RETIRED, QUEUE_FREE, interrupts, the last job's outcome. Answers while a job runs |
| `npu selftest` | The three C-oracle cases in the small arena from +3 MiB. A check |
| `npu run` | The job loaded at `FX1_NPU_JOB_BASE`: copied, checked (an invalid job never reaches the NPU), input CRCs checked, operations run one at a time, result block written at `FX1_NPU_RESULT_BASE`. Returns at once |
| `npu wait [MS]` | Waits until the job has ended, then prints its summary as a check (only OK passes). With MS it gives up waiting after MS ms; this is not a failure, and the job goes on |
| `npu wait start [MS]` | Waits until the job posted last has passed its checks and the NPU has accepted its first operation, so what follows in a script runs while the NPU executes. Not a check |
| `npu reset` | Recovery (ABORT, every engine idle), then a soft reset and a reopen. A running job is stopped first and ends ABORTED. A check |

A script must wait for the job's outcome (`npu wait` without a limit) before
`poweroff`.

## Where things live

| What | Address | Written by |
|---|---|---|
| Job image (header, tensor table, op table) | `FX1_NPU_JOB_BASE` `0x8065_0000`, up to 64 KiB | host (`fx1_soc --load`) |
| Result block, 256 bytes, magic `FXNR` written last | `FX1_NPU_RESULT_BASE` `0x8066_0000` | NPU task |
| Ownership lock word | `FX1_NPU_LOCK_BASE` `0x8066_1000` | `fx1_npu_claim` / `fx1_npu_release` |
| Tensors | the job's window: arena base + `window_offset`, `window_size` bytes | host (INPUT, CONST), NPU (OUTPUT, SCRATCH) |
| Small arena | `0x8090_0000`, 4 MiB; `npu selftest` writes it from +3 MiB | |
| Large arena | `0x8E00_0000`, 32 MiB | |

`fx1_memory_map.h` asserts at compile time that these areas stay apart from
the firmware image, the stacks, the ISP job and stream areas, and each other.

The self-test range is an ordinary part of the small arena, not a protected
one. If a job's window overlaps +3 MiB, run `npu selftest` before the host
loads the job's tensors, or load them again afterwards. INPUT_CRC detects
overwritten inputs; it does not preserve them.

## Job format, version 1 (`drivers/npu/fx1_npu_job.h`)

A job is a list of operations on tensors. The host never sends register
writes: the firmware builds every NPU instruction itself, with the driver's
builders, from the operation and the tensors it names, after checking them.
All fields are little-endian.

**Header (128 bytes)**

- `magic` "FXNJ", `version` 1, `header_bytes` 128.
- `total_bytes`: header + tables. `job_crc`: the CRC-32 of those bytes,
  computed with this word read as 0.
- `abi_major` 1.
- `features`: the FEATURE bits the job needs. The bits of its operation kinds
  are added automatically.
- `model_hash`: the `VP_MODEL_HASH` the goldens were made with; 0 accepts any
  model.
- `arena` (0 small, 1 large), `window_offset` and `window_size` (both 4 KiB
  multiples).
- Table counts and offsets: the tensor table starts at byte 128, and the op
  table follows it.
- `timeout_ms`: 1..60000 ms of simulated time, the **execution budget**. It
  runs from the first push or start to the last completion. No operation is
  pushed or started once it has passed. The firmware's input CRCs and canaries
  before the run, and its output CRCs after it, are not counted (they take
  about 0.1 s of simulated time per 64 KiB on the hart).
- `name`: up to 31 characters from `[A-Za-z0-9_.-]`.

**Tensor (32 bytes), at most 32**

- `offset`, `bytes`: the tensor's place in the window.
- `role`:
  - INPUT: loaded by the host; its CRC is checked before the run.
  - CONST: weights, bias, requant tables; checked like INPUT.
  - OUTPUT: written by one operation; its CRC is compared after the run.
  - SCRATCH: written by one operation and read by later ones; not compared.
- `dtype`: int8 or int32.
- `layout` and `rank`: FLAT `[n]`, CHW `[c][h][w]`, MATRIX `[rows][cols]`,
  CONV_W `[cin][kh][kw][cout]`.
- `dims[4]`.
- `crc`: the CRC-32 of the bytes as loaded (INPUT, CONST), or the expected CRC
  (OUTPUT).

**Operation (64 bytes), at most 32**

Each operation has a `kind`, eight tensor slots `t[]` (0xFF means none) and
twelve parameters `p[]`.

| Kind | Tensors | Parameters |
|---|---|---|
| GEMM_BANK | a int8 [32][K], w int8 [K][32], c int32 [32][32] | K (1..992) |
| CONV (GEMM_FUSED) | in CHW, w CONV_W, bias / scale / shift int32 [cout], skip (optional) CHW, out CHW | kh, kw, stride, pad, act (0 none, 1 ReLU), zp_out |
| MAX_POOL | in CHW, out CHW | k, pad, stride |
| AVG_POOL | in CHW, out CHW | k, pad (0), stride, so, sho |
| ADD | a, b, out: int8, same size, any layout | zp_a zp_b zp_o sa sha sb shb so sho |
| NEO_MAX_POOL | as MAX_POOL, pad 0, through the neo_lite register file | as MAX_POOL |
| NEO_RESIDUAL | as ADD, through the neo_lite register file (n a multiple of 32, expressible as RESIDUAL) | as ADD |

The arithmetic (requant, rounding, clamping) is that of the model drop's
reference. The C oracle (`npu_selftest/npu_oracle.c`) implements it and was
cross-checked against the drop's reference before delivery.

## What the firmware checks, and in what order

1. **The image, before any NPU access** (`fx1_npu_job_check`). A failure gives
   BAD_JOB, with the reason and the index of the failing field, tensor or op.
   - The header: magic, version, sizes, CRC, ABI, reserved words 0, name,
     timeout, arena and window.
   - Every tensor:
     - role, dtype and layout;
     - dims × element size equal to `bytes`;
     - 4-byte aligned and inside the window;
     - no two tensors overlap.
   - Every operation:
     - its kind, and the slots it uses;
     - every tensor of the right role, dtype and shape for its geometry
       (output sizes are computed, never trusted);
     - every source written before it is read;
     - every OUTPUT and SCRATCH written by exactly one operation;
     - the instruction can be built.
2. **The NPU's features, model hash and window limit.** A missing one gives
   UNSUPPORTED.
3. **The window.** The firmware sets the rich window, then checks the CRC of
   every INPUT and CONST tensor. A mismatch gives INPUT_CRC, and the NPU is
   not started. It then fills OUTPUT and SCRATCH with `0xA5`, so a write that
   never happens cannot pass.
4. **The operations, in order and one at a time.** Each one is pushed, or
   started, only once the previous one has completed. This keeps the model
   counters exact per operation and lets a failure name its operation.
   Errors:
   - a refused instruction gives REFUSED;
   - an instruction that ran and failed gives FAILED, with the error
     registers;
   - an expired execution budget gives TIMEOUT;
   - `npu reset` gives ABORTED.

   After TIMEOUT and ABORTED the job is recovered (ABORT, every engine idle).
   A recovery that fails gives UNAVAILABLE, and the NPU stays closed until
   `npu reset` finds it idle.
5. **The outputs.** The CRC of every OUTPUT is compared with the expected one;
   a difference gives MISMATCH.

The firmware copies the job image into its own RAM first, so the host cannot
change it between the check and the run. The buffers belong to the NPU until
the job's outcome is known. After TIMEOUT or ABORTED, they are released only
once the recovery has succeeded.

## Result block (`fx1_npu_job_result`, 64 words)

- `magic`, `version`, `status`, `detail`.
- `op_index`: the failing operation.
- `driver_status`, then the error registers at the failure: STATUS,
  LAST_ERROR, IRQ_STATUS, RICH_STATUS, VP_ERROR, VP_ERROR_INFO, VP_STATUS.
- The operations done (rich, neo, GEMM bank), and the ACCEPTED and RETIRED
  deltas.
- Model execution cycles, MAC operations, GEMM-bank cycles.
- `ticks`: simulated time from the first submit to the last completion, in
  50 ns `mtime` ticks. It covers the NPU's execution and DMA, plus the
  firmware's work between operations and its scheduling. The model's own time
  is the execution cycles, 1.25 ns each.
- Interrupts and stale interrupts.
- Outputs compared and matching, the job CRC, the model hash, and the CRC
  found for each tensor.

None of these counters is FX1 end-to-end throughput.

## Fixtures (`test/npu/`)

| Fixture | Content | Declared outcome |
|---|---|---|
| `chain` | conv 3x3 ReLU, conv 1x1 + skip, MAX_POOL, AVG_POOL, ADD | OK |
| `gemm` | GEMM bank, K=128 | OK |
| `dfc` | neo MAX_POOL [16][12][12], neo RESIDUAL n=1024 | OK |
| `mixed` | conv, GEMM bank, MAX_POOL, neo RESIDUAL: three batches | OK |
| `large` | large arena, window 1 MiB above its base: conv 3x3 [16][32][32] -> [32][32][32], MAX_POOL | OK |
| `long` | 8 chained MAX_POOL 5 on [16][64][64] (used for reset and timeout tests) | OK |
| `busy` | 8 x MAX_POOL 31 on [16][64][64], large arena (long NPU work, for coexistence tests) | OK |
| `bad_bounds` | a tensor past the window | BAD_JOB, tensor bounds |
| `bad_input` | one loaded byte that differs from its CRC | INPUT_CRC |

Each fixture directory holds these files:

- `job.bin`;
- `window.bin`: the INPUT and CONST tensors, from the window base;
- `golden_<i>.bin`: one per OUTPUT;
- `fixture.json`: addresses, sizes, CRCs, declared outcome;
- `FIXTURE.txt`: what it is and where it comes from.

The data is synthetic, from fixed seeds: no network, no weights, no images.
The goldens come from the firmware's C oracle, not from the model. The rich and
DFC operations of the valid fixtures were cross-checked byte for byte with the
drop's numpy reference before delivery. `BUILD_MANIFEST.json` records every
fixture's file hashes and the outcome of its run at packaging time.

### Your own job

`make -C sw/bootloader/sources/VP_FX1_V3.0 npu-jobs` builds the generator with
the host compiler. The generator reuses the firmware's job checks, builders and
C oracle. It writes every fixture into `sw/bootloader/build/VP_FX1_V3.0/npu_jobs/`
(or `NPUJOBS=DIR`), and the shipped fixtures are exactly these files.

The generator deletes nothing it did not write:

- The output directory must be new, empty, or one it made (marker file
  `.npu_job_gen`). Otherwise it stops with exit 2 before writing anything.
- In its own directory it replaces only `job.bin`, `window.bin`,
  `fixture.json` and `golden_<n>.bin` of the fixtures it generates (and its
  marker).
- Your notes, and fixture directories of your own, stay.
- It never writes through a link. A symbolic link at its marker, at one of
  those file names or at a fixture directory stops it with exit 2 before it
  writes anything; replace the link with a copy, or use another `NPUJOBS`.
  Each file is created anew, so a hard link's other names keep their content.

To make a new job, add a fixture function to `npu_jobs/npu_job_gen.c`. The
generator provides tensor allocation, random data, `conv` / `pool` / `add` /
`gemm` and the oracle. Rebuild, then run the job with
`run_vp.sh npu <dir>`.

## The driver (`drivers/npu/fx1_npu.h`)

The header comment is the contract; the main rules are below.

- **Probe first.** `fx1_npu_init` checks VP_ID, the ABI major and the
  features you require before any other access. It opens only an idle NPU and
  fails closed.
- **One owner.** Nothing in the driver is reentrant. Between harts, the owner
  is whoever holds the lock word:
  - `fx1_npu_claim(lock, hart)` returns OK, or BUSY when the other hart holds
    the lock;
  - `fx1_npu_init_shared` opens the NPU for the holder;
  - every call that accesses the NPU checks the holder (NOT_OWNER, with no
    access);
  - `fx1_npu_release(npu, base, lock, hart)` frees the lock only once the NPU
    at `base` is idle: nothing outstanding, no engine busy, no copy-back. This
    holds even when the context is already closed.

  The FreeRTOS NPU task holds the lock for hart 0 for the whole session, so
  hart 1 cannot use the NPU while the CLI image runs.
- **Buffers.** A job's inputs and outputs belong to the NPU until its
  wait/poll returned OK or JOB, or until `fx1_npu_recover` returned OK. A
  TIMEOUT of a wait does not mean the NPU stopped.
- **Completion.**
  - A GEMM-bank job runs alone: while one is outstanding, rich and
    configuration calls return NOT_IDLE without any access.
  - Rich instructions form a batch, which passes when RETIRED has advanced by
    the number of instructions accepted.
  - Error latches are cleared only when the NPU is quiet, so a completion is
    never lost.
- **Interrupts.** DONE comes once per GEMM-bank job, and once per rich batch
  that ended without an error. After an ERROR interrupt, or a refused push,
  the batch ends without DONE, so an interrupt-driven owner must also poll. A
  PLIC claim can find IRQ_STATUS 0; the handler must accept it.
- **Recovery.** `fx1_npu_recover` stops the NPU (ABORT) and waits until
  QUIESCENT is set and every engine is idle. If the engines never go idle, the
  context is closed (UNAVAILABLE) and only a new init reopens it.
- The raw native interface (SAURIA configuration and SRAM windows) is not part
  of the driver. `fx1_npu_native_read/write` are accessors for bring-up only.

## Limits

- **Operations.** VP ABI 1.0 supports the GEMM bank, GEMM_FUSED convolution
  (one tile for the whole output), MAX_POOL, AVG_POOL, ADD, and neo MAX_POOL /
  RESIDUAL. The strict hardware refuses neo GEMM_FUSED and neo AVG_POOL.
  DMA_TENSOR, slice / concat, attention and layer norm are refused at accept
  time, with nothing written.
- **Not a network runtime.** Jobs are structured operations on one window.
  The drop's full networks, such as YOLOv8m (which needs slice / concat and
  host upsample steps), do not run on the FX1 NPU as delivered.
- **Functional, loosely timed.** Model cycles and simulated times are not FX1
  throughput or latency figures.
- **Hart speed.** The harts run from DDR at about 150 ns per instruction, so
  the firmware's CRCs over large tensors take simulated time (about 0.1 s per
  64 KiB).
- **One job at a time, on hart 0.** The job's checks and CRCs share the CPU
  with the console (both at priority 2), so their time grows with console
  load. The interrupt handler is not delayed by task priorities.
- **The VP aperture and memory map are placeholders**, pending the HAS. Use
  the macros of `fx1_memory_map.h` and `fx1_npu_regs.h`, never literals.
