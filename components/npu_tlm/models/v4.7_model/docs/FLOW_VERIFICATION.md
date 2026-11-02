# Verification by Instruction Flow

This document explains what a PASS of the vector unit (GVU) connected to the SAURIA core means. Each operation of the
vector unit runs as a flow of numbered steps: the steps of the flow drawings of the vector unit. For every step the
tool `tools/has/verify_flows.py` names the checks that look at that step, reads their results from the logs of one
regression run, and writes a log that says, for each check:

- what the step does (the step text of the drawing, restated so that the log reads without the drawing);
- what is checked;
- the reference it is compared with, and the kind of that reference (section 2);
- how it is compared (every output, every cycle, a count, a digest);
- the measured result and the log line it was read from;
- the verdict.

A step is PASS only when every check of the step passes. Negative checks run the same comparison against a reference
that is wrong on purpose and must find the error; they show that the comparison can fail.

## 1. Running it

```bash
bash tools/has/regress_gvu.sh <output dir>           # the last item runs verify_flows.py
python3 tools/has/verify_flows.py <output dir>        # or on the logs of an earlier run
```

Output: `<output dir>/flows/<FLOW>.log` (one per flow) and `<output dir>/flows/SUMMARY.md` (one row per step). The exit
code is 0 when no step fails and every step is checked at the step or end to end. `--out <dir>` writes the logs
elsewhere. The tool only reads logs: it runs nothing and changes nothing.

## 2. Reference kinds and verdicts

| Kind | Reference | What a PASS shows |
|---|---|---|
| R | RTL simulation: the same inputs were run on the hardware RTL (Verilator or ModelSim); its outputs are stored as vector files or a digest of them (`tools/has/gvu_oracle/rtl_trace_digest.txt`) | the model gives the outputs of the RTL, cycle by cycle where the check says so |
| S | Specification: a reference written apart from the model, from the micro-architecture specification or the RTL source as written (Python programs such as `tools/has/ref_program.py`, formulas in the bench, vectors and golden outputs delivered with the RTL) | the model computes the function as specified |
| D | Proposed definition: the RTL has no such function yet; the definition is in the named document and an independent reference implements it | the model follows the proposal; the hardware has to decide it |
| M | Model only: two paths of the model agree (for example the clocked unit and the call-style unit) | the two paths are consistent, not that the function is right |

| Verdict | Meaning |
|---|---|
| PASS | every check of the step passes |
| PASS (end to end only) | no check looks at the output of this step alone; its correctness follows from the final result of the flow |
| PASS, OPEN: known RTL difference (note) | the model follows the specification; the delivered RTL differs here, the note says how and the regression measures the size of the difference |
| FAIL | a check does not hold |
| NOT RUN | the log of a check is missing |
| OPTIONAL | the input of the check is not in this package (vectors of the hardware team); the step is decided by its other checks |

The R checks rerun on any machine without the RTL: `tools/has/gvu_oracle/check_rtl_trace.sh` regenerates the vector
files of the stored RTL runs from the model and compares their digest with the stored one (`same <file> <n> beats`).

## 3. Log format

One step of `MAX_POOL.log`:

```
[MAX_POOL] [step 4] The address generator produces vbank#0 addresses, the pooling feeder spreads the window (mode = k - 2, stride 3 bits).
  check : load / store unit (address registers, three address modes, port routing, mask), every output every cycle
    reference : (R, RTL simulation) gvu_vlsu.sv simulated in Verilator with the same inputs
    method    : this tree regenerates the vector file and its digest must equal the digest stored after the RTL run (0 mismatches there)
    result    : 40000 cycles, digest equal
    evidence  : rtl_trace.log:51: same    vlsub.txt              40000 beats
    verdict   : PASS
  check : addresses of the pooling mode against the same walk written as an N-D address program
    reference : (D, proposed definition) AGU_ND proposal (SLICE_CONCAT_ON_GVU.md)
    method    : address by address
    result    : 189 runs, 11520 reads, 0 differ
    evidence  : tb_gvu_agu_nd.log:3: T3 AGU_POOLING = AGU_ND program: 189 runs, 11520 reads (want 11520), 0 runs without last write, 0 differ
    verdict   : PASS
  => step 4: PASS
```

`evidence` is the file and line in the regression output that holds the number; the source of each bench is under
`tools/has/`.

## 4. Flows and results

Ten flows: the nine flows of the vector unit and slice / concat by the 4-D DMA. 62 steps and one whole-flow row for
five of the flows: 67 rows. MEASURED (`bash tools/has/regress_gvu.sh`, default configuration):

