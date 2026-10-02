#!/usr/bin/env python3
"""Generate sauria_targets.h (C manifest) from sauria_targets.csv (single source).

Usage:  python3 tools/gen_targets.py            # regenerate sauria_targets.h
        python3 tools/gen_targets.py --check     # verify .h is up to date (CI)
        python3 tools/gen_targets.py --sh         # emit run_shape.sh case blocks to stdout

Run from the unified/ root. Keeps the C header, and optionally the shell IDX/dtype
tables, consistent with the one CSV so IDX widths are never duplicated by hand.
"""
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV = os.path.join(ROOT, "sauria_targets.csv")
HDR = os.path.join(ROOT, "sauria_targets.h")

COLS = ["name", "X", "Y", "ia_w", "ib_w", "oc_w", "op_type",
        "idx_a", "idx_w", "idx_o", "in_bytes", "out_bytes", "dtype", "build_flags"]


def load():
    rows = []
    with open(CSV) as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#") or line.startswith("name|"):
                continue
            parts = line.split("|")
            if len(parts) != len(COLS):
                raise SystemExit(f"[gen_targets] bad row ({len(parts)} fields): {line}")
            rows.append(dict(zip(COLS, parts)))
    if not rows:
        raise SystemExit("[gen_targets] no rows parsed from CSV")
    return rows


def gen_header(rows):
    L = []
    L.append("// AUTO-GENERATED from sauria_targets.csv by tools/gen_targets.py — DO NOT EDIT.")
    L.append("// SAURIA NPU HW-version manifest: single source of truth for geometry, element")
    L.append("// widths, packed-config index widths, and dtype build flags. Consumed by the C")
    L.append("// encoder (libsauria_cfg) and any tool needing per-target constants.")
    L.append("#ifndef SAURIA_TARGETS_H")
    L.append("#define SAURIA_TARGETS_H")
    L.append("")
    L.append("#include <cstdint>")
    L.append("#include <cstring>")
    L.append("")
    L.append("namespace sauria {")
    L.append("")
    L.append("enum SauriaDtype { SAURIA_DT_INT8 = 0, SAURIA_DT_FP16 = 1, SAURIA_DT_INT16 = 2 };")
    L.append("")
    L.append("struct SauriaTarget {")
    L.append("    const char *name;")
    L.append("    int X, Y;                 // systolic array geometry")
    L.append("    int ia_w, ib_w, oc_w;     // element bit widths (act, wei, psum/out)")
    L.append("    int op_type;              // 0=int, 1=FP; also the PE arithmetic_type")
    L.append("    int idx_a, idx_w, idx_o;  // packed-config index widths (ACT/WEI/OUT)")
    L.append("    int in_bytes, out_bytes;  // DRAM element sizes (ia_w/8, oc_w/8)")
    L.append("    SauriaDtype dtype;")
    L.append("    const char *build_flags;  // dtype -D flags (IDX/EVAL derived from fields)")
    L.append("};")
    L.append("")
    L.append("static const SauriaTarget SAURIA_TARGETS[] = {")
    dt_enum = {"INT8": "SAURIA_DT_INT8", "FP16": "SAURIA_DT_FP16", "INT16": "SAURIA_DT_INT16"}
    for r in rows:
        L.append("    {{\"{name}\", {X}, {Y}, {ia_w}, {ib_w}, {oc_w}, {op_type}, "
                 "{idx_a}, {idx_w}, {idx_o}, {in_bytes}, {out_bytes}, {dt}, \"{bf}\"}},".format(
                     dt=dt_enum[r["dtype"]], bf=r["build_flags"], **r))
    L.append("};")
    L.append("")
    L.append("static const int SAURIA_NUM_TARGETS = "
             "(int)(sizeof(SAURIA_TARGETS) / sizeof(SAURIA_TARGETS[0]));")
    L.append("")
    L.append("// Look up a target by name; returns nullptr if unknown.")
    L.append("inline const SauriaTarget *sauria_find_target(const char *name) {")
    L.append("    for (int i = 0; i < SAURIA_NUM_TARGETS; i++)")
    L.append("        if (std::strcmp(SAURIA_TARGETS[i].name, name) == 0)")
    L.append("            return &SAURIA_TARGETS[i];")
    L.append("    return nullptr;")
    L.append("}")
    L.append("")
    L.append("} // namespace sauria")
    L.append("")
    L.append("#endif // SAURIA_TARGETS_H")
    return "\n".join(L) + "\n"


def gen_sh(rows):
    """Emit the run_shape.sh IDX + dtype case blocks (for wiring/regen)."""
    idx, dt = [], []
    for r in rows:
        idx.append('  {name}) W="-DSAURIA_ACT_IDX_W={a} -DSAURIA_WEI_IDX_W={w} '
                   '-DSAURIA_OUT_IDX_W={o}" ;;'.format(name=r["name"], a=r["idx_a"],
                                                       w=r["idx_w"], o=r["idx_o"]))
        if r["build_flags"]:
            dt.append('  {name}) DTYPE_FLAGS="{bf}" ;;'.format(name=r["name"], bf=r["build_flags"]))
    return "# IDX widths:\n" + "\n".join(idx) + "\n\n# dtype flags:\n" + "\n".join(dt) + "\n"


def main():
    rows = load()
    if "--sh" in sys.argv:
        sys.stdout.write(gen_sh(rows))
        return
    text = gen_header(rows)
    if "--check" in sys.argv:
        cur = open(HDR).read() if os.path.exists(HDR) else ""
        if cur != text:
            raise SystemExit("[gen_targets] sauria_targets.h is STALE — run tools/gen_targets.py")
        print("[gen_targets] sauria_targets.h up to date")
        return
    with open(HDR, "w", newline="\n") as f:
        f.write(text)
    print(f"[gen_targets] wrote {HDR} ({len(rows)} targets)")


if __name__ == "__main__":
    main()
