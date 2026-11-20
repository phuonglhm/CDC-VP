# Driving the NPU directly from the CPU (without the data flow controller)

This guide is for firmware that programs the SAURIA, GVU and DMA blocks of the NPU itself, as the configuration port of neo_lite RTL
revision 2 allows, instead of handing a job to the data flow controller (DFC). In the model the mode is enabled with `HAS_CPU_DIRECT=1`
(it requires `HAS_NEO_REV=2`). The DFC path is unchanged and remains the default. Section 9 shows how to run a firmware program on the model.

## 1. How the hardware allows it

The configuration port of `neo_top` is an AXI4-Lite slave. An address demultiplexer selects one of four blocks (controller, SAURIA, GVU,
DMA). In front of the SAURIA, GVU and DMA blocks an AXI4-Lite multiplexer (arbiter) serves two masters: the CPU and the DFC. There is no
select bit: whichever master has a transaction is served. **Firmware must not let the CPU and the DFC program the blocks at the same time**
(the model applies such writes and reports the first one on stderr).

## 2. Address map

The offset inside a block is the offset of the RTL; only the base differs between platforms.

| Block | FPGA (`cfg_base` + ...) | Virtual platform / model | Contents |
|---|---|---|---|
| Controller | `+0x0_0000` | `0x10800` (address translation to `0x40000800`) | 82-word job image for the DFC (unchanged) |
| SAURIA | `+0x1_0000` | `0x11000` | `0x000` CONTROL, `0x004` global interrupt enable, `0x008` interrupt enable, `0x00C` interrupt status (write 1 to clear), `0x010` .. `0x01C` status, cycle counter, stall counter, version (read), `0x200` / `0x400` / `0x600` / `0x800` the CON / ACT / WEI / OUT configuration words (29 packed words) |
| GVU | `+0x2_0000` | `0x12000` | `0x00`..`0x34`: the 14 registers of the vector unit (CONTROL, OPCODE, LUT reset, GEMM_FUSED, SOFTMAX 0-2, RESIDUAL 0-4, POOLING 0-1) |
| DMA | `+0x3_0000` | `0x13000` | `0x00`..`0x3F` one-dimensional DMA (CONTROL, interrupt enable, interrupt status, read address, write address, bytes), `0x80`..`0xC4` tensor operations |
| SYNC (model only) | — | `0x14000` | see section 5 |

Model address = `0x10000 + (RTL block offset >> 4) + offset`.

## 3. Memories seen by the DMA

A DMA address selects the memory: `0x0000_0000` SRAM A, `0x0004_0000` SRAM B, `0x0008_0000` SRAM C of SAURIA; `0x0010_0000` vbank0,
`0x0012_0000` vbank1, `0x0014_0000` vbank2, `0x0018_0000` sbank of the GVU scratchpad; any other address is system memory (DRAM). Every
local memory is double-buffered: the DMA reaches the half the accelerator does not use. The halves change:

* SAURIA: at a START (unless the keep bit of that SRAM is set in CONTROL) and at a memory switch (CONTROL bit 16).
* GVU: at the rising edge of the enable bit (CONTROL bit 0), and when CONTROL bit 3 (force buffer swap) is written.

## 4. Sequences

All waits are **polling**: the model ORs the SAURIA and DMA interrupts into its single interrupt line, but the virtual platform does not
route it to the CPU, and the GVU has no interrupt to the CPU at all.

**One-dimensional DMA command:** write the read address (`+0x10`), write address (`+0x20`) and byte count (`+0x30`); write CONTROL = `0x3`
(start reader and writer; `0x102` writes zeros without reading); poll the interrupt status (`+0x0C`) until bit 1 (writer done) is set;
write `0x3` to the interrupt status to clear it. Do not change the address registers while a command runs.

