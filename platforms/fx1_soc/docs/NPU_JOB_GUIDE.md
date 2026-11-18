# FX1 NPU jobs: format, CLI and launcher

This guide is for firmware and software engineers who run work on the FX1 VP
NPU (SAURIA v4.7, VP aperture `0x1300_0000`). It covers the job format, the
FreeRTOS `npu` commands, the fixtures and the host launcher. The register ABI
and the bare-metal driver are in `components/FX1_Components/npu/README.md` and
`fw/fx1_soc/drivers/fx1_npu.h`.

A job is a list of operations on tensors. The host never sends register
writes. The firmware builds every NPU instruction itself, with the reference
driver's builders, from the operation and the tensors it names, and only after
checking them.

## Where things live

| What | Address | Written by |
|---|---|---|
| Job image (header, tensor table, op table) | `FX1_NPU_JOB_BASE` `0x8065_0000`, up to 64 KiB | host (`fx1_soc --load`) |
| Result block, 256 bytes, magic `FXNR` written last | `FX1_NPU_RESULT_BASE` `0x8066_0000` | NPU task |
| Tensors | the job's window: arena base + `window_offset`, `window_size` bytes | host (INPUT, CONST), NPU (OUTPUT, SCRATCH) |
| Small arena | `0x8090_0000`, 4 MiB; `npu selftest` writes it from +3 MiB | |
| Large arena | `0x8E00_0000`, 32 MiB | |

`fx1_memory_map.h` asserts that these areas stay apart from the firmware image,
the stacks, the ISP job and stream areas, and each other. Only the NPU task
uses the arenas, and it runs one job at a time.

The selftest range is an ordinary part of the small arena, not a protected
one. If a job's window overlaps +3 MiB.., run `npu selftest` before the host
loads the job's tensors, or load them again afterwards. INPUT_CRC detects
overwritten inputs; it does not preserve them.

## Job format, version 1 (`fw/fx1_soc/drivers/fx1_npu_job.h`)

All fields are little-endian.

**Header (128 bytes)**
- `magic` "FXNJ", `version` 1, `header_bytes` 128.
- `total_bytes`: header + tables. `job_crc`: the CRC-32 of those bytes,
  computed with this word read as 0.
- `abi_major` 1.
- `features`: FEATURE bits the job needs. The bits of its operation kinds are
  added automatically.
- `model_hash`: the `VP_MODEL_HASH` the goldens come from; 0 accepts any model.
- `arena` (0 small, 1 large), `window_offset` and `window_size` (both 4 KiB
  multiples).
- Table counts and offsets: the tensor table starts at 128, the op table
  follows it.
- `timeout_ms`: 1..60000 ms of simulated time, the **execution budget**. It
  runs from the first push or start to the last completion, and no operation
  is pushed or started once it has passed. The firmware's input CRCs and
  canaries before, and its output CRCs after, are not part of it (about 0.1 s
  of simulated time per 64 KiB on the hart).
- `name`: up to 31 characters from `[A-Za-z0-9_.-]`.

**Tensor (32 bytes), at most 32**
- `offset`, `bytes` (in the window).
- `role`:
  - INPUT: loaded by the host; its CRC is checked before the run.
  - CONST: weights, bias, requant tables; checked like INPUT.
  - OUTPUT: written by one operation; its CRC is compared after the run.
  - SCRATCH: written by one operation and read by later ones; not compared.
- `dtype`: int8 or int32.
- `layout` and `rank`: FLAT `[n]`, CHW `[c][h][w]`, MATRIX `[rows][cols]`,
  CONV_W `[cin][kh][kw][cout]`.
- `dims[4]`.
- `crc`: the CRC-32 of the bytes as loaded (INPUT, CONST), or the expected one
  (OUTPUT).

**Operation (64 bytes), at most 32**

Each operation has a `kind`, eight tensor slots `t[]` (0xFF = none) and twelve
parameters `p[]`.

| Kind | Tensors | Parameters |
|---|---|---|
| GEMM_BANK | a int8 [32][K], w int8 [K][32], c int32 [32][32] | K (1..992) |
| CONV (GEMM_FUSED) | in CHW, w CONV_W, bias / scale / shift int32 [cout], skip (optional) CHW, out CHW | kh, kw, stride, pad, act (0 none, 1 ReLU), zp_out |
| MAX_POOL | in CHW, out CHW | k, pad, stride |
| AVG_POOL | in CHW, out CHW | k, pad (0), stride, so, sho |
| ADD | a, b, out: int8, same size, any layout | zp_a zp_b zp_o sa sha sb shb so sho |
| NEO_MAX_POOL | as MAX_POOL, pad 0, through the neo_lite register file | as MAX_POOL |
| NEO_RESIDUAL | as ADD, through the neo_lite register file (n multiple of 32, expressible as RESIDUAL) | as ADD |

