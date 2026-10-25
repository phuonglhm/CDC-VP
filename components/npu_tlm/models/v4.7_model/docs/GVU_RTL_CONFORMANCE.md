# Vector unit: conformance to the hardware RTL

This document compares the vector-unit blocks of the model with the RTL of the hardware team (`neo_core/gvu`) and lists
what has to change for the model to follow the RTL. It covers two sets of files:

- the **functional model** `has/gvu_*.h`, which the instruction path (`has/has_npu_top.h`) runs today. It follows the
  drawings and the micro-architecture specification; results are bit-exact against the Python reference and timing is a
  formula per pipeline;
- the **RTL-ref blocks** `has/rtl_ref_gvu_*.h`. They are ported line by line from the RTL and were checked against an
  RTL simulation. Nothing in the instruction path calls them yet.

Labels used below: MEASURED = from a run; DERIVED = computed from measured values or read from the code; ESTIMATE = a
formula.

## 1. RTL revisions

Four deliveries of the RTL are referred to:

| Name | Content |
|---|---|
| revision A | `gvu_pkg.sv`, the functional units of `FUs/`, the stream registers of `stream_switch/`, a behavioural scratchpad wrapper. No controller, no load/store unit, no top level |
| revision B | revision A with corrections in the LUT unit, the comparator, the 48x40 multiplier and `stream_vrf.sv`; a rewritten scratchpad on SRAM wrappers with fixed bank depths; new `gvu_top.sv`, `gvu_pipe_controller.sv`, `gvu_vlsu.sv` and a top-level testbench |
| revision C | revision B with: the zero flag of the LUT lookup carried through the pipeline (the zero-flag output is gone); the missing `else` of `vstream_reg_int32` added; a load / store unit with an address mode per bank (fixed, increment, wrapped) and one address register for vbank#1 and one for vbank#2; corrections in the pipeline controller; most of the softmax data path of `gvu_top.sv` connected |

| revision D | revision C with: the round-shift units take one shift amount per lane; the clamp unit rewritten as `gvu_vector_clamp32` (signed 32 bits, scalar bounds, the upper bound tested first); the load counter of the LUT memory widened so that a write of 128 bytes advances the address; the byte mask of the SRAM wrappers one bit per byte; the narrow Scratchpad ports select the part of a word with the address delayed by the read latency; an address reset input on the load / store unit, raised by the controller when its state changes; a tile counter and the stage-3 data path of Softmax connected in the controller and the top level; test vectors for the LUT unit and Softmax |

The RTL-ref blocks follow **revision D** and were simulated against it (section 3), with one deliberate exception, the
round-shift of negative inputs (section 3.1). Sections 4 to 6 were written on revision B; where a later revision changes
a statement, the text says so. The Scratchpad port of revision A is no longer part
of the model.

## 2. Files

| File | Ported from | Content |
|---|---|---|
| `has/rtl_ref_gvu_fu.h` | `gvu_pkg.sv`, `pipeline_reg.sv`, `FUs/clz.sv`, `gvu_full_adder.sv`, `gvu_adder32_lane.sv`, `gvu_adder64_lane.sv`, `gvu_vadder32.sv`, `gvu_vadder64.sv`, `gvu_vcmp32.sv`, `gvu_vector_clamp.sv`, `gvu_vector_shift.sv`, `gvu_vroundshift*.sv`, `gvu_vmul32x32.sv`, `gvu_vmul48x40.sv` | value functions per lane and per vector, cycle models where the unit has registers, latency constants for the RTL parameters |
| `has/rtl_ref_gvu_lut.h` | `FUs/gvu_lut_mem.sv`, `gvu_lut_lookup_lane.sv`, `gvu_lut_unit.sv` | table memory with its write port, the five combinational segments of a lookup, cycle model of one lane and of the 32-lane unit |
| `has/rtl_ref_gvu_vrf.h` | `stream_switch/vrf.sv`, `vstream_reg_int32.sv`, `vstream_reg_int64_type0.sv`, `vstream_reg_int64_type1.sv`, `stream_vrf.sv` | register file with its port counts, the three stream-register kinds, the stream register file with its 19 ports |
| `has/rtl_ref_gvu_scratchpad.h` | `scratchpad/gvu_scratchpad.sv`, `sram_single_port.sv`, `sram_dual_port.sv` | four banks with the depths of `gvu_pkg.sv`, two buffers per bank, the accelerator ports, two-clock read path |
| `has/rtl_ref_gvu_vlsu.h` | `gvu_vlsu.sv` | five address registers with their address mode, port routing, mask expansion |
| `has/rtl_ref_gvu_ctrl.h` | the table-load states of `gvu_pipe_controller.sv` as connected in `gvu_top.sv` | table-load sequence built from the ported load / store unit, Scratchpad and table memory |
| `tools/has/tb_rtl_ref_gvu_fu.cpp` | | self-checking test of the functional units; `--dump <dir>` writes the vectors for the RTL oracle |
| `tools/has/tb_rtl_ref_gvu_g2.cpp` | | self-checking test of LUT, register file, stream registers, Scratchpad and load / store unit |
| `tools/has/tb_rtl_ref_gvu_g2dump.cpp`, `tb_rtl_ref_gvu_bdump.cpp` | | write stimulus and expected outputs, cycle by cycle, for the RTL oracle (register file; LUT, comparator, stream registers, Scratchpad, load / store unit) |
| `tools/has/tb_rtl_ref_gvu_cmp.cpp` | | report: the functional model against the RTL-ref blocks on the same vectors (section 4) |
| `tools/has/gvu_oracle/tb_oracle_fu.sv`, `tb_oracle_g2.sv`, `tb_oracle_b.sv`, `tb_oracle_directed.sv` | | SystemVerilog harnesses that replay the dumped vectors on the RTL and count mismatching beats |
| `tools/has/gvu_oracle/run_verilator*.sh`, `run_all.sh` | | build and run the harnesses with Verilator 5; `run_all.sh` does everything and prints a summary |

