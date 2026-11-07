"""Verification by instruction flow of the vector unit (GVU) connected to the SAURIA core.

Every flow of the vector unit (GEMM_FUSED, max pool, average pool, residual add, Softmax, FUSED_ATTN, LayerNorm, DMA_TENSOR
COPY, MOVE, slice / concat by the 4-D DMA) is described as numbered steps, the steps of the flow drawings of the vector unit,
restated in each log. For every step this script names the checks that cover it, reads their results from the logs of one run of tools/has/regress_gvu.sh and
writes one log per flow. Each step of a flow log says what the step does, what is checked, against which reference, how
it is compared, the measured result with the log line it was read from, and a verdict. A step counts as checked only
when a check looks at that step's own output; a step whose correctness is only implied by the final result of the flow
is reported as "end to end only", and a step without any check as "NOT CHECKED".

Reference kinds:
  R  RTL simulation: the same inputs were run on the hardware RTL (Verilator or ModelSim) and its outputs are stored as
     vector files or a digest of them; the check compares the model with those outputs.
  S  Specification: a reference written independently of the model from the micro-architecture specification (MAS)
     or from the RTL source as written (Python programs, hand-written formulas in the bench, vectors of the hardware team).
  D  Definition proposed by the model team where the RTL has no such function yet; the definition is in the named
     document and an independent reference implements it.
  M  Model only: two paths of the model agree (for example the clocked unit and the call-style unit). This shows the
     paths are consistent, not that the function is right.
Verdicts: PASS, FAIL, NOT RUN (log missing), OPEN (a known difference to the delivered RTL that the model does not follow,
described in the note).

usage: python3 tools/has/verify_flows.py <regression output dir> [--out <dir>]
The regression output dir is the first argument given to regress_gvu.sh. Default output: <regression dir>/flows.
Exit code 0 when no step fails and every step is checked at the step or end to end.
"""
import os
import re
import sys

KINDS = {"R": "RTL simulation", "S": "specification", "D": "proposed definition", "M": "model only"}


class Check:
    """One check: the first line of `log` that matches `pattern` (every matching line with every=True) must satisfy `cond`
    (a Python expression over the named groups). optional=True: the input of the check is not shipped (for example the vectors
    of the hardware drop); a missing log is then reported and does not decide the step."""
    def __init__(self, log, pattern, cond, checks, kind, reference, method, result, scope="step", negative=False, every=False,
                 optional=False):
        self.log, self.pattern, self.cond = log, pattern, cond
        self.checks, self.kind, self.reference, self.method, self.result = checks, kind, reference, method, result
        self.scope, self.negative, self.every, self.optional = scope, negative, every, optional


def C(*a, **k):
    return Check(*a, **k)


def N(*a, **k):
    k["negative"] = True
    return Check(*a, **k)


# Each flow: name, drawing page, list of steps (number, what the step does, checks, notes).
FLOWS = []


def flow(name, page, steps, whole=()):
    FLOWS.append((name, page, steps, list(whole)))


