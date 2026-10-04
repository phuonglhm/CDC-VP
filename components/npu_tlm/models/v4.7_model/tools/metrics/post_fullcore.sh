#!/bin/bash
# Post-processing for a chained full-core tb_fe_core_net run, or a tools/has/tb_has_npu_top run (detected from run.log:
# [DFC] lines; per-layer data then come from the run.log [STEP] comments, --prog is not used, PE/GOPS use real MACs).
# Read-only on fe_work; writes only to the --out directory.
#
# Usage: bash tools/metrics/post_fullcore.sh [options]   (from any directory)
#   tb_has_npu_top example: --run $FE_WORK/reference_runs/yolov8m_run_c --net $FE_WORK/has/insts_pe3 --layers --fsm --no-detect
#   --no-snapshot   do not print the DRAM snapshot listing
#   --no-detect     skip detection decode (fe_sysc_detect.py) and its tables
#   --layers        also print the per-layer table (83 rows)
#   --fsm           also print the FSM state distribution
#   --run DIR       run directory   (required)
#   --out DIR       output directory (default tools/metrics/out/fullcore_0310)
#   --prog FILE     tile plan        (default fe_work/step6/program.json)
#   --freq GHZ      clock used for latency/FPS/TOPS (default 0.8)
#   --net DIR       exported net (manifest + golden) (default fe_work/step6b/net)
#   --image FILE    source image (default: looked up by name under fe_work/datasets)
#   --labels FILE   YOLO ground-truth labels (default: images/ -> labels/, .jpg -> .txt)
#   --draw          also draw the boxes on the image -> OUT/detections.png
#   -h, --help      show this help

SHOW_SNAP=1; DO_DETECT=1; SHOW_LAYERS=0; SHOW_FSM=0; FREQ=0.8; DRAW=0
RUN=""
OUT=tools/metrics/out/fullcore_0310
PROG=fe_work/step6/program.json
NET=fe_work/step6b/net; IMAGE=""; LABELS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --no-snapshot) SHOW_SNAP=0 ;;
    --no-detect)   DO_DETECT=0 ;;
    --layers)      SHOW_LAYERS=1 ;;
    --fsm)         SHOW_FSM=1 ;;
    --run)  RUN=$2;  shift ;;
    --out)  OUT=$2;  shift ;;
    --prog) PROG=$2; shift ;;
    --freq) FREQ=$2; shift ;;
    --net)    NET=$2;    shift ;;
    --image)  IMAGE=$2;  shift ;;
    --labels) LABELS=$2; shift ;;
    --draw)   DRAW=1 ;;
    -h|--help) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown option: $1 (see --help)"; exit 2 ;;
  esac
  shift
done
if [ -z "$RUN" ]; then sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; echo "error: --run DIR is required"; exit 2; fi

cd "$(dirname "$0")/../.." || exit 1   # project root (tools/metrics/ -> ../..)
mkdir -p "$OUT/rollup" "$OUT/agg"
HAS=0; grep -q '^\[DFC\] instr\|^\[tb_has_npu_top\]' "$RUN/run.log" 2>/dev/null && HAS=1
# fe_sysc_detect.py needs FE_WORK (config scales, datasets): default = the work dir holding the net dir (<FE_WORK>/<a>/<net>)
[ -n "$FE_WORK" ] || export FE_WORK=$(cd "$NET/../.." 2>/dev/null && pwd)