Build and run a C++ test from the repository root (no SystemC, no simulator needed):

```
g++ -std=c++17 -O2 -I. tools/has/tb_rtl_ref_gvu_fu.cpp -o tb_rtl_ref_gvu_fu && ./tb_rtl_ref_gvu_fu
g++ -std=c++17 -O2 -I. tools/has/tb_rtl_ref_gvu_g2.cpp -o tb_rtl_ref_gvu_g2 && ./tb_rtl_ref_gvu_g2
g++ -std=c++17 -O2 -I. tools/has/tb_rtl_ref_gvu_cmp.cpp -o tb_rtl_ref_gvu_cmp && ./tb_rtl_ref_gvu_cmp
```

RTL oracle (needs Verilator 5 and a copy of `neo_core/gvu` of revision C):

```
bash tools/has/gvu_oracle/run_all.sh <rtl gvu dir> <work dir>
```

It prints one `ORACLE <block> ... mismatches <m>` line per block and ends with `RESULT: <n> oracle lines, 0 with
mismatches, 0 harness builds failed`.

## 3. RTL-ref blocks against the RTL simulation

MEASURED with Verilator 5.052 on revision D. "Beats" are vectors for combinational units and clock cycles for units with
state; every output is compared at every beat.

| RTL unit | Beats | Mismatches |
|---|---:|---:|
| `clz` | 200,000 | 0 |
| `gvu_adder32_lane` (32 / 16 / 8 bit, add and subtract) | 400,000 | 0 |
| `gvu_adder64_lane`, no pipeline register | 400,000 | 0 |
| `gvu_adder64_lane`, with pipeline register | 100,000 | 0 |
| `gvu_vcmp32`, element-wise, values | 400,000 | 0 |
| `gvu_vcmp32`, element-wise and accumulate, random valid / ready, without output register | 200,000 | 0 |
| `gvu_vcmp32`, the same with output register | 200,000 | 0 |
| `gvu_vector_clamp32` (half of the vectors with the lower bound above the upper one) | 400,000 | 0 |
| `gvu_vector_shift` | 400,000 | 0 |
| `gvu_vroundshift64` (seven rounding modes, narrowing to 32 / 16 / 8), inputs that are not negative or shift 0 | 159,128 | 0 |
| `gvu_vroundshift72`, shift 0 to 127, inputs that are not negative or shift 0 | 209,287 | 0 |
| `gvu_vroundshift64` / `72`, negative input with a shift (section 3.1) | 140,872 / 190,713 | 109,484 / 158,755, not followed |
| `gvu_vmul32x32`, four modes | 300,000 | 0 |
| `gvu_vmul48x40`, four modes, each through its own interface | 300,000 | 0 |
| `gvu_lut_unit`, one lane, every output (lookups) | 300,000 | 0 |
| `gvu_lut_unit`, table load through the write port (done flag, address, counter, the four tables) | 6,000 | 0 |
| `gvu_lut_unit`, 32 lanes with one input per lane | 60,000 | 0 |
| table load of `gvu_top.sv` (controller states, load / store unit, Scratchpad, table memory at 1024 bits per write) | 10 cycles and the four tables | 0 |
| `stream_vrf` (the three stream-register kinds and the plain registers, 19 ports) | 6,000 | 0 |
| `gvu_scratchpad`, every accelerator port, depths of `gvu_pkg.sv` | 40,000 | 0 |
| `gvu_vlsu`, every output | 40,000 | 0 |
| `vrf.sv` | 20,000 | 0 |

Measured latencies of the RTL with its default parameters (cycles from an accepted input to its output): multiplier
32x32 = 4, 32x16 = 2, 32x8 = 1, 16x16 = 2; multiplier 48x40: 40x32 = 4, 32x32 + 16x8 = 4 (both products), 32x8 = 1,
3 x 16x8 = 1; round-shift = 1; adders, clamp, shift, comparator = 0; LUT direct = 0, log-scale = 2, piecewise-linear = 4;
Scratchpad read = 2.

### 3.1 Round-shift of a negative input in revision D