# --------------------------------------------------------------------------------------------------------------------
# GEMM_FUSED: PSM -> GVU stream
# --------------------------------------------------------------------------------------------------------------------
flow("GEMM_FUSED", "GEMM_FUSED flow", [
    (1, "The data flow controller (DFC) opens the layer once: Reg 3 (zero point, skip_act); Reg 1 set to 1 then 0 when the "
        "layer loads a new activation table; CONTROL (op 2, en 1).", [
        C("regs_dfc_L4.log", r"\[regs\] sequences decoded by the register file: (?P<what>.*) ok$", "True",
          "the register fields decoded by the register file after the DFC write sequences: zero point, skip activation, "
          "LUT reset pair, enable, done flag, clear, pooling fields",
          "S", "the field layout of gvu_config_regs.sv (Reg 0..8; the register file port itself replays the RTL with 0 "
               "mismatches, GVU_RTL_CONFORMANCE.md section 3)",
          "every decoded field equals the value the layer asked for", "{what}: all as written"),
        C("regs_dfc_L4.log", r"writes land at edges (?P<e>[\d ]+), sequence over at edge (?P<o>\d+) \(expected (?P<x>[\d, ]+)\) ok", "True",
          "the edges at which three writes of 4 edges each land, and the end of the sequence",
          "S", "write port of gvu_config_regs.sv: one write per L edges", "edges counted in the bench against the expected list",
          "writes at edges {e}, over at edge {o} (expected {x})"),
        C("gate_regs.log", r"register writes (?P<w>\d+).*layer exits by oc_last & oc_tile_last (?P<x>\d+) \(model stop as fallback (?P<f>\d+), timeouts (?P<t>\d+)\)",
          "int(f) == 0 and int(t) == 0",
          "check program of 6 instructions through the register file: register writes, and how each GEMM layer ended",
          "M", "the same program with the call-style unit (DRAM image and every epilogue beat equal, steps 6 and 7)",
          "count of layer exits through the flags, model fallback and timeouts must be 0",
          "{w} register writes, {x} layers left by the flags, fallback {f}, timeouts {t}", scope="step"),
    ]),
    (2, "Controller: LUT_ACT_INIT when the table is not loaded (DMA 256 B into vbank#1, then into the LUT), then "
        "GEMM_FUSED_ST1 with o_psum_ready = 1; en stays on for the whole layer.", [
        C("rtl_trace.log", r"ORACLE lut_load cycles (?P<c>\d+) mismatches (?P<m>\d+)", "int(m) == 0",
          "table load of gvu_top.sv: controller states, load / store unit, Scratchpad and table memory, cycle by cycle",
          "R", "gvu_top.sv simulated in Verilator 5.052 (digest of the vector files in tools/has/gvu_oracle/rtl_trace_digest.txt)",
          "the ported blocks regenerate the vector files; their digest must equal the digest of the run without mismatch",
          "{c} cycles, {m} mismatches against the RTL"),
        C("rtl_trace.log", r"same\s+lutload\.txt\s+(?P<b>\d+) beats", "True",
          "vector file of the table load regenerated by this tree", "R", "digest of the RTL run",
          "FNV-1a digest of the regenerated file against the stored one", "{b} beats, digest equal"),
        C("gate_regs.log", r"activation tables loaded (?P<t>\d+)", "int(t) >= 1",
          "check program: activation tables loaded by the layers that use one", "M",
          "the program's two table layers", "count", "{t} tables loaded"),
    ]),
    (3, "Last Cin pass: every PSM write into SRAM-C is also one packet into a FIFO of 4 packets; when the FIFO is full "
        "o_stream_stall stops the whole core.", [
        C("gate_st.log", r"FIFO max depth (?P<d>\d+), STALL NEEDED (?P<s>\d+)", "int(d) <= 4 and int(s) == 0",
          "check program with the stream ready held low for 300 edges: largest FIFO occupancy and packets taken with the FIFO full",
          "S", "FIFO of 4 packets with valid / ready between psm_top and the vector unit (RTL psm_top.sv interface)",
          "the FIFO may never hold more than 4 packets and no packet may be taken while it is full",
          "FIFO max {d}, packets taken with the FIFO full {s}"),
        C("gate_st.log", r"core stall cycles (?P<c>\d+) \(o_stream_stall held the core\)", "int(c) > 0",
          "cycles the core was held by o_stream_stall in the same run", "S", "stall rule of the stream interface",
          "the core must have been stopped (ready held 300 edges)", "core held {c} cycles"),
        C("gate_st.log", r"packets pushed (?P<p>\d+) \(.*latched (?P<l>\d+), results (?P<r>\d+), lanes (?P<n>\d+), differ from the model (?P<d>\d+)",
          "p == l == r and int(d) == 0",
          "packets pushed, latched by GvuTop and written back, and their values, with the core stopped by the stall",
          "M", "the call-style epilogue of the model on the same writes", "count equality and lane-by-lane compare",
          "{p} pushed = {l} latched = {r} written back, {n} lanes, {d} differ"),
        N("gate_hold_off.log", r"FIFO max depth (?P<d>\d+), STALL NEEDED (?P<s>\d+)", "int(d) > 4 and int(s) > 0",
          "same run with the core stall switched off (HAS_GVU_STALL=0)", "S", "stall rule of the stream interface",
          "the check must fail when the stall is missing", "FIFO max {d}, packets taken with the FIFO full {s}: caught"),
    ]),
    (4, "One beat per cycle through the requant chain: multiply by Out_Scale (4) -> v32int64 (1) -> round-shift by "
        "Out_Shift, saturate to int16 (1) -> register (1) -> add zero point (1) -> clamp int8 (1) -> activation LUT "
        "(0 when skip_act): 9 computing stages; with the two cycles of the Out_Scale / Out_Shift read the RTL measures 11, the "
        "default depth of the model.", [
        C("stream_after.log", r"packets off the expected timing \(.*latch->result (?P<lat>\d+)\): (?P<off>\d+) of (?P<n>\d+)", "int(off) == 0",
          "latency of every packet from latch to result in the stream unit test", "S",
          "depth 11 as the RTL measures (the default; GVU_GEMM_STREAM_DEPTH=9, the MAS drawing, is a build option of the regression)",
          "per packet: result edge - latch edge", "{off} of {n} packets off latency {lat}"),
        C("tb_gvu_top_params.log", r"RESULT: PASS \(runs not as expected: (?P<b>\d+) of (?P<n>\d+)\)", "int(b) == 0",
          "8 runs per depth (9 and 11) of 400 beats with each rounding mode and activation on / off, 8 tile runs and 8 stream "
          "runs of 4,096 / ~8,600 lanes: value of every lane and latency of every beat",
          "S", "has::requant, the requant formula of the MAS (itself checked against mas_ref.py, below)",
          "lane by lane; latency off the depth counted", "{b} of {n} runs not as expected"),
        C("tb_gvu_top_params.log", r"narrowing, stream mode\s*: lanes (?P<n>\d+), outside int16 (?P<o>\d+), differ from has::requant (?P<d>\d+)", "int(d) == 0",
          "saturation to 16 bits after the round-shift on lanes outside int16", "S", "MAS narrowing rule (SAT16 and WRAP16)",
          "lane by lane", "{n} lanes, {o} outside int16, {d} differ"),
        C("rtl_trace.log", r"files (?P<f>\d+), beats (?P<b>\d+), files that differ (?P<d>\d+)", "int(d) == 0",
          "the blocks of the chain (multipliers, round-shift, adders, clamp, LUT) on random vectors", "R",
          "each RTL unit simulated in Verilator 5.052 with the same vectors; digest stored", "digest of every vector file",
          "{f} vector files, {b} beats, {d} files differ from the RTL run"),
        C("root_gemm_fused_tile.log", r"Total mismatches:\s+(?P<d>\d+) / (?P<n>\d+)", "int(d) == 0",
          "GEMM_FUSED vectors of the vector-unit owner (tile set): requant and activation of 4,096 values", "S",
          "golden output delivered with the vectors (MAS GEMM_FUSED)", "every value", "{d} of {n} differ"),
        C("tb_gvu_mas.log", r"^RESULT: PASS \((?P<f>\d+) failed checks\)", "int(f) == 0",
          "the model's requant, Softmax, LayerNorm and residual formulas against the reference vectors", "S",
          "tools/has/mas_ref.py, an independent Python transcription of the MAS formulas", "value by value",
          "{f} failed checks"),
        N("stream_after.log", r"differences counted against the model (?P<d>\d+) \(expected 1: the wrong lane\)", "int(d) == 1",
          "one packet carries a wrong lane on purpose", "M", "call-style epilogue", "the compare must count exactly one difference",
          "{d} difference counted: caught"),
    ]),
    (5, "Out_Scale and Out_Shift: the RTL reads them from the sbank by channel, the channel index travels with the packet; "
        "the model carries them in the packet.", [
        C("gate_st.log", r"address / mask not carried (?P<a>\d+)", "int(a) == 0",
          "address and lane mask of every packet arriving at the write-back", "M", "the write of the PSM that made the packet",
          "count of packets whose address or mask was lost", "{a} not carried"),
        C("gate_st.log", r"scale index of the AGU != channel (?P<f>\d+)", "True",
          "channel index the RTL address generator would use for the scale, observed on every packet", "S",
          "RTL address generator of the sbank as written",
          "observed, not driven: the model takes the scale of the packet's channel",
          "{f} packets where the RTL index is not the channel (known RTL difference, see note)"),
    ], ["Known RTL difference: the RTL reads the scale with an index that is not the packet's channel on the counted packets; the model uses the "
        "channel. Reported to the hardware team (GVU_RTL_CONFORMANCE.md section 8)."]),
    (6, "The int8 result is written back into SRAM-C by GvuTopStream, 12 cycles after the push; the RTL writes vbank#2 "
        "through the write mux (the delivered RTL needs a change to do so).", [
        C("gate_st.log", r"latency push->latch [\d.]+ edges, latch->result (?P<a>\d+)\.\.(?P<b>\d+) edges", "a == b",
          "latch-to-result latency of every packet of the check program", "S", "depth 11 (step 4) + write", "min and max over all packets",
          "latch->result {a}..{b} edges"),
        C("gate_st.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), tensors (?P<t>\d+), elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
          "int(be) == 0",
          "every output tensor of the check program in DRAM after the write-back", "S",
          "golden tensors written by tools/has/make_ref_gate.py (independent Python implementation of the layers)",
          "byte by byte", "{s} steps, {e} elements, {be} bad", scope="step"),
    ]),
    (7, "The beat that carries oc_last and oc_tile_last leaves the chain => o_pipe_done: en <- 0, done <- 1; the DFC clears "
        "done (Reg 0 bit 2). The flag is set only on the packet holding the last element of the last call.", [
        C("stream_after.log", r"observer: exits of GEMM_FUSED_OBP \(oc_last & oc_tile_last\) (?P<x>\d+) \(expected (?P<e>\d+)\)", "x == e",
          "exits of the GEMM_FUSED state in the stream unit test", "S", "exit rule of gvu_pipe_controller.sv (oc_last and oc_tile_last)",
          "count against the expected", "{x} exits (expected {e})"),
        C("gate_regs.log", r"calls with packets (?P<c>\d+), calls whose oc_last packets != contexts (?P<o>\d+).*packets with oc_tile_last (?P<t>\d+), calls with the flag not on the one last packet (?P<n>\d+)",
          "int(n) == 0",
          "placement of oc_tile_last on the packets of every call of the check program", "D",
          "exit flag on the packet that holds the last element of the tile (GVU_DFC_REFERENCE_FLOW.md)",
          "per call: the flag must be on exactly one packet, the last", "{c} calls, {t} flagged packets, {n} calls with the flag misplaced"),
    ]),
], whole=[
    C("gate_regs.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), tensors (?P<t>\d+), elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
      "int(be) == 0", "check program through the register file", "S", "make_ref_gate.py golden", "byte by byte",
      "{s} steps, {e} elements, {be} bad", scope="end"),
    C("i86_top3.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
      "int(be) == 0", "instructions 86-88 of YOLOv8m (real layer shapes)", "S", "golden tensors of the network frontend", "byte by byte",
      "{s} steps, {e} elements, {be} bad", scope="end"),
    C("winD_top3.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
      "int(be) == 0", "window D of YOLOv8m", "S", "golden tensors of the network frontend", "byte by byte",
      "{s} steps, {e} elements, {be} bad", scope="end"),
])

