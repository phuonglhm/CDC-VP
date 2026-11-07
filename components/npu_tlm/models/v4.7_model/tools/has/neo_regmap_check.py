#!/usr/bin/env python3
"""neo_regmap_check.py -- compare the register map of the model with the RTL of neo_lite.

What is compared (all of it for the int8_32x32 configuration that the model builds):
  * the address parameters of neo_addr_pkg.sv, the offsets inside the SAURIA / GVU blocks (neo_interface.sv) and the bus
    addresses of the local memories (neo_dma_controller.sv)                       <-> namespace neo::addr of driver/libneo_cfg.h
  * the opcode enums (neo_opcode_t, gvu_opcode_t) and the GVU register numbers     <-> enums of driver/libneo_cfg.h
  * position and width of every field of the 82 configuration words              <-> neo::FIELDS (DFC words, GVU registers, controller
    window), the packed core layout of sauria_cfg_layout.h + neo::CORE_EXTRA (core words)
  * the shifts and masks the encoder / decoder use                                 <-> fixed patterns in has/has_host_neo.h,
    driver/libsauria_cfg.h and driver/libneo_cfg.h ("uses")

Two sources for the RTL side:
  --rtl <neo_lite repository>   read the files with `git show <commit>:<path>` (read-only: no checkout, no file is created there)
  --table <file>                the frozen table made by --emit-table (no RTL tree needed)
Each mismatch is printed with the RTL file:line it comes from. Exit code 0 = no mismatch.

  python3 tools/has/neo_regmap_check.py --rtl <neo_lite> [--emit-table tools/has/neo_regmap_rev1.txt]
  python3 tools/has/neo_regmap_check.py --table tools/has/neo_regmap_rev1.txt
"""
import argparse
import math
import os
import re
import subprocess
import sys

COMMIT = "1fb7978"   # git revision of RTL revision 1 that --rtl reads (data, not a name)
PARAM_CONFIG = "int8_32x32"
RTL = {
    "addr_pkg": "rtl/src/neo_addr_pkg.sv",
    "iface": "rtl/src/df_controller/neo_interface.sv",
    "dfc_top": "rtl/src/df_controller/df_controller_top.sv",
    "dfc_pkg": "rtl/src/df_controller/df_controller_pkg.sv",
    "dma": "rtl/src/df_controller/neo_dma_controller.sv",
    "cfg": "rtl/src/neo_core/config/config_regs.sv",
    "spkg": "rtl/src/sauria_pkg.sv",
    "gcfg": "rtl/src/neo_core/gvu/gvu_config_regs.sv",
    "gpkg": "rtl/src/neo_core/gvu/gvu_pkg.sv",
    "svh": "rtl/param_config/%s.svh" % PARAM_CONFIG,
}
SPACES = ("file", "gvureg", "ctlwin")


class Report:
    def __init__(self):
        self.checked = 0
        self.bad = 0
        self.notes = []

    def ok(self, n=1):
        self.checked += n

    def mismatch(self, area, text, src=""):
        self.checked += 1
        self.bad += 1
        print("MISMATCH %-8s %s%s" % (area, text, ("   [RTL %s]" % src) if src else ""))


# ---------------------------------------------------------------- small helpers
def ev(expr, env):
    """Integer expression with + - * | << >> and names from env."""
    s = expr.strip()
    s = re.sub(r"(\d+)'[hH]([0-9A-Fa-f_]+)", lambda m: str(int(m.group(2).replace("_", ""), 16)), s)
    s = re.sub(r"\b0[xX]([0-9A-Fa-f_]+)[uU]?\b", lambda m: str(int(m.group(1).replace("_", ""), 16)), s)
    s = re.sub(r"\b(\d+)[uU]\b", r"\1", s)
    s = re.sub(r"[A-Za-z_]\w*", lambda m: str(env[m.group(0)]), s)
    if not re.fullmatch(r"[0-9\s+\-*|()<>&]+", s):
        raise ValueError("cannot evaluate %r" % expr)
    return eval(s, {"__builtins__": {}}, {})


def hx(v):
    return "0x%X" % v


def strip_comment(line):
    return line.split("//")[0]


# ---------------------------------------------------------------- RTL side
class Rtl:
    """Records extracted from the RTL: dicts keyed by identity, values (value, src)."""

    def __init__(self):
        self.const = {}      # name -> (value, src)
        self.enum = {}       # (set, name) -> (value, src)
        self.field = {}      # (space, word, hi, lo, name) -> src
        self.core = {}       # (bit, width, name) -> src
        self.errors = []     # extractor out of date