In revision D the round-shift takes the lane out of a packed array (`x[l] >>> in_shift[l]` with `x` declared
`logic signed [LANES-1:0][IN_W-1:0]`). An element of a packed array is unsigned, so the shift is logical. MEASURED: the
same 300,000 vectors give 0 mismatches on revision C and 109,480 on revision D, every one of them on a negative input
with a shift other than 0; inputs that are not negative, and a shift of 0, give none. Example: input all ones, shift 1,
saturating to 8 bits: the RTL returns `0x7F` with the overflow flag, the value is -1. The ported unit keeps the
arithmetic shift of revision C; the replay counts these beats apart (`KNOWN` lines). The Softmax vectors of the hardware
team do not reach it, because every value that passes the round-shift there is non-negative.

### 3.2 Test vectors of the hardware team

`tools/has/tb_rtl_ref_gvu_hwvec.cpp` runs the vectors delivered with revision D on the ported units, composed as the
three Softmax stages of `gvu_top.sv`. MEASURED: exponential table 256 values, reciprocal 64, inverse square root 256,
Softmax 64 and 4,096 values (64 rows of 64): none different from the expected outputs of the hardware team. The same
rows give the same result with ties rounded up, so the vectors do not separate the two rounding modes. The tables of the
vectors are Q15 capped at 32,767; the functional model generates Q14 tables by default.

Behaviours of the RTL that the ports follow as written (each was reproduced on the simulation):

- `gvu_adder64_lane` with the default parameters ignores its subtract inputs on the combinational path: it always adds.
  Only the path with the pipeline register subtracts. The pipeline drawings use this adder for additions only.
- Memory wrappers of the Scratchpad: revision D writes one byte per mask bit and the port follows it (40,000 cycles,
  no mismatch). Up to revision C the byte mask indexed the data by the mask bit number instead of eight times that
  number; the port reproduces that with `wmask_as_written`.
- LUT table load: the write counter has 3 bits and the address increment 7 bits. With 128 bits per write (the default
  of the unit) the done flag rises after 7 writes and the address returns to 0 after the 8th, so only 128 of the 256
  entries of the activation and exponential tables and 64 entries of the other two can be written. With 1024 bits per
  write (the value `gvu_top.sv` uses) both tables are loaded completely in revision D (the address increment has 8
  bits there), taking 9 cycles from the start to the first softmax state. Up to revision C the increment had 7 bits and
  the upper half of the exponential table was never written.
- 48x40 multiplier: the pipeline of an interface advances only while the mode input selects that interface, and the data
  registers are shared. A mode change between two modes of the same latency while a beat is in flight is accepted, and
  the earlier beat later appears with the product of the later one.
- Scratchpad narrow ports: the part of a word is selected by the address present in the cycle the data is output, two
  clocks after the read access.
- Load / store unit: the first address of a burst is the start address of the clock before the request; the counters do
  not stop at the depth of a bank. In the fixed mode the register follows the start address input during the burst; in
  the wrapped mode it keeps its value (no wrap bound exists). The register of vbank#1 advances with a request even when
  the port select names no port.
- Comparator without output register: a beat is accepted only while the consumer is ready, including an accumulate
  beat that produces no output.
- Scratchpad DMA access: the port exists and is connected by `gvu_top.sv`, but no bank is driven from it.
- LUT unit, log-scale mode: the result leaves after the second pipeline register while the beat goes on to the fifth,
  so a result is lost when the consumer is not ready in that cycle.

Corrected by the hardware team in revision C (the ports no longer reproduce them): `vstream_reg_int32` in the 4 x int8
mode now follows the ready inputs of the int8 ports; a zero input of the LUT unit takes the same number of cycles as any
other input, keeps its place in the stream and returns 0 (MEASURED: streams with one zero beat in four, latency 4 in the
piecewise-linear mode and 2 in the log-scale mode).

Not compared value by value: the softmax stages of `gvu_pipe_controller.sv` in `gvu_top.sv` (section 6), the Scratchpad
without its pipeline registers, addresses at or above the depth of a bank, the spill register variant of the pipeline
registers.

## 4. Functional model against the RTL

"Now" is the behaviour of `has/gvu_*.h` in the configuration of the instruction path with the five vector-unit knobs
(`HAS_GVU_TOP`, `HAS_GVU_STALL`, `HAS_GVU_ELEM_CLK`, `HAS_GVU_REGS`, `HAS_GVU_AGU_ND`) set to 0, the model's own epilogue, add and
max pool. In the new default these three operations run on `GvuTop`, which is made of the RTL-ref blocks of section 3
(bit-exact against the RTL simulation); the differences listed in this section for them are then not reached. They remain
for Softmax, LayerNorm and FUSED_ATTN, which stay on the functional models (`KNOWN_LIMITATIONS.md`, O29). The column "With
`HAS_SPEC=rtl`" applies when that group of knobs is switched on. The mismatch counts come from
`tools/has/tb_rtl_ref_gvu_cmp` (MEASURED on the RTL-ref blocks, synthetic vectors with boundary values; they are not
data of a network). Both networks run with `HAS_SPEC=rtl` are bit-exact against their reference images, so none of the
differences below is reached by YOLOv8m or ViT-B/16 as planned today.