# --------------------------------------------------------------------------------------------------------------------
# MAX_POOL
# --------------------------------------------------------------------------------------------------------------------
flow("MAX_POOL", "MaxPool flow", [
    (1, "DMA loads up to cmax channels, padded with -128, into vbank#0 buffer 0; cmax = min(800 / (Hp x Wt), "
        "1024 / (Ho x Wt), 255).", [
        C("tb_gvu_top_maxpool.log", r"YOLOv8m SPPF 288x20x20 p2 \(24x24\) k5 s1\s*\|\s*(?P<c>\d+) \|\s*(?P<r>\d+) \|\s*(?P<cy>\d+) \|\s*(?P<o>\d+) \|\s*(?P<d>\d+)",
          "int(d) == 0",
          "SPPF layer of YOLOv8m, 288 channels padded to 24 x 24, split into runs of cmax channels", "S",
          "max over the window computed directly in the bench from the unpadded input",
          "every output; a wrong pad value or channel split shows as a difference at the border or in a whole channel",
          "{c} channels in {r} runs, {o} outputs, {d} differ", scope="end"),
        C("tb_gvu_top_maxpool.log", r"YOLOv8m SPPF 288x20x20 p2 \(24x24\) k5 s1\s*\|\s*(?P<c>\d+) \|\s*(?P<r>\d+) \|",
          "int(r) == -(-int(c) // min(800 // (24 * 1), 1024 // (20 * 1), 255))",
          "number of runs the 288 channels of the SPPF layer are split into", "S",
          "cmax of the drawing, computed here: Hp 24, Wt 1, Ho 20 -> min(800 / 24, 1024 / 20, 255) = 33 channels per run",
          "runs counted by the bench against ceil(288 / cmax)", "{r} runs for {c} channels (expected ceil(288 / 33) = 9)"),
    ]),
    (2, "The DFC writes [Reg 16, 21..24 when AGU_ND keeps the destination], POOLING_0 (Reg 7), POOLING_1 (Reg 8), then "
        "CONTROL (op 7, en 1).", [
        C("regs_dfc_L4.log", r"\[regs\] max pool k5 s1 p2  70 x 20 x 20: call style (?P<a>\d+) cycles (?P<r>\d+) runs, by registers (?P<b>\d+) cycles (?P<r2>\d+) runs, elapsed (?P<e>\d+) edges .* (?P<d>\d+) elements differ ok",
          "a == b and int(d) == 0",
          "a 70-channel max pool configured only through the register writes of the DFC", "M",
          "the same pool configured by call (values and cycles)", "outputs and cycles compared; elapsed edges against "
          "1 + runs x (3 L + 2) + cycles", "{r2} runs, {b} cycles = {a}, elapsed {e} edges, {d} elements differ"),
        C("tb_gvu_agu_nd.log", r"T4 register packing \(Reg 16\.\.24\): (?P<n>\d+) descriptors, (?P<d>\d+) differ", "int(d) == 0",
          "packing of address-generator descriptors into Reg 16..24 and back", "D",
          "register proposal Reg 16..24 (SLICE_CONCAT_ON_GVU.md)", "round trip of random descriptors", "{n} descriptors, {d} differ"),
        C("gate_mp_regs.log", r"max pool runs (?P<m>\d+)", "int(m) > 0",
          "check program with 3 max pools through the register file: runs started by registers", "M", "program content",
          "count", "{m} max pool runs by registers"),
    ]),
    (3, "Controller IDLE -> POOLING_MAX, configures the VLSU and the functional unit.", [
        C("tb_gvu_top_maxpool_vectors_tile.log", r"Stage POOLING_MAX completed after (?P<c>\d+) cycles", "int(c) > 0",
          "controller states during one pool of the hardware team's tile vectors", "S",
          "states of gvu_pipe_controller.sv", "state trace of the run", "IDLE -> POOLING_MAX for {c} cycles -> IDLE"),
    ]),
    (4, "The address generator produces vbank#0 addresses, the pooling feeder spreads the window (mode = k - 2, stride 3 bits).", [
        C("rtl_trace.log", r"same\s+vlsub\.txt\s+(?P<c>\d+) beats", "True",
          "load / store unit (address registers, three address modes, port routing, mask), every output every cycle", "R",
          "gvu_vlsu.sv simulated in Verilator with the same inputs", "this tree regenerates the vector file and its digest must equal the digest stored after the RTL run (0 mismatches there)", "{c} cycles, digest equal"),
        C("tb_gvu_agu_nd.log", r"T3 AGU_POOLING = AGU_ND program: (?P<r>\d+) runs, (?P<n>\d+) reads \(want (?P<w>\d+)\), (?P<x>\d+) runs without last write, (?P<d>\d+) differ",
          "int(d) == 0 and n == w",
          "addresses of the pooling mode against the same walk written as an N-D address program", "D",
          "AGU_ND proposal (SLICE_CONCAT_ON_GVU.md)", "address by address", "{r} runs, {n} reads, {d} differ"),
    ]),
    (5, "The functional unit compares and keeps the maximum, pooling_valid goes back to the VLSU.", [
        C("tb_gvu_top_maxpool.log", r"RESULT: PASS \(sets not as expected: (?P<s>\d+) of (?P<n>\d+); socket cases not as expected: (?P<k>\d+) of (?P<m>\d+)\)",
          "int(s) == 0 and int(k) == 0",
          "7 generated windows (2x2 s1/s2, 3x3 s1, 4x4 s1/s2/s4, 5x5 s1) and the SPPF layer; 8 cases through the "
          "instruction socket", "S", "max over the window computed in the bench", "every output",
          "{s} of {n} sets and {k} of {m} socket cases not as expected"),
        C("tb_gvu_top_maxpool_vectors_tile.log", r"Total mismatches:\s+(?P<d>\d+) / (?P<n>\d+)", "int(d) == 0",
          "max pool vectors of the hardware team (tile set, 3 x 80 x 80, 5x5 s1)", "S",
          "golden output delivered with the vectors (MAS max pool)", "every byte of vbank#2", "{d} of {n} differ"),
        C("tb_gvu_top_maxpool_vectors_vec.log", r"Total mismatches:\s+(?P<d>\d+) / (?P<n>\d+)", "int(d) == 0",
          "max pool vectors of the hardware team (vec set)", "S", "golden output delivered with the vectors",
          "every byte of vbank#2", "{d} of {n} differ"),
        C("rtl_trace.log", r"same\s+cmp\.txt\s+(?P<n>\d+) beats", "True",
          "comparator gvu_vcmp32 used by the pool, every output", "R", "gvu_vcmp32 simulated in Verilator with the same inputs",
          "this tree regenerates the vector file and its digest must equal the digest stored after the RTL run (0 mismatches there)", "{n} vectors, digest equal"),
    ]),
    (6, "The result goes through the write mux into vbank#2 (or the AGU_ND destination).", [
        C("tb_gvu_top_maxpool_vectors_tile.log", r"Comparing 100% of (?P<b>vbank2)", "True",
          "location of the result", "S", "vbank#2 of the hardware team's bench", "the compare reads vbank#2",
          "result read from {b} (step 5)"),
    ]),
    (7, "o_maxpool_pipe_done => o_pipe_done: en <- 0, done <- 1; the DFC clears done. The next run starts with a reset of "
        "the feeder; the registers are not cleared.", [
        C("tb_gvu_top_maxpool.log", r"YOLOv8m SPPF 288x20x20 p2 \(24x24\) k5 s1\s*\|\s*\d+ \|\s*(?P<r>\d+) \|\s*\d+ \|\s*(?P<o>\d+) \|\s*(?P<d>\d+)",
          "int(r) > 1 and int(d) == 0", "9 runs one after another with the feeder reset between them", "S",
          "max computed in the bench", "every output", "{r} runs, {o} outputs, {d} differ"),
        N("tb_gvu_top_maxpool.log", r"DIAG back-to-back runs \(no reset\).*runs (?P<r>\d+), cycles (?P<c>\d+), outputs (?P<o>\d+), differ (?P<d>\d+)",
          "int(d) > 0", "the same 9 runs without the reset of the feeder", "S", "max computed in the bench",
          "the compare must find the error", "{d} of {o} outputs differ: caught"),
        C("tb_gvu_top_maxpool.log", r"socket k3 s2 p1 6x12x12 -> 6x6 \| (?P<s>refused)", "True",
          "a window the RTL feeder cannot do (3x3 stride 2)", "S", "window field of POOLING_0 / POOLING_1",
          "the instruction must be refused, not run wrongly", "{s}"),
    ]),
], whole=[
    C("gate_mp_regs.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
      "int(be) == 0", "check program with 3 max pools, through the register file", "S", "make_ref_gate_mp.py golden",
      "byte by byte", "{s} steps, {e} elements, {be} bad", scope="end"),
    C("replay_top.log", r"\[replay\] max pool: (?P<i>\d+) instructions, (?P<e>\d+) elements, (?P<d>\d+) differ from the reference", "int(d) == 0",
      "the 3 max pools of YOLOv8m on GvuTop", "S", "outputs of the network frontend", "every element",
      "{i} instructions, {e} elements, {d} differ", scope="end"),
])