The arithmetic is that of the drop's reference (`tools/has/ref_program.py`):
requant, rounding, clamping. The firmware's C oracle
(`fw/fx1_soc/tests/npu_common/npu_oracle.c`) follows it and is cross-checked
against it.

## What the firmware checks, and in what order

1. **The image, before any NPU access** (`fx1_npu_job_check`). A failure
   gives BAD_JOB, with the reason and the index of the failing field, tensor
   or op.
   - The header: magic, version, sizes, CRC, ABI, reserved words 0, name,
     timeout, arena and window.
   - Every tensor: role, dtype and layout; dims × element size equal to
     bytes; 4-byte aligned; inside the window; no two tensors overlapping.
   - Every operation:
     - its kind, and the slots it uses;
     - every tensor of the right role, dtype and shape for its geometry
       (output sizes are computed, never trusted);
     - every source written before it is read;
     - every OUTPUT and SCRATCH written by exactly one operation;
     - the instruction buildable.
2. **The NPU's features, model hash and window limit.** A missing one gives
   UNSUPPORTED.
3. **The window.** The firmware sets `RICH_WINDOW`, then checks the CRC of
   every INPUT and CONST tensor; a mismatch gives INPUT_CRC, with the NPU not
   started. It then fills OUTPUT and SCRATCH with `0xA5`, so a write that never
   happens cannot pass.
4. **The operations, in order and one at a time.** Each one is pushed, or
   started, only once the previous one has completed:
   - the model counters, which cover one hardware batch, are exact per
     operation;
   - a failure names its operation.

   Errors:
   - a refused instruction gives REFUSED;
   - a failed one gives FAILED (with the error registers);
   - the execution budget passing gives TIMEOUT. It is checked right before
     every push or start, after every point where the task may wait or be
     preempted, so an operation never starts late;
   - `npu reset` gives ABORTED.

   For the last two the job is recovered (ABORT, every engine idle). A
   recovery that fails gives UNAVAILABLE, and the NPU stays closed until
   `npu reset` finds it idle.
5. **The outputs.** The CRC of every OUTPUT is compared with the expected one;
   a difference gives MISMATCH.

The firmware copies the job image into its own RAM first, so the host cannot
change it between the check and the run. Buffers belong to the NPU until the
job's outcome is known; after TIMEOUT or ABORTED, only once the recovery has
succeeded.

## Result block (`fx1_npu_job_result`, 64 words)

- `magic`, `version`, `status`, `detail`.
- `op_index`: the failing operation.
- `driver_status`, then the error registers at the failure: STATUS,
  LAST_ERROR, IRQ_STATUS, RICH_STATUS, VP_ERROR, VP_ERROR_INFO, VP_STATUS.
- Operations done (rich, neo, GEMM bank), and the ACCEPTED and RETIRED deltas.
- Model execution cycles, MAC operations, GEMM-bank cycles.
- `ticks`: the NPU time of the job, in 50 ns `mtime` ticks.
- Interrupts and stale interrupts.
- Outputs compared and matching, the job CRC, the model hash, and the CRC
  found for each tensor.

These counters are model time. They are not FX1 end-to-end throughput.

## Fixtures (`fx1_npu_jobs`, built with `fx1_all`)

`fw/fx1_soc/host/npu_job_gen.c` generates the fixtures, with their goldens
from the C oracle, into `build/fw/fx1_soc/npu_jobs/<name>/`:

| Fixture | Content | Declared outcome |
|---|---|---|
| `chain` | conv 3x3 ReLU, conv 1x1 + skip, MAX_POOL, AVG_POOL, ADD | OK |
| `gemm` | GEMM bank K=128 | OK |
| `dfc` | neo MAX_POOL [16][12][12], neo RESIDUAL n=1024 | OK |
| `mixed` | conv, GEMM bank, MAX_POOL, neo RESIDUAL | OK |
| `large` | large arena, window 1 MiB above its base: conv 3x3 [16][32][32] -> [32][32][32], MAX_POOL | OK |
| `long` | 8 chained MAX_POOL 5 on [16][64][64] (for reset and timeout tests) | OK |
| `bad_bounds` | a tensor past the window | BAD_JOB, tensor bounds |
| `bad_input` | one loaded byte that differs from its CRC | INPUT_CRC |

Each fixture directory holds `job.bin`, `window.bin` (the INPUT and CONST
tensors from the window base), `golden_<i>.bin` per OUTPUT, and
`fixture.json` (addresses, sizes, CRCs, declared outcome).