### 4.1 Summary

| Block | Model function | Values now | With `HAS_SPEC=rtl` | Timing now | To change |
|---|---|---|---|---|---|
| Round-shift, 64 bit | `rshift` (`gvu_quant.h`) | equal for all seven modes, 0 of 300,000 per mode | equal | counted as 1 stage; RTL 1 | nothing for values; rounding-mode codes differ (map by name) |
| Round-shift, 72 bit | `rshift_wide` (`gvu_layernorm.h`) | equal for shifts up to 72; for shifts of 73 and more the three nearest modes of the RTL return 1 for non-negative inputs | same | 1 stage | measure the largest shift LayerNorm uses; keep the model function above 72 and report it to the hardware team |
| Multiplier 32x32 | product inside `requant`, `dequant` | product kept in 128 bit, operands 64 bit; RTL operands are 32 bit | same | 5 / 3 stages; RTL 4 / 2 / 1 | check operand ranges on network data; take latencies from the RTL |
| Multiplier 48x40 | wide products of LayerNorm | mathematical value in 128 bit, no mode assignment | same | no figure | assign each LayerNorm stage to a multiplier mode (section 5); output is 72 bit |
| Requant (multiply, round-shift, zero point, clamp) | `requant` | differs when the narrowed value plus the zero point leaves int16: the RTL adder wraps at 16 bit, the model clamps. 52,877 of 500,000 synthetic vectors | equal, 0 of 500,000 (`zp_add_wrap16`) | part of `lat_obp` / `lat_add2` | decide the default of `zp_add_wrap16` once the hardware team confirms the wrap is intended |
| Dequant | `dequant` | equal, 0 of 500,000 | equal | part of `lat_add1` | nothing |
| 32-bit add of the residual pipeline | `add_sat32` | saturates; the RTL adder wraps. Equal while no int32 overflow occurs (0 of 500,000 for operands below 2^24; 125,153 of 500,000 over the full range) | no knob | counted as 1 stage; RTL 0 | add a knob for the wrapping adder; count int32 overflows on both networks |
| Sums held as int16 / int32 in the drawings | attention row sums, softmax sigma, LayerNorm sum and variance | kept in 64 bit, overflow only counted | same | | narrow to the register width of the drawing behind a knob |
| Clamp | `clamp128` | equal for signed scalar bounds | equal | counted as 1 stage; RTL 0 | per-lane bounds and the unsigned mode are not modelled (not used by the pipelines) |
| Comparator | row maximum of softmax, max-pool window, lower bound in LayerNorm stage 3 | equal results | equal | counted as 1 stage; RTL 0 by default | the RTL has no comparison flag output that LayerNorm stage 3 needs for its multiplexer: open question |
| LUT, direct | `lut_direct` | table address = input + 128. Revision B does the same for the activation table (`i_lut_compens`) | equal | 1 stage; RTL 0 | nothing for values |
| LUT, piecewise-linear | `LutIndirect` | equal with the default tables (0 of 1,496,409 for recip, 0 of 1,496,428 for rsqrt) | equal | `lut_stages` = 4; RTL 4 | interpolation difference is signed 8 bit in the RTL and `W` and the interpolation shift are inputs: add them as parameters (the controller of revision B fixes W = 16, shift = 8) |
| LUT, log-scale | `LutLogScale` | equal, 0 of 600,000 | equal | 3; RTL 2 | not used by the instruction path; revision B fixes the mode to piecewise-linear |
| LUT, zero input | `lut_zero0` | returns 0, as the RTL | equal | | nothing: in revision C a zero input passes the pipeline like any other input |
| LUT, table load | activation table through the host bus, recip / rsqrt built in the code | no load of recip / rsqrt / exp | same | not counted | revision B loads every table from vbank#1 through the pipeline controller before the first softmax: add the load sequence and its cycles |
| Stream register file | `vrf_fits` (`gvu_vrf.h`) | same organisation as `stream_vrf.sv` (3 x int64 + 4 x int32, 1.25 KB) | | capacity check only | no data, no valid / ready, no flush in the model; needed for a cycle-level pipeline |
| Scratchpad | `HasScratchpad`, `gvu_sp_map.h` | flat byte array, capacities of the default split | | read counted as 1 stage | section 4.9 |
| Load/store unit | `gvu_lsu.h` | port table and a 4-D address generator of the specification | | not in the instruction path | revision B has an address counter per port only; keep the specification model, add the RTL one |
| Pipeline controller | `gvu_pipeline_ctrl.h` | the three state diagrams of the specification, not connected to the instruction path | | costs per state are assumptions | section 6 |

### 4.2 Rounding and narrowing configuration

The pipeline controller of revision B drives both round-shift units with round-to-nearest, ties away from zero, and
narrowing by **wrapping** (to 32 bit on the 72-bit unit, to 16 bit on the 64-bit unit). The model's requant narrows to
int16 by **saturation** by default (`req_narrow`). The two agree while the value fits 16 bit. Action: compare on network
data and ask the hardware team whether wrapping is the final configuration.