# --------------------------------------------------------------------------------------------------------------------
# AVG_POOL (proposal)
# --------------------------------------------------------------------------------------------------------------------
flow("AVG_POOL", "AVG_POOL flow", [
    (1, "DMA loads up to cmax channels into vbank#0 buffer 0, without padding.", [
        C("avg_gvu.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
          "int(be) == 0", "AVG_POOL check program (8 instructions)", "S", "make_avgpool_gate.py golden", "byte by byte",
          "{s} steps, {e} elements, {be} bad", scope="end"),
    ]),
    (2, "The DFC writes POOLING_0, POOLING_1 (Reg 7, 8), Avg_Scale (Reg 25), Avg_Shift and round (Reg 26), then CONTROL "
        "(op 8, en 1); GvuTop reads its configuration back from the registers.", [
        C("avg_gvu.log", r"avg pool by registers \(Reg 7, 8, 25, 26, Reg 0\) \| writes (?P<w>\d+) \| runs whose values the register fields do not hold (?P<b>\d+)",
          "int(b) == 0", "register writes of every run and the configuration GvuTop reads back", "D",
          "Reg 25, 26 proposal (GVU_OPS_ON_GVUTOP.md section 2)", "configuration read back against the instruction",
          "{w} writes, {b} runs with a mismatch"),
    ]),
    (3, "Controller IDLE -> POOLING_AVG (proposed state).", [
        C("tb_gvu_top_avgpool_states.log", r"states: (?P<seq>IDLE -> POOLING_AVG[^|]*) \| cases (?P<n>\d+), other sequences (?P<o>\d+)", "int(o) == 0",
          "controller state sequence of every case", "D", "POOLING_AVG state proposal (GVU_OPS_ON_GVUTOP.md section 2)",
          "state trace per case", "{seq} in {n} cases, {o} other sequences"),
    ]),
    (4, "Address generator and feeder spread the window as for max pool.", [
        C("tb_gvu_top_avgpool_cases.log", r"RESULT: PASS \((?P<f>\d+) failing\)", "int(f) == 0",
          "18 cases: windows 2x2, 3x3, 4x4, 5x5, strides 1, 2, 4, several W tiles, 40 channels", "D",
          "sum over the window computed in the bench (GVU_OPS_ON_GVUTOP.md section 2)",
          "every output is the scaled sum of one window, so a window spread wrongly by the address generator or the feeder gives a "
          "wrong output", "{f} failing"),
    ]),
    (5, "Per lane: window sum on 16 bits (adder #1) -> sum register (1) -> multiplier #1 32x16 by Avg_Scale (2) -> v32int64 "
        "(1) -> round-shift #1 by Avg_Shift, rounding, saturate int8 (1): tail of 5 cycles.", [
        C("tb_gvu_avgpool_pipe.log", r"A1 pipe: (?P<n>\d+) lane checks, (?P<b>\d+) bad; tie cases (?P<t>\d+) bad (?P<tb>\d+).*tail: bad (?P<tl>\d+)",
          "int(b) == 0 and int(tb) == 0 and int(tl) == 0",
          "tail of the average pool: values of every lane, ties for each rounding mode, tail length", "D",
          "tail proposal built from RTL blocks (multiplier 32x16, round-shift) whose latency was measured in ModelSim",
          "lane by lane against a formula in the bench", "{n} lane checks, {b} bad, ties {t} bad {tb}, tail bad {tl}"),
        C("tb_gvu_top_avgpool_vectors.log", r"vector tile .*outputs (?P<o>\d+), differ (?P<d>\d+)", "int(d) == 0",
          "average pool vectors of the hardware team (tile set)", "S", "golden output and golden window sums delivered with the vectors",
          "every output", "{o} outputs, {d} differ"),
        C("tb_gvu_top_avgpool_vectors.log", r"vector vec .*outputs (?P<o>\d+), differ (?P<d>\d+)", "int(d) == 0",
          "average pool vectors of the hardware team (vec set)", "S", "golden output delivered with the vectors",
          "every output", "{o} outputs, {d} differ"),
        N("tb_gvu_top_avgpool_asrtl.log", r"RESULT: FAIL \((?P<f>\d+) failing\)", "int(f) > 0",
          "the same 19 cases with the rounding and narrowing of the delivered RTL tail", "D", "the proposal",
          "the proposal and the delivered RTL must differ where documented", "{f} cases differ: the difference is visible"),
    ]),
    (6, "The write mux writes vbank#2; done comes 5 cycles after the done of the same max pool.", [
        C("tb_gvu_top_avgpool_states.log", r"done after max pool: (?P<d>[-\d]+) cycles in every case \| cases (?P<n>\d+), other (?P<o>\d+)", "int(o) == 0 and int(d) == 5",
          "done cycle of every case against a max pool of the same window and shape", "D",
          "tail of 5 cycles (step 5)", "difference of the two done cycles", "{d} cycles in {n} cases, {o} other"),
    ]),
    (7, "o_avgpool_pipe_done => o_pipe_done; the DFC clears done (Reg 0 bit 2).", [
        C("avg_gvu.log", r"jobs (?P<j>\d+) \(failed (?P<f>\d+)\), edges (?P<e>\d+), unit ticks (?P<t>\d+) \(\+(?P<a>\d+) after done to IDLE\)",
          "int(f) == 0 and int(a) == 0", "end of every job of the check program", "D", "job protocol (GVU_OPS_ON_GVUTOP.md section 1)",
          "a job must end on done and the unit be IDLE at once", "{j} jobs, {f} failed, {a} ticks after done"),
        C("avg_gvu.log", r"avg pool\s*\| calls (?P<c>\d+) \| not expressible (?P<x>\d+) \| lanes (?P<l>\d+) \| differ (?P<d>\d+)", "int(d) == 0",
          "outputs of the instructions that ran on GvuTop", "S", "make_avgpool_gate.py golden", "lane by lane",
          "{c} calls ({x} with a window the RTL does not pool stay in the model), {l} lanes, {d} differ"),
    ]),
])

# --------------------------------------------------------------------------------------------------------------------
# Residual ADD (proposal with AGU_ND)
# --------------------------------------------------------------------------------------------------------------------
flow("ADD", "ADD (residual) flow", [
    (1, "DMA loads A into vbank#0 and B into vbank#1 (up to 128 rows per run; B shares vbank#1 with the LUT tables).", [
        C("gate_regs.log", r"residual add by registers: runs (?P<r>\d+), runs whose configuration read back from the register file differs from the call style (?P<d>\d+), runs with B in the vbank1 buffer 1 (?P<b>\d+)",
          "int(d) == 0 and b == r", "placement of B for every run of the check program", "D",
          "ADD proposal (B in vbank#1 buffer 1, GVU_DFC_REFERENCE_FLOW.md)", "count", "{r} runs, B in vbank#1 buffer 1 in {b}"),
    ]),
    (2, "The DFC writes Reg 9..13 once; per run [Reg 16..24], Reg 4 (rows), Reg 14 (first row), Reg 15 (lane mask) when they "
        "change; then CONTROL (op 6, en 1).", [
        C("regs_dfc_L4.log", r"\[regs\] add 12000 elements \(3 runs\): call style (?P<a>\d+) cycles, by registers (?P<b>\d+) cycles, (?P<w>\d+) writes \(expected (?P<x>\d+)\).* (?P<d>\d+) elements differ ok",
          "a == b and w == x and int(d) == 0", "a 12,000-element add in 3 runs configured through the registers", "D",
          "write sequence of the ADD proposal (GVU_DFC_REFERENCE_FLOW.md)", "write count against the sequence, outputs and cycles against the call style",
          "{w} writes (expected {x}), {b} cycles = {a}, {d} differ"),
        C("tb_rtl_ref_gvu_config_regs.log", r"RESULT: PASS \((?P<f>\d+) checks failed\)", "int(f) == 0",
          "register file with the proposed registers 9..15", "R",
          "gvu_config_regs.sv simulated in ModelSim / Verilator for Reg 0..8; proposals 9..15 per GVU_DFC_REFERENCE_FLOW.md",
          "field by field", "{f} checks failed"),
    ]),
    (3, "RESIDUAL_ST1: AGU_ND reads A (vbank#0) and B (vbank#1); (A - zp_A) x scale_A (multiplier #0 48x40) -> round-shift #0; "
        "(B - zp_B) x scale_B (multiplier #1 32x16) -> round-shift #1; adder #1 -> int32 sum.", [
        C("root_residual_add_tile.log", r"Stage 1 .*mismatches:?\s*(?P<d>\d+)\s*/\s*(?P<n>\d+)", "int(d) == 0",
          "int32 sums in vbank#2 after stage 1 (hardware team's residual add vectors, tile set)", "S",
          "golden_sum.txt delivered with the vectors (MAS section 5.2.6)", "every sum", "{d} of {n} differ"),
        C("root_residual_add_vec.log", r"Stage 1 .*mismatches:?\s*(?P<d>\d+)\s*/\s*(?P<n>\d+)", "int(d) == 0",
          "int32 sums after stage 1 (vec set)", "S", "golden_sum.txt", "every sum", "{d} of {n} differ"),
        C("rtl_trace.log", r"same\s+(?P<f>mul[ab])\.txt\s+(?P<n>\d+) beats", "True",
          "multipliers of the dequant of A and B (48x40 and 32x32 units), every product", "R",
          "gvu_vmul48x40 and gvu_vmul32x32 simulated in Verilator with the same inputs", "this tree regenerates the vector file and its digest must equal the digest stored after the RTL run (0 mismatches there)", "{f}: {n} vectors, digest equal",
          every=True),
    ]),
    (4, "The int32 sum goes through the write mux into vbank#2 port 0.", [
        C("root_residual_add_tile.log", r"Stage 1 .*mismatches:?\s*(?P<d>\d+)\s*/\s*(?P<n>\d+)", "int(d) == 0",
          "sums read from vbank#2 port 0", "S", "golden_sum.txt", "every sum", "{d} of {n} differ"),
    ]),
    (5, "RESIDUAL_ST2: vbank#2 x scale_out (multiplier #1 32x32) -> round-shift by shift_out, saturate 16 -> add zp_out -> clamp int8 (or int16).", [
        C("root_residual_add_tile.log", r"Stage 2 .*mismatches:?\s*(?P<d>\d+)\s*/\s*(?P<n>\d+)", "int(d) == 0",
          "int8 outputs after stage 2 (tile set)", "S", "golden_output.txt delivered with the vectors", "every output",
          "{d} of {n} differ"),
        C("tb_gvu_top_vs_model.log", r"RESULT: PASS \(runs that differ or did not finish: (?P<d>\d+) of (?P<n>\d+)\)", "int(d) == 0",
          "51 runs incl. residual adds with a lane mask on the last row", "M", "the call-style add of the model",
          "lane by lane", "{d} of {n} runs differ"),
    ]),
    (6, "The write mux writes the result into vbank#0 (destination given by AGU_ND).", [
        C("tb_gvu_agu_nd.log", r"T1 random descriptors: (?P<p>\d+) programs, (?P<a>\d+) accesses, (?P<d>\d+) differ", "int(d) == 0",
          "addresses of the N-D destination", "D", "AGU_ND proposal (SLICE_CONCAT_ON_GVU.md), independent address walk",
          "address by address", "{p} programs, {a} accesses, {d} differ"),
    ]),
    (7, "o_residual_pipe_done (proposed into o_pipe_done) -> DFC; the DFC clears done.", [
        C("gate_regs.log", r"residual add and max pool on the clock \(HAS_GVU_ELEM_CLK=1\): jobs (?P<j>\d+) \(failed (?P<f>\d+)\).*ticks after done to IDLE (?P<a>\d+)",
          "int(f) == 0 and int(a) == 0", "end of every add job of the check program", "D", "job protocol",
          "a job must end on done and the unit be IDLE at once", "{j} jobs, {f} failed, {a} ticks after done"),
    ]),
], whole=[
    C("replay_top.log", r"\[replay\] residual add: (?P<i>\d+) instructions, (?P<e>\d+) elements, (?P<d>\d+) differ from the reference", "int(d) == 0",
      "the 12 residual adds of YOLOv8m on GvuTop", "S", "outputs of the network frontend", "every element",
      "{i} instructions, {e} elements, {d} differ", scope="end"),
])