| Flow | Steps | Main checks and references | Result |
|---|---|---|---|
| GEMM_FUSED | 7 + whole | register writes of the data flow controller (DFC) read back; LUT load against the RTL (R); stream FIFO of 4 packets and the core stop; requant chain on 25 RTL vector files (R) and on the vectors of the vector-unit owner (S); write-back 10 cycles after the push; end-of-layer flag | every step PASS; step 5 OPEN: the RTL reads Out_Scale with an index that is not the packet's channel on 1,168 packets of the RTL run (`GVU_RTL_CONFORMANCE.md`, section 8) |
| MAX_POOL | 7 + whole | cmax and channel runs of the SPPF layer (S); max pool by registers against the call style (M); load / store unit and comparator against the RTL every cycle (R); the hardware team's tile and vec sets (S); 9 runs back to back with the feeder reset, and the negative check without it (97,557 of 115,200 outputs wrong) | every step PASS |
| AVG_POOL | 7 | register read-back; state sequence IDLE -> POOLING_AVG -> IDLE in 18 cases; tail of 5 cycles against an independent reference (58,784 lanes); datasets of the vector-unit owner; negative check with the rounding of the delivered RTL (12 cases differ); done 5 cycles after max pool | every step PASS; step 1 (DMA load) end to end only |
| ADD | 7 + whole | stage 1 and stage 2 against `golden_sum.txt` and the golden output of the vector-unit owner (S); both multipliers against the RTL, 300,000 vectors each (R); AGU_ND programs (D) | every step PASS |
| SOFTMAX | 7 + whole | register writes per group; buffer select flips once per instruction on three ViT-B/16 heads (S) and the negative check with the delivered level flip (1,686 to 5,156 of 6,304 outputs wrong); ST1 row maxima, ST2 sums, ST3 outputs per head (S); the stage-3 shift of the delivered RTL counted as a negative check (326 to 3,443 of 6,304) | every step PASS; steps 3 and 6 OPEN (buffer flip, stage-3 shift: changes proposed to the hardware team) |
| FUSED_ATTN | 7 | phase products, scores and outputs against `has::` references of the specification (S); requant job of each phase, 327,958 lanes, 0 different | every step PASS |
| LAYERNORM | 7 + whole | the lane machine stopped at the end of each tier and compared with the row LayerNorm of the specification: S, V, v (tiers 1 to 2), w, r, E (tier 3), n_k, y_k (1,714 tokens, 647,785 elements); beta / gamma placement; negative check with a wrong variance scale (32 of 32 tokens differ); MAS vectors | every step PASS; no RTL exists, the cycle count is an estimate |
| DMA_COPY | 3 | 15 push-time cases, 10 refused and 5 accepted as expected; every COPY of the check program against the cycle formula of `DMA_TENSOR_MODE.md`, section 4 (D); 25-step check program against `ref_program.py` (S) | every step PASS |
| MOVE | 5 | descriptor packing; MOVE against its definition every cycle (D); N rows in N + 3 cycles | every step PASS; MOVE is a proposal in `has/gvu_agu_nd_move.h`, not on the instruction path |
| DMA_SLICE_CONCAT | 5 | knob off refuses modes 6 / 7; 4-D descriptors of `tb_has_dma`; 256 KB tensors; 27-instruction program against `ref_program.py` (S); tile gather / scatter | every step PASS (option `HAS_DMA_ND`, on by default) |

Summary of the run: 67 of 67 rows PASS; 1 step end to end only; 3 steps PASS with a known RTL difference.

## 5. What a PASS does not cover

- Steps with only M or D references show consistency with the model's own other path or with a proposal, not with
  hardware. The table in `SUMMARY.md` lists the reference kinds of every step.
- The cycle counts of LayerNorm, MOVE and the DMA flows have no RTL to compare with; they follow the definitions
  named in the logs.
- Checks on the vectors delivered with the hardware RTL (Softmax and exponential table vectors) are OPTIONAL in this
  package because those vectors are not shipped; on a machine that has them the same tool decides them.
- Whole networks are not part of the regression; their results are in `VERIFICATION_REPORT.md`.

## 6. Adding a check

A flow is a list of steps in `tools/has/verify_flows.py`; each step lists `C(...)` checks (and `N(...)` negative
checks) with the log name, a regular expression for the line, a condition on its named groups, the reference kind and
the texts printed in the log. A bench that should be read by the tool prints one summary line per measured quantity.
