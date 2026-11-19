#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run one FX1 NPU job fixture on the VP and check it (Python standard library only).

  npu_run.py --fx1-soc <fx1_soc> --elf <freertos_cli.elf> <fixture dir> [--out DIR]
             [--timeout-ms N] [--quantum-ns N] [--print-command]

A fixture directory (made by fx1_fw_npu_job_gen, fw/fx1_soc/host/npu_job_gen.c)
holds job.bin, window.bin, golden_<i>.bin and fixture.json. The launcher:

  1. loads job.bin at the job address and window.bin at the job's window
     (fx1_soc --load), so the firmware finds them before it starts;
  2. drives the FreeRTOS CLI from a script: `npu info`, `npu run`,
     `npu wait`, `npu status`, `poweroff`;
  3. dumps the result block and every OUTPUT tensor after the run
     (fx1_soc --dump) and compares them with the goldens, byte for byte;
  4. writes summary.json / summary.txt in the output directory.

The simulator gets a clean environment: only PATH, HOME, USER, LOGNAME, TMPDIR,
TZ, TERM, LANG, LC_* and LD_LIBRARY_PATH are passed on. The NPU model reads
configuration knobs from the environment (HAS_*, SAURIA_*, NPU_*, ...); a
fixture runs with the drop's defaults, and the run fails unless the platform
banner reports env=none. The knob-like names removed are listed in the summary.

The output directory belongs to the launcher: it must not be, contain or lie
inside the fixture, and it must be new, empty, or one an earlier npu_run made
(marker file .npu_run, or holding only the launcher's own files: cli.txt,
result.bin, out_<n>.bin, uart.log, platform.log, summary.*). Only those files
are replaced there;
anything else in it is kept. --print-command prints the fx1_soc command with
its clean environment (to run it by hand, or with --uart0-socket for an
interactive console) and exits; it writes only cli.txt and the marker.