### 4.3 Shift amount per lane

Revision D gives the round-shift units one shift amount per lane, which settles the question below for them. As written
for revisions A to C: both round-shift units and the shift unit take one shift amount for all 32 lanes. The LUT unit returns one `E_shift` per
lane, and the model shifts every row by its own amount in softmax stage 3 and LayerNorm stage 4. With the RTL as
delivered, 32 rows with different exponents cannot pass the round-shift in one beat. This is the main structural
question for softmax and LayerNorm; the softmax data path of `gvu_top.sv` has to be read and simulated to settle it.

### 4.9 Scratchpad

| Item | Revision B | Model now | To change |
|---|---|---|---|
| Capacity per half | sbank 768 words = 3,072 B; vbank#0 800 rows of 32 B = 25,600 B; vbank#1 32 rows of 128 B = 4,096 B; vbank#2 256 rows of 128 B = 32,768 B; total 65,536 B | 10,784 / 5,600 / 43,008 B + sbank 6,144 B (three banks larger than the RTL) | preset with the RTL depths: `HAS_SP_PRESET=rtl` (not the default); section 4.10 |
| LayerNorm, 768 channels (DERIVED) | vbank#2 holds 10 rows of 768 int32 values; sbank holds 3,072 B | 14 rows; gamma + beta need 6,144 B with one word per parameter | more row groups, so more cycles on ViT-B/16; the sbank does not hold gamma + beta of 768 channels in either layout (3,840 B at 5 B per channel): open question |
| Logical ports | sbank: int32 and int8 (the int8 port is byte addressed); vbank#0: int8; vbank#1: int32, int16, int8; vbank#2: int32, int8 | port table of the specification | matches the specification now; `sp_s_word32` (one word per int8 parameter) no longer follows the RTL |
| Read latency | 2 cycles (memory output register plus a pipeline register per bank), MEASURED | 1 stage | add one cycle per scratchpad read in the stage depths |
| Buffer select | one select input per bank; the core side uses one half, the DMA the other | ping-pong as a scheduling formula | keep; model the select per bank when the scratchpad is ported |
| DMA port | declared (byte address, data, write mask) and not connected to any bank | cost formula | wait for the hardware team to define it |
| Storage unit | rows of 32 lanes; word address on the core side | byte address, any length | ported in `has/rtl_ref_gvu_scratchpad.h`; the instruction path keeps the byte model |

### 4.10 Scratchpad with the RTL depths, value ranges

`HAS_SP_PRESET=rtl` selects the bank depths of the RTL in the functional model (the other knobs stay at their defaults).
MEASURED on the transformer check of the regression (14 instructions): 285,205 vector-unit cycles with the default
configuration, 307,045 with the preset. With these depths a LayerNorm of 768 channels runs 10 rows per group instead of
14, and the scalar bank (3,072 B) does not hold gamma and beta of 768 channels; the model counts the instruction and
continues. The two networks have not been run with the preset.

`HAS_ARITH_RTL=1` routes the arithmetic of the functional model through the RTL-ref blocks (`has/gvu_rtl_arith.h`):
`requant`, `dequant`, the 32-bit add and the lookups of `LutIndirect` and `LutLogScale` compute their own result, then the
composition of ported units of section 4.1; when the operands fit the units, the value of the units is the one the
pipeline uses, and every call is counted as equal or different. Off by default; with it off nothing changes (MEASURED:
regression of the functional model, 27 of 27). MEASURED on the transformer check with the knob on: 14 of 14 instructions
bit-exact, 285,205 cycles; 570,028 requant calls, 1,342 reciprocal and 256 inverse-square-root lookups through the
units, none different from the model arithmetic and none outside the operand widths of the units. Not routed: the
exponential table and the comparator of softmax, the wide multiplications and round-shifts of LayerNorm, the sums of
FUSED_ATTN, average pooling and the activation table.

`HAS_ELEM_RTL=1` takes ELEM_WISE ADD and MAX_POOL from the cycle-level pipelines of `has/rtl_ref_gvu_elemwise.h`
(`has/gvu_rtl_elem.h`): the functional model computes a chunk first, the pipeline then runs on the same data with the
ported load / store unit and Scratchpad, every element is compared, and the value and the compute cycles of the chunk
are those of the pipeline. Chunking, the DMA and the ping-pong schedule stay those of the model; the separable max pool
is not expressible and keeps the model. Off by default; with it off nothing changes (MEASURED: regression of the
functional model, 27 of 27, two-instruction YOLOv8m window 1,658,986 cycles). MEASURED instruction by instruction with
the knob on: the 15 ELEM_WISE instructions of YOLOv8m (12 ADD, 3 MAX_POOL 5 x 5) are bit-exact with no element
different from the model; an ADD takes the same number of cycles (its compute time stays hidden behind the DMA), a
MAX_POOL takes 268 cycles more. MEASURED on the whole YOLOv8m with the knob on: 98 of 98 instructions bit-exact,
62,666,994 cycles (871 above the reference run), 6,374,400 residual-add and 345,600 max-pool elements with none
different from the model. ViT-B/16 has not been run as a whole network with the knob. MEASURED outside the instruction
path (`tools/has/tb_rtl_ref_gvu_replay.cpp`): the operands of every ELEM_WISE instruction of both networks replayed on
the pipelines give the golden image (YOLOv8m 12 ADD and 3 MAX_POOL, ViT-B/16 25 ADD; neither network has an average
pool).