Every valid fixture passes the firmware's own checks. Its rich and DFC
operations are also run through the drop's `ref_program.py`:
`fx1_regression_npu_job_xcheck_*` checks that the whole window equals the C
oracle's.

To make a job of your own, add a fixture function to `npu_job_gen.c`. It
offers tensor allocation, random data, `conv`/`pool`/`add`/`gemm` and the
oracle. Rebuild `fx1_npu_jobs`.

## Running a job

**With the launcher** (Python standard library only):

```sh
python3 platforms/fx1_soc/demo/npu_run.py \
    --fx1-soc build/platforms/fx1_soc/fx1_soc --elf build/fw/fx1_soc/freertos_cli.elf \
    build/fw/fx1_soc/npu_jobs/chain --out /tmp/npu_chain
```

The launcher:
1. loads `job.bin` and `window.bin`;
2. drives the CLI (`npu info`, `npu run`, `npu wait`, `npu status`,
   `poweroff`);
3. dumps the result block and every output;
4. compares the outputs with the goldens byte for byte;
5. writes `summary.json` and `summary.txt`: outcome, detail, failing
   operation, error registers, counts, model cycles, NPU time, simulated and
   wall time, and input SHA-256.

The simulator gets a clean environment. Only `PATH`, `HOME`, `USER`,
`LOGNAME`, `TMPDIR`, `TZ`, `TERM`, `LANG`, `LC_*` and `LD_LIBRARY_PATH` are
passed on, because the model reads configuration knobs from the environment
(`HAS_*`, `SAURIA_*`, `NPU_*`, and others). The run counts only when the
platform banner reports `env=none`. The knob-like names that were removed are
listed in the summary.

The output directory belongs to the launcher. It is refused when it is, holds
or lies inside the fixture (or contains the simulator or the image). It must be
new, empty, or one an earlier run made: marker `.npu_run`, or only the
launcher's own files. Those are exactly `cli.txt`, `result.bin`,
`out_<n>.bin`, `uart.log`, `platform.log` and `summary.*`; a name such as
`out_camera_capture.raw` is not one of them. In such a directory the launcher
replaces only its own files (a stale `result.bin` is removed before the run),
and anything else is kept.

The CLI script waits with `npu wait` without a limit, so `poweroff` comes only
once the job's outcome is known.

Exit codes:
- 0: the declared outcome, outputs equal to the goldens;
- 1: anything else;
- 2: bad arguments or fixture;
- 3: the simulator failed or wrote no result block.

`--print-command` prints the `fx1_soc` command line with its clean
environment (`env -i ...`). It writes only `cli.txt` and the marker.

**By hand**, for an interactive console:

```sh
build/platforms/fx1_soc/fx1_soc --fw build/fw/fx1_soc/freertos_cli.elf --timeout-ms 0 \
    --load 0x80650000:npu_jobs/chain/job.bin --load 0x80900000:npu_jobs/chain/window.bin \
    --uart0-socket 5555 --uart0-wait
```

Then, at the console:

```
root@fx1:~# npu run
npu run: started; `npu status`, `npu wait [MS]`, `npu reset`
root@fx1:~# npu wait
npu run: chain: OK, 5/5 ops (rich 5, neo 0, GEMM bank 0), accepted +5 retired +5, outputs 4/4 match
npu run: model exec cycles 14783, MAC ops 360448, GEMM-bank cycles 0, interrupts 5 (stale 0), simulated ... us
  npu run: PASS
```

The window address of a fixture is in its `fixture.json` (`window_addr`).

## Limits

- **Operations:** GEMM bank, GEMM_FUSED convolution (one tile for the whole
  output), MAX_POOL, AVG_POOL, ADD, and neo MAX_POOL / RESIDUAL. The strict
  hardware refuses neo GEMM_FUSED and AVG_POOL (N2). DMA_TENSOR, attention and
  layer norm are not in the job format.
- **Not a network runtime:** replaying drop-generated programs and full
  networks are later work (plan N4 and beyond).
- **Hart speed:** the harts run from DDR (about 150 ns per instruction), so the
  firmware's CRCs over large tensors take simulated time (about 0.1 s for
  64 KiB).
- **One job at a time, on hart 0.** Hart 1 does not run the FreeRTOS CLI.
- **`npu wait` without a limit** returns when the job ends: within its
  execution budget plus the firmware's CRC time. `npu wait MS` only stops
  waiting; the job goes on, and it is not a failure. A script must wait for, or
  check, the outcome before `poweroff`.
- **The NPU task runs at priority 1, below the CLI.** Completion handling in
  the task can wait behind CLI work; the interrupt handler itself is not
  delayed by task priorities.