Exit codes: 0 the fixture's declared outcome, outputs equal to the goldens;
1 another outcome or an output differs; 2 bad arguments or fixture; 3 the
simulator failed or wrote no result block.
"""
import argparse
import hashlib
import json
import os
import re
import shlex
import struct
import subprocess
import sys
import time

RESULT_MAGIC = 0x524E5846  # "FXNR"
STATUS = ["OK", "BAD_JOB", "NO_NPU", "UNSUPPORTED", "INPUT_CRC", "REFUSED", "FAILED", "TIMEOUT", "ABORTED",
          "UNAVAILABLE", "MISMATCH"]
REASONS = ["none", "magic", "version", "size", "crc", "abi", "arena", "window", "table", "timeout", "name",
           "reserved", "tensor role", "tensor type", "tensor shape", "tensor align", "tensor bounds",
           "tensor overlap", "tensor crc", "op kind", "op tensor", "op role", "op shape", "op param", "op order",
           "op build", "unwritten"]
DRIVER = ["OK", "ABSENT", "ABI", "FEATURE", "ARG", "TIMEOUT", "REFUSED", "JOB", "UNAVAILABLE", "NOT_IDLE",
          "PENDING"]
# fx1_npu_job_result, 64 little-endian words (fw/fx1_soc/drivers/fx1_npu_job.h).
RESULT_FIELDS = (["magic", "version", "status", "detail", "op_index", "driver_status"] +
                 ["err_status", "err_last_error", "err_irq_status", "err_rich_status", "err_vp_error",
                  "err_vp_error_info", "err_vp_status"] +
                 ["ops_done", "rich_instr", "neo_instr", "gemm_jobs", "accepted_delta", "retired_delta",
                  "exec_cycles", "mac_ops_lo", "mac_ops_hi", "gemm_cycles", "ticks", "irqs", "stale_irqs",
                  "n_outputs", "outputs_ok", "job_crc", "model_hash"] +
                 ["out_crc_%d" % i for i in range(32)] + ["reserved0", "reserved1"])
MTIME_NS = 50  # 20 MHz
MARKER = ".npu_run"
# Files the launcher writes in its output directory (nothing else is ever removed).
MANAGED = ("cli.txt", "result.bin", "uart.log", "platform.log", "summary.json", "summary.json.tmp", "summary.txt")
OUT_DUMP = re.compile(r"out_[0-9]+\.bin")  # the output-tensor dumps, out_<tensor index>.bin


def managed(name):
    """A file the launcher writes in its output directory: the only names it
    ever claims or removes (out_camera_capture.raw, out_notes.txt are not)."""
    return name in MANAGED or OUT_DUMP.fullmatch(name) is not None
ENV_KEEP = ("PATH", "HOME", "USER", "LOGNAME", "TMPDIR", "TZ", "TERM", "LANG", "LD_LIBRARY_PATH")
# Prefixes of the environment knobs the NPU model drop reads (reported when removed).
MODEL_KNOBS = ("HAS_", "SAURIA_", "NPU_", "OBP_", "GVU_", "FE_", "S1_")


def clean_env():
    keep = {k: v for k, v in os.environ.items() if k in ENV_KEEP or k.startswith("LC_")}
    knobs = sorted(k for k in os.environ if k not in keep and k.startswith(MODEL_KNOBS))
    other = sum(1 for k in os.environ if k not in keep and not k.startswith(MODEL_KNOBS))
    return keep, knobs, other


def inside(child, parent):
    """child lies in parent or is parent (real paths)."""
    try:
        return os.path.commonpath([child, parent]) == parent
    except ValueError:
        return False


def prepare_out(out, protected):
    """Refuses an output directory that is, holds or lies in an input; claims a
    new or empty one; in one made earlier removes only the launcher's files.
    Returns an error text, or None."""
    real = os.path.realpath(out)
    for p in protected:
        if inside(real, p) or inside(p, real):
            return "output directory %s overlaps the input %s" % (out, p)
    if os.path.lexists(out) and not os.path.isdir(real):
        return "output %s exists and is not a directory" % out
    if os.path.isdir(real):
        entries = os.listdir(real)
        if entries and MARKER not in entries and not all(managed(e) for e in entries):
            return "output directory %s is not empty and was not made by npu_run (no %s); choose another" % (out, MARKER)
        for e in entries:
            if managed(e):
                path = os.path.join(real, e)
                if os.path.isfile(path) or os.path.islink(path):
                    os.remove(path)  # a stale result never passes for this run's
    else:
        os.makedirs(real)
    with open(os.path.join(real, MARKER), "w") as f:
        f.write("npu_run output directory\n")
    return None


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def name_of(table, i):
    return table[i] if 0 <= i < len(table) else "?(%d)" % i


def parse_result(path):
    try:
        with open(path, "rb") as f:
            b = f.read()
    except OSError:
        return None
    if len(b) < 256:
        return None
    r = dict(zip(RESULT_FIELDS, struct.unpack("<64I", b[:256])))
    return r if r["magic"] == RESULT_MAGIC else None


def sim_time_ns(log):
    m = re.search(r"\[fx1_soc\] simulated ([0-9.]+) (ps|ns|us|ms|s),", log)
    if not m:
        return None
    scale = {"ps": 1e-3, "ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}[m.group(2)]
    return int(float(m.group(1)) * scale)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("fixture")
    ap.add_argument("--fx1-soc", required=True)
    ap.add_argument("--elf", required=True)
    ap.add_argument("--out", help="output directory (default: ./npu_run/<fixture name>)")
    ap.add_argument("--timeout-ms", type=int, default=0, help="simulated-time limit (default: from the job)")
    ap.add_argument("--quantum-ns", type=int, default=1000)
    ap.add_argument("--print-command", action="store_true")
    a = ap.parse_args()

    fx = os.path.abspath(a.fixture)
    try:
        with open(os.path.join(fx, "fixture.json")) as f:
            spec = json.load(f)
        files = [os.path.join(fx, spec["job_file"]), os.path.join(fx, spec["window_file"])]
        outputs = [t for t in spec["tensors"] if t["role"] == "OUTPUT"]
        for p in files + [os.path.join(fx, t["golden"]) for t in outputs if t["golden"]]:
            if not os.path.isfile(p):
                raise FileNotFoundError(p)
        if os.path.getsize(files[0]) != spec["job_bytes"] or os.path.getsize(files[1]) != spec["load_bytes"]:
            raise ValueError("job.bin / window.bin size differs from fixture.json")
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as e:
        print("npu_run: bad fixture %s: %s" % (fx, e))
        return 2
    for p in (a.fx1_soc, a.elf):
        if not os.path.isfile(p):
            print("npu_run: not found: %s" % p)
            return 2

    name = spec["name"]
    out = os.path.abspath(a.out or os.path.join("npu_run", name))
    protected = [os.path.realpath(fx)] + [os.path.realpath(p) for p in (a.fx1_soc, a.elf)]
    err = prepare_out(out, protected)
    if err:
        print("npu_run: %s" % err)
        return 2
    out = os.path.realpath(out)
    job_ms = spec["timeout_ms"]
    # The CLI script. `npu wait` without a limit returns only once the job has
    # ended (the job's own deadline and the firmware's CRCs bound it), so
    # poweroff never comes before the job's outcome is known.
    script = os.path.join(out, "cli.txt")
    with open(script, "w") as f:
        f.write("npu info\nnpu run\nnpu wait\nnpu status\npoweroff\n")
    # Simulated time: boot, console input, the firmware's checks (hart-side CRCs), the job.
    limit = a.timeout_ms or 4000 + 2 * job_ms
    dumps = [("result", spec["result_addr"], spec["result_bytes"], os.path.join(out, "result.bin"))]
    for t in outputs:
        dumps.append(("t%d" % t["index"], t["addr"], t["bytes"], os.path.join(out, "out_%d.bin" % t["index"])))
    cmd = [a.fx1_soc, "--fw", a.elf, "--timeout-ms", str(limit), "--quantum-ns", str(a.quantum_ns),
           "--load", "0x%08X:%s" % (spec["job_addr"], files[0]),
           "--load", "0x%08X:%s" % (spec["window_addr"], files[1]),
           "--uart0-rx-file", script, "--uart0-rx-delay-us", "60000", "--uart0-rx-byte-us", "87",
           "--uart-log", os.path.join(out, "uart.log")]
    for _, addr, n, path in dumps:
        cmd += ["--dump", "0x%08X:%d:%s" % (addr, n, path)]
    env, knobs, other = clean_env()
    if a.print_command:
        print("env -i " + " ".join("%s=%s" % (k, shlex.quote(v)) for k, v in sorted(env.items())) + " " +
              " ".join(shlex.quote(c) for c in cmd))
        if knobs:
            print("# removed from the environment (model knobs): " + " ".join(knobs))
        return 0
    scrubbed = knobs
    t0 = time.monotonic()
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
    wall = time.monotonic() - t0
    log = p.stdout.decode("utf-8", "replace")
    with open(os.path.join(out, "platform.log"), "w") as f:
        f.write(log)

    s = {"fixture": name, "description": spec.get("description", ""), "fixture_dir": fx,
         "command": cmd, "env_removed": scrubbed, "env_removed_other": other, "simulator_rc": p.returncode, "wall_s": round(wall, 3),
         "simulated_ns": sim_time_ns(log), "expected": spec["expect"]["status"],
         "inputs": {os.path.basename(f): sha256(f) for f in files}}
    m = re.search(r"\[fx1_soc\] npu: at [^\n]*", log)
    s["npu_banner"] = m.group(0) if m else None
    cfg = re.search(r"profile=(\S+) host=(\S+) env=([^,\s]+)", s["npu_banner"] or "")
    s["model_config"] = {"profile": cfg.group(1), "host": cfg.group(2), "env": cfg.group(3)} if cfg else None
    s["uart_npu_lines"] = [l.strip() for l in log.splitlines() if re.match(r"\s*npu (run|wait|reset)?:", l)]
    r = parse_result(dumps[0][3])
    if p.returncode not in (0, 1) or r is None:
        s["outcome"] = "SIM_ERROR"
        s["reason"] = ("no result block (is the NPU present? see platform.log)" if r is None
                       else "simulator exit %d" % p.returncode)
        return finish(out, s, 3)
    if not s["model_config"] or s["model_config"]["env"] != "none":
        s["outcome"] = "FAIL"
        s["reason"] = "the model did not run with the drop's defaults (banner: %s)" % s["npu_banner"]
        return finish(out, s, 1)
    st = name_of(STATUS, r["status"])
    res = {"status": st, "op_index": None if r["op_index"] == 0xFFFFFFFF else r["op_index"],
           "driver_status": name_of(DRIVER, r["driver_status"]), "ops_done": r["ops_done"],
           "rich_instr": r["rich_instr"], "neo_instr": r["neo_instr"], "gemm_jobs": r["gemm_jobs"],
           "accepted_delta": r["accepted_delta"], "retired_delta": r["retired_delta"],
           "model_exec_cycles": r["exec_cycles"], "mac_ops": r["mac_ops_hi"] << 32 | r["mac_ops_lo"],
           "gemm_bank_cycles": r["gemm_cycles"], "npu_job_ns": r["ticks"] * MTIME_NS,
           "interrupts": r["irqs"], "stale_interrupts": r["stale_irqs"],
           "outputs_ok_fw": "%d/%d" % (r["outputs_ok"], r["n_outputs"]),
           "model_hash": "0x%08x" % r["model_hash"], "job_crc": "0x%08x" % r["job_crc"],
           "error_registers": {k[4:]: "0x%08x" % r[k] for k in RESULT_FIELDS if k.startswith("err_")}}
    if st == "BAD_JOB":
        res["detail"] = "%s, index %d" % (name_of(REASONS, r["detail"] >> 16), r["detail"] & 0xFFFF)
    elif st in ("INPUT_CRC", "MISMATCH"):
        res["detail"] = "tensor %d" % r["detail"]
    else:
        res["detail"] = "0x%x" % r["detail"]
    s["result"] = res
    compared = []
    if st == "OK":
        for t in outputs:
            got = open(os.path.join(out, "out_%d.bin" % t["index"]), "rb").read()
            want = open(os.path.join(fx, t["golden"]), "rb").read()
            diff = sum(1 for x, y in zip(got, want) if x != y) + abs(len(got) - len(want))
            fw_crc = r["out_crc_%d" % t["index"]]
            compared.append({"tensor": t["index"], "name": t["name"], "bytes": t["bytes"], "bytes_differ": diff,
                             "fw_crc32": "0x%08x" % fw_crc, "expected_crc32": "0x%08x" % t["crc32"],
                             "sha256": hashlib.sha256(got).hexdigest()})
    s["outputs"] = compared
    ok = st == spec["expect"]["status"] and all(c["bytes_differ"] == 0 and c["fw_crc32"] == c["expected_crc32"]
                                                 for c in compared)
    if ok and st != "OK" and spec["expect"]["detail"] and r["detail"] != spec["expect"]["detail"]:
        ok = False
    # The firmware's verdict follows the job: poweroff passes for OK, fails for any other outcome.
    if ok and p.returncode != (0 if st == "OK" else 1):
        ok = False
    s["outcome"] = "PASS" if ok else "FAIL"
    s["reason"] = ("as declared (%s)" % st) if ok else ("got %s, declared %s" % (st, spec["expect"]["status"]))
    return finish(out, s, 0 if ok else 1)


def finish(out, s, rc):
    s["exit_code"] = rc
    tmp = os.path.join(out, "summary.json.tmp")
    with open(tmp, "w") as f:
        json.dump(s, f, indent=2, allow_nan=False)
        f.write("\n")
    os.replace(tmp, os.path.join(out, "summary.json"))
    lines = ["NPU %s %s: %s" % (s["fixture"], s["outcome"], s.get("reason", ""))]
    r = s.get("result")
    if r:
        lines.append("  status %s (declared %s), detail %s, op %s, driver %s" %
                     (r["status"], s["expected"], r["detail"], r["op_index"], r["driver_status"]))
        lines.append("  ops %d (rich %d, neo %d, GEMM bank %d), accepted +%d, retired +%d, outputs (firmware) %s" %
                     (r["ops_done"], r["rich_instr"], r["neo_instr"], r["gemm_jobs"], r["accepted_delta"],
                      r["retired_delta"], r["outputs_ok_fw"]))
        lines.append("  model exec cycles %d, MAC ops %d, GEMM-bank cycles %d, interrupts %d (stale %d)" %
                     (r["model_exec_cycles"], r["mac_ops"], r["gemm_bank_cycles"], r["interrupts"],
                      r["stale_interrupts"]))
        lines.append("  NPU job %.1f us of simulated time, first submit to last completion (incl. DMA and per-operation firmware work)" %
                     (r["npu_job_ns"] / 1000.0))
    for c in s.get("outputs", []):
        lines.append("  output %d %-10s %7d bytes, %d differ from the golden, CRC-32 %s (expected %s)" %
                     (c["tensor"], c["name"], c["bytes"], c["bytes_differ"], c["fw_crc32"], c["expected_crc32"]))
    sim = s.get("simulated_ns")
    lines.append("  platform: simulated %s ms, wall %.1f s, simulator exit %s%s" %
                 ("%.1f" % (sim / 1e6) if sim is not None else "?", s["wall_s"], s["simulator_rc"],
                  (", removed from the environment: " + " ".join(s["env_removed"])) if s["env_removed"] else ""))
    lines.append("  files: %s" % out)
    text = "\n".join(lines) + "\n"
    with open(os.path.join(out, "summary.txt"), "w") as f:
        f.write(text)
    sys.stdout.write(text)
    return rc


if __name__ == "__main__":
    sys.exit(main())