# --------------------------------------------------------------------------------------------------------------------
# Softmax
# --------------------------------------------------------------------------------------------------------------------
SMX_REF = "Softmax rows of ViT-B/16 written by the functional model (tools/has/gvu_top_vectors), whose Softmax matches " \
          "tools/has/mas_ref.py and the hardware team's vectors (step 6)"
flow("SOFTMAX", "Softmax flow", [
    (1, "Groups of up to 32 score rows, one row per lane: position p in word p of vbank#0 buffer 0. Tables: DMA 768 B into "
        "vbank#1 buffer 1 (words 0..3 reciprocal 16 bit, 4..5 exponential 8 bit), once per instruction.", [
        C("rce_smx.log", r"softmax\s*\| instructions (?P<i>\d+) \| groups (?P<g>\d+) \|.*\| table loads (?P<t>\d+)", "t == i",
          "groups and table loads of the transformer check program", "D", "one table load per instruction (GVU_OPS_ON_GVUTOP.md section 3)",
          "count", "{i} instructions, {g} groups, {t} table loads"),
    ]),
    (2, "The DFC writes Reg 5 (ATTN_Scale), Reg 6 (ATTN_Shift, Z_ATTN), Reg 4 (length [8:0], tiles), then CONTROL (op 3, en 1).", [
        C("rce_smx.log", r"softmax by registers \(Reg 4\.\.6, Reg 0\) \| writes (?P<w>\d+) \| groups whose values the register fields do not hold (?P<b>\d+)",
          "int(b) == 0", "register writes per group and the configuration read back", "S",
          "Reg 4..6 of gvu_config_regs.sv", "configuration read back against the instruction", "{w} writes, {b} groups with a mismatch"),
    ]),
    (3, "Controller LUT_RECIP_INIT -> LUT_EXP_INIT when not loaded; the buffer bit of vbank#1 flips ONCE on the rising edge of "
        "lut_init_done (the delivered RTL flips on the level: change needed).", [
        C("smx_edge.log", r"buf_sel flips (?P<f>\d+)", "int(f) == 1",
          "flips of the vbank#1 buffer select during one Softmax (3 heads)", "S", "table load of gvu_top.sv (step 2 of GEMM_FUSED, RTL digest)",
          "count per head", "{f} flip", every=True),
        N("smx_level.log", r"A differ (?P<d>\d+) / (?P<n>\d+)", "int(d) > 0",
          "the same rows with the buffer select flipped on the level, as the delivered register file does", "S",
          "functional model rows", "the outputs must go wrong", "{d} of {n} differ", every=True),
        C("rtl_trace.log", r"ORACLE lut_load cycles (?P<c>\d+) mismatches (?P<m>\d+)", "int(m) == 0",
          "table load states of gvu_top.sv", "R", "Verilator run of gvu_top.sv", "cycle by cycle", "{c} cycles, {m} mismatches"),
    ], ["Known RTL difference: the delivered register file flips the vbank#1 buffer select on every cycle lut_init_done is high; the model flips "
        "it once on the rising edge. Change proposed to the hardware team."]),
    (4, "ST1: read vbank#0 -> comparator (running max) -> row maximum written into vbank#1 (port 2).", [
        C("smx_edge.log", r"rows=(?P<r>\d+) L=(?P<L>\d+): M differ (?P<m>-?\d+)", "int(m) == 0",
          "row maximum of every row in vbank#1 when ST1 ends (3 heads of ViT-B/16)", "S", SMX_REF,
          "per row", "{r} rows of length {L}, {m} maxima differ", every=True),
    ]),
    (5, "ST2: x - max (adder #2) -> exponential LUT -> e written into vbank#2 (port 1); sum of e (adder #1) into vbank#1 (port 0).", [
        C("smx_edge.log", r"SIG differ (?P<s>-?\d+)", "int(s) == 0",
          "sum of the exponentials of every row in vbank#1 when ST2 ends", "S", SMX_REF, "per row", "{s} sums differ", every=True),
        C("tb_rtl_ref_gvu_hwvec.log", r"LUT exp, direct read\s+(?P<n>\d+) values, different (?P<d>\d+)", "int(d) == 0",
          "exponential table lookups", "R", "vectors delivered by the hardware team with the RTL", "every value",
          "{n} values, {d} different", optional=True),
    ]),
    (6, "ST3: reciprocal LUT of the sum -> r, E; e x r (multiplier #1 16x16) -> round-shift #0 by E + Q - P (the delivered RTL "
        "shifts by E: change needed) -> x ATTN_Scale (multiplier #0) -> round-shift #1 by ATTN_Shift -> add Z_ATTN -> clamp int8.", [
        C("smx_edge.log", r"A differ (?P<a>\d+) / (?P<n>\d+)", "int(a) == 0",
          "output probabilities of every row (3 heads of ViT-B/16, shift E + Q - P)", "S", SMX_REF, "every output",
          "{a} of {n} differ", every=True),
        C("root_softmax_tile.log", r"Total mismatches:\s+(?P<d>\d+) / (?P<n>\d+)", "int(d) == 0",
          "Softmax vectors of the vector-unit owner (tile set, 4,096 values)", "S",
          "golden output delivered with the vectors (MAS Softmax)", "every value in vbank#0", "{d} of {n} differ"),
        C("tb_rtl_ref_gvu_hwvec.log", r"Softmax, 64 rows of 64 \(ported data path\)\s+(?P<n>\d+) values, different (?P<d>\d+)", "int(d) == 0",
          "the three stages on the hardware team's Softmax vectors", "R", "vectors delivered by the hardware team with the RTL",
          "every value", "{n} values, {d} different", optional=True),
        C("tb_gvu_softmax.log", r"\[tb_gvu_softmax\] pwl\s+(?P<r>\d+) rows /\s+(?P<e>\d+) elements, (?P<m>\d+) rows mismatching", "int(m) == 0",
          "the model's Softmax rows", "S", "reference vectors of the MAS formulas", "row by row", "{r} rows, {e} elements, {m} rows differ"),
        N("smx_rtlshift.log", r"A differ (?P<a>\d+) / (?P<n>\d+)", "int(a) > 0",
          "the same rows with the stage-3 shift of the delivered RTL (shift E)", "S", SMX_REF,
          "the outputs must differ where documented", "{a} of {n} differ", every=True),
    ], ["Known RTL difference: the delivered RTL shifts by E in stage 3; with the score scales of ViT-B/16 this loses the result (counted above). "
        "Change proposed to the hardware team (GVU_OPS_ON_GVUTOP.md section 3)."]),
    (7, "The write mux writes the probabilities back into vbank#0 in place; o_softmax_pipe_done -> DFC; the DFC clears done.", [
        C("rce_smx.log", r"softmax\s*\| instructions (?P<i>\d+) \| groups (?P<g>\d+) \| not expressible (?P<x>\d+) \| lanes (?P<l>\d+) \| differ (?P<d>\d+)",
          "int(d) == 0", "every Softmax lane of the transformer check program", "M",
          "the model's Softmax on the same groups (itself checked in steps 4 to 6)", "lane by lane",
          "{g} groups, {l} lanes, {d} differ"),
        N("rce_smx_asrtl.log", r"lanes (?P<l>\d+) \| differ (?P<d>\d+)", "int(d) > 0",
          "the check program with the delivered stage-3 shift", "M", "the model's Softmax", "must differ",
          "{d} of {l} lanes differ: caught"),
    ]),
], whole=[
    C("rce_smx.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
      "int(be) == 0", "transformer check program (14 instructions)", "S", "make_rce_gate.py golden", "byte by byte",
      "{s} steps, {e} elements, {be} bad", scope="end"),
])

