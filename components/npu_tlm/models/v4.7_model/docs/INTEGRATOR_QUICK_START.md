# Quick start for the platform integrator: getting the performance figures out

For the person who builds and launches the virtual platform. It lists what to do so that the figures of the model
(cycles, frames per second, PE utilization, DMA wait, bus bytes) and its diagnostics come out of a run. Nothing here
needs a change in the source code of the system wrapper. Details are in `SW_INTEGRATION_GUIDE.md`, section 10, and
`SW_EVALUATION_GUIDE.md`.

## 1. Two ways the figures leave the model

| Way | Who collects | What the integrator does |
|---|---|---|
| Registers | Firmware on the CPU reads the performance registers of the wrapper after every instruction and adds them up | Update the model tree and rebuild (section 2) |
| Files | The model writes a report on the machine that runs the simulation | Update the model tree, rebuild, and set one environment variable in the launch script (sections 2 and 3) |

Both give the same numbers. The file way needs no firmware change and is the quickest to put in place.

## 2. Update the model tree and rebuild

1. Replace the model tree the wrapper bundles (the directory its build takes the model from) with this tree. Check the
   copy: `sha256sum -c package_manifest.txt` in the copied tree reports no difference.
2. Keep the wrapper's model root on the subdirectory `core_rtl/` of the tree (for example the build variable
   `SAURIA_NPU_ROOT`). With the repository root instead, extended instructions run on the functional decoder and none
   of the figures below exist.
3. Rebuild the platform as usual. No build flag is added and no wrapper source file changes.

### 2.1 Platform files to update for this release

This release adds no register and no error code (`CHANGES_SINCE_INTEGRATED_MODEL.md`, section 2). The integrated model
already added six extension registers and one error code; if the platform files below do not list them yet, add them.
Programs that do not use them run unchanged, so nothing below blocks the update of section 2. Firmware that
uses padding per side or dilation, or that decodes the error codes of the model, needs the names; the wrapper's
document should list them. In the system wrapper repository these are integration-owned files; the model tree is
not edited.

1. **Firmware register header** (`fw/common/include/soc/regs/soc_regs_npu_v4.h`, also copied into the firmware
   tree that runs on the platform). Its extension list ends at `MASK_ADDR` (`0x104E4`) and its error list at
   `NO_TILING` (10). Add, next to those lines:

   ```c
   #define CDC_NPU_VP_RICH_PAD_VALUE   (CDC_NPU_VP_RICH_ALIAS_BASE + 0x4E8u) /* int8 in the low byte; the input zero point */
   #define CDC_NPU_VP_RICH_PAD_T       (CDC_NPU_VP_RICH_ALIAS_BASE + 0x4ECu) /* writing any of PAD_T/L/B/R replaces PADDING */
   #define CDC_NPU_VP_RICH_PAD_L       (CDC_NPU_VP_RICH_ALIAS_BASE + 0x4F0u)
   #define CDC_NPU_VP_RICH_PAD_B       (CDC_NPU_VP_RICH_ALIAS_BASE + 0x4F4u)
   #define CDC_NPU_VP_RICH_PAD_R       (CDC_NPU_VP_RICH_ALIAS_BASE + 0x4F8u)
   #define CDC_NPU_VP_RICH_DILATION    (CDC_NPU_VP_RICH_ALIAS_BASE + 0x4FCu) /* 0 or not written: 1; (KW - 1) x DILATION <= 63 */
   #define CDC_NPU_RICH_ERROR_SWEEP    11u   /* a row sweep of the IFMAP feeder would exceed 256 positions */
   ```

   The same registers under the names of this package: `tools/has/wrapper/fw/npu_has_fw.h` (`NPU_HAS_PAD_VALUE` ...
   `NPU_HAS_DILATION`, `NPU_HAS_ERR_SWEEP`). Meaning and rules: `SW_INTEGRATION_GUIDE.md`, section 3.3 (rows 31 to
   36, padding, dilation) and the error table of section 3.2.
2. **Wrapper document** (the register tables of the wrapper's README): the compact rich alias now reaches
   `0x104FC` (model `0x400004FC`) instead of `0x104E4`; the extension table gets the six rows above; the error table
   of `RICH_STATUS` gets `11 SWEEP`.
3. **Wrapper source, one constant (recommended).** The wrapper stages its DRAM window for an instruction when one of
   the model addresses `0x4000046C` to `0x400004E7` was written since the previous push. The model treats every
   write up to `0x400004FF` as extended. Today every extended GEMM_FUSED also writes the geometry registers below
   `0x400004E8`, so no program depends on it; to keep the two ranges equal, change the end of that range in the
   wrapper's parameter decode from `0x400004E8` to `0x40000500` (the change carried by
   `tools/has/wrapper/npu_tlm_rich_window.patch` for older revisions).
4. **Firmware status check.** The wrapper's `STATUS` reports DONE also for an instruction the model refused at the
   push; the refusal and its code are in `RICH_STATUS` (`0x10318`, bit 2 and bits 31:16). Firmware that checks
   results should read it after every extended push (`SW_EVALUATION_GUIDE.md`, section 4).

The native path and the wrapper's 32x32 GEMM bank are not changed by this release
(`CHANGES_SINCE_INTEGRATED_MODEL.md`, section 7).

## 3. Switch the outputs on in the launch script

The model reads these variables once, when the platform constructs it. Set them in the environment of the simulation
process, before it starts:

```bash
export SAURIA_CORE_RTL_REPORT=/path/to/run1/npu_report.csv   # performance report (section 4)
export SAURIA_CORE_RTL_RECORD=/path/to/run1/record           # optional: the program, for a replay (section 6)
<command that starts the platform> 2> /path/to/run1/npu_stderr.log
```

- The directory of the report file and the directory above the record directory must exist and be writable.
- Keep stderr of the simulation process (do not send it to `/dev/null`): the warnings of section 5 are printed there.
- Do not set any `HAS_*` variable: they change cycle counts.
- None of the variables above changes a result or a cycle count.

## 4. What comes out

`npu_report.csv`: one line per instruction the RTL-accurate core retired, in order.

```
instruction,operation,in_c,in_h,in_w,out_c,out_h,out_w,kh,kw,stride,pad,tile_cout,tile_h,tile_w,tile_cin,elements,cycles,cycles_so_far,tiles,core_passes,core_busy_cycles,real_macs,pe_utilization_percent,dma_wait_cycles,epilogue_cycles,epilogue_config_cycles,elem_wise_cycles,rce_cycles,ddr_read_bytes,ddr_write_bytes,state
1,GEMM_FUSED,8,12,12,16,12,12,3,3,1,1,16,12,12,0,2304,14433,14433,1,1,9346,165888,1.73,865,10,4098,0,0,11520,2304,ok
```

`npu_report.csv.summary.txt`: the totals of the run so far, rewritten after every instruction, so it is complete
however the simulation ends.

```
performance of the instructions HasNpuTop retired (SAURIA_CORE_RTL_REPORT); steps of the CPU and the copies of a wrapper are not included
instructions_retired        14
pushes_refused              0
instructions_with_errors    0
instructions_in_flight      0
cycles                      285205
clock_period_ns             1.2500
time_ms                     0.3565
frames_per_second           2804.9999   (one run of these instructions per frame)
pe_utilization_percent      23.18   (real MACs 30984960 / core busy cycles 130524 x 1024 PEs)
core_busy_percent           45.76
dma_wait_percent            0.00   (0 cycles)
epilogue_percent            0.00   (0 cycles, configuration 0)
tiles                       0
core_passes                 65
ddr_read_bytes              0
ddr_write_bytes             0
FUSED_ATTN                  9 instructions, 202197 cycles, 70.90 %
LAYERNORM                   5 instructions, 83008 cycles, 29.10 %
```

How to read it:

- `cycles` is the NPU time of the run; `frames_per_second` uses the clock that drives the model in the platform. The
  work the CPU does between instructions and the copies the wrapper makes between RAM and the model are not included.
- A line exists only for an instruction that wrote at least one extension register (`0x4000046C..0x400004E7`) before
  its push. Other instructions run on the functional decoder and are not in the report.
- `pushes_refused` above 0 or a line with `errors`: look at stderr (section 5).
- The model does not know layer names: join the lines with the program by their order.

## 5. Diagnostics on stderr

Lines that start with `[core_rtl] WARNING:` tell that an instruction was refused at the push (it does not run, although
the wrapper reports completion), that it fell back to converted real-valued scales or to a default tiling, or that it
retired with an internal error. A correct run prints none. `SW_EVALUATION_GUIDE.md`, section 4.1, lists them.

## 6. When a result is wrong

1. Run again with `SAURIA_CORE_RTL_RECORD` set. The directory then holds `segNNN/mmio.txt`, `dram_init.bin`,
   `dram_golden.bin` and `record.csv`.
2. On any machine with Python 3 and numpy: `python3 tools/has/ref_program.py <record directory>/seg000`. It computes
   every convolution and ELEM_WISE instruction again, independently of the model, and names the first instruction that
   differs (`SW_EVALUATION_GUIDE.md`, section 5.1).
3. Send the record directory, the stderr log and the output of step 2 (`SW_EVALUATION_GUIDE.md`, section 9).

## 7. The register way, for the firmware author

After `STATUS.DONE` of every instruction, read these registers of the wrapper (low and high word each) and add them
up over the run; the wrapper clears them at every push.

| Figure | Registers |
|---|---|
| Cycles | `PERF_EXEC_CYCLES` |
| PE utilization | sum of `PERF_ACTIVE_PE_CYCLES` / sum of `PERF_TOTAL_PE_CYCLES` |
| DMA wait, epilogue, ELEM_WISE, FUSED_ATTN and LAYERNORM | `PERF_DMA_ENGINE_CYCLES`, `PERF_OBP_CYCLES`, `PERF_POOLING_ENGINE_CYCLES`, `PERF_REDUCTION_ENGINE_CYCLES` |
| Bus bytes | `PERF_DDR_READ_BYTES`, `PERF_DDR_WRITE_BYTES` |

`CYCLE_COUNT` of the wrapper is not updated for extended instructions. `SW_INTEGRATION_GUIDE.md`, section 10.3, has the
definitions.

## 8. What needs a change in the wrapper itself

Not available from the model alone; each is a request to the owner of the wrapper:

- internal error counters readable through registers (firmware on the CPU cannot see stderr);
- counters that accumulate over a run instead of being cleared at every push;
- `CYCLE_COUNT` for extended instructions;
- the time of the copies between RAM and the model, and of the steps the CPU does.

## 9. What was checked

The report, the warnings and the recorder were run through the wrapper with a SystemC test program on the host: the
report of a 14-instruction program adds up to the cycles and the PE utilization read from the registers, and switching
the report on changes neither outputs nor cycles. They were not run inside a platform with firmware on a CPU model:
where that platform's launch script sets environment variables, and which directory its process may write to, is for
the integrator to settle.