**SAURIA run:** DMA the input (SRAM A), weights (SRAM B) and partial sums (SRAM C) in; write the 29 configuration words (the same words as
words 40..68 of the controller image); write 1 to the global and done interrupt enables; from the second run on, write CONTROL =
`0x0080_0000` (soft reset, section 8) and poll CONTROL until bit 3 (ready) is set; write CONTROL = `0x1` (start; the SRAM halves flip;
a keep bit of CONTROL must be written in the same write as the start, for example `0x0008_0001` keeps SRAM C, because every write of
CONTROL sets all its bits); poll the interrupt status until bit 0 is set; write 1 to it; write CONTROL = `0x10000` (memory switch: the
result half returns to the DMA side); DMA SRAM C out. The status registers read back the cycle count of the last run (`0x014`), its
stall count (`0x018`) and the version `0x0077_2020` (`0x01C`: 32 x 32 array, 8-bit integer operands).

**GEMM_FUSED (core results streamed into the vector unit):** one layer is one enable of the vector unit and one SAURIA run per core call.
(1) Before the enable (the scratchpad halves flip at the enable, so the DMA fills the half the unit will use): DMA the 256-byte activation
table to vbank1; DMA the output scales (int32) to the sbank from byte 0 and the output shifts (int8) to the sbank from byte 2456
(`0x0018_0998`). **Entry k belongs to the k-th result packet since the enable, not to output channel k**: a packet is one write of the
partial-sum manager (one output channel over up to 32 positions), so a layer of several calls needs the entries of its first call, then of
its second, in the order the packets are written. (2) GVU registers: GEMM_FUSED (offset `0x0C`: zero point in bits 15..8, bit 0 skips
the activation), OPCODE (`0x04`) = 2, CONTROL = 1 last. (3) Each core call as a SAURIA run above, with the three stream bits of the output
region: `psum_stream_en` = 1, `cin_tile_last` = 1 on the last input-channel pass of the tile, `oc_tile_last` = 1 only in the last call of
the layer. (4) After the last call: poll the GVU CONTROL until bit 1 (done) is set; write CONTROL = `0xC`; DMA vbank2 out (row r = 32
int8 lanes at byte 32 r, in packet order; a lane without data holds the activation of the zero point). Limits per enable: 614 packets
(sbank), 1,024 rows (vbank2); a larger layer is split by the firmware into several enables, each with its own sbank contents and its own
last call. The activation table stays loaded; before a layer with another table write 1 to bit 0 of the LUT reset register (offset `0x08`; bits 1, 2 and 3 do the same for the exponential, reciprocal and reciprocal square root tables of Softmax and LayerNorm). Each bit is a pulse of one clock: the register clears itself.
Arithmetic of this path: rounding half away from zero, narrowing with wrap-around to 16 bits after the round-shift, zero point, clamp to
int8 — the golden of the software has to use the same arithmetic.

**Softmax:** DMA the reciprocal and exponential tables (768 bytes) to vbank1 and the scores to vbank0; write SOFTMAX_0 (`0x10`: row
length, bit 16 set), SOFTMAX_1 (`0x14`: scale), SOFTMAX_2 (`0x18`: shift in bits 7..0, zero point in bits 15..8), OPCODE = 3, CONTROL = 1
last; poll CONTROL bit 1; write CONTROL = `0xC`; DMA the result out of vbank0 (the result replaces the scores).

**GVU max pool / residual:** DMA the operands into vbank0 (and vbank1 for residual); write the GVU registers, the enable bit of CONTROL
last; poll CONTROL until bit 1 (done) is set; write CONTROL = `0xC` (clear done and force the buffer swap: the output buffer goes to the
DMA side); DMA the result out of vbank2 (both max pool and residual write their result to vbank2).

## 5. DRAM on the virtual platform (SYNC)