# --------------------------------------------------------------------------------------------------------------------
# FUSED_ATTN
# --------------------------------------------------------------------------------------------------------------------
flow("FUSED_ATTN", "FUSED_ATTN flow", [
    (1, "Phase 1: the core computes Q x K^T (1x1 layer, 32 channels per run); the int32 sum already holds "
        "- Zk x Qrow - Zq x Kcol + d x Zq x Zk.", [
        C("tb_gvu_fused_attn.log", r"vit_b6_h3\s+L=(?P<L>\d+) d=(?P<d>\d+) \| X (?P<x>\d+)\s+stats (?P<s>\d+)\s+A (?P<a>\d+)\s+O (?P<o>\d+) mismatches",
          "int(x) == 0", "scores X of an attention head of ViT-B/16 (zero-point terms included)", "S",
          "reference vectors of the MAS formulas", "every score", "L {L}, d {d}: X {x} mismatches"),
    ]),
    (2, "The DFC opens the phase as a GEMM_FUSED layer: Reg 3 (skip_act = 1, zero point), CONTROL (op 2); proposal: reuse the "
        "GEMM_FUSED chain, no new block.", [
        C("rce_fa.log", r"attention requant by registers \(Reg 3, Reg 0\) \| writes (?P<w>\d+)", "True",
          "register writes of the 18 phases of the transformer check", "D", "GVU_OPS_ON_GVUTOP.md section 4", "count",
          "{w} writes"),
    ]),
    (3, "After the core products, the requant job feeds one beat of 32 lanes per edge into the GEMM_FUSED chain (M int32, "
        "shift 0..63, int8 out); masked scores are set to -128 after the requant.", [
        C("rce_fa.log", r"attention requant \(HAS_GVU_FA\) \| phases (?P<p>\d+) \| not expressible (?P<x>\d+) \| lanes (?P<l>\d+) \| differ (?P<d>\d+) \| beats (?P<b>\d+).*masked scores set after the requant (?P<m>\d+)",
          "int(d) == 0", "requant of every phase on GvuTop", "M", "the model's requant (checked against mas_ref.py, GEMM_FUSED step 4)",
          "lane by lane", "{p} phases, {b} beats, {l} lanes, {d} differ, {m} masked scores"),
    ]),
    (4, "Phase 2: int8 scores into vbank#0, Softmax per group of 32 rows. Proposal: ST1 takes the row max from the phase-1 "
        "beats (not modelled; on the clock ST1 still runs on its own).", [
        C("tb_gvu_fused_attn.log", r"vit_b6_h3\s+L=\d+ d=\d+ \| X \d+\s+stats (?P<s>\d+)\s+A (?P<a>\d+)", "int(s) == 0 and int(a) == 0",
          "row statistics and probabilities of the head", "S", "reference vectors of the MAS formulas", "every value",
          "stats {s}, A {a} mismatches"),
        C("rce_fa.log", r"softmax ST1 cycles behind phase 1 (?P<c>\d+)", "True",
          "cycles of ST1 hidden behind phase 1", "D", "proposal, not modelled", "measured", "{c} (ST1 runs on its own)"),
    ]),
    (5, "The probabilities A (int8) stay in vbank#0 in place.", [
        C("tb_gvu_fused_attn.log", r"vit_b11_h7\s+L=\d+ d=\d+ \| X \d+\s+stats \d+\s+A (?P<a>\d+)", "int(a) == 0",
          "probabilities of another head", "S", "reference vectors", "every value", "A {a} mismatches"),
    ]),
    (6, "Phase 3: the core computes A x V; Za x Vcol is subtracted through the psum preload of the core (proposal); requant as in step 3.", [
        C("tb_gvu_fused_attn.log", r"RESULT: PASS \((?P<h>\d+) heads\)", "True", "outputs O of 9 heads (5 listed, incl. causal mask)",
          "S", "reference vectors of the MAS formulas", "every output", "{h} heads, O 0 mismatches"),
    ]),
    (7, "O int8 -> DRAM. Cycles = DMA + core passes + Softmax job + requant job of each phase.", [
        C("rce_fa.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
          "int(be) == 0", "transformer check program with the attention requant on GvuTop", "S", "make_rce_gate.py golden",
          "byte by byte", "{s} steps, {e} elements, {be} bad"),
    ]),
])

# --------------------------------------------------------------------------------------------------------------------
# LayerNorm with lane statistics (proposal)
# --------------------------------------------------------------------------------------------------------------------
flow("LAYERNORM", "LayerNorm by lanes flow", [
    (1, "Groups of up to 32 tokens, one token per lane, one channel per cycle: x_k in word k of vbank#0 buffer 0 (H <= 768).", [
        C("tb_gvu_ln_lanes_tiers.log", r"tier 1-2: rows (?P<r>\d+), S differ (?P<s>\d+)", "int(s) == 0",
          "sum S of every token after tier 1, read word k lane l of vbank#0 (a wrong layout changes S)", "S", "has::layernorm_row, the row LayerNorm of the MAS formulas (its statistics and outputs match the MAS reference vectors, step 6)",
          "per token", "{r} tokens (1 to 32 per group, H 1 to 768), S {s} differ"),
        C("rce_ln.log", r"layernorm \(HAS_GVU_LN, lane statistics\) \| groups (?P<g>\d+) \| not expressible (?P<x>\d+) \| lanes (?P<l>\d+) \| differ (?P<d>\d+)",
          "int(d) == 0", "groups staged by the DFC in the transformer check program", "M", "the model's row LayerNorm", "lane by lane",
          "{g} groups, {l} lanes, {d} differ", scope="end"),
    ]),
    (2, "beta_k (int32) in sbank buffer 0, gamma_k (int8) in sbank buffer 1, word k.", [
        C("tb_gvu_ln_lanes_tiers.log", r"parameters: beta words (?P<b>\d+), gamma words (?P<g>\d+), placement errors (?P<e>\d+)", "int(e) == 0",
          "placement of beta and gamma in the sbank", "D", "GVU_OPS_ON_GVUTOP.md section 5", "word of every parameter",
          "{b} beta and {g} gamma words, {e} errors"),
    ]),
    (3, "Tier 1: S += x_k (adder #1). Tier 2: D_k = H x x_k - S (multiplier #0, adder #2), V += D_k^2 (adder #0, 64 bit), "
        "v = requant(V >> Pre_Shift).", [
        C("tb_gvu_ln_lanes_tiers.log", r"tier 1-2: rows (?P<r>\d+), S differ (?P<s>\d+), V differ (?P<v>\d+), v differ (?P<q>\d+)", "int(s) == 0 and int(v) == 0 and int(q) == 0",
          "per token: S, V and v after tiers 1 and 2", "S", "has::layernorm_row, the row LayerNorm of the MAS formulas (its statistics and outputs match the MAS reference vectors, step 6)",
          "per token", "{r} tokens, S {s}, V {v}, v {q} differ"),
        N("tb_gvu_ln_lanes_tiers.log", r"negative: reference with Pre_Shift \+ 1, v differ (?P<d>\d+) of (?P<n>\d+)", "int(d) > 0",
          "the same check against a reference with a wrong variance scale", "S", "has::layernorm_row with Pre_Shift + 1",
          "the compare must find the error", "{d} of {n} tokens differ: caught"),
    ]),
    (4, "Tier 3 per lane: w = clamp((v - Z_var) x M0_7 + E_bias ...) - Z_7 -> rsqrt LUT -> r, E.", [
        C("tb_gvu_ln_lanes_tiers.log", r"tier 3: rows (?P<r>\d+), w differ (?P<w>\d+), r differ (?P<a>\d+), E differ (?P<e>\d+)", "int(w) == 0 and int(a) == 0 and int(e) == 0",
          "w, r and E of every token", "S", "has::layernorm_row, the row LayerNorm of the MAS formulas (its statistics and outputs match the MAS reference vectors, step 6)", "per token",
          "{r} tokens, w {w}, r {a}, E {e} differ"),
    ]),
    (5, "Tier 4: D_k recomputed from x_k; n_k = clamp(RS(r x D_k x M0_div, TS_div + E) + Z_div) written in place in vbank#0.", [
        C("tb_gvu_ln_lanes_tiers.log", r"tier 4: elements (?P<n>\d+), n differ (?P<d>\d+)", "int(d) == 0",
          "n_k of every element after tier 4", "S", "has::layernorm_row, the row LayerNorm of the MAS formulas (its statistics and outputs match the MAS reference vectors, step 6)", "per element",
          "{n} elements, {d} differ"),
    ]),
    (6, "Tier 5: y_k = clamp(RS((n_k - Z_div) x gamma_k x M0_mul, TS_mul) + beta_k + Z_out) written in place in vbank#0.", [
        C("tb_gvu_ln_lanes_tiers.log", r"tier 5: elements (?P<n>\d+), y differ (?P<d>\d+)", "int(d) == 0",
          "y_k of every element after tier 5", "S", "has::layernorm_row, the row LayerNorm of the MAS formulas (its statistics and outputs match the MAS reference vectors, step 6)", "per element",
          "{n} elements, {d} differ"),
        C("tb_gvu_ln_lanes.log", r"tb_gvu_ln_lanes: (?P<g>\d+) groups, lanes (?P<l>\d+), differ (?P<d>\d+)", "int(d) == 0",
          "300 random groups", "M", "has::layernorm_row (the model's row LayerNorm)", "lane by lane",
          "{g} groups, {l} lanes, {d} differ"),
        C("tb_gvu_layernorm.log", r"RESULT: PASS \((?P<c>\d+) cases\)", "True",
          "the model's row LayerNorm, statistics and n / y, ViT-B/16 rows", "S", "reference vectors of the MAS formulas",
          "row by row", "{c} cases, 0 mismatches"),
    ]),
    (7, "Job on the clock: each tier reads the bank once per edge, beat k is processed at edge k + 2; no Controller state and "
        "no registers; the DFC waits for the job.", [
        C("rce_ln.log", r"jobs (?P<j>\d+) \(failed (?P<f>\d+)\), edges (?P<e>\d+), unit ticks (?P<t>\d+) \(\+(?P<a>\d+) after done to IDLE\)",
          "int(f) == 0 and int(a) == 0", "LayerNorm jobs of the transformer check program on the clock", "D",
          "job protocol (GVU_OPS_ON_GVUTOP.md section 1)", "a job must end and the DFC go on at once", "{j} jobs, {f} failed, {e} edges"),
        C("rce_ln.log", r"layernorm \(HAS_GVU_LN, lane statistics\) \| groups (?P<g>\d+) \| not expressible (?P<x>\d+) \| lanes (?P<l>\d+) \| differ (?P<d>\d+)",
          "int(d) == 0", "LayerNorm of the transformer check program on the lanes", "M", "the model's row LayerNorm",
          "lane by lane", "{g} groups, {l} lanes, {d} differ"),
    ], ["No RTL exists for this flow; the cycle count is an estimate."]),
], whole=[
    C("rce_ln.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
      "int(be) == 0", "transformer check program with LayerNorm on the lanes", "S", "make_rce_gate.py golden", "byte by byte",
      "{s} steps, {e} elements, {be} bad", scope="end"),
])

