#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""fx1_regression_npu_run_launcher: output-directory safety and the clean
environment of demo/npu_run.py (review N3 R1, R3).

  test_npu_run_launcher.py <npu_run.py> <fx1_soc> <freertos_cli.elf> <chain fixture> <work dir>

Every case works on a copy of the fixture. Paths: an output directory that is
the fixture, its parent, a symlink to it, inside it, or a foreign non-empty
directory is refused (exit 2) with every input and user file untouched;
--print-command writes only cli.txt and the marker; in an npu_run directory a
stale result is removed, user files kept. Links (review N5 R3): a marker that
is a symbolic link is refused (exit 2), its target untouched; a hard-linked
marker and links at the launcher's file names are replaced by new files, their
targets untouched. Environment: inherited HAS_*,
SAURIA_CORE_RTL_* and NPU_* knobs are removed (named in the summary) and the run
equals a clean one (banner env=none, model counters, output bytes).
Name tables: the launcher's STATUS, REASONS and DRIVER lists equal the enums of
fw/fx1_soc/drivers/fx1_npu_job.h and fx1_npu.h, in order (a status added there
is never printed as a number).
"""
import hashlib
import importlib.util
import json
import os
import re
import shutil
import stat
import subprocess
import sys

failures = 0


def check(ok, what):
    global failures
    print("  [%s] %s" % (" ok " if ok else "FAIL", what))
    if not ok:
        failures += 1


def tree(d):
    """{relative path: sha256} of every file below d."""
    out = {}
    for root, _, files in os.walk(d):
        for f in files:
            p = os.path.join(root, f)
            with open(p, "rb") as fh:
                out[os.path.relpath(p, d)] = hashlib.sha256(fh.read()).hexdigest()
    return out


def main():
    launcher, sim, elf, fixture, work = sys.argv[1:6]
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    fx = os.path.join(work, "fx")
    shutil.copytree(fixture, fx)
    with open(os.path.join(fx, "user_tuning_notes.txt"), "w") as f:
        f.write("not the launcher's\n")
    before = tree(work)

    def run(out, *extra, env=None):
        return subprocess.run([sys.executable, "-B", launcher, "--fx1-soc", sim, "--elf", elf, fx, "--out", out, *extra],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)

    # R1: refused output directories, nothing touched.
    os.symlink(fx, os.path.join(work, "link_to_fx"))
    user = os.path.join(work, "userdir")
    os.makedirs(user)
    with open(os.path.join(user, "keep.txt"), "w") as f:
        f.write("mine\n")
    before = tree(work)
    for label, out in [("the fixture", fx), ("the fixture's parent", work), ("a symlink to the fixture", os.path.join(work, "link_to_fx")),
                       ("inside the fixture", os.path.join(fx, "sub")), ("a foreign non-empty directory", user)]:
        for extra in ((), ("--print-command",)):
            p = run(out, *extra)
            check(p.returncode == 2 and tree(work) == before and not os.path.exists(os.path.join(fx, "sub")),
                  "output %s%s: refused (exit %d), every file kept" % (label, " with --print-command" if extra else "",
                                                                       p.returncode))
    # --print-command into a new directory: cli.txt and the marker only.
    pc = os.path.join(work, "printed")
    p = run(pc, "--print-command")
    check(p.returncode == 0 and sorted(os.listdir(pc)) == [".npu_run", "cli.txt"] and p.stdout.startswith("env -i ") and
          tree(fx) == {k[3:]: v for k, v in before.items() if k.startswith("fx/")},
          "--print-command: exit 0, only cli.txt + marker written, a clean-environment command, the fixture kept")

    # A stale result in an npu_run directory is removed; user files there are kept.
    fake = os.path.join(work, "fake_fx1_soc")
    with open(fake, "w") as f:
        f.write("#!/bin/sh\nexit 0\n")
    os.chmod(fake, os.stat(fake).st_mode | stat.S_IXUSR)
    with open(os.path.join(pc, "result.bin"), "wb") as f:
        f.write(b"\x46\x58\x4e\x52" + bytes(252))  # a stale block with the result magic
    with open(os.path.join(pc, "mine.txt"), "w") as f:
        f.write("mine\n")
    p = subprocess.run([sys.executable, "-B", launcher, "--fx1-soc", fake, "--elf", elf, fx, "--out", pc],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    s = json.load(open(os.path.join(pc, "summary.json")))
    check(p.returncode == 3 and s["outcome"] == "SIM_ERROR" and not os.path.exists(os.path.join(pc, "result.bin")) and
          os.path.exists(os.path.join(pc, "mine.txt")),
          "stale result.bin removed before the run (no false result), user file kept: exit %d %s" % (p.returncode, s["outcome"]))

    # Names that only look like the launcher's (out_* but not out_<n>.bin) are user files:
    # a directory holding them is foreign (refused); next to a marker they are kept.
    lookalike = os.path.join(work, "lookalike")
    os.makedirs(lookalike)
    for n in ("out_camera_capture.raw", "out_tuning_notes.txt", "out_.bin", "out_3.bin.bak"):
        with open(os.path.join(lookalike, n), "w") as f:
            f.write("user data\n")
    before_l = tree(lookalike)
    for extra in ((), ("--print-command",)):
        p = run(lookalike, *extra)
        check(p.returncode == 2 and tree(lookalike) == before_l,
              "a directory of out_* look-alikes, no marker%s: refused (exit %d), every file kept" %
              (" with --print-command" if extra else "", p.returncode))
    open(os.path.join(lookalike, ".npu_run"), "w").close()
    open(os.path.join(lookalike, "out_7.bin"), "wb").close()
    p = run(lookalike, "--print-command")
    after_l = tree(lookalike)
    check(p.returncode == 0 and not os.path.exists(os.path.join(lookalike, "out_7.bin")) and
          all(after_l.get(k) == v for k, v in before_l.items()),
          "with the marker: out_7.bin (the launcher's) removed, every look-alike kept")

    # Review N5 R3: never written through a link to a file outside the output directory.
    outside = os.path.join(work, "outside")
    os.makedirs(outside)

    def target(n):
        t = os.path.join(outside, n)
        with open(t, "w") as f:
            f.write("user-owned %s\n" % n)
        return t

    def unchanged(t):
        with open(t) as f:
            return f.read() == "user-owned %s\n" % os.path.basename(t)

    lm = os.path.join(work, "link_marker")
    os.makedirs(lm)
    t = target("marker_notes.txt")
    os.symlink(t, os.path.join(lm, ".npu_run"))
    for extra in ((), ("--print-command",)):
        p = run(lm, *extra)
        check(p.returncode == 2 and "not write through links" in p.stdout and unchanged(t) and
              os.listdir(lm) == [".npu_run"] and os.path.islink(os.path.join(lm, ".npu_run")),
              "the marker a symbolic link outside%s: refused (exit %d), link and target unchanged" %
              (" with --print-command" if extra else "", p.returncode))
    hm = os.path.join(work, "hard_marker")
    os.makedirs(hm)
    t = target("hard_marker.txt")
    os.link(t, os.path.join(hm, ".npu_run"))
    p = run(hm, "--print-command")
    check(p.returncode == 0 and unchanged(t) and os.stat(t).st_nlink == 1,
          "the marker a hard link: exit 0, a new marker, the other name unchanged")
    ln = os.path.join(work, "link_files")
    os.makedirs(ln)
    open(os.path.join(ln, ".npu_run"), "w").close()
    links = {n: target("backup_" + n) for n in ("cli.txt", "result.bin", "out_3.bin", "summary.txt")}
    for n, t in links.items():
        os.symlink(t, os.path.join(ln, n))
    p = run(ln, "--print-command")
    check(p.returncode == 0 and all(unchanged(t) for t in links.values()) and
          not any(os.path.islink(os.path.join(ln, n)) for n in links) and os.path.isfile(os.path.join(ln, "cli.txt")),
          "links at the launcher's names (cli.txt, result.bin, out_3.bin, summary.txt): links removed, targets unchanged")
    links = {n: target("run_" + n) for n in ("platform.log", "summary.json", "summary.txt", "summary.json.tmp")}
    for n, t in links.items():
        os.symlink(t, os.path.join(ln, n))
    p = subprocess.run([sys.executable, "-B", launcher, "--fx1-soc", fake, "--elf", elf, fx, "--out", ln],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    check(p.returncode == 3 and all(unchanged(t) for t in links.values()) and
          os.path.isfile(os.path.join(ln, "summary.json")) and not os.path.islink(os.path.join(ln, "summary.json")),
          "a run with links at platform.log / summary.*: exit %d, new files, targets unchanged" % p.returncode)

    # A directory holding only the launcher's files (an older run, no marker) is accepted.
    older = os.path.join(work, "older")
    os.makedirs(older)
    for n in ("result.bin", "out_10.bin", "platform.log"):
        open(os.path.join(older, n), "wb").close()
    p = run(older, "--print-command")
    check(p.returncode == 0 and not os.path.exists(os.path.join(older, "result.bin")),
          "a directory with only launcher files (no marker): accepted, stale files removed")

    # R3: inherited model knobs.
    clean_dir, dirty_dir = os.path.join(work, "clean"), os.path.join(work, "dirty")
    p = run(clean_dir)
    c = json.load(open(os.path.join(clean_dir, "summary.json")))
    check(p.returncode == 0 and c["outcome"] == "PASS" and c["model_config"]["env"] == "none", "clean run: PASS, env=none")
    env = dict(os.environ, HAS_DUMMY_KNOB="1", SAURIA_CORE_RTL_PROFILE="legacy", NPU_AXI_DRAM_LAT="500")
    p = run(dirty_dir, env=env)
    d = json.load(open(os.path.join(dirty_dir, "summary.json")))
    removed = set(d.get("env_removed", []))
    check(p.returncode == 0 and d["outcome"] == "PASS" and d["model_config"] == c["model_config"],
          "inherited knobs: PASS with the banner of the clean run (%s)" % d["model_config"])
    check({"HAS_DUMMY_KNOB", "SAURIA_CORE_RTL_PROFILE", "NPU_AXI_DRAM_LAT"} <= removed,
          "summary names the removed knobs: %s" % sorted(removed))
    check(d["result"]["model_exec_cycles"] == c["result"]["model_exec_cycles"] and
          [o["sha256"] for o in d["outputs"]] == [o["sha256"] for o in c["outputs"]],
          "model counters and output bytes as the clean run (exec cycles %d / %d)" %
          (d["result"]["model_exec_cycles"], c["result"]["model_exec_cycles"]))
    p = run(os.path.join(work, "printed2"), "--print-command", env=env)
    check(p.returncode == 0 and "SAURIA_CORE_RTL_PROFILE" in p.stdout and "SAURIA_CORE_RTL_PROFILE=legacy" not in p.stdout,
          "--print-command names the removed knobs and does not pass them")
    # Name tables against the firmware headers (the source tree of the launcher).
    drivers = os.path.join(os.path.dirname(os.path.abspath(launcher)), "..", "..", "..", "fw", "fx1_soc", "drivers")
    spec = importlib.util.spec_from_file_location("npu_run_tables", launcher)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    job_h = open(os.path.join(drivers, "fx1_npu_job.h")).read()
    drv_h = open(os.path.join(drivers, "fx1_npu.h")).read()

    def enum_after(text, first):
        body = text[text.index(first):]
        body = body[:body.index("}")]
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
        return [n for n in re.findall(r"\b(FX1_NPU_\w+)", body) if not n.endswith("_COUNT")]  # not the sentinels
    status = [n[len("FX1_NPU_JOB_"):] for n in enum_after(job_h, "FX1_NPU_JOB_OK = 0")]
    reasons = [n[len("FX1_NPU_JOB_E_"):].lower().replace("_", " ") for n in enum_after(job_h, "FX1_NPU_JOB_E_NONE = 0")]
    driver = [re.sub(r"^FX1_NPU_(E_)?", "", n) for n in enum_after(drv_h, "FX1_NPU_OK = 0")]
    check(mod.STATUS == status, "STATUS = fx1_npu_job_status (%d names)" % len(status))
    check(mod.REASONS == reasons, "REASONS = FX1_NPU_JOB_E_* (%d names)" % len(reasons))
    check(mod.DRIVER == driver, "DRIVER = fx1_npu_status_t (%d names: %s)" % (len(driver), " ".join(driver[-3:])))
    print("RESULT %s (%d failure%s)" % ("FAIL" if failures else "PASS", failures, "" if failures == 1 else "s"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
