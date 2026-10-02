#!/usr/bin/env python3
"""Render the network-level (`network` row) 60-metric table + provenance labels from <dir> (produced by sw_eval). Read-only. usage: render_table.py <dir>"""
import csv, json, os, sys
d = sys.argv[1]
P = json.load(open(os.path.join(d, "parameters.json")))
files = ["cycles", "engine", "dma", "stall", "utilization", "bandwidth", "memory", "memory_wait", "summary"]
out = ["# Network-level 60-metric table -- MODE %s (sw_eval)\n" % P.get("mode", "B"),
       "**MODE %s**: built from sauria_model's shape table x the program's tile count; **not** measured through the core port over a network run. "
       "Scope: %s\n" % (P.get("mode", "B"), "83 core conv layers (excludes OBP, OBP config load, host ops, inter-layer DMA)"),
       "| CSV | Metric | Value | Label |\n|---|---|---:|---|"]
for f in files:
    r = [x for x in csv.DictReader(open(os.path.join(d, f + ".csv"))) if x["layer_id"] == "network"][0]
    for k, v in r.items():
        if k in ("layer_id", "layer_name", "op_type"):
            continue
        out.append("| %s | `%s` | %s | %s |" % (f, k, v if v != "" else "(empty)", P["metric_provenance"][k].replace("|", " * ")))
out.append("\n## Metrics under the project's own definitions (names differ from the schema)\n")
r = [x for x in csv.DictReader(open(os.path.join(d, "project_metrics.csv"))) if x["layer_id"] == "network"][0]
out.append("| Metric | Value |\n|---|---:|")
for k, v in r.items():
    if k not in ("layer_id", "layer_name", "op_type"):
        out.append("| `%s` | %s |" % (k, v))
out.append("\n## Parameters (parameters.json)\n")
for k in ("l1_size_bytes", "l1_size_verified", "l2_size_bytes", "l2_size_breakdown_bytes", "l2_usable_per_tile_bytes", "scratch", "unverified_parameters"):
    out.append("- `%s`: %s" % (k, json.dumps(P[k], ensure_ascii=False)))
out.append("- `double_buffer`: %s -- %s" % (P["dma_timing_model"]["double_buffer"], P["dma_timing_model"]["double_buffer_note"]))
out.append("- `utilization_definition`: %s" % json.dumps(P["utilization_definition"], ensure_ascii=False))
open(os.path.join(d, "network_table.md"), "w", encoding="utf-8").write("\n".join(out) + "\n")
print("ok:", os.path.join(d, "network_table.md"))