The DRAM the model's DMA uses is a separate memory of the wrapper, which the wrapper fills only when a job is pushed to the controller.
In CPU-direct mode firmware asks for the copies explicitly: write SYNC_ADDR (`0x14000`, system RAM address) and SYNC_SIZE (`0x14004`,
bytes), then SYNC_CMD (`0x14008`) = 1 to copy system RAM into the model before a DMA reads it, or 2 to copy the model back to system RAM
after a DMA wrote it. This needs a small change in the wrapper (a proposed change of about 40 lines, `tools/has/wrapper/npu_tlm_cpu_direct_sync.patch`, tried: with
it the DMA, one-run and three-run programs give the expected bytes in the system RAM of the virtual platform; without it they do not); on
the FPGA the SYNC writes are not needed. The system address of DRAM offset 0 that the model assumes is `0x8000_0000` and can be changed
with `HAS_CPU_DIRECT_DRAM_BASE`; firmware keeps the block bases and the DRAM base as two platform constants.

## 6. What was checked on the RTL

The sequences of section 4 were run on a simulation of `neo_top` revision 2 (Verilator, the data flow controller idle, the CPU writing the
registers through the configuration port) and on the model, with the same programs:

| Sequence | RTL | Model |
|---|---|---|
| One-dimensional DMA between DRAM and SRAM A / B / C, vbank0 / 1 / 2, sbank (aligned, unaligned, write-zero) | all regions exact | exact |
| SAURIA run (29 words, START, polling, memory switch), SRAM C 9 216 bytes | exact; 9 311 core cycles from the START write to the interrupt | exact; 9 345 |
| Max pool, 192 output rows | exact; 6 168 cycles from enable to done | exact; 6 168 |
| RESIDUAL add and subtract with bias, 64 rows, result read from vbank2 | exact; 80 cycles from enable to done (16 + N) | exact; 80 |
| Three SAURIA runs in a row, with a soft reset before each START (also keeping SRAM C to accumulate) | exact; 9,311 cycles per run | exact |
| Three SAURIA runs in a row without the soft reset | the second run stalls on every clock and never finishes | runs (prints a warning, section 8) |
| The same programs on the RTL as delivered (core word count of 92, section 8) | one run, three runs with soft reset, DMA, max pool, RESIDUAL: same results and cycles as above | — |
| One-dimensional DMA from DRAM to DRAM, with a two-port memory in the testbench | exact (aligned, unaligned, through a local memory) | exact |
| Softmax, rows of 32 and 64 elements | exact; 142 and 238 cycles from enable to done | exact; 142 and 238 |
| GEMM_FUSED, 3 core calls, 96 result packets | no correct result: the unit computes every packet correctly (activation, scale, shift, int8 write), but the write address of vbank2 does not advance between packets, so every packet overwrites row 0 (section 8) | exact (equal to the controller path packet by packet) |

## 7. Known limits of the model

* The SAURIA core of the model is reset at each START and runs without the soft reset that the RTL needs (section 8); the model counts and
  reports a START without a soft reset instead of stalling. The deadlock flags and state fields of the status register read 0.
* The time of a one-dimensional DMA command is the measured command cost of the model, not the cycle behaviour of the reader and writer.
* The model ends a GEMM_FUSED layer at the last packet of its last call; the RTL ends it at the first packet of that call (section 8).
* Average pool, LayerNorm and the attention requantization have no data path in the vector unit of the RTL; the model runs them only on
  the DFC path (its own design).

## 8. Notes for firmware that also runs on the FPGA

The model runs the intended behaviour. Where the RTL of revision 2 behaves differently on the sequences above, firmware written on the
model must follow the rule below, or it may stall or give other results on the FPGA:

1. **Soft reset before every SAURIA START after the first** (CONTROL = `0x0080_0000`, then poll the ready bit). Without it the second run of
   the core stalls on every clock (measured on the RTL). The model does not stall; it prints `START without a soft reset` and counts it,
   so a test program that runs clean on the model has no such line.
2. **The last call of a GEMM_FUSED layer runs one context.** The RTL stamps `oc_tile_last` on every packet of the call and the vector unit
   leaves GEMM_FUSED at the first packet that carries it; a last call with several contexts stops the unit early and the core stalls.
   Split the last tile so that its call runs a single context, or run its other contexts in calls without `oc_tile_last`.
