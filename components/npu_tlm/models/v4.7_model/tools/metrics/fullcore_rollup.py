#!/usr/bin/env python3
"""Roll up a chained tb_fe_core_net run without extrapolating missing tiles.

Usage:
  python3 tools/metrics/fullcore_rollup.py --program fe_work/step6/program.json \
      --csv fe_work/step6b/RUN/metrics_tiles.csv --log fe_work/step6b/RUN/run.log \
      --out-dir fe_work/step6b/RUN/rollup
  python3 tools/metrics/fullcore_rollup.py --has \
      --csv fe_work/has/RUN/metrics_tiles.csv --log fe_work/has/RUN/run.log --out-dir OUT/rollup

The input CSV may still be growing. Duplicate (step,tile) records from a resume are
resolved by keeping the last complete record. Report distinguishes measured core
counters, testbench counters, and derived ratios. It never fills missing tiles.

--has: run of tools/has/tb_has_npu_top (instruction stream, HasNpuTop). The CSV `step` is the 0-based instruction
index and `tile` counts core passes (an input-channel split tile has several passes). Layer names, output shapes and
kernel geometry come from the [STEP] comments of run.log, not from a program.json. PE utilization and GOPS use the
REAL MACs of each layer (output C*H*W * cin*kh*kw); the CSV `macs_theory` also counts PAD_TAIL padding and is
reported as executed MACs.
"""

import argparse
import csv
import hashlib
import json
import re
from collections import Counter, OrderedDict
from pathlib import Path

SUM_FIELDS = (
    "ticks_all", "busy", "exec", "pe_cycles", "mac_nz", "macs_theory",
    "a_rd_beats", "a_rd_bytes", "b_rd_beats", "b_rd_bytes",
    "c_rd_beats", "c_rd_bytes", "c_wr_beats", "c_wr_bytes",
) + tuple("st%02d" % i for i in range(1, 25))
FREQUENCY_HZ = 800_000_000
PE_COUNT = 1024
STEP_RE = re.compile(r"\[STEP\] (\d+)/(\d+).*?bad=(\d+) (PASS|FAIL) tiles=(\d+) via_core=(\d+) sim_cycles=(\d+)")
FINAL_RE = re.compile(r"RESULT: (PASS|FAIL) \(steps (\d+), tensors (\d+), elements (\d+), bad tensors (\d+), bad elements (\d+), framing errors (\d+), core deadlocks (\d+)\)")
TB_RE = re.compile(r"\[tb_fe_core_net\] tiles (\d+) \(via core (\d+)\), OBP vectors (\d+), MACs \(stand-in tiles\) (\d+), sim cycles (\d+) \(DMA wait (\d+), OBP (\d+), OBP config approx (\d+)\)")