`has/rtl_ref_gvu_requant.h` composes the Requant pipeline and the GEMM_FUSED epilogue from the ported units along the
register cuts of the specification: multiplier 4 + register 1, round-shift 1 + register 1, zero-point add + register 1,
clamp 0 = 8 cycles; with the int8 register and the direct activation table = 9 cycles. MEASURED against the functional
model (`tools/has/tb_rtl_ref_gvu_requant.cpp`): 62,062 beats, every element equal, every beat leaves after exactly 8 / 9
cycles. The depths are a consequence of the composition, not a measurement on an RTL pipeline: the RTL has none.

By default the epilogue of the instruction path stays a function on the partial-sum write with a tail of 10 cycles per
core pass. `HAS_OBP_RTL=1` (`has/gvu_rtl_obp.h`) sends every beat through the cycle-level epilogue as well: the
functional model computes the beat first, the pipeline then runs on the same 32 lanes with the scale, shift, zero point
and activation table of the instruction, every masked lane is compared, the value written is the one of the pipeline,
and the tail of a core pass is the depth of the pipeline (9 cycles). The write still lands in the cycle of the
partial-sum write: the pipeline is run to completion inside the call, so this is not a clocked module between the
partial-sum manager and the SRAM. Off by default; with it off nothing changes (MEASURED: regression of the functional
model, 27 of 27, two-instruction YOLOv8m window 1,658,986 cycles). MEASURED with the knob on: that window (two
convolutions, 252 tiles, 432 core passes, one of them with an input-channel split) is bit-exact, 29,808 beats and
596,736 lanes with none different from the model and every beat leaving after 9 cycles, 1,658,651 cycles (335 fewer;
the rest of the 432 is absorbed by the alignment with the DMA); three detection-head convolutions of YOLOv8m, 108,544
lanes, none different, also with `HAS_SPEC=rtl`. MEASURED on the whole YOLOv8m with `HAS_ELEM_RTL=1` and `HAS_OBP_RTL=1`:
98 of 98 instructions bit-exact, 1,996,784 beats and 52,017,408 lanes with none different from the model, every beat
leaving after 9 cycles, 62,661,785 cycles (5,209 fewer than with `HAS_ELEM_RTL=1` alone, for 7,695 core passes).

`HAS_GVU_TOP=1` or `2` (`has/gvu_top_bridge.h`; no longer the default, which is level 3, `GVU_TOP_CORE_INTEGRATION.md`) runs the GEMM epilogue and the residual add on the vector-unit top
level `GvuTop` instead, through the nine-stage stream mode of its GEMM_FUSED state; it wins over `HAS_OBP_RTL` and
`HAS_ELEM_RTL`, and max pool stays on the pipeline of `HAS_ELEM_RTL`. `HAS_OBP_DUMP=<file>` together with
`HAS_OBP_RTL=1` writes every activation table and epilogue beat to a file for `tools/has/tb_gvu_top_replay.cpp`.
MEASURED on the window above: bit-exact, 1,658,651 cycles as with `HAS_OBP_RTL=1`, and 1,650,477 with `HAS_GVU_TOP=2`,
which charges the activation table as `GvuTop`'s own path; details in `VERIFICATION_REPORT.md`, V35; the connection levels
are described in `GVU_TOP_CORE_INTEGRATION.md`, section 1.

`HAS_PSM_PROBE=1` (`has/gvu_psm_probe.h`) observes, per core pass, how the partial-sum manager uses the PSUM SRAM and
prints a table when the program exits; it changes no value and no cycle count (MEASURED: the same three programs give
the same cycle counts with it on). It answers what a clocked epilogue between the partial-sum manager and the SRAM,
whose write lands 9 cycles later, would meet. MEASURED on the window above, on the three detection-head convolutions
and on the transformer check:

| Question | Measured |
|---|---|
| Is a word written more than once in a pass | Yes, twice (38,016 writes on 18,900 words in the window); the lane masks of the two writes never overlap. Half of the writes of a tile with full vectors carry an empty mask |
| Does the manager read a word it has already written in the pass | Yes (1,944 reads in the window). On passes with the epilogue the smallest distance from the write to the read is 64 cycles; none at 9 cycles or less. On the FUSED_ATTN products (no epilogue) the smallest distance is 10 cycles |
| Rhythm of the writes | Runs of one write per cycle, at most 64 cycles long, more than 64 cycles apart |
| Output channels under the lanes of one write | Always one, also when the positions of a channel are not a multiple of 32 (22,320 such writes in the window) |
| Cycles from the last write of a pass to the done signal of the core | Always 4 |

On these programs a write delayed by 9 cycles would therefore meet no overwritten lane, no read of a stale word, no need
for back-pressure and no beat with two scales, and its last write would land 5 cycles after done. Not measured on a
whole network.