# --------------------------------------------------------------------------------------------------------------------
# DMA_TENSOR COPY (proposal, opcode 0x16)
# --------------------------------------------------------------------------------------------------------------------
flow("DMA_COPY", "DMA_TENSOR COPY flow", [
    (1, "The front end emits one COPY per contiguous input segment of a concat; slices stay views. Push-time checks: LEN 0, "
        "beyond 2^32 or overlap -> BAD_GEOM; op != 0 -> UNSUPPORTED_MODE; knob off -> UNSUPPORTED_OP.", [
        C("tb_has_dma_tensor_isa.log", r"push-time checks: cases (?P<n>\d+), refused as expected (?P<r>\d+), accepted as expected (?P<a>\d+), not as expected (?P<x>\d+)",
          "int(x) == 0", "every push-time check of the instruction unit test", "D", "DMA_TENSOR_MODE.md", "status code per case",
          "{n} cases, {r} refused and {a} accepted as expected, {x} not as expected"),
        N("dma_neg.log", r"REFUSED", "True", "the check program with HAS_DMA_TENSOR=0", "D", "DMA_TENSOR_MODE.md",
          "the COPY instructions must be refused", "refused (count checked by the regression: 19)"),
    ]),
    (2, "The DFC waits until every DMA channel is idle, then reads segment k + 1 (CH0, C = 1024 B) into one half of the "
        "staging bank (bank 4 of the DMA SRAM) ...", [
        C("dma_cycles.log", r"COPY cycles: instructions (?P<n>\d+), equal to the formula (?P<e>\d+)", "e == n",
          "cycles of every COPY of the check program", "D",
          "T = sum max(R_k, W_k-1) + (n + 1), R = W = L + 8 x ceil(c / (8 x B/beat)) (DMA_TENSOR_MODE.md)",
          "per instruction", "{e} of {n} equal to the formula"),
    ]),
    (3, "... while segment k is written from the other half; the COPY ends when both ports are idle.", [
        C("dma.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
          "int(be) == 0", "check program of 25 instructions (19 COPY: aligned and unaligned, 1 to 129 bytes, read after write, write after read)",
          "S", "make_dma_tensor_gate.py golden", "byte by byte", "{s} steps, {e} elements, {be} bad"),
        C("dma_ref.log", r"RESULT: MATCH", "True", "the model's DRAM image", "S",
          "tools/has/ref_program.py, an independent interpreter of the instruction stream", "whole image", "MATCH"),
    ]),
])

# --------------------------------------------------------------------------------------------------------------------
# MOVE (proposal, unit test only)
# --------------------------------------------------------------------------------------------------------------------
flow("MOVE", "MOVE flow (proposal)", [
    (1, "The DFC writes Reg 16..24 (source and destination descriptors), then CONTROL (op 10 proposed, en 1).", [
        C("tb_gvu_agu_nd.log", r"T4 register packing \(Reg 16\.\.24\): (?P<n>\d+) descriptors, (?P<d>\d+) differ", "int(d) == 0",
          "descriptor packing into Reg 16..24", "D", "SLICE_CONCAT_ON_GVU.md", "round trip", "{n} descriptors, {d} differ"),
    ]),
    (2, "The controller enables MOVE in the VLSU (ELEM_WISE MOVE).", [
        C("tb_gvu_agu_nd_move.log", r"T5 GvuVlsuNd address = GvuAguNd, general programs: (?P<c>\d+) cycles, (?P<d>\d+) differ", "int(d) == 0",
          "addresses of the load / store unit in MOVE mode", "D", "AGU_ND program", "every cycle", "{c} cycles, {d} differ"),
    ]),
    (3, "The source AGU_ND reads one row per cycle from vbank#0.", [
        C("tb_gvu_agu_nd_move.log", r"concat C: A \[6\]\[5\]\[3\] -> out\[0:6\]\s+N\s+(?P<n>\d+) cycles\s+(?P<c>\d+) idle\s+(?P<i>\d+)\s+rows wrong (?P<w>\d+)",
          "int(w) == 0", "a channel concat moved row by row", "D", "rows of the expected tensor built in the bench", "every row",
          "{n} rows, {c} cycles, {w} rows wrong"),
    ]),
    (4, "The destination AGU_ND writes the same row into vbank#2, 2 cycles later.", [
        C("tb_gvu_agu_nd_move.log", r"permute \[C\]\[H\]\[Wt\] -> \[H\]\[C\]\[Wt\], 5x6x2\s+N\s+(?P<n>\d+) cycles\s+(?P<c>\d+) idle\s+(?P<i>\d+)\s+rows wrong (?P<w>\d+)",
          "int(w) == 0 and int(c) == int(n) + 3", "a permute moved row by row", "D", "expected tensor built in the bench",
          "every row; cycles = N + 3", "{n} rows, {c} cycles, {w} rows wrong"),
    ]),
    (5, "o_pipe_done -> DFC. N rows without idle cycles take N + 3 cycles.", [
        C("tb_gvu_agu_nd_move.log", r"C2f model\.6 tile: in2 \[4\]\[40\]\[2\] -> out\[8:\]\s+N\s+(?P<n>\d+) cycles\s+(?P<c>\d+) idle\s+(?P<i>\d+)",
          "int(c) == int(n) + 3 and int(i) == 0", "a YOLOv8m C2f tile", "D", "N + 3 cycles", "cycles", "{n} rows, {c} cycles, idle {i}"),
    ], ["MOVE exists only in has/gvu_agu_nd_move.h, not in GvuTop and not on the instruction path."]),
])

# --------------------------------------------------------------------------------------------------------------------
# Slice / concat by the 4-D DMA (HAS_DMA_ND)
# --------------------------------------------------------------------------------------------------------------------
flow("DMA_SLICE_CONCAT", "Slice / concat by the 4-D DMA flow", [
    (1, "ELEM_WISE mode 6 (slice) or 7 (concat) with HAS_DMA_ND=1; with the knob off the instruction is refused.", [
        N("dsc_neg.log", r"REFUSED", "True", "the slice / concat check program with HAS_DMA_ND=0", "D", "DMA_SLICE_CONCAT.md",
          "every slice / concat instruction must be refused", "refused (count checked by the regression: 14)"),
    ]),
    (2, "The DFC builds 4-D descriptors (base, sizes and strides of up to four axes) for source and destination.", [
        C("tb_has_dma.log", r"\[D3\] 4-D DMA Slice/Split/Concat .*mismatches (?P<m>\d+), rd_nd_ops (?P<r>\d+), wr_nd_ops (?P<w>\d+)", "int(m) == 0",
          "slices and concats along C, H, W and an SPPF 4-way concat through the 4-D descriptors", "S",
          "expected tensors computed in the bench with plain loops", "every byte", "{r} reads, {w} writes, {m} mismatches"),
    ]),
    (3, "The tensor moves in chunks of up to 1024 B through DMA bank 4: 4-D read into the bank, 4-D write out of it.", [
        C("big_sc.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
          "int(be) == 0", "slices and concats of 256 KB tensors, larger than SRAM-A", "S",
          "golden written with numpy by make_dma_nd_big_gate.py", "byte by byte", "{s} steps, {e} elements, {be} bad"),
    ]),
    (4, "Concat = the inputs written into one destination at their offsets along the axis.", [
        C("tb_has_dma.log", r"\[D5\] YOLOv8m C2f 3-way split/concat \+ 300 random 4-D slice/concat descriptors: mismatches (?P<m>\d+)", "int(m) == 0",
          "C2f 3-way split / concat and 300 random descriptors", "S", "expected tensors computed in the bench", "every byte",
          "{m} mismatches"),
        C("dsc.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
          "int(be) == 0", "check program of 27 instructions (slices along C / H / W with stride, 2- and 3-way concats, C2f, SPPF)",
          "S", "golden of make_ref_gate_dma_slice_concat.py", "byte by byte", "{s} steps, {e} elements, {be} bad"),
        C("dsc_ref.log", r"RESULT: MATCH", "True", "the model's DRAM image", "S", "tools/has/ref_program.py", "whole image", "MATCH"),
    ]),
    (5, "Tile gather / scatter of GEMM tiles through the same descriptors (int32 SRAM-C -> int8 write-back).", [
        C("tb_has_dma.log", r"\[D4\] 3-D tile SRAM-C int32->int8 write-back scatter/concat: mismatches (?P<m>\d+)", "int(m) == 0",
          "tile write-back by 3-D descriptors", "S", "expected bytes computed in the bench", "every byte", "{m} mismatches"),
        C("gate_dnd.log", r"\[tb_fe_core_net\] RESULT: PASS \(steps (?P<s>\d+), .*elements (?P<e>\d+), bad tensors (?P<bt>\d+), bad elements (?P<be>\d+)",
          "int(be) == 0", "check program of 6 instructions with tile gather / scatter by 4-D descriptors", "M",
          "the same program with the knob off: DRAM image and cycles equal (checked by the regression)", "byte by byte",
          "{s} steps, {e} elements, {be} bad"),
    ]),
])