3. **Order the sbank by result packet**, not by output channel, and keep a layer within 614 packets per enable (section 4).
4. **Write every bit of CONTROL in one write**: a keep bit goes with the start bit.
5. **Polling only**: the GVU has no interrupt to the CPU, and on the virtual platform no interrupt of the NPU reaches the CPU.
6. **The core word count of the RTL as delivered** (92 instead of 29 because of a rounding in the packages) does not affect the CPU path:
   the 29 words written at `0x200` / `0x400` / `0x600` / `0x800` are decoded the same way (measured, section 6). It does affect the image of
   the data flow controller.
7. **GEMM_FUSED by the CPU does not give a correct result on revision 2 as delivered**: the write address of vbank2 is reloaded with its start
   address on every clock without an access, so every result packet overwrites row 0 (measured). With the address held between accesses in GEMM_FUSED (a correction proposed to the hardware team) the same program is exact on the RTL simulation and the other sequences keep their results and cycles (measured). Until the RTL is corrected, use the
   CPU path for DMA, SAURIA runs, max pool, RESIDUAL and Softmax, and requantize the core results by another means (for example on the CPU
   from SRAM C read back by the DMA). The model computes GEMM_FUSED correctly.
8. The FPGA addresses of the blocks are `cfg_base` + the RTL offsets (section 2); on the virtual platform add the SYNC writes (section 5).
9. **A layer with another table needs the table reset** (section 4): without it the unit keeps the table it loaded for an earlier layer and
   uses it again, on the RTL and on the model alike.

## 9. Running firmware on the model

The testbench `tools/has/tb_has_npu_top` runs a firmware program written as a list of register accesses, in the format of the programs of the
compiler (build as in `BUILD_AND_RUN.md`):

```
bash tools/has/build_tb_has_npu_top.sh
HAS_NEO_REV=2 HAS_CPU_DIRECT=1 timeout 600 tools/has/tb_has_npu_top <program dir>
```

| File of the program directory | Content |
|---|---|
| `mmio.txt` | the program, one access per line (below) |
| `dram_init.bin` | the DRAM at the start; DRAM offset 0 is the system address `0x8000_0000` (`HAS_CPU_DIRECT_DRAM_BASE`) |
| `dram_golden.bin` | the expected DRAM, compared at the `K` lines |

| Line of `mmio.txt` (hexadecimal; `#` starts a comment) | Meaning |
|---|---|
| `W <address> <value>` | write a 32-bit register: `0x11000` SAURIA, `0x12000` GVU, `0x13000` DMA, `0x14000` SYNC, plus the offset |
| `P <address> <mask> <value>` | read the register every 16 clocks until (value read AND mask) = value; there is no time limit, hence `timeout` |
| `K <DRAM offset> <bytes>` | the byte count in decimal: compare that DRAM region with `dram_golden.bin`; prints `region ... bad=N PASS` or `FAIL` |

The run ends with `RESULT: PASS` or `RESULT: FAIL (...)` and a summary of the CPU-direct path: register writes and reads, DMA commands and
their cycles, SAURIA starts, soft resets and STARTs without a soft reset, GVU edges, GEMM_FUSED runs; `sim cycles` gives the time. A program
meant for the FPGA as well should end with `RESULT: PASS` and print no `without a soft reset` warning (section 8).

Example programs: `tools/has/cpu_direct/mk_cpu_*.py` write program directories for DMA commands, a core run, max pool, RESIDUAL, Softmax and
GEMM_FUSED; `mk_cpu_chain.py` writes a chain of two GEMM_FUSED layers in which the second layer reads the result that the CPU wrote to DRAM
for the first (rows of 32 positions per output channel are the layout SRAM A takes), each layer with its own table, sbank and calls, then a
RESIDUAL of the two results: the pattern of a firmware for a whole network. `tools/has/regress_gvu.sh` shows the command line of each.

## 10. A whole network on the CPU path

`tools/has/cpu_direct/mk_cpu_net.py` writes the CPU program of a whole compiled network, without hand-written firmware. The model first runs
the network program on the data flow controller with a trace on, then the tool turns the trace into register accesses for the CPU:

```
HAS_NEO_REV=2 HAS_CPU_DIRECT=1 HAS_GVU_REGS_NEO=1 HAS_GVU_AGU_ND=0 HAS_ROUND_MODE=2 HAS_REQ_NARROW=1   HAS_CPU_DIRECT_NET=1 HAS_CPU_DIRECT_DUMP=<trace dir> tools/has/tb_has_npu_top <program dir> [--first F --count N]
python3 tools/has/cpu_direct/mk_cpu_net.py <trace dir> <program dir> <out dir> [--check] [--compact] [--instr A:B]
HAS_NEO_REV=2 HAS_CPU_DIRECT=1 HAS_DMA_OPGEN=1 HAS_GVU_REGS_NEO=1 HAS_GVU_AGU_ND=0 tools/has/tb_has_npu_top <out dir>
```

The trace run uses the register map of the vector unit of the RTL and its arithmetic (rounding half away from zero, wrapping 16-bit narrowing),
because the CPU path runs GEMM_FUSED with that arithmetic; for YOLOv8m this arithmetic gives the same bytes as the default one (no requantized
value of the network leaves the 16-bit range and no value falls on a rounding tie, MEASURED over the 83 convolutions; not measured for ViT-B/16).
The trace holds, in time order, every core call with its 29 words and a hash of the SRAM contents the core reads, the packets of the output
stream, and every run of the vector unit with its register writes; `--check` rebuilds the bytes of every core call from the DRAM image and
compares the hashes.

The program follows section 8: a soft reset before every START after the first; polling only; keep bits in the same write as the start bit; the
sbank holds the scale and shift of the k-th result packet since the enable; at most 613 result packets per enable, cut at tile boundaries; every
enable is closed by a core call of one context (one channel, 32 positions) that carries `oc_tile_last` and keeps all three SRAM halves; the
activation table is reset before a layer with another table. Input windows with padding are built in a staging area of DRAM by one PAD2D
descriptor of the tensor DMA and moved into SRAM A by one command; result rows go from vbank2 to the staging area by one command and are scattered
into the tensor by COPY descriptors of up to three loops. Constant data the controller derives (the partial-sum preload = bias over the positions,
the sbank images, the max pool frame) is laid out once after the program image. Element-wise ADD runs on the RESIDUAL registers; upsampling stays
a host step (`H upsample2x`). `--compact` packs the DRAM regions the program uses from offset 0, for a bench with a small memory.

**One tile must fit one enable.** Because an enable can only be cut between tiles, a tile whose last core call gives more than 613 result packets
cannot be programmed; the tool stops with `one tile has N packets`. The packets of a tile are its rows times its columns, twice that with
`Y_USED` 31 (MEASURED in the trace). In the YOLOv8m program planned for the SRAM of revision 2, one layer (`dark2.conv`, 3x3 stride 2, 48 to 96
channels, tile 10 x 31) gives 620; the front end option `--r2-tile dark2.conv:9x31` (`COMPILER_GUIDE.md`, section 4) re-plans that layer with
tiles of 9 rows (558 packets) and leaves every other instruction and both DRAM images unchanged. Every other layer has at most 372 packets per tile.

| Network | Register writes | Polls | DMA commands | Core calls | Enables of the vector unit | Result | Cycles (CPU) | Cycles (controller) |
|---|---|---|---|---|---|---|---|---|
| YOLOv8m, RTL revision 2, `--r2-tile dark2.conv:9x31` | 2,869,025 | 215,213 | 45,836 | 9,199 | 5,285 | PASS, 137 / 137 tensors exact | 98,464,826 | 64,242,875 (the trace run: the original tiles, the register map and the arithmetic of the RTL vector unit) |

The checks of this path (programs of sections 4 and 9 on the model and on the RTL) run alone with `bash tools/has/cpu_direct/run_cpu_direct.sh
<output dir>`; `FLOW_VERIFICATION.md`, section 7, explains what each check shows.