`HAS_RANGE_PROBE=1` prints, when the program exits, the range of every value of the vector-unit arithmetic and how many
values do not fit the container the RTL gives them (multiplier operands, 64 / 72-bit round-shift inputs, shift fields,
int16 / int32 registers). It only observes: the cycle count and the results of a run do not change (MEASURED on the
transformer check). On that check no value is outside its container.

### 4.11 What the default flow assumes about the RTL

The new default (`GVU_TOP_CORE_INTEGRATION.md`, `GVU_DFC_REFERENCE_FLOW.md`) assumes the RTL with the patches of the hardware
feedback package at these points. The model is built so that each assumption is a parameter or a separate code path.

| Id | The model assumes | The RTL as delivered | Where in the model |
|---|---|---|---|
| B13 | all four bytes of a GVU register can be written | only byte 0 | `GvuConfigParams::b13_fixed` (`has/rtl_ref_gvu_config_regs.h`) |
| B17 | the register index comes from the word address (`addr[5:2]`) | from the byte address | `GvuConfigParams::word_addressed` |
| B1 | the GEMM_FUSED result is written to `vbank#2` | the read strobe is raised instead | not modelled: the result goes back into SRAM-C (question F8) |
| B9 | the end flag is the beat that carries `oc_last & oc_tile_last`, qualified with `i_psum_valid` | the first flag delayed by a fixed 9 beats | `GvuTop::stream_exit` (`has/rtl_ref_gvu_top.h`) |
| B18 | the address generators hold their address on idle cycles of the stream | they return to the first address | not visible: scale and shift travel with the packet |
| F1′ (version 3) | `oc_tile_last` sits on the packet that holds the last element of the last context | on the first `oc_last` packet of the call | `StreamPacket::call_last` (`has/gvu_top_stream.h`, `has/has_npu_top.h`) |
| F2b | the OC index travels with the packet | the scale index is a counter that runs on across contexts | the model carries scale and shift with the packet and counts the beats where the RTL's index would be wrong |
| B12 | the pooling feeder is reset between max pool runs | its counter keeps its value | `GvuTopStream::reset_unit` before each run; the patched RTL run back to back gives the same trace (`VERIFICATION_REPORT.md`, V39) |
| B19 | the round-shift is arithmetic for negative inputs | logical (the packed element is unsigned) | the frozen port keeps the arithmetic shift |
| R3 (proposal) | registers 9 to 15 and the residual add path | neither exists | `GvuTopStream::job_step_add_regs` |

## 5. Pipeline depths

Stage counts per pipeline stage: (a) the specification (cuts of the pipeline drawings plus the depth range of each
functional unit), (b) the cuts of the drawings plus the measured latency of the RTL units, one stream register per
step, (c) the figure the model uses. (a) and (b) are DERIVED; (b) does not yet include the second cycle of a scratchpad
read (section 4.9).

| Pipeline stage | (a) specification | (b) drawing + RTL | (c) model |
|---|---|---|---|
| GEMM_FUSED epilogue | 9 to 12 | 9 | 10 |
| Softmax stage 1 | 2 | 2 | 3 |
| Softmax stage 2 | 4 | 3 | 5 |
| Softmax stage 3, piecewise-linear LUT | 17 to 23 | 18 | 18 |
| FUSED_ATTN phase 1, tail after the end of a query row | 12 to 16 | 14 | no separate figure |
| FUSED_ATTN phase 3, tail | 9 to 13 | 11 | no separate figure |
| LayerNorm stage 1 | 2 | 2 | 3 |
| LayerNorm stage 2 | 8 to 11, plus a tail of 9 to 13 | 13, plus a tail of 10 | 5 |
| LayerNorm stage 3 | 9 to 12 | 10 | 10 |
| LayerNorm stage 4 | 16 to 22 | 17 | 10 |
| LayerNorm stage 5 | 14 to 18 | 16 | 10 |
| Residual add stage 1 | 7 to 10 (9 to 12 on the branch through the 48x40 multiplier) | 8 (10) | 8 |
| Residual add stage 2 | 8 to 11 | 9 | 10 |
| Max pool | 2 | 2 | 3 |
| Average pool | 2, plus a tail of 4 to 6 | 2, plus a tail of 4 | 10 |

These depths are added once per row group, not per element. ESTIMATE of the effect of moving the model from (c) to (b)
on ViT-B/16: about 11,600 cycles of 41.68 million.

Multiplier modes the drawings need: 32x32 + 16x8 in LayerNorm stage 2; 3 x 16x8 in FUSED_ATTN, softmax stage 3 and
LayerNorm stage 5. These are the two modes corrected in revision B.

## 6. What revision B covers at the level of a whole pipeline