# --------------------------------------------------------------------------------------------------------------------
def run_check(c, logdir):
    path = os.path.join(logdir, c.log)
    if not os.path.isfile(path):
        if c.optional:
            return "OPTIONAL", "not run here: the input of this check is not part of this tree", ""
        return "NOT RUN", "log %s not found" % c.log, ""
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()
    rx = re.compile(c.pattern)
    found = []
    for i, line in enumerate(lines):
        m = rx.search(line)
        if not m:
            continue
        g = {k: v for k, v in m.groupdict().items()}
        env = dict(g)
        env.update({"int": int, "min": min})
        try:
            ok = bool(eval(c.cond, {"__builtins__": {}}, env))
        except Exception as e:
            return "FAIL", "condition error %s" % e, "%s:%d: %s" % (c.log, i + 1, line.strip()[:200])
        found.append((ok, c.result.format(**g), "%s:%d: %s" % (c.log, i + 1, line.strip()[:200])))
        if not c.every:
            break
    if not found:
        return "FAIL", "expected line not found (pattern %s)" % c.pattern, c.log
    if not c.every:
        return ("PASS" if found[0][0] else "FAIL"), found[0][1], found[0][2]
    res = "; ".join(f[1] for f in found)
    ev = "\n                ".join(f[2] for f in found)
    return ("PASS" if all(f[0] for f in found) else "FAIL"), "%s (%d lines)" % (res, len(found)), ev


def dma_copy_cycles(logdir):
    """Cycles of every COPY of the DMA_TENSOR check program (trace lines of HAS_TRACE_DMA_TENSOR=1 in dma.log) against the formula of
    DMA_TENSOR_MODE.md section 4, computed here from the bytes, the chunk and the DMA profile line of the same log. Writes
    dma_cycles.log."""
    src = os.path.join(logdir, "dma.log")
    if not os.path.isfile(src):
        return
    beat, lat = None, None
    rows = []
    with open(src, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.search(r"DMA has \((\d+) B/beat, burst (\d+), DRAM latency (\d+)", line)
            if m:
                beat, burst, lat = int(m.group(1)), int(m.group(2)), int(m.group(3))
            m = re.search(r"\[dma-tensor\] push (\d+): bytes (\d+), chunk (\d+), cycles waiting for idle DMA (\d+), cycles of the copy (\d+)", line)
            if m:
                rows.append(tuple(int(x) for x in m.groups()))
    if beat is None or not rows:
        return

    def side(c):   # R_k = W_k = L + 8 * ceil(c / (8 * B))
        return lat + 8 * -(-c // (8 * beat))

    eq = 0
    with open(os.path.join(logdir, "dma_cycles.log"), "w", encoding="utf-8") as out:
        out.write("COPY cycles against T(N) = sum_k max(R_k, W_k-1) + (n + 1), R_k = W_k = L + 8 ceil(c_k / (8 B)); B %d, L %d\n" % (beat, lat))
        for push, n, C, waited, got in rows:
            nk = -(-n // C)
            c = [min(C, n - k * C) for k in range(nk)]
            t = sum(max(side(c[k]) if k < nk else 0, side(c[k - 1]) if k > 0 else 0) for k in range(nk + 1)) + (nk + 1)
            eq += t == got
            out.write("push %d: bytes %d, chunk %d, chunks %d, formula %d, model %d %s (waited for idle DMA %d before)\n"
                      % (push, n, C, nk, t, got, "equal" if t == got else "DIFFERENT", waited))
        out.write("COPY cycles: instructions %d, equal to the formula %d\n" % (len(rows), eq))


def block(fh, flow_name, step_no, text, checks, notes, whole=False):
    fh.write("[%s] [step %s] %s\n" % (flow_name, step_no, text))
    verdicts, scopes = [], []
    for c in checks:
        v, res, ev = run_check(c, LOGDIR)
        if v != "OPTIONAL":
            verdicts.append(v)
            scopes.append(c.scope)
        tag = "negative " if c.negative else ""
        fh.write("  %scheck : %s\n" % (tag, c.checks))
        fh.write("    reference : (%s, %s) %s\n" % (c.kind, KINDS[c.kind], c.reference))
        fh.write("    method    : %s\n" % c.method)
        fh.write("    result    : %s\n" % res)
        if ev:
            fh.write("    evidence  : %s\n" % ev)
        fh.write("    verdict   : %s%s\n" % (v, " (end to end)" if c.scope == "end" and v == "PASS" and not whole else ""))
    for n in notes:
        fh.write("  note   : %s\n" % n)
    if not verdicts:
        verdict = "NOT CHECKED"
    elif "FAIL" in verdicts:
        verdict = "FAIL"
    elif "NOT RUN" in verdicts:
        verdict = "NOT RUN"
    elif all(s == "end" for s in scopes) and not whole:
        verdict = "PASS (end to end only)"
    else:
        verdict = "PASS"
    if verdict.startswith("PASS") and any(n.startswith("Known RTL difference") for n in notes):
        verdict += ", OPEN: known RTL difference (note)"
    fh.write("  => step %s: %s\n\n" % (step_no, verdict))
    kinds = "".join(sorted(set(c.kind for c in checks if not c.negative)))
    neg = sum(1 for c in checks if c.negative)
    return verdict, kinds, neg


def main():
    global LOGDIR
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    LOGDIR = os.path.abspath(sys.argv[1])
    out = os.path.join(LOGDIR, "flows")
    if "--out" in sys.argv:
        out = os.path.abspath(sys.argv[sys.argv.index("--out") + 1])
    os.makedirs(out, exist_ok=True)
    dma_copy_cycles(LOGDIR)
    rows, bad = [], 0
    for name, page, steps, whole in FLOWS:
        p = os.path.join(out, name + ".log")
        with open(p, "w", encoding="utf-8") as fh:
            fh.write("Flow %s (drawing page \"%s\"), checks read from %s\n" % (name, page, LOGDIR))
            fh.write("Reference kinds: R RTL simulation, S specification, D proposed definition, M model only.\n\n")
            for st in steps:
                no, text, checks = st[0], st[1], st[2]
                notes = st[3] if len(st) > 3 else []
                v, kinds, neg = block(fh, name, no, text, checks, notes)
                rows.append((name, no, v, kinds, neg, len(checks)))
                bad += not v.startswith("PASS")
            if whole:
                v, kinds, neg = block(fh, name, "whole flow", "The whole flow on check programs and network layers.", whole, [], True)
                rows.append((name, "all", v, kinds, neg, len(whole)))
                bad += not v.startswith("PASS")
    with open(os.path.join(out, "SUMMARY.md"), "w", encoding="utf-8") as fh:
        fh.write("# Verification by instruction flow\n\nChecks read from `%s`. One log per flow in this directory.\n\n" % LOGDIR)
        fh.write("Reference kinds: R RTL simulation, S specification, D proposed definition, M model only.\n\n")
        fh.write("| Flow | Step | Verdict | References | Checks | Negative checks |\n|---|---|---|---|---:|---:|\n")
        for r in rows:
            fh.write("| %s | %s | %s | %s | %d | %d |\n" % (r[0], r[1], r[2], r[3], r[5], r[4]))
        n_pass = sum(1 for r in rows if r[2].startswith("PASS"))
        fh.write("\n%d of %d steps PASS; %d not PASS.\n" % (n_pass, len(rows), len(rows) - n_pass))
    for r in rows:
        print("%-18s %-10s %s" % (r[0], r[1], r[2]))
    print("RESULT: %s (%d of %d steps not PASS), logs in %s" % ("PASS" if bad == 0 else "FAIL", bad, len(rows), out))
    return 0 if bad == 0 else 1


LOGDIR = "."
if __name__ == "__main__":
    sys.exit(main())