def git_reader(repo, commit):
    cache = {}

    def read(path):
        if path not in cache:
            r = subprocess.run(["git", "-C", repo, "show", "%s:%s" % (commit, path)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if r.returncode != 0:
                raise SystemExit("git show %s:%s failed: %s" % (commit, path, r.stderr.decode("utf-8", "replace").strip()))
            cache[path] = r.stdout.decode("utf-8", "replace").replace("\r\n", "\n").split("\n")
        return cache[path]
    return read


def parse_enum(lines, name):
    """(entries, first line) of `typedef enum ... { ... } name;` -- entries (ident, value or None, line)."""
    end = None
    for i, l in enumerate(lines):
        if re.search(r"\}\s*%s\s*;" % re.escape(name), l):
            end = i
            break
    if end is None:
        raise SystemExit("enum %s not found" % name)
    start = end
    while "typedef enum" not in lines[start]:
        start -= 1
    body = []
    for i in range(start, end + 1):
        t = strip_comment(lines[i])
        t = re.sub(r"typedef enum[^{]*\{", "", t)
        t = re.sub(r"\}\s*%s\s*;" % re.escape(name), "", t)
        body.append((i + 1, t))
    out = []
    for ln, t in body:
        for m in re.finditer(r"([A-Za-z_]\w*)\s*(?:=\s*(\d+'d\d+|\d+))?\s*(?:,|$)", t.strip()):
            val = m.group(2)
            if val is not None:
                val = int(val.split("'d")[-1])
            out.append((m.group(1), val, ln))
    return out


def extract(read):
    R = Rtl()

    def src(key, i):
        return "%s:%d" % (RTL[key], i)

    # --- neo_addr_pkg.sv: every `parameter NAME = 32'h...;`
    for i, l in enumerate(read(RTL["addr_pkg"]), 1):
        m = re.match(r"\s*parameter\s+(\w+)\s*=\s*32'h([0-9A-Fa-f_]+)\s*;", l)
        if m:
            R.const[m.group(1)] = (int(m.group(2).replace("_", ""), 16), src("addr_pkg", i))
    # --- neo_interface.sv: offsets inside the SAURIA and GVU blocks
    for i, l in enumerate(read(RTL["iface"]), 1):
        m = re.match(r"\s*localparam\s+(SAURIA_CONTROL_OFFSET|INTERRUPT_EN_OFFSET|INTERRUPT_STATUS_OFFSET|GVU_CONTROL_OFFSET|GVU_CFG_OFFSET)\s*=\s*13'h([0-9A-Fa-f]+)\s*;", l)
        if m:
            R.const[m.group(1)] = (int(m.group(2), 16), src("iface", i))
    # --- neo_dma_controller.sv: bus address of the local memories = DMA window + offset
    for i, l in enumerate(read(RTL["dma"]), 1):
        m = re.match(r"\s*localparam\s+(SRAMA|SRAMB|SRAMC|SBANK|VBANK0|VBANK1|VBANK2)_OFFSET\s*=\s*neo_addr_pkg::(\w+)\s*\+\s*neo_addr_pkg::(\w+)\s*;", l)
        if m:
            R.const["BUS_" + m.group(1)] = (R.const[m.group(2)][0] + R.const[m.group(3)][0], src("dma", i))
    # --- controller window (df_controller_top.sv)
    top = read(RTL["dfc_top"])
    word = None
    names = {0: "control", 2: "ien", 3: "istatus"}
    for i, l in enumerate(top, 1):
        if "reg_write" in l:
            word = None
        m = re.search(r"awaddr\[2 \+: N_REGS_BITS\]\s*==\s*(\d+)", l)
        if m:
            word = int(m.group(1))
        m = re.match(r"\s*if \(bb==(\d+)\)\s+(\w+)_d\s*=", l)
        if m and word is not None:
            R.field[("ctlwin", word, int(m.group(1)), int(m.group(1)), m.group(2))] = src("dfc_top", i)
        m = re.search(r"awaddr\[2 \+: N_REGS_BITS\\]\s*>=\s*5'h10", l)
        m = re.search(r"wr_idx\s*=\s*config_axilite\.awaddr\[2 \+: N_REGS_BITS\]\s*-\s*\(5'h([0-9A-Fa-f]+)>>2\)", l)
        if m:
            R.const["REG_BASE"] = (int(m.group(1), 16), src("dfc_top", i))
    for key in ("REG_BASE",):
        if key not in R.const:
            R.errors.append("df_controller_top.sv: register base of the window not found")
    # --- counts
    for i, l in enumerate(read(RTL["dfc_pkg"]), 1):
        m = re.match(r"\s*localparam\s+N_CTRL_REGS\s*=\s*(\d+)\s*;", l)
        if m:
            R.const["N_CTRL_REGS"] = (int(m.group(1)), src("dfc_pkg", i))
    gp = read(RTL["gpkg"])
    for i, l in enumerate(gp, 1):
        m = re.match(r"\s*parameter int unsigned GVU_NUM_REGS\s*=\s*(\d+)\s*;", l)
        if m:
            R.const["GVU_NUM_REGS"] = (int(m.group(1)), src("gpkg", i))
        if re.match(r"\s*parameter int unsigned GVU_CFG_REGS\s*=\s*GVU_NUM_REGS\s*-\s*1\s*;", l):
            R.const["GVU_CFG_REGS"] = (R.const["GVU_NUM_REGS"][0] - 1, src("gpkg", i))
    # --- widths of the configuration words (sauria_pkg.sv + the parameter set)
    env = {}
    for i, l in enumerate(read(RTL["svh"]), 1):
        m = re.match(r"`define\s+(\w+)\s+(\d+)", l)
        if m:
            env[m.group(1)] = int(m.group(2))
    spkg = read(RTL["spkg"])
    formulas = {
        "SRAMA_W": "IA_W*Y", "SRAMB_W": "IB_W*X", "RESULT_LANES": "(X>Y)?X:Y", "SRAMC_W": "OC_W*RESULT_LANES",
        "SRAMA_N": "int'(SRAMA_W/IA_W)", "SRAMB_N": "int'(SRAMB_W/IB_W)", "SRAMC_N": "int'(SRAMC_W/OC_W)",
        "ADRA_W": "$clog2(SRAMA_DEPTH)", "ADRB_W": "$clog2(SRAMB_DEPTH)", "ADRC_W": "$clog2(SRAMC_DEPTH)",
        "ACT_IDX_W": "ADRA_W+$clog2(SRAMA_N)+1", "WEI_IDX_W": "ADRB_W+$clog2(SRAMB_N)+1", "OUT_IDX_W": "ADRC_W+$clog2(SRAMC_N)+1",
        "TH_W": "2", "PARAMS_W": "8", "DILP_W": "64",
        "TOTAL_BITS_CON": "ACT_IDX_W+2*OUT_IDX_W+TH_W+2", "TOTAL_BITS_ACT": "Y+DILP_W+10*ACT_IDX_W+Y*PARAMS_W",
        "TOTAL_BITS_WEI": "X+6*WEI_IDX_W+1", "TOTAL_BITS_OUT": "2+PARAMS_W+9*OUT_IDX_W+3",
    }
    seen = {}
    for i, l in enumerate(spkg, 1):
        m = re.match(r"\s*(?:parameter|localparam)\s+(?:real\s+|int\s+)?(\w+)\s*=\s*([^;]+);", strip_comment(l))
        if m and m.group(1) in formulas:
            seen[m.group(1)] = (re.sub(r"\s+", "", m.group(2)), i)
    for name, want in formulas.items():
        if name not in seen:
            R.errors.append("sauria_pkg.sv: %s not found" % name)
        elif seen[name][0] != want and not (name in ("TH_W", "PARAMS_W", "DILP_W") and seen[name][0] == want):
            R.errors.append("sauria_pkg.sv:%d: %s is now %s (extractor knows %s)" % (seen[name][1], name, seen[name][0], want))
    env["IB_W"] = env["IA_W"] if "IB_W" not in env else env["IB_W"]
    env.update({"TH_W": 2, "PARAMS_W": 8, "DILP_W": 64})
    clog2 = lambda v: int(math.ceil(math.log2(v)))
    env["SRAMA_W"] = env["IA_W"] * env["Y"]
    env["SRAMB_W"] = env["IB_W"] * env["X"]
    env["RESULT_LANES"] = max(env["X"], env["Y"])
    env["SRAMC_W"] = env["OC_W"] * env["RESULT_LANES"]
    env["SRAMA_N"] = env["SRAMA_W"] // env["IA_W"]
    env["SRAMB_N"] = env["SRAMB_W"] // env["IB_W"]
    env["SRAMC_N"] = env["SRAMC_W"] // env["OC_W"]
    env["ACT_IDX_W"] = clog2(env["SRAMA_DEPTH"]) + clog2(env["SRAMA_N"]) + 1
    env["WEI_IDX_W"] = clog2(env["SRAMB_DEPTH"]) + clog2(env["SRAMB_N"]) + 1
    env["OUT_IDX_W"] = clog2(env["SRAMC_DEPTH"]) + clog2(env["SRAMC_N"]) + 1
    env["TOTAL_BITS_CON"] = env["ACT_IDX_W"] + 2 * env["OUT_IDX_W"] + env["TH_W"] + 2
    env["TOTAL_BITS_ACT"] = env["Y"] + env["DILP_W"] + 10 * env["ACT_IDX_W"] + env["Y"] * env["PARAMS_W"]
    env["TOTAL_BITS_WEI"] = env["X"] + 6 * env["WEI_IDX_W"] + 1
    env["TOTAL_BITS_OUT"] = 2 + env["PARAMS_W"] + 9 * env["OUT_IDX_W"] + 3
    regs = {k: int(math.ceil(env["TOTAL_BITS_" + k] / 32.0)) for k in ("CON", "ACT", "WEI", "OUT")}
    sp_line = seen.get("TOTAL_BITS_CON", ("", 0))[1]
    for k in ("CON", "ACT", "WEI", "OUT"):
        R.const["TOTAL_REGS_" + k] = (regs[k], src("spkg", seen.get("TOTAL_BITS_" + k, ("", 0))[1]))
    for k in ("ACT_IDX_W", "WEI_IDX_W", "OUT_IDX_W"):
        R.const[k] = (env[k], src("spkg", seen.get(k, ("", 0))[1]))
    R.const["N_DFC"] = R.const["N_CTRL_REGS"]
    R.const["N_CORE"] = (sum(regs.values()), R.const["TOTAL_REGS_OUT"][1])
    R.const["N_GVU"] = R.const["GVU_CFG_REGS"]
    nl = next((i for i, l in enumerate(top, 1) if re.match(r"\s*localparam N_REGS\s*=\s*df_ctrl_pkg::N_CTRL_REGS \+ sauria_pkg::TOTAL_REGS_CON \+ sauria_pkg::TOTAL_REGS_ACT \+ sauria_pkg::TOTAL_REGS_WEI \+ sauria_pkg::TOTAL_REGS_OUT \+ gvu_pkg::GVU_CFG_REGS;", l)), None)
    if nl is None:
        R.errors.append("df_controller_top.sv: the N_REGS sum changed (extractor assumes DFC + CON + ACT + WEI + OUT + GVU)")
    R.const["N_REGS"] = (R.const["N_DFC"][0] + R.const["N_CORE"][0] + R.const["N_GVU"][0], src("dfc_top", nl or 0))
    R.const["IDX_CORE"] = R.const["N_DFC"]
    R.const["IDX_GVU"] = (R.const["N_DFC"][0] + R.const["N_CORE"][0], R.const["N_CORE"][1])
    # the DFC adds the three pieces in this order (df_controller_top.sv N_REGS)
    base = {"CON": 0, "ACT": regs["CON"], "WEI": regs["CON"] + regs["ACT"], "OUT": regs["CON"] + regs["ACT"] + regs["WEI"]}

    # --- enums
    for set_name, lines, key in (("neo_opcode_t", read(RTL["dfc_pkg"]), "dfc_pkg"), ("gvu_opcode_t", gp, "gpkg"), ("gvu_cfg_reg_t", gp, "gpkg")):
        nxt = 0
        for ident, val, ln in parse_enum(lines, set_name):
            if val is None:
                val = nxt
            nxt = val + 1
            R.enum[(set_name, ident)] = (val, src(key, ln))

    # --- DFC words: READ_ARGS of neo_interface.sv
    iface = read(RTL["iface"])
    a = next(i for i, l in enumerate(iface) if "READ_ARGS: begin" in l)
    b = next(i for i in range(a, len(iface)) if "state <= SAURIA_SEND_ARGS" in iface[i])
    for i in range(a, b):
        t = strip_comment(iface[i])
        for m in re.finditer(r"control_regs\[(\d+)\]\[(\d+)(?::(\d+))?\]", t):
            w, hi = int(m.group(1)), int(m.group(2))
            lo = int(m.group(3)) if m.group(3) is not None else hi
            if "<=" in t:
                name = t.split("<=")[0].strip()
            elif t.strip().startswith("if"):
                name = "start_gate"
            else:
                continue
            R.field[("file", w, hi, lo, name)] = src("iface", i + 1)

    # --- GVU registers: gvu_config_regs.sv
    reg_no = {n: v[0] for (s, n), v in R.enum.items() if s == "gvu_cfg_reg_t"}
    gbase = R.const["IDX_GVU"][0]
    gl = read(RTL["gcfg"])

    def gword(reg):
        return gbase + reg_no[reg] - 1

    for i, l in enumerate(gl, 1):
        t = strip_comment(l)
        m = re.match(r"\s*assign\s+(\w+(?:\[\d+\])?)\s*=\s*(.*);", t)
        if m and "cfg_reg" in m.group(2):
            name, rhs = m.group(1), m.group(2)
            cat = re.match(r"\{\s*cfg_reg\[(\w+)\]\s*,\s*cfg_reg\[(\w+)\]\s*\}", rhs.strip())
            if cat:
                for reg, part in ((cat.group(2), "[31:0]"), (cat.group(1), "[63:32]")):
                    R.field[("file", gword(reg), 31, 0, name + part)] = src("gcfg", i)
                    R.field[("gvureg", reg_no[reg], 31, 0, name + part)] = src("gcfg", i)
                continue
            for mm in re.finditer(r"cfg_reg\[(\w+_REG)\](?:\[(\d+)(?::(\d+))?\])?", rhs):
                reg = mm.group(1)
                hi = int(mm.group(2)) if mm.group(2) is not None else 31
                lo = int(mm.group(3)) if mm.group(3) is not None else (hi if mm.group(2) is not None else 0)
                R.field[("file", gword(reg), hi, lo, name)] = src("gcfg", i)
                R.field[("gvureg", reg_no[reg], hi, lo, name)] = src("gcfg", i)
        m = re.match(r"\s*if \(i_wmask\[(\d+)\]\)\s+(lut_\w+_reset)_d\s*=\s*i_wdata\[\1\]", t)
        if m:
            R.field[("file", gword("LUT_REG"), int(m.group(1)), int(m.group(1)), m.group(2))] = src("gcfg", i)
            R.field[("gvureg", reg_no["LUT_REG"], int(m.group(1)), int(m.group(1)), m.group(2))] = src("gcfg", i)
        m = re.match(r"\s*if \(i_wmask\[0\]\) pipe_en_d\s*=\s*i_wdata\[0\]", t)
        if m:
            R.field[("gvureg", 0, 0, 0, "pipe_en")] = src("gcfg", i)
        m = re.match(r"\s*if \(i_wmask\[2\]\) pipe_done_d\s*=\s*pipe_done_q & ~i_wdata\[2\]", t)
        if m:
            R.field[("gvureg", 0, 2, 2, "done_clear")] = src("gcfg", i)
        if re.match(r"\s*cfg_reg_nxt\[CONTROL_REG\]\[1\]\s*=\s*pipe_done_d", t):
            R.field[("gvureg", 0, 1, 1, "done")] = src("gcfg", i)
        if re.search(r"i_wren && waddr == CONTROL_REG && i_wmask\[3\] && i_wdata\[3\]", t):
            R.field[("gvureg", 0, 3, 3, "force_swap")] = src("gcfg", i)

    # --- core words: config_regs.sv
    cl = read(RTL["cfg"])
    starts = [(i, re.search(r"TOTAL_BITS_(CON|ACT|WEI|OUT)", l).group(1)) for i, l in enumerate(cl)
              if re.search(r"for \(integer b=0; b<sauria_pkg::TOTAL_BITS_(CON|ACT|WEI|OUT)", l)]
    stops = [s[0] for s in starts[1:]] + [next(i for i, l in enumerate(cl) if "o_start = start_edge" in l)]
    for (s, reg), stop in zip(starts, stops):
        prev, pend = 0, None
        for i in range(s + 1, stop):
            t = strip_comment(cl[i])
            m = re.search(r"(?:if|else if)\s*\(b<([^)]+)\)", t)
            if m:
                pend = ev(m.group(1), env)
                continue
            m = re.match(r"\s*(\w+?)(?:\[[^\]]*\])*\s*=\s*reg_(?:con|act|wei|out)_mux", t)
            if m:
                end = pend if pend is not None else env["TOTAL_BITS_" + reg]
                name = re.sub(r"_BE$", "", m.group(1))
                name = re.sub(r"^o_", "", name).lower()
                R.core[(32 * base[reg] + prev, end - prev, name)] = src("cfg", i + 1)
                prev, pend = end, None
        if prev != env["TOTAL_BITS_" + reg]:
            R.errors.append("config_regs.sv: the %s fields end at bit %d, TOTAL_BITS is %d" % (reg, prev, env["TOTAL_BITS_" + reg]))
    return R


# ---------------------------------------------------------------- the frozen table
def write_table(R, path):
    out = ["# Register map of the neo_lite RTL, revision 1, for %s, extracted from the RTL by neo_regmap_check.py --emit-table." % PARAM_CONFIG,
           "# const NAME VALUE SRC | enum SET NAME VALUE SRC | field SPACE WORD HI LO NAME SRC | core BIT WIDTH NAME SRC",
           "# SRC = RTL file:line at that commit. SPACE: file = word of the 82-word file, gvureg = GVU register number, ctlwin = word of the controller window.",
           "commit %s" % COMMIT, "param_config %s" % PARAM_CONFIG]
    for k in sorted(R.const):
        out.append("const %s %s %s" % (k, hx(R.const[k][0]), R.const[k][1]))
    for k in sorted(R.enum, key=lambda x: (x[0], R.enum[x][0])):
        out.append("enum %s %s %d %s" % (k[0], k[1], R.enum[k][0], R.enum[k][1]))
    for k in sorted(R.field, key=lambda x: (SPACES.index(x[0]), x[1], -x[2], x[4])):
        out.append("field %s %d %d %d %s %s" % (k[0], k[1], k[2], k[3], k[4], R.field[k]))
    for k in sorted(R.core):
        out.append("core %d %d %s %s" % (k[0], k[1], k[2], R.core[k]))
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")


def read_table(path):
    R = Rtl()
    with open(path) as f:
        for ln, l in enumerate(f, 1):
            p = l.split()
            if not p or p[0].startswith("#"):
                continue
            if p[0] == "const":
                R.const[p[1]] = (int(p[2], 16), p[3])
            elif p[0] == "enum":
                R.enum[(p[1], p[2])] = (int(p[3]), p[4])
            elif p[0] == "field":
                R.field[(p[1], int(p[2]), int(p[3]), int(p[4]), p[5])] = p[6]
            elif p[0] == "core":
                R.core[(int(p[1]), int(p[2]), p[3])] = p[4]
            elif p[0] in ("commit", "param_config"):
                continue
            else:
                raise SystemExit("%s:%d: unknown record %r" % (path, ln, p[0]))
    return R


# ---------------------------------------------------------------- the model side
class Tree:
    def __init__(self, roots):
        self.roots = roots

    def text(self, rel):
        for r in self.roots:
            p = os.path.join(r, rel)
            if os.path.exists(p):
                with open(p, newline="") as f:
                    return f.read().replace("\r\n", "\n")
        raise SystemExit("%s not found under %s" % (rel, ", ".join(self.roots)))


def block(text, name):
    m = re.search(r"//\s*BEGIN %s\b.*?\n(.*?)//\s*END %s\b" % (name, name), text, re.S)
    if not m:
        raise SystemExit("libneo_cfg.h: block %s not found" % name)
    first = text[:m.start(1)].count("\n") + 1
    return m.group(1), first


def parse_enum_cpp(text, name):
    m = re.search(r"enum\s+%s\s*:\s*unsigned\s*\{(.*?)\};" % name, text, re.S)
    if not m:
        raise SystemExit("libneo_cfg.h: enum %s not found" % name)
    body = re.sub(r"//[^\n]*", "", m.group(1))
    out, nxt = {}, 0
    for item in body.split(","):
        item = item.strip()
        if not item:
            continue
        mm = re.match(r"(\w+)\s*(?:=\s*(\w+))?$", item)
        v = int(mm.group(2), 0) if mm.group(2) else nxt
        out[mm.group(1)] = v
        nxt = v + 1
    return out


GVU_REG_NAME = {"G_CONTROL": "CONTROL_REG", "G_OPCODE": "OPCODE_REG", "G_LUT": "LUT_REG", "G_GEMM": "GEMM_FUSED_REG", "G_SM0": "SOFTMAX_0_REG",
                "G_SM1": "SOFTMAX_1_REG", "G_SM2": "SOFTMAX_2_REG", "G_RES0": "RESIDUAL_0_REG", "G_RES1": "RESIDUAL_1_REG",
                "G_RES2": "RESIDUAL_2_REG", "G_RES3": "RESIDUAL_3_REG", "G_RES4": "RESIDUAL_4_REG", "G_POOL0": "POOLING_0_REG",
                "G_POOL1": "POOLING_1_REG"}
CORE_RENAME = {"auxlim": "klim", "auxstep": "kstep", "loc_woffs": "per_row_off"}
MODEL_ONLY_OP = {"NONE"}

# (file, regex, space, word, hi, lo): a shift / mask the model uses, tied to the position it must have in the RTL
PINS = []


def pin(f, rx, space, word, hi, lo):
    PINS.append((f, rx, space, word, hi, lo))


H = "has/has_host_neo.h"
for rx, w, hi, lo in ((r"xl = im\.reg\[neo::XY_LIM\] & 0xFFFFu", 0, 15, 0), (r"yl = im\.reg\[neo::XY_LIM\] >> 16", 0, 31, 16),
                      (r"cl = im\.reg\[neo::CK_LIM\] & 0xFFFFu", 1, 15, 0), (r"kl = im\.reg\[neo::CK_LIM\] >> 16", 1, 31, 16),
                      (r"lo = \(im\.reg\[neo::CTRL_BITS\] >> 16\) & 3u", 38, 17, 16)):
    pin(H, rx, "file", w, hi, lo)
for rx, reg, hi, lo in ((r"li\.act = \(gm & 1u\)", 3, 0, 0), (r"sext8\(gm >> 8\)", 3, 15, 8),
                        (r"\(r0 & 0xFFFFu\) \* 32u", 7, 15, 0), (r"sext8\(r0 >> 16\)", 7, 23, 16), (r"\(r0 >> 24\) & 0x3Fu", 7, 29, 24),
                        (r"hp\.z\) & 0xFFu\) << 16", 7, 23, 16), (r"hp\.sh\) & 0x3Fu\) << 24", 7, 29, 24),
                        (r"l\.add\.zpO\) & 0xFFu\) << 16", 7, 23, 16), (r"l\.add\.sO\) & 0x3Fu\) << 24", 7, 29, 24),
                        (r"static_cast<int>\(p0 & 3u\)", 12, 1, 0), (r"static_cast<int>\(\(p0 >> 2\) & 7u\)", 12, 4, 2),
                        (r"num_w = \(p0 >> 8\) & 0xFFu", 12, 15, 8), (r"oh_r = \(p0 >> 16\) & 0xFFu", 12, 23, 16), (r"ct = p0 >> 24", 12, 31, 24),
                        (r"c_move = \(p1 >> 16\) & 0x3FFu", 13, 25, 16),
                        (r"static_cast<uint32_t>\(\(k - 2\) & 3\)", 12, 1, 0), (r"static_cast<uint32_t>\(s\) & 7u\) << 2", 12, 4, 2),
                        (r"static_cast<uint32_t>\(nw\) & 0xFFu\) << 8", 12, 15, 8), (r"static_cast<uint32_t>\(oh\) & 0xFFu\) << 16", 12, 23, 16),
                        (r"static_cast<uint32_t>\(ct\) & 0xFFu\) << 24", 12, 31, 24),
                        (r"G_POOL1\) = \(static_cast<uint32_t>\(nw \* s\) & 0x3FFu\)", 13, 9, 0),
                        (r"static_cast<uint32_t>\(nw \* hp\) & 0x3FFu\) << 16", 13, 25, 16)):
    pin(H, rx, "gvureg", reg, hi, lo)
L = "driver/libneo_cfg.h"
pin(L, r"\(skip_act \? 1u : 0u\)", "gvureg", 3, 0, 0)
pin(L, r"static_cast<uint32_t>\(zp\) & 0xFFu\) << 8", "gvureg", 3, 15, 8)
pin(L, r"set_bits\(a\[21\], 17, 16", "file", 38, 17, 16)
S = "driver/libsauria_cfg.h"


def check_model(R, tree, rep):
    lib = tree.text("driver/libneo_cfg.h")

    # ---- constants in namespace neo::addr
    body, first = block(lib, "rtl_constants")
    env, hdr = {}, {}
    for m in re.finditer(r"constexpr\s+uint32_t\s+(\w+)\s*=\s*([^;]+);", body):
        v = ev(m.group(2), env)
        env[m.group(1)] = v
        hdr[m.group(1)] = (v, first + body[:m.start()].count("\n"))
    for name, (v, src) in sorted(R.const.items()):
        if name in ("N_CTRL_REGS", "GVU_NUM_REGS", "GVU_CFG_REGS", "N_DFC", "N_CORE", "N_GVU", "N_REGS", "IDX_CORE", "IDX_GVU", "REG_BASE") or name.startswith("TOTAL_REGS_") or name.endswith("_IDX_W"):
            continue   # compared below, outside namespace addr
        if name not in hdr:
            rep.mismatch("addr", "%s = %s missing in libneo_cfg.h" % (name, hx(v)), src)
        elif hdr[name][0] != v:
            rep.mismatch("addr", "%s: libneo_cfg.h:%d has %s, RTL has %s" % (name, hdr[name][1], hx(hdr[name][0]), hx(v)), src)
        else:
            rep.ok()
    for name, (v, line) in hdr.items():
        if name not in R.const:
            rep.mismatch("addr", "libneo_cfg.h:%d defines %s that the RTL does not have" % (line, name))

    # ---- counts and the controller window offsets of the existing constants
    consts = {}
    for m in re.finditer(r"constexpr uint32_t CTRL = (\w+), IEN = (\w+), ISTATUS = (\w+), REG_BASE = (\w+);", lib):
        consts.update(CTRL=int(m.group(1), 0), IEN=int(m.group(2), 0), ISTATUS=int(m.group(3), 0), REG_BASE=int(m.group(4), 0))
    m = re.search(r"constexpr unsigned N_DFC = (\d+), N_CORE = (\d+), N_GVU = (\d+), N_REGS = N_DFC \+ N_CORE \+ N_GVU;", lib)
    if not m or not consts:
        raise SystemExit("libneo_cfg.h: window constants not found")
    consts.update(N_DFC=int(m.group(1)), N_CORE=int(m.group(2)), N_GVU=int(m.group(3)))
    consts["N_REGS"] = consts["N_DFC"] + consts["N_CORE"] + consts["N_GVU"]
    m = re.search(r"constexpr unsigned IDX_CORE = (\w+), IDX_GVU = (\w+) \+ (\w+);", lib)
    consts["IDX_CORE"] = consts["N_DFC"]
    consts["IDX_GVU"] = consts["N_DFC"] + consts["N_CORE"]
    for name in ("N_DFC", "N_CORE", "N_GVU", "N_REGS", "IDX_CORE", "IDX_GVU", "REG_BASE"):
        v, src = R.const[name]
        if consts[name] != v:
            rep.mismatch("count", "%s: libneo_cfg.h has %d, RTL has %d" % (name, consts[name], v), src)
        else:
            rep.ok()
    for name, space_word in (("CTRL", 0), ("IEN", 2), ("ISTATUS", 3)):
        if consts[name] != 4 * space_word:
            rep.mismatch("window", "%s = %s, the controller window decodes word %d at byte %s" % (name, hx(consts[name]), space_word, hx(4 * space_word)))
        else:
            rep.ok()
    # the window words the RTL decodes
    seen_ctl = {(k[1], k[2]) for k in R.field if k[0] == "ctlwin"}
    want_ctl = {(0, 0), (0, 1), (2, 0), (3, 0)}
    if seen_ctl != want_ctl:
        rep.mismatch("window", "controller window bits in the RTL are %s, the model assumes %s" % (sorted(seen_ctl), sorted(want_ctl)))
    else:
        rep.ok()

    # ---- enums
    for cpp, rtl_set in (("Op", "neo_opcode_t"), ("GvuOp", "gvu_opcode_t")):
        for name, v in parse_enum_cpp(lib, cpp).items():
            if cpp == "Op" and name in MODEL_ONLY_OP:
                continue
            k = (rtl_set, name)
            if k not in R.enum:
                rep.mismatch("enum", "%s::%s is not in %s" % (cpp, name, rtl_set))
            elif R.enum[k][0] != v:
                rep.mismatch("enum", "%s::%s = %d, %s has %d" % (cpp, name, v, rtl_set, R.enum[k][0]), R.enum[k][1])
            else:
                rep.ok()
    gv = parse_enum_cpp(lib, "GvuReg")
    for name, v in gv.items():
        k = ("gvu_cfg_reg_t", GVU_REG_NAME[name])
        if k not in R.enum:
            rep.mismatch("enum", "GvuReg::%s: %s not in gvu_cfg_reg_t" % (name, GVU_REG_NAME[name]))
        elif R.enum[k][0] != v:
            rep.mismatch("enum", "GvuReg::%s = %d, gvu_cfg_reg_t has %d" % (name, v, R.enum[k][0]), R.enum[k][1])
        else:
            rep.ok()
    if len(gv) != sum(1 for k in R.enum if k[0] == "gvu_cfg_reg_t"):
        rep.mismatch("enum", "GvuReg has %d registers, gvu_cfg_reg_t has %d" % (len(gv), sum(1 for k in R.enum if k[0] == "gvu_cfg_reg_t")))
    dfc = parse_enum_cpp(lib, "Dfc")

    # ---- fields of the DFC words, the GVU registers and the controller window
    body, first = block(lib, "rtl_fields")
    hf = {}
    for m in re.finditer(r"\{\s*FS_(FILE|GVUREG|CTLWIN)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*\"([^\"]*)\"\s*,\s*\"(\w*)\"\s*\}", body):
        key = (m.group(1).lower(), int(m.group(2)), int(m.group(3)), int(m.group(4)), m.group(5))
        line = first + body[:m.start()].count("\n")
        hf[key] = (m.group(6), line)
        if key[0] in ("file", "gvureg") and m.group(6):
            sym = m.group(6)
            if key[0] == "file" and sym in dfc:
                want = dfc[sym]
            elif sym in gv:
                want = consts["IDX_GVU"] + gv[sym] - 1 if key[0] == "file" else gv[sym]
            else:
                want = None
            if want is None or want != key[1]:
                rep.mismatch("symbol", "libneo_cfg.h:%d: %s is word %s in the table, the enum gives %s" % (line, sym, key[1], want))
            else:
                rep.ok()
    rtl_dfc_gvu = {k for k in R.field}
    only_rtl = sorted(rtl_dfc_gvu - set(hf), key=lambda x: (x[0], x[1], -x[2]))
    only_hdr = sorted(set(hf) - rtl_dfc_gvu, key=lambda x: (x[0], x[1], -x[2]))
    paired = set()
    for k in only_rtl:
        twin = [h for h in only_hdr if h[0] == k[0] and h[1] == k[1] and h[4] == k[4]]
        if twin:
            paired.add(twin[0])
            rep.mismatch("field", "%s word %d %s: libneo_cfg.h:%d has [%d:%d], RTL has [%d:%d]" % (k[0], k[1], k[4], hf[twin[0]][1], twin[0][2], twin[0][3], k[2], k[3]), R.field[k])
        else:
            rep.mismatch("field", "%s word %d [%d:%d] %s missing in neo::FIELDS" % k, R.field[k])
    for k in only_hdr:
        if k not in paired:
            rep.mismatch("field", "libneo_cfg.h:%d: %s word %d [%d:%d] %s is not in the RTL" % ((hf[k][1],) + k))
    both = rtl_dfc_gvu & set(hf)
    rep.ok(len(both))
    for sym, v in dfc.items():
        if not any(k[0] == "file" and k[1] == v and s == sym for k, (s, _) in hf.items()):
            rep.mismatch("symbol", "Dfc::%s = %d has no row in neo::FIELDS" % (sym, v))
        else:
            rep.ok()

    # ---- core words: the packed layout of the model + the extra bits of the RTL
    lay = tree.text("sauria_cfg_layout.h")
    tg = tree.text("sauria_targets.h")
    m = re.search(r'\{"%s",\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)' % PARAM_CONFIG, tg)
    X, Y, _ia, _ib, _oc, _op, idx_a, idx_w, idx_o = [int(x) for x in m.groups()]
    th = int(re.search(r"th_w = (\d+)", lay).group(1))
    dilp = int(re.search(r"dilp_w = (\d+)", lay).group(1))
    prm = int(re.search(r"params_w = (\d+)", lay).group(1))
    wk = {"ACT_IDX": idx_a, "WEI_IDX": idx_w, "OUT_IDX": idx_o, "TH": th, "DILP": dilp, "PARAMS": prm, "Y": Y, "X": X, "ONE": 1}
    cursor = 0
    mod = {}
    for m in re.finditer(r"\{F_CFG_(\w+),\s*WK_(\w+),\s*(true|false),\s*SP_(\w+)\}", lay):
        width = wk[m.group(2)] * (Y if m.group(4) == "SKIP_PER_ROW" else 1)
        mod[(cursor, width, m.group(1).lower())] = "sauria_cfg_layout.h:%d" % (lay[:m.start()].count("\n") + 1)
        cursor += width
        if m.group(3) == "true" and cursor % 32:
            cursor = (cursor // 32 + 1) * 32
    body, first = block(lib, "rtl_core_extra")
    env2 = {}
    for m in re.finditer(r"constexpr\s+unsigned\s+(\w+)\s*=\s*([^;]+);", body):
        env2[m.group(1)] = ev(m.group(2), env2)
    hdr_idx = {k: env2[k] for k in ("CORE_IDX_A", "CORE_IDX_W", "CORE_IDX_O") if k in env2}
    for k in ("CON", "ACT", "WEI", "OUT"):
        v, src = R.const["TOTAL_REGS_" + k]
        if env2.get("CORE_WORDS_" + k) != v:
            rep.mismatch("core", "CORE_WORDS_%s in libneo_cfg.h is %s, the RTL has %d words" % (k, env2.get("CORE_WORDS_" + k), v), src)
        else:
            rep.ok()
    for m in re.finditer(r"\{\s*([^,{}]+?)\s*,\s*(\d+)\s*,\s*\"(\w+)\"\s*\}", body):
        mod[(ev(m.group(1), env2), int(m.group(2)), m.group(3))] = "libneo_cfg.h:%d" % (first + body[:m.start()].count("\n"))
    rtl_core = {}
    for (bit, width, name), src in R.core.items():
        rtl_core[(bit, width, CORE_RENAME.get(name, name))] = src
    for k in sorted(set(rtl_core) - set(mod)):
        near = [x for x in mod if x[2] == k[2]]
        rep.mismatch("core", "RTL field %s at bit %d width %d; the model has %s" % (k[2], k[0], k[1], ("bit %d width %d" % (near[0][0], near[0][1])) if near else "no such field"), rtl_core[k])
    for k in sorted(set(mod) - set(rtl_core)):
        near = [x for x in rtl_core if x[2] == k[2]]
        if not near:
            rep.mismatch("core", "model field %s (bit %d width %d, %s) is not in the RTL" % (k[2], k[0], k[1], mod[k]))
    rep.ok(len(set(rtl_core) & set(mod)))
    for name, v in (("CORE_IDX_A", R.const["ACT_IDX_W"][0]), ("CORE_IDX_W", R.const["WEI_IDX_W"][0]), ("CORE_IDX_O", R.const["OUT_IDX_W"][0])):
        if hdr_idx.get(name) != v:
            rep.mismatch("core", "%s in libneo_cfg.h is %s, the RTL width is %d" % (name, hdr_idx.get(name), v), R.const[{"CORE_IDX_A": "ACT_IDX_W", "CORE_IDX_W": "WEI_IDX_W", "CORE_IDX_O": "OUT_IDX_W"}[name]][1])
        else:
            rep.ok()
    if (idx_a, idx_w, idx_o) != (R.const["ACT_IDX_W"][0], R.const["WEI_IDX_W"][0], R.const["OUT_IDX_W"][0]):
        rep.mismatch("core", "sauria_targets.h %s has index widths %s, the RTL has %s" % (PARAM_CONFIG, (idx_a, idx_w, idx_o), (R.const["ACT_IDX_W"][0], R.const["WEI_IDX_W"][0], R.const["OUT_IDX_W"][0])))
    else:
        rep.ok()

    # ---- shifts and masks the encoder / decoder use
    lost = 0
    for f, rx, space, word, hi, lo in PINS:
        txt = tree.text(f)
        n = len(re.findall(rx, txt))
        if n == 0:
            lost += 1
            rep.mismatch("uses", "%s: the pattern for [%d:%d] of %s %d is gone (%s); update PINS in this tool" % (f, hi, lo, space, word, rx))
            continue
        if not any(k[0] == space and k[1] == word and k[2] == hi and k[3] == lo for k in R.field):
            rep.mismatch("uses", "%s uses [%d:%d] of %s %d; the RTL has no such field" % (f, hi, lo, space, word))
        else:
            rep.ok()
    txt = tree.text(S)
    for m in re.finditer(r"a\[(\d+)\] = set_bits\(a\[\1\], (\d+), (\d+),", txt):
        w, hi, lo = int(m.group(1)), int(m.group(2)), int(m.group(3))
        if w == 21:
            w = R.const["N_DFC"][0] - 2   # libsauria_cfg args[21] (the control bits) is word 38 of the file: libneo_cfg.h encode_gemm
        if not any(k[0] == "file" and k[1] == w and k[2] == hi and k[3] == lo for k in R.field):
            rep.mismatch("uses", "%s:%d sets [%d:%d] of word %d; the RTL has no such field" % (S, txt[:m.start()].count("\n") + 1, hi, lo, w))
        else:
            rep.ok()
    return


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rtl", help="neo_lite git repository (read with git show)")
    ap.add_argument("--table", help="frozen table instead of the RTL tree")
    ap.add_argument("--emit-table", help="with --rtl: write the extracted table")
    ap.add_argument("--commit", default=COMMIT)
    ap.add_argument("--root", action="append", help="directory holding the model sources (searched in the order given; default: this tree)")
    a = ap.parse_args()
    if bool(a.rtl) == bool(a.table):
        ap.error("exactly one of --rtl and --table")
    here = os.path.dirname(os.path.abspath(__file__))
    roots = a.root or [os.path.dirname(os.path.dirname(here))]
    if a.rtl:
        R = extract(git_reader(a.rtl, a.commit))
        print("source: RTL %s@%s" % (os.path.basename(os.path.abspath(a.rtl)), a.commit))
        if a.emit_table:
            if os.path.abspath(a.emit_table).startswith(os.path.abspath(a.rtl) + os.sep):
                raise SystemExit("refusing to write inside the RTL tree")
            write_table(R, a.emit_table)
            print("table written: %s (%d consts, %d enums, %d fields, %d core fields)" % (a.emit_table, len(R.const), len(R.enum), len(R.field), len(R.core)))
    else:
        R = read_table(a.table)
        print("source: table %s" % a.table)
    rep = Report()
    for e in R.errors:
        rep.mismatch("extract", e)
    check_model(R, Tree(roots), rep)
    n_dfc, n_core, n_gvu = R.const["N_DFC"][0], R.const["N_CORE"][0], R.const["N_GVU"][0]
    covered = {k[1] for k in R.field if k[0] == "file"}
    for (bit, width, _name) in R.core:
        covered.update(n_dfc + w for w in range(bit // 32, (bit + width - 1) // 32 + 1))
    missing = sorted(set(range(n_dfc + n_core + n_gvu)) - covered)
    if missing:
        rep.mismatch("cover", "words with no field in the RTL extraction: %s" % missing)
    else:
        rep.ok()
    print("words covered by at least one field: %d of %d (%d DFC + %d core + %d GVU)" % (len(covered), n_dfc + n_core + n_gvu, n_dfc, n_core, n_gvu))
    print("RESULT: %d checked, %d mismatches" % (rep.checked, rep.bad))
    sys.exit(1 if rep.bad else 0)


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    main()