| Item | State in revision B |
|---|---|
| Softmax, three stages, input from the scratchpad | controller states, load/store requests, stream-switch and unit configuration present; one row group per run |
| LUT load from the scratchpad | present (recip, exp) |
| Softmax fed from the systolic array (FUSED_ATTN phase 1) | input select is fixed to the scratchpad; marked to do |
| LayerNorm | five state names only |
| Residual add, max pool, average pool, requant / dequant alone, GEMM_FUSED epilogue | not present |
| Load/store unit | one address counter per port (revision C: per bank, with an address mode), start address from the controller; no 4-D stride mode, no direct write to the system bus |
| `gvu_top.sv` | MEASURED (`tools/has/gvu_oracle/run_verilator_top.sh`, with a stand-in for the `shift_reg` module that is not part of the delivery): it elaborates; with a token length of 8 the controller loads the tables, runs stage 1 and stays in stage 2 for good, the done output never rises. The inputs of both multipliers, both round-shift units, the clamp and two adders, and every write-data input of the Scratchpad are not connected, so no softmax result is produced. Not instantiated by `neo_core.sv` |
| Top-level testbench | starts a softmax of 256 tokens and waits for the done signal; no data is loaded and no result is checked |
| Interface between the partial-sum manager and the vector unit | ports `i_psum_valid`, `o_psum_ready`, `i_psum_data` (32 bit) exist at the top level and are not used |

Revision C, MEASURED with the same script and two stand-in modules (`shift_reg`, and `spill_register_flushable`, which
the stream registers of the top level now need; both are written for this harness and are not the hardware team's
sources): the softmax of 8 tokens runs to the end -- table load, stage 1 from cycle 9, stage 2 from 19, stage 3 from 31,
done at cycle 57 -- with 8 rows written to vbank#0. The values written were not compared: the byte mask of the memory
wrappers and the load of the exponential table (section 3) are unchanged, and the attention scale and shift inputs are
not used yet. The statements of the table above and of the next paragraph describe revision B.

Revision D, MEASURED with the same script: the softmax of 8 tokens and one tile runs to the end, done seen before
cycle 60, 8 rows written to vbank#0; the attention scale, shift and zero point are connected. The values of this data
path were compared through the vectors of the hardware team (section 3.2).

Consequence for the model: no pipeline can be compared with the RTL end to end yet, softmax included. From the top
level only the table-load sequence and the timing of softmax stage 1 can be taken (table load: 4 writes for the
reciprocal table, the exponential table is loaded incorrectly; stage 1 of 8 tokens: 10 cycles). Every pipeline keeps the
control of the model, with the units and depths of sections 4 and 5.

## 7. Order of work

1. RTL-ref blocks after revision B: done (section 3).
2. Scratchpad preset with the RTL depths in the functional model: done for the preset and the single instructions; the
   two networks remain to be run with it.
3. Arithmetic behind a knob: `requant`, `dequant`, the 32-bit add, the LUT lookups call the RTL-ref blocks; default
   unchanged; with the knob both networks must stay bit-exact or every difference must be explained. Done for the knob
   (`HAS_ARITH_RTL`, section 4.10) and the transformer check; the two networks remain to be reported.
4. Softmax as a whole pipeline against `gvu_top.sv`: controller, load/store unit, scratchpad, LUT load.
5. Stage depths of section 5 behind a knob for the pipelines without an RTL controller.
6. LayerNorm, residual add and pooling composed from the RTL-ref blocks following the pipeline drawings, compared with
   the functional model on vectors exported from the two networks.
7. Interface between the partial-sum manager and the vector unit (valid / ready stream, start / busy / done handshake,
   write path to the input-feature-map SRAM) as a proposal of the model, behind a knob.

## 8. Open questions for the hardware team

- Round-shift of a negative input in revision D shifts logically (section 3.1).
- LUT table load: 3-bit write counter against tables of 16 to 64 writes (section 3).
- Scratchpad DMA access: address map per bank, data width, and how data and LUT tables reach the Scratchpad.
- 48x40 multiplier: mode change with a beat in flight (section 3).
- Load / store unit: start address one clock ahead of the request; no check against the bank depth.
- `gvu_adder64_lane`: subtract inputs without effect on the combinational path.
- `gvu_vroundshift72`: result for shifts of 73 and more in the three nearest modes; largest shift the pipelines use.
- LUT unit: in log-scale mode a result is lost when the consumer is not ready.
- Load / store unit: meaning of the wrapped address mode (no bound input exists); the address register advancing while
  no port is selected.
- One shift amount for 32 lanes against one `E_shift` per lane (section 4.3).
- Wrapping zero-point adder and wrapping narrowing: intended configuration?
- Scratchpad: gamma and beta of 768 channels against an sbank of 3,072 B; bank sizes for a model dimension of 768.
- `gvu_pipe_controller.sv`: `o_sbank_rd` is used without a declaration.
- `gvu_top.sv`: sources of `shift_reg` and `spill_register_flushable`; width of `i_psum_data` (one int32 against a vector of 32 lanes).
- Whether the stage count of a functional unit in the specification includes the stream register after it; final values
  of the pipeline parameters of the round-shift, the comparator and the LUT.
- LayerNorm stage 3 needs a comparison flag that `gvu_vcmp32` does not output.
- Dequant: zero point added after the round-shift in one drawing and subtracted before the multiplier in another.