# fmt_tables [FILE]: redraw every markdown table as an aligned box table; other lines pass through
# (headings become section titles; ** and ` markup is stripped for the terminal).
fmt_tables() {
  python3 -c '
import re, sys, unicodedata
def w(s): return sum(2 if unicodedata.east_asian_width(c) in "WF" else 1 for c in s)
def clean(s): return s.replace("**", "").replace("`", "").strip()
def is_num(s): return bool(re.fullmatch(r"[-+]?\d[\d,.\s/]*\s*(%|B|x|cycles|ms|FPS)?", s))
def draw(rows, align):
    ncol = max(len(r) for r in rows)
    rows = [r + [""] * (ncol - len(r)) for r in rows]
    align = (align + ["l"] * ncol)[:ncol]
    for c in range(ncol):
        if align[c] == "l" and all(is_num(r[c]) for r in rows[1:] if r[c]):
            align[c] = "r"
    wid = [max(w(r[c]) for r in rows) for c in range(ncol)]
    def line(l, m, r): return l + m.join("─" * (x + 2) for x in wid) + r
    def cell(s, c):
        pad = " " * (wid[c] - w(s))
        right = align[c] == "r" and (is_num(s) or s == rows[0][c])
        return " " + (pad + s if right else s + pad) + " "
    out = [line("┌", "┬", "┐"), "│" + "│".join(cell(rows[0][c], c) for c in range(ncol)) + "│", line("├", "┼", "┤")]
    out += ["│" + "│".join(cell(r[c], c) for c in range(ncol)) + "│" for r in rows[1:]]
    out.append(line("└", "┴", "┘"))
    print("\n".join(out))
lines = open(sys.argv[1]).read().splitlines() if len(sys.argv) > 1 else sys.stdin.read().splitlines()
buf = []
def flush():
    global buf
    if not buf: return
    rows, align = [], []
    for l in buf:
        cells = [clean(c) for c in l.strip().strip("|").split("|")]
        if all(re.fullmatch(r":?-{3,}:?", c) for c in cells if c):
            align = ["r" if c.endswith(":") else "l" for c in cells]
        else:
            rows.append(cells)
    draw(rows, align); buf = []
for l in lines:
    if l.lstrip().startswith("|"): buf.append(l); continue
    flush()
    m = re.match(r"^(#+)\s*(.*)", l)
    print(("\n== " + clean(m.group(2)) + " ==") if m else clean(l))
flush()
' "$@"
}

section() { printf '\n==== %s ====\n' "$1"; }

# ---------------------------------------------------------------- plan
section "Tile plan"
if [ "$HAS" = 1 ]; then
  { echo "| Item | Value |"; echo "|---|---|"
    echo "| Plan | instruction stream of the run (tb_has_npu_top); tiles from run.log [STEP] lines |"
    [ -f "$RUN/cmd.txt" ] && echo "| Command | $(head -1 "$RUN/cmd.txt" | tr '|' '/') |"
    grep -m1 'mmio.txt' "$RUN/sha256.txt" 2>/dev/null | awk '{print "| mmio.txt SHA-256 | " substr($1,1,16) "… |"}'
    echo "| Run dir | $RUN |"; } | fmt_tables
else
{ echo "| Item | Value |"; echo "|---|---|"
  echo "| Plan file | $PROG |"
  echo "| SHA-256 | $(sha256sum "$PROG" | cut -c1-16)… |"
  echo "| Run dir | $RUN |"; } | fmt_tables
fi

# ---------------------------------------------------------------- functional verdict
section "Functional result"
RES=$(grep "RESULT:" "$RUN/run.log" | tail -1)
if [ -z "$RES" ]; then
  echo "No RESULT line in $RUN/run.log (run not finished?)"
else
  echo "$RES" | python3 -c '
import re, sys
s = sys.stdin.read()
v = re.search(r"RESULT:\s*(\w+)", s).group(1)
kv = re.findall(r"([a-z ]+?)\s+(\d+)(?=[,)])", s.split("(", 1)[1])
print("| Item | Value |\n|---|---:|\n| Verdict | " + v + " |")
for k, n in kv: print("| " + k.strip().capitalize() + " | " + format(int(n), ",") + " |")
' | fmt_tables
  grep "via core" "$RUN/run.log" | tail -1 | python3 -c '
import re, sys
s = sys.stdin.read()
g = lambda p: (re.search(p, s) or [None, "n/a"])[1]
rows = [("Tiles (via core / total)", g(r"via core (\d+)") + " / " + g(r"tiles (\d+)")),
        ("OBP vectors", g(r"OBP vectors (\d+)")),
        ("Simulated cycles", g(r"sim cycles (\d+)")),
        ("DMA wait cycles", g(r"DMA wait (\d+)")),
        ("OBP cycles", g(r"OBP (\d+),")),
        ("OBP config cycles (approx.)", g(r"OBP config approx (\d+)"))]