def read_tiles(path):
    meta, rows, header, malformed, duplicates = {}, OrderedDict(), None, 0, 0
    with path.open(encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            if line.startswith("# meta,"):
                meta = dict(part.split("=", 1) for part in line[7:].split(",") if "=" in part)
                continue
            if line.startswith("step,tile,"):
                header = next(csv.reader([line]))
                continue
            if header is None:
                malformed += 1
                continue
            parts = next(csv.reader([line]))
            if len(parts) != len(header):
                malformed += 1
                continue
            try:
                row = {key: int(value) for key, value in zip(header, parts)}
            except ValueError:
                malformed += 1
                continue
            key = row["step"], row["tile"]
            if key in rows:
                duplicates += 1
            rows[key] = row
    return meta, rows, malformed, duplicates


def read_log(path):
    steps, result, tb = [], None, None
    for line in path.open(encoding="utf-8", errors="replace"):
        match = STEP_RE.search(line)
        if match:
            n, total, bad, verdict, tiles, via_core, cycles = match.groups()
            steps.append(dict(step=int(n), total_steps=int(total), bad_elements=int(bad),
                              verdict=verdict, tiles=int(tiles), via_core=int(via_core),
                              sim_cycles=int(cycles)))
        match = FINAL_RE.search(line)
        if match:
            v, n, tensors, elems, bad_t, bad_e, framing, deadlocks = match.groups()
            result = dict(verdict=v, steps=int(n), tensors=int(tensors), elements=int(elems),
                          bad_tensors=int(bad_t), bad_elements=int(bad_e),
                          framing_errors=int(framing), core_deadlocks=int(deadlocks))
        match = TB_RE.search(line)
        if match:
            fields = ("tiles", "via_core", "obp_vectors", "standin_macs", "sim_cycles",
                      "dma_wait_cycles", "obp_cycles", "obp_config_approx_cycles")
            tb = dict(zip(fields, map(int, match.groups())))
    return steps, result, tb


def totals(rows):
    out = {field: 0 for field in SUM_FIELDS}
    out["tiles_measured"] = 0
    out["tiles_ok"] = 0
    for row in rows:
        out["tiles_measured"] += 1
        out["tiles_ok"] += row["ok"] == 1
        for field in SUM_FIELDS:
            out[field] += row[field]
    out["stall"] = out["busy"] - out["exec"]
    out["pe_util_pct"] = 100 * out["macs_theory"] / (out["busy"] * PE_COUNT) if out["busy"] else None
    out["exec_share_pct"] = 100 * out["exec"] / out["busy"] if out["busy"] else None
    out["core_gops"] = 2 * out["macs_theory"] * FREQUENCY_HZ / out["busy"] / 1e9 if out["busy"] else None
    return out


FSM_NAMES = ("START_FLAGS", "ARRAY_PREP", "ARRAY_FILL", "FIRST_SHIFT", "START_COMP", "DRAIN_FEED", "ARRAY_FLUSH",
             "WAIT_CSWITCH", "WAIT_CSWITCH_STALL", "WAIT_OBUF", "WAIT_OBUF_STALL", "SCND_SHIFT", "SCND_SHIFT_STALL",
             "ALL_BUSY_SHIFT", "ALL_BUSY", "ARRAY_BUSY", "OBUF_BUSY_SHIFT", "FORCE_STALL", "OBUF_BUSY", "ARRAY_CSWITCH",
             "ARRAY_CSWITCH_STALL", "LAST_SHIFT", "LAST_WAIT", "DONE")  # same order as agg_bigrun.py
HAS_STEP_RE = re.compile(r"\[STEP\] (\d+)/(\d+) kind=\d+ tensor=\d+ \[([\d,]+)\].*?bad=(\d+) (PASS|FAIL) tiles=(\d+) "
                         r"via_core=(\d+) sim_cycles=(\d+)\s*#\s*\[(\d+)\] (?:step \d+ )?(\w+) (.*)$")
DFC_RE = re.compile(r"\[DFC\] instr (\d+) op \S+ (\w+) tiles (\d+) cycles (\d+)")
PASSES_RE = re.compile(r"core passes (\d+) for (\d+) tiles")
ATTN_CORE_RE = re.compile(r"RCE (?:products )?on (?:the )?core.*?passes (\d+), core cycles (\d+)")
VU_EST_RE = re.compile(r"FUSED_ATTN (\d+), LAYERNORM (\d+), rows (\d+), cycles (\d+)")


def has_real_macs(op, dims, detail):
    """Real MACs of one instruction: same rule as tools/metrics/has_rollup.py real_macs() (kept identical)."""
    if op == "GEMM_FUSED":
        m = re.search(r"\bcin (\d+)", detail)
        if not m or len(dims) != 3:
            return None
        k = re.search(r"\b(\d+)x(\d+) s\d+", detail)
        kk = 1 if (not k or "im2col" in detail) else int(k.group(1)) * int(k.group(2))
        return dims[0] * dims[1] * dims[2] * int(m.group(1)) * kk
    if op == "FUSED_ATTN":
        m = re.search(r"\bNQ (\d+) L (\d+) D (\d+)", detail)
        return 2 * int(m.group(1)) * int(m.group(2)) * int(m.group(3)) if m else None
    return 0


def main_has(args):
    """Roll-up of a tb_has_npu_top run (see the module docstring, --has)."""
    meta, records, malformed, duplicates = read_tiles(args.csv)
    instrs, dfc, result, tb, passes_line, attn_core, vu_est = OrderedDict(), {}, None, None, None, None, None
    prev_cycles = prev_tiles = 0
    for line in args.log.open(encoding="utf-8", errors="replace"):
        m = HAS_STEP_RE.search(line)
        if m:
            n, total, shape, bad, verdict, tiles_cum, _, cyc, idx, op, rest = m.groups()
            dims = [int(x) for x in shape.split(",")]
            # layer name = text before the geometry ("stem 3x3 s2 ...", "b0 qkv cin 768 ...", "embed 16x16 s16 ...")
            name = re.split(r"\s+(?=\d+x\d+ s\d+|cin |head \d|rows \d|x0 =|y = |x1 = |\d+ elements)", rest)[0].rstrip(",")
            if op == "FUSED_ATTN":
                name = re.split(r"\s+NQ ", rest)[0]
            op_name = op
            if op == "ELEM_WISE":
                op_name = "ELEM_" + rest.split(" ")[0]
            instrs[int(idx)] = dict(instr=int(n), total=int(total), op=op_name, name=name, shape=shape, verdict=verdict,
                                    bad_elements=int(bad), tiles=int(tiles_cum) - prev_tiles,
                                    real_macs=has_real_macs(op, dims, rest), cycles=int(cyc) - prev_cycles)
            prev_cycles, prev_tiles = int(cyc), int(tiles_cum)
        m = DFC_RE.search(line)
        if m:
            dfc[int(m.group(1)) - 1] = int(m.group(4))
        m = FINAL_RE.search(line)
        if m:
            v, n, tensors, elems, bad_t, bad_e, framing, deadlocks = m.groups()
            result = dict(verdict=v, steps=int(n), tensors=int(tensors), elements=int(elems), bad_tensors=int(bad_t),
                          bad_elements=int(bad_e), framing_errors=int(framing), core_deadlocks=int(deadlocks))
        m = TB_RE.search(line)
        if m:
            tb = dict(zip(("tiles", "via_core", "obp_vectors", "executed_macs", "sim_cycles", "dma_wait_cycles",
                           "obp_cycles", "obp_config_approx_cycles"), map(int, m.groups())))
        m = PASSES_RE.search(line)
        if m:
            passes_line = int(m.group(1))
        m = ATTN_CORE_RE.search(line)
        if m:
            attn_core = dict(passes=int(m.group(1)), core_cycles=int(m.group(2)))
        m = VU_EST_RE.search(line)
        if m:
            vu_est = dict(fused_attn=int(m.group(1)), layernorm=int(m.group(2)), rows=int(m.group(3)), cycles=int(m.group(4)))
    for i, c in dfc.items():  # exact per-instruction cycles when the run used --trace
        if i in instrs:
            instrs[i]["cycles"] = c

    issues, by_step = [], OrderedDict()
    for (step_id, tile_id), row in records.items():
        if step_id not in instrs or instrs[step_id]["op"] != "GEMM_FUSED":
            issues.append("CSV row %d:%d is not a GEMM_FUSED instruction of run.log" % (step_id, tile_id))
            continue
        if row["exec"] > row["busy"] or row["busy"] > row["ticks_all"]:
            issues.append("cycle ordering mismatch %d:%d" % (step_id, tile_id))
        if sum(row["st%02d" % i] for i in range(1, 25)) != row["busy"]:
            issues.append("FSM busy mismatch %d:%d" % (step_id, tile_id))
        if row["pe_cycles"] != row["exec"] * PE_COUNT:
            issues.append("PE cycles mismatch %d:%d" % (step_id, tile_id))
        by_step.setdefault(step_id, []).append(row)

    layer_rows = []
    for idx, ins in instrs.items():
        if ins["op"] != "GEMM_FUSED":
            continue
        rows = by_step.get(idx, [])
        t = totals(rows)
        real = ins["real_macs"] if ins["real_macs"] is not None else t["macs_theory"]
        fp, tl = Counter(), Counter()
        cap = 0
        for r in rows:
            fp[(r["nch"], r["yu"])] += 1
            tl[(r["wt"], r["ht"], r["nch"], r["cin"])] += 1
            cap += r["n_ctx"] * r["cin"] * r["kh"] * r["kw"] * PE_COUNT
        t.update(instr=ins["instr"], layer=ins["name"], tiles=ins["tiles"], passes=len(rows), instr_cycles=ins["cycles"],
                 real_macs=real, executed_macs=t["macs_theory"],
                 pe_util_pct=100 * real / (t["busy"] * PE_COUNT) if t["busy"] else None,
                 spatial_pct=100 * real / cap if cap else None,
                 core_gops=2 * real * FREQUENCY_HZ / t["busy"] / 1e9 if t["busy"] else None,
                 xy_used=", ".join("%dx%d" % k for k, _ in fp.most_common()),
                 tile_whcc="%d/%d/%d/%d" % tl.most_common(1)[0][0] if tl else "?")
        layer_rows.append(t)

    network = totals(r for rows in by_step.values() for r in rows)
    real_total = sum(l["real_macs"] for l in layer_rows)
    real_attn = sum(i["real_macs"] or 0 for i in instrs.values() if i["op"] == "FUSED_ATTN")
    ops = OrderedDict()
    for i in instrs.values():
        o = ops.setdefault(i["op"], dict(count=0, cycles=0))
        o["count"] += 1
        o["cycles"] += i["cycles"]
    network.update(real_macs_attention=real_attn, attention_core=attn_core, vector_unit_estimate=vu_est)
    if attn_core and real_attn:
        network["pe_util_with_attention_pct"] = 100 * (real_total + real_attn) / ((network["busy"] + attn_core["core_cycles"]) * PE_COUNT)
    network.update(real_macs=real_total, executed_macs=network["macs_theory"],
                   pe_util_pct=100 * real_total / (network["busy"] * PE_COUNT) if network["busy"] else None,
                   core_gops=2 * real_total * FREQUENCY_HZ / network["busy"] / 1e9 if network["busy"] else None,
                   tiles=tb["tiles"] if tb else sum(l["tiles"] for l in layer_rows),
                   passes_expected=passes_line if passes_line else (tb["tiles"] if tb else None))
    if result:
        if network["passes_expected"] is not None and network["tiles_measured"] != network["passes_expected"]:
            issues.append("CSV has %d core passes, run.log reports %s" % (network["tiles_measured"], network["passes_expected"]))
        if result["verdict"] != "PASS":
            issues.append("testbench result is FAIL")
    if malformed:
        issues.append("%d malformed/incomplete CSV lines" % malformed)
    if network["tiles_ok"] != network["tiles_measured"]:
        issues.append("%d core-pass rows have ok!=1" % (network["tiles_measured"] - network["tiles_ok"]))
    for ins in instrs.values():
        if ins["verdict"] != "PASS":
            issues.append("instruction %d FAIL: %d bad elements" % (ins["instr"], ins["bad_elements"]))

    payload = dict(source=dict(mode="has", csv=str(args.csv), log=str(args.log), csv_meta=meta),
                   status="COMPLETE" if result else "IN_PROGRESS", network=network, testbench=tb, result=result,
                   latest_instruction=list(instrs.values())[-1] if instrs else None,
                   csv_malformed_lines=malformed, csv_duplicate_rows=duplicates, issues=issues)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "network.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    columns = ("instr", "layer", "tiles", "passes", "xy_used", "tile_whcc", "instr_cycles", *SUM_FIELDS, "stall",
               "real_macs", "executed_macs", "pe_util_pct", "spatial_pct", "exec_share_pct", "core_gops")
    with (args.out_dir / "layers.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(layer_rows)
    with (args.out_dir / "steps.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=("instr", "total", "op", "name", "shape", "verdict", "bad_elements",
                                                      "tiles", "cycles", "real_macs"))
        writer.writeheader()
        writer.writerows(instrs.values())

    fmt = lambda n: format(n, ",")
    pct = lambda x: "%.2f%%" % x if x is not None else "n/a"
    md = ["# Full-core roll-up (tb_has_npu_top, instruction stream)", "",
          "Status: **%s**; instructions finished: **%d / %s**" % (payload["status"], len(instrs),
                                                                   list(instrs.values())[-1]["total"] if instrs else "?"), "",
          "| Metric | Value | Basis |", "|---|---:|---|",
          "| Tiles / core passes measured | %s / %s | run.log / CSV rows (input-channel split: several passes per tile) |"
          % (fmt(network["tiles"]), fmt(network["tiles_measured"])),
          "| Core busy cycles | %s | measured FSM counters |" % fmt(network["busy"]),
          "| Core exec / stall cycles | %s / %s | measured / derived busy−exec |" % (fmt(network["exec"]), fmt(network["stall"])),
          "| MACs, real (GEMM_FUSED) | %s | derived: output C*H*W * cin*kh*kw per layer (run.log [STEP] comments) |" % fmt(real_total),
          "| MACs, executed incl. padding | %s | measured CSV workload counter |" % fmt(network["macs_theory"]),
          "| PE utilization | %s | derived real MAC/(busy×1024) |" % pct(network["pe_util_pct"]),
          "| Core GOPS @0.8 GHz | %s | derived from measured busy, real MACs |"
          % ("%.2f" % network["core_gops"] if network["core_gops"] is not None else "n/a"),
          "| SRAM A/B reads | %s / %s B | measured core counters |" % (fmt(network["a_rd_bytes"]), fmt(network["b_rd_bytes"])),
          "| SRAM C reads/writes | %s / %s B | measured core counters |" % (fmt(network["c_rd_bytes"]), fmt(network["c_wr_bytes"]))]
    if real_attn:
        md.append("| MACs, real (FUSED_ATTN Q.K^T + A.V) | %s | derived: 2 x NQ x L x D per head |" % fmt(real_attn))
    if attn_core:
        md.append("| Attention products on the core | %s passes / %s cycles | measured (not part of the GEMM core busy above) |"
                  % (fmt(attn_core["passes"]), fmt(attn_core["core_cycles"])))
    if network.get("pe_util_with_attention_pct") is not None:
        md.append("| PE utilization incl. attention | %s | derived (GEMM + attention MACs) / ((busy + attention core cycles) x 1024) |"
                  % pct(network["pe_util_with_attention_pct"]))
    if vu_est:
        md.append("| Vector unit (FUSED_ATTN %d, LAYERNORM %d) | %s cycles | ESTIMATE: softmax / LayerNorm pipelines + their DMA |"
                  % (vu_est["fused_attn"], vu_est["layernorm"], fmt(vu_est["cycles"])))
    if tb:
        md += ["| Whole-testbench cycles | %s | SystemC elapsed cycles; includes DMA, core, OBP, ELEM_WISE |" % fmt(tb["sim_cycles"]),
               "| DMA wait / OBP | %s / %s cycles | not hidden by the ping-pong schedule / epilogue; do not add to total |"
               % (fmt(tb["dma_wait_cycles"]), fmt(tb["obp_cycles"])),
               "| OBP configuration | %s cycles | testbench approximation |" % fmt(tb["obp_config_approx_cycles"])]
    if result:
        md += ["| Functional result | %s; bad elements %s; core noncompletions %s | testbench final verdict |" %
               (result["verdict"], fmt(result["bad_elements"]), fmt(result["core_deadlocks"]))]
    md += ["", "Full counters: `network.csv` is not written in this mode; per layer (incl. 24 FSM states): `layers.csv`; "
           "per instruction: `steps.csv`.", "", "## Per operation", "",
           "| Operation | Instructions | Cycles | Share of instruction cycles |", "|---|---:|---:|---:|"]
    all_cyc = sum(o["cycles"] for o in ops.values())
    for k, o in ops.items():
        md.append("| %s | %d | %s | %.1f%% |" % (k, o["count"], fmt(o["cycles"]), 100.0 * o["cycles"] / all_cyc if all_cyc else 0))
    md += ["", "## Per layer", "",
           "| Instr | Layer | Tiles | Passes | X x Y used | Tile W/H/Cout/Cin | Instr cycles | Busy | Exec | Stall | PE util | Spatial usage |",
           "|---:|---|---:|---:|---|---:|---:|---:|---:|---:|---:|---:|"]
    for l in layer_rows:
        md.append("| %d | %s | %d | %d | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            l["instr"], l["layer"], l["tiles"], l["passes"], l["xy_used"], l["tile_whcc"], fmt(l["instr_cycles"]),
            fmt(l["busy"]), fmt(l["exec"]), fmt(l["stall"]), pct(l["pe_util_pct"]), pct(l["spatial_pct"])))
    md += ["", "X x Y used: array columns (output channels) x rows (positions per context) of the core passes, most frequent first.",
           "Spatial usage: real MACs / (n_ctx x K x 1024), K = cin*kh*kw of the pass; PE util = real MACs / (busy x 1024).",
           "Instr cycles: whole instruction incl. DMA not hidden and epilogue ([DFC] lines if run with --trace, else [STEP] deltas).",
           "", "## FSM state distribution", "", "| State | Cycles | % of busy |", "|---|---:|---:|"]
    for q in range(1, 25):
        v = network["st%02d" % q]
        if v:
            md.append("| s%02d %s | %s | %.1f%% |" % (q, FSM_NAMES[q - 1], fmt(v), 100.0 * v / network["busy"]))
    md += ["", "## Checks", "", "- %s" % ("No issues found" if not issues else "Issues: %d" % len(issues))]
    md.extend("- %s" % issue for issue in issues[:30])
    md.append("")
    (args.out_dir / "report.md").write_text("\n".join(md), encoding="utf-8")
    print("%s: %d instructions, %d/%s core passes, %d issue(s); %s" % (
        payload["status"], len(instrs), network["tiles_measured"], network["passes_expected"], len(issues),
        args.out_dir / "report.md"))
    return 1 if issues else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--program", type=Path)
    parser.add_argument("--csv", type=Path, required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--has", action="store_true", help="tb_has_npu_top run (instruction stream), no program.json")
    args = parser.parse_args()
    if args.has:
        return main_has(args)
    if args.program is None:
        parser.error("--program is required (unless --has)")

    program_bytes = args.program.read_bytes()
    program = json.loads(program_bytes)
    plan = program["steps"]
    planned_tiles = sum(len(step.get("tiles", [])) for step in plan if step.get("kind") == "conv")
    meta, records, malformed, duplicates = read_tiles(args.csv)
    steps, result, tb = read_log(args.log)
    per_layer = OrderedDict()
    issues = []
    for (step_id, tile_id), row in records.items():
        if not 0 <= step_id < len(plan) or plan[step_id].get("kind") != "conv" or not 0 <= tile_id < len(plan[step_id]["tiles"]):
            issues.append("CSV tile %d:%d not in program" % (step_id, tile_id))
            continue
        step = plan[step_id]
        tile = step["tiles"][tile_id]
        expected = (tile["c"][1] - tile["c"][0], tile["oy"][1] - tile["oy"][0],
                    tile["ox"][1] - tile["ox"][0], tile["y_used"])
        actual = tuple(row[key] for key in ("nch", "ht", "wt", "yu"))
        if actual != expected:
            issues.append("CSV geometry mismatch %d:%d" % (step_id, tile_id))
            continue
        if row["exec"] > row["busy"] or row["busy"] > row["ticks_all"]:
            issues.append("cycle ordering mismatch %d:%d" % (step_id, tile_id))
        if sum(row["st%02d" % i] for i in range(1, 25)) != row["busy"]:
            issues.append("FSM busy mismatch %d:%d" % (step_id, tile_id))
        if row["pe_cycles"] != row["exec"] * PE_COUNT:
            issues.append("PE cycles mismatch %d:%d" % (step_id, tile_id))
        if row["c_wr_bytes"] != 4 * row["nch"] * row["ht"] * row["wt"]:
            issues.append("SRAM-C write mismatch %d:%d" % (step_id, tile_id))
        name = step["job"]
        per_layer.setdefault(name, []).append(row)

    layer_rows = []
    for step in plan:
        if step.get("kind") != "conv":
            continue
        name = step["job"]
        data = totals(per_layer.get(name, []))
        data = dict(layer=name, tiles_planned=len(step["tiles"]), **data)
        layer_rows.append(data)
    network = totals(row for rows in per_layer.values() for row in rows)
    network["tiles_planned"] = planned_tiles
    network["coverage_pct"] = 100 * network["tiles_measured"] / planned_tiles if planned_tiles else 0
    if result and tb:
        if tb["tiles"] != planned_tiles or tb["via_core"] != planned_tiles:
            issues.append("final tile coverage differs from plan")
        if network["tiles_measured"] != planned_tiles:
            issues.append("final CSV lacks measured tiles")
        if result["verdict"] != "PASS":
            issues.append("testbench result is FAIL")
    if malformed:
        issues.append("%d malformed/incomplete CSV lines" % malformed)
    if network["tiles_ok"] != network["tiles_measured"]:
        issues.append("%d tile rows have ok!=1" % (network["tiles_measured"] - network["tiles_ok"]))
    for step in steps:
        if step["verdict"] != "PASS":
            issues.append("step %d FAIL: %d bad elements" % (step["step"], step["bad_elements"]))

    payload = dict(source=dict(program=str(args.program), program_sha256=hashlib.sha256(program_bytes).hexdigest(),
                               csv=str(args.csv), log=str(args.log), csv_meta=meta),
                   status="COMPLETE" if result else "IN_PROGRESS", network=network,
                   testbench=tb, result=result, latest_step=steps[-1] if steps else None,
                   csv_malformed_lines=malformed, csv_duplicate_tiles=duplicates,
                   issues=issues)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "network.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    columns = ("layer", "tiles_planned", "tiles_measured", "tiles_ok", *SUM_FIELDS,
               "stall", "pe_util_pct", "exec_share_pct", "core_gops")
    with (args.out_dir / "layers.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(layer_rows)
    with (args.out_dir / "steps.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=("step", "total_steps", "verdict", "bad_elements",
                                                      "tiles", "via_core", "sim_cycles"))
        writer.writeheader()
        writer.writerows(steps)
    with (args.out_dir / "network.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("metric", "value", "basis"))
        writer.writerow(("status", payload["status"], "run"))
        for key, value in network.items():
            basis = "derived_core" if key in ("stall", "pe_util_pct", "exec_share_pct", "core_gops", "coverage_pct") else "measured_core_csv"
            if key == "tiles_planned":
                basis = "program_plan"
            writer.writerow((key, value, basis))
        if tb:
            for key, value in tb.items():
                writer.writerow(("tb_" + key, value, "approx_testbench" if "approx" in key else "measured_testbench"))
        if result:
            for key, value in result.items():
                writer.writerow(("result_" + key, value, "testbench_verdict"))

    fmt = lambda n: format(n, ",")
    markdown = ["# Full-core E2E roll-up", "", "Source plan SHA-256: `%s`  " % payload["source"]["program_sha256"],
                "Status: **%s**; latest step: **%s**" % (payload["status"], steps[-1]["step"] if steps else "none"), "",
                "| Metric | Value | Basis |", "|---|---:|---|",
                "| Core tiles measured / planned | %s / %s | CSV / program.json |" % (fmt(network["tiles_measured"]), fmt(planned_tiles)),
                "| Core busy cycles | %s | measured FSM counters |" % fmt(network["busy"]),
                "| Core exec / stall cycles | %s / %s | measured / derived busy−exec |" % (fmt(network["exec"]), fmt(network["stall"])),
                "| MACs, theoretical | %s | measured CSV workload counter |" % fmt(network["macs_theory"]),
                "| PE utilization | %s | derived MAC/(busy×1024) |" % ("%.2f%%" % network["pe_util_pct"] if network["pe_util_pct"] is not None else "n/a"),
                "| Core GOPS @0.8 GHz | %s | derived from measured busy |" % ("%.2f" % network["core_gops"] if network["core_gops"] is not None else "n/a"),
                "| SRAM A/B reads | %s / %s B | measured core counters |" % (fmt(network["a_rd_bytes"]), fmt(network["b_rd_bytes"])),
                "| SRAM C reads/writes | %s / %s B | measured core counters |" % (fmt(network["c_rd_bytes"]), fmt(network["c_wr_bytes"]))]
    if tb:
        markdown += ["| Whole-testbench cycles | %s | SystemC elapsed cycles; includes DMA, core, OBP, host flow |" % fmt(tb["sim_cycles"]),
                     "| DMA wait / OBP | %s / %s cycles | testbench counters, overlap possible; do not add to total |" % (fmt(tb["dma_wait_cycles"]), fmt(tb["obp_cycles"])),
                     "| OBP configuration | %s cycles | testbench approximation |" % fmt(tb["obp_config_approx_cycles"])]
    if result:
        markdown += ["| Functional result | %s; bad elements %s; core noncompletions %s | testbench final verdict |" %
                     (result["verdict"], fmt(result["bad_elements"]), fmt(result["core_deadlocks"]))]
    markdown += ["", "Full counters: `network.csv`; all layer counters (including 24 FSM states): `layers.csv`; functional step verdicts: `steps.csv`.",
                 "These are measured tiles only; no missing tile is extrapolated.",
                 "", "## Per layer", "", "| Layer | Tiles | Busy | Exec | Stall | PE util |", "|---|---:|---:|---:|---:|---:|"]
    for layer in layer_rows:
        markdown.append("| %s | %d/%d | %s | %s | %s | %s |" %
                        (layer["layer"], layer["tiles_measured"], layer["tiles_planned"],
                         fmt(layer["busy"]), fmt(layer["exec"]), fmt(layer["stall"]),
                         "%.2f%%" % layer["pe_util_pct"] if layer["pe_util_pct"] is not None else "n/a"))
    markdown += ["", "## Checks", "", "- %s" % ("No issues found" if not issues else "Issues: %d" % len(issues))]
    markdown.extend("- %s" % issue for issue in issues[:30])
    markdown += ["", "`ticks_all` is the per-tile core call window; summing it does not equal whole-network elapsed cycles.",
                 "The `core_deadlocks` final field counts core tile noncompletions, not transient backpressure pulses.", ""]
    (args.out_dir / "report.md").write_text("\n".join(markdown), encoding="utf-8")
    print("%s: %d/%d core tiles, %d issue(s); %s" %
          (payload["status"], network["tiles_measured"], planned_tiles, len(issues), args.out_dir / "report.md"))
    return 1 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