f = lambda x: " / ".join(format(int(p), ",") if p.isdigit() else p for p in x.split(" / "))
print("| Counter | Value |\n|---|---:|")
for k, x in rows: print("| " + k + " | " + f(x) + " |")
' | fmt_tables
fi

# ---------------------------------------------------------------- snapshot
SNAP="$RUN/snap/dram_snapshot.bin"
if [ "$SHOW_SNAP" = 1 ]; then
  section "DRAM snapshot"
  { echo "| File | Size | Modified |"; echo "|---|---:|---|"
    for f in "$RUN"/snap/*; do
      [ -e "$f" ] || continue
      echo "| $(basename "$f") | $(numfmt --to=iec --suffix=B "$(stat -c %s "$f")") | $(date -r "$f" '+%Y-%m-%d %H:%M') |"
    done; } | fmt_tables
fi

# ViT program dir (vit_full: summary.json, no manifest.json): no box decode; the golden summary is printed instead
if [ "$DO_DETECT" = 1 ] && [ ! -f "$NET/manifest.json" ]; then
  DO_DETECT=0
  if [ -f "$NET/summary.json" ]; then
    section "Program summary (golden, $NET/summary.json)"
    python3 -c '
import json, sys
s = json.load(open(sys.argv[1]))
print("| Item | Value |")
print("|---|---:|")
for k, lab in (("image", "Image"), ("blocks", "Encoder blocks"), ("instructions", "Instructions"), ("top1_int", "Top-1 integer golden (= NPU when 0 mismatches)"),
               ("top1_float", "Top-1 float model"), ("cos_logits", "Cosine logits int vs float"), ("dram_bytes", "DRAM bytes")):
    if k in s:
        v = s[k]
        print("| %s | %s |" % (lab, ("%.4f" % v) if isinstance(v, float) else (format(v, ",") if isinstance(v, int) else v)))
' "$NET/summary.json" | fmt_tables
  else
    echo "(no manifest.json in $NET: detection decode skipped)"
  fi
fi

# ---------------------------------------------------------------- detections
if [ "$DO_DETECT" = 1 ]; then
  section "Detections (decoded from the final DRAM snapshot)"
  if [ -f "$SNAP" ]; then
    # source image + ground-truth labels: explicit flags, else look the manifest's image up by name
    IMGNAME=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["image"])' "$NET/manifest.json")
    [ -n "$IMAGE" ] || IMAGE=$(find fe_work/datasets -path "*/images/*" -name "$IMGNAME" 2>/dev/null | sort | head -1)
    if [ -z "$LABELS" ] && [ -n "$IMAGE" ]; then
      LABELS=$(echo "$IMAGE" | sed 's|/images/|/labels/|; s|\.jpg$|.txt|')
    fi
    DARGS=(--dram "$SNAP" --net-dir "$NET" --compare-golden --json "$OUT/detections.json")
    [ -n "$IMAGE" ] && DARGS+=(--image "$IMAGE")
    [ "$DRAW" = 1 ] && DARGS+=(--draw "$OUT/detections.png")
    python3 tools/fe/fe_sysc_detect.py "${DARGS[@]}" \
        > "$OUT/detect.log" 2>&1 || echo "fe_sysc_detect.py failed, see $OUT/detect.log"
    [ -f "$OUT/detections.json" ] && python3 -c '
import json, sys
sys.path.insert(0, "tools/fe")
from fe_sysc_detect import COCO80
d = json.load(open(sys.argv[1]))
print("Image: " + d["image"] + "   config: " + d["config"] + "   (boxes in original-image pixels)")
print("| # | Class | Conf | x1 | y1 | x2 | y2 |\n|---:|---|---:|---:|---:|---:|---:|")
for i, (x1, y1, x2, y2, c, k) in enumerate(d["detections_original"]):
    print("| %d | %s | %.3f | %.1f | %.1f | %.1f | %.1f |" % (i, COCO80[int(k)], c, x1, y1, x2, y2))
g = d.get("vs_golden", {})
if g:
    print("\nComparison with golden:")
    print("| Check | Value |\n|---|---:|")
    rows = [("Boxes (reference / got / matched)", "%d / %d / %d" % (g["n_ref"], g["n_got"], g["tp"])),
            ("Precision", "%.3f" % g["precision"]), ("Recall", "%.3f" % g["recall"]),
            ("Mean IoU", "%.4f" % g["iou_mean"]), ("Max confidence delta", "%.4f" % g["dconf_max"]),
            ("Output tensors identical to golden", "yes" if g.get("tensors_identical_to_golden") else "NO"),
            ("Differing bytes inside tensors", str(g.get("bytes_diff_in_tensors", "n/a")))]
    for k, v in rows: print("| " + k + " | " + v + " |")
' "$OUT/detections.json" | fmt_tables
    # ground truth (human labels): greedy match, same class, IoU >= 0.5, highest-confidence detection first
    if [ -f "$OUT/detections.json" ] && [ -n "$LABELS" ] && [ -f "$LABELS" ]; then
      section "Detections vs ground-truth labels (same class, IoU >= 0.5)"
      python3 -c '
import json, sys
sys.path.insert(0, "tools/fe")
from fe_sysc_detect import COCO80
d = json.load(open(sys.argv[1]))
if "original_size" not in d: sys.exit("no original_size in detections.json (image not found)")
W, H = d["original_size"]
gt = []
for l in open(sys.argv[2]):
    p = l.split()
    if len(p) != 5: continue
    k, cx, cy, w, h = int(p[0]), *map(float, p[1:])
    gt.append((k, [(cx - w / 2) * W, (cy - h / 2) * H, (cx + w / 2) * W, (cy + h / 2) * H]))
def iou(a, b):
    ix = max(0, min(a[2], b[2]) - max(a[0], b[0])); iy = max(0, min(a[3], b[3]) - max(a[1], b[1]))
    u = (a[2] - a[0]) * (a[3] - a[1]) + (b[2] - b[0]) * (b[3] - b[1]) - ix * iy
    return ix * iy / u if u > 0 else 0.0
dets = sorted(enumerate(d["detections_original"]), key=lambda t: -t[1][4])
gt_hit, det_hit = {}, {}
for di, det in dets:
    best, bi = 0.5, None
    for gi, (k, box) in enumerate(gt):
        if gi in gt_hit or k != int(det[5]): continue
        v = iou(det[:4], box)
        if v >= best: best, bi = v, gi
    if bi is not None: gt_hit[bi] = (di, best); det_hit[di] = bi
print("Labels: " + sys.argv[2])
print("| GT # | Class | Size (% of image) | Detected by | Conf | IoU |\n|---:|---|---:|---|---:|---:|")
for gi, (k, b) in enumerate(gt):
    area = 100.0 * (b[2] - b[0]) * (b[3] - b[1]) / (W * H)
    if gi in gt_hit:
        di, v = gt_hit[gi]
        print("| %d | %s | %.1f%% | det #%d | %.3f | %.3f |" % (gi, COCO80[k], area, di, d["detections_original"][di][4], v))
    else:
        print("| %d | %s | %.1f%% | MISSED | - | - |" % (gi, COCO80[k], area))
fp = [di for di, _ in dets if di not in det_hit]
tp = len(gt_hit)
print("\n| Summary | Value |\n|---|---:|")
for k, v in [("Ground-truth objects", len(gt)), ("Detections", len(dets)), ("True positives", tp),
             ("Missed (false negatives)", len(gt) - tp), ("Unmatched detections (false positives)", len(fp)),
             ("Precision", "%.3f" % (tp / len(dets)) if dets else "n/a"), ("Recall", "%.3f" % (tp / len(gt)) if gt else "n/a")]:
    print("| %s | %s |" % (k, v))
if fp:
    print("\nUnmatched detections: " + ", ".join("#%d %s %.3f" % (di, COCO80[int(d["detections_original"][di][5])],
          d["detections_original"][di][4]) for di in fp))
print("Note: single image, int8 model quality vs human labels -- not a core-correctness check (that is the golden table).")
' "$OUT/detections.json" "$LABELS" | fmt_tables
    fi
  else
    echo "No DRAM snapshot at $SNAP (skipped)"
  fi
fi

# ---------------------------------------------------------------- roll-up
section "Network roll-up (measured, no extrapolation)"
if [ "$HAS" = 1 ]; then RARGS=(--has); else RARGS=(--program "$PROG"); fi
python3 tools/metrics/fullcore_rollup.py "${RARGS[@]}" --csv "$RUN/metrics_tiles.csv" --log "$RUN/run.log" \
    --out-dir "$OUT/rollup" > "$OUT/rollup/rollup.log" 2>&1 && tail -1 "$OUT/rollup/rollup.log" \
    || { echo "fullcore_rollup.py failed:"; tail -5 "$OUT/rollup/rollup.log"; }
if [ -f "$OUT/rollup/report.md" ]; then
  sed -n '/^| Metric/,/^$/p' "$OUT/rollup/report.md" | fmt_tables
  if [ "$HAS" = 1 ]; then
    echo; echo "Per operation:"
    sed -n '/^## Per operation/,/^## Per layer/p' "$OUT/rollup/report.md" | grep '^|' | fmt_tables
  fi
fi

# ---------------------------------------------------------------- performance summary
# Recomputed straight from the measured per-tile CSV + the run.log total (independent of fullcore_rollup.py;
# the Core busy figure must equal the roll-up table above). Array size is fixed at 32x32 = 1024 PEs.
section "Performance summary @ $FREQ GHz"
# tb_has_npu_top: MACs = real MACs of the roll-up (the CSV counter also counts PAD_TAIL padding); else the CSV counter
# (ViT: the FUSED_ATTN products join the whole-network throughput rows; the PE breakdown stays on the GEMM tiles of the CSV)
REALMAC=""; ATTNMAC=0
if [ "$HAS" = 1 ]; then
  REALMAC=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["network"]["real_macs"])' \
      "$OUT/rollup/network.json" 2>/dev/null)
  ATTNMAC=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["network"].get("real_macs_attention", 0))' \
      "$OUT/rollup/network.json" 2>/dev/null)
fi
python3 -c '
import csv, json, re, sys
csvf, log, f_ghz = sys.argv[2], sys.argv[3], float(sys.argv[4])
real = int(sys.argv[5]) if len(sys.argv) > 5 and sys.argv[5] else None
attn = int(sys.argv[6]) if len(sys.argv) > 6 and sys.argv[6] else 0
PE, F = 1024, float(sys.argv[4]) * 1e9
rows = csv.reader(l for l in open(csvf) if not l.startswith("#"))
h = next(rows); ix = {k: i for i, k in enumerate(h)}
busy = exe = mac = used = cap = 0
for r in rows:
    busy += int(r[ix["busy"]]); exe += int(r[ix["exec"]]); mac += int(r[ix["macs_theory"]])
    # Spatial usage: array capacity during pure compute = n_ctx * K * 1024 with K = cin*kh*kw
    # (one context streams K operands); weighting by context COUNT alone over-weighted the short stem contexts (K=27).
    K = int(r[ix["cin"]]) * int(r[ix["kh"]]) * int(r[ix["kw"]])
    used += int(r[ix["macs_theory"]]); cap += int(r[ix["n_ctx"]]) * K * PE
if real is not None:  # padded MACs are waste: they count against spatial usage, not as work done
    mac = used = real
m = re.findall(r"sim cycles (\d+) \(DMA wait", open(log).read())
tb = int(m[-1]) if m else None
mac_all = mac + attn  # whole network (GEMM + attention products) for the throughput rows
ideal = mac_all / PE
ms = lambda cyc: "%.1f ms" % (1e3 * cyc / F)
fps = lambda cyc: "%.2f" % (F / cyc)
pct = lambda x: "%.2f%%" % (100 * x)
spatial, exec_share, pe_exec = used / cap, exe / busy, mac / (exe * PE)
temporal = pe_exec / spatial
print("Latency and throughput")
print("| Metric | Value | Basis |\n|---|---:|---|")
rows_a = []
if tb:
    rows_a += [("Latency per image (whole testbench)", ms(tb), "derived: whole-testbench cycles / f"),
               ("Throughput (whole testbench)", fps(tb) + " FPS", "derived: f / whole-testbench cycles")]
rows_a += [("Latency per image (core busy only)", ms(busy), "derived: core busy / f; excludes DMA, OBP, host"),
           ("Throughput (core busy only)", fps(busy) + " FPS", "derived: upper bound if off-core work fully overlapped"),
           ("Ideal cycles (MACs / 1024)", format(round(ideal), ","), "derived: 100% of the 32x32 array every cycle"),
           ("Throughput ceiling", fps(ideal) + " FPS", "derived: f / ideal cycles; max for this array size")]
if tb:
    rows_a += [("Efficiency vs ideal", pct(ideal / tb), "derived: ideal cycles / whole-testbench cycles"),
               ("Effective TOPS", "%.3f" % (2 * mac_all * F / tb / 1e12), "derived: 2 x MACs x FPS (whole testbench)")]
rows_a += [("Peak TOPS", "%.3f" % (2 * PE * F / 1e12), "derived: 2 x 1024 x f")]
for k, v, b in rows_a: print("| %s | %s | %s |" % (k, v, b))
print("\nPE utilization breakdown (PE utilization = spatial usage x temporal efficiency x exec share)")
print("| Metric | Value | Basis |\n|---|---:|---|")
for k, v, b in [
    ("Spatial usage", pct(spatial), "derived: MACs / (n_ctx x K x 1024), K = cin*kh*kw (tile shape loss)"),
    ("Temporal efficiency in exec", pct(temporal), "derived: n_ctx x K / exec (fill/drain per context)"),
    ("Exec share of busy", pct(exec_share), "measured: exec / busy (rest = stalls, context switches)"),
    ("PE utilization in exec", pct(pe_exec), "derived: MACs / (exec x 1024)"),
    ("PE utilization", pct(mac / (busy * PE)), "derived: MACs / (busy x 1024) = product of the first three"),
    ("Core busy (cross-check)", format(busy, ","), "measured: must equal the roll-up table")]:
    print("| %s | %s | %s |" % (k, v, b))
' "$PROG" "$RUN/metrics_tiles.csv" "$RUN/run.log" "$FREQ" "$REALMAC" "$ATTNMAC" | fmt_tables
[ -n "$REALMAC" ] && echo "MACs = real MACs (roll-up); PAD_TAIL padding is executed but counted as lost spatial usage."
[ -n "$ATTNMAC" ] && [ "$ATTNMAC" != 0 ] && echo "Throughput rows include the FUSED_ATTN products ($ATTNMAC MACs); the PE breakdown covers the GEMM_FUSED tiles only (PE incl. attention: roll-up table)."

if [ -f "$OUT/rollup/report.md" ] && [ "$HAS" = 1 ]; then
  if [ "$SHOW_LAYERS" = 1 ]; then
    echo; echo "Per layer:"
    sed -n '/^## Per layer/,/^## FSM/p' "$OUT/rollup/report.md" | grep '^|' | fmt_tables
    sed -n '/^## Per layer/,/^## FSM/p' "$OUT/rollup/report.md" | grep -E '^(X x Y|Spatial usage|Instr cycles):'
  fi
  if [ "$SHOW_FSM" = 1 ]; then
    section "FSM state distribution"
    sed -n '/^## FSM/,/^## Checks/p' "$OUT/rollup/report.md" | grep '^|' | fmt_tables
  fi
  section "Checks"
  sed -n '/^## Checks/,$p' "$OUT/rollup/report.md" | grep '^- '
elif [ -f "$OUT/rollup/report.md" ]; then
  if [ "$SHOW_LAYERS" = 1 ]; then
    echo; echo "Per layer:"
    # Adds 3 columns from program.json (x_used/y_used, tile w/h/cout) and the measured CSV (cin, n_ctx):
    #   X x Y used     : distinct array footprints of the layer's tiles, most frequent first
    #   Tile layout    : W/H/Cout/Cin of the nominal tile (w_t/h_t/cout_t from the plan, Cin from the CSV)
    #   Spatial usage  : sum(MACs) / sum(n_ctx * K * 1024), K = cin*kh*kw -- share of the 32x32 array used while
    #                    contexts stream (tile shape loss only; independent of stalls).
    sed -n '/^## Per layer/,$p' "$OUT/rollup/report.md" | grep '^|' | python3 -c '
import csv, json, sys, collections
prog = json.load(open(sys.argv[1]))
rows = csv.reader(l for l in open(sys.argv[2]) if not l.startswith("#"))
hdr = next(rows); ix = {k: i for i, k in enumerate(hdr)}
meas = {(int(r[ix["step"]]), int(r[ix["tile"]])): r for r in rows}
info = {}
for si, s in enumerate(prog["steps"]):
    if not s.get("tiles"): continue
    fp, used, total, cin = collections.Counter(), 0, 0, None
    for ti, t in enumerate(s["tiles"]):
        fp[(t["x_used"], t["y_used"])] += 1
        m = meas.get((si, ti))
        if not m: continue
        if cin is None: cin = m[ix["cin"]]
        K = int(m[ix["cin"]]) * int(m[ix["kh"]]) * int(m[ix["kw"]])
        used += int(m[ix["macs_theory"]]); total += int(m[ix["n_ctx"]]) * K * 1024
    info[s["job"]] = ((", ".join("%dx%d" % k for k, _ in fp.most_common())),
                      "%d/%d/%d/%s" % (s["w_t"], s["h_t"], s["cout_t"], cin or "?"),
                      "%.2f%%" % (100.0 * used / total) if total else "n/a")
for n, l in enumerate(sys.stdin):
    c = [x.strip() for x in l.strip().strip("|").split("|")]
    if n == 0:   c = c[:2] + ["X x Y used", "Tile W/H/Cout/Cin"] + c[2:] + ["Spatial usage"]
    elif n == 1: c = c[:2] + ["---", "---:"] + c[2:] + ["---:"]
    else:
        a, b, u = info.get(c[0], ("?", "?", "?"))
        c = c[:2] + [a, b] + c[2:] + [u]
    print("| " + " | ".join(c) + " |")
' "$PROG" "$RUN/metrics_tiles.csv" | fmt_tables
    echo "X x Y used: array columns x rows occupied per tile (distinct shapes, most frequent first)."
    echo "Spatial usage: MACs / (n_ctx x K x 1024), K = cin*kh*kw; PE util = MACs / (busy x 1024), i.e. also counts stalls."
  fi
fi

# ---------------------------------------------------------------- agg_bigrun (kept for its files)
if [ "$HAS" = 0 ]; then  # needs a program.json; the tb_has_npu_top roll-up already holds the FSM table
python3 tools/metrics/agg_bigrun.py --csv "$RUN/metrics_tiles.csv" --program "$PROG" --out-dir "$OUT/agg" \
    > "$OUT/agg/bigrun_report.md" 2>&1 || echo "agg_bigrun.py exited with code $?"
if [ "$SHOW_FSM" = 1 ]; then
  section "FSM state distribution"
  sed -n '/^## 4\./,/^## 5\./p' "$OUT/agg/bigrun_report.md" | grep '^|' | fmt_tables
fi
fi

section "Output files"
{ echo "| File | Content |"; echo "|---|---|"
  echo "| $OUT/rollup/report.md | network + per-layer report |"
  echo "| $OUT/rollup/layers.csv | per-layer counters (incl. 24 FSM states) |"
  echo "| $OUT/rollup/steps.csv | per-step functional verdicts |"
  [ "$HAS" = 0 ] && echo "| $OUT/agg/bigrun_report.md | coverage, FSM distribution, self-checks |"
  [ "$DO_DETECT" = 1 ] && echo "| $OUT/detections.json | detection boxes + golden comparison |"; } | fmt_tables
