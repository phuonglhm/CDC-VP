#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""tools/migrate_profile_upd.py: v1.07 profile -> CSR extension 0x20261109.

    test_migrate_profile.py OUT_DIR

  - BLC_SCALE_Gg (g = 0..3) becomes four per-channel lines with the same
    value, in place, comments, order and other lines unchanged; the result
    names only registers of the generated header;
  - idempotent; --check reports without writing (exit 1 when needed);
  - a nonzero CCM offset is reported and kept; --strict is an error and
    leaves the file unchanged;
  - an unreadable file is a usage/I-O error (exit 2);
  - review B/C/P/D R2: CCM offsets in every profile syntax (legacy, alias,
    group) are decoded by csrw_codec: zero passes, nonzero is reported (or
    refused by --strict) with its signed value; a malformed line is exit 2
    with file:line and no traceback, the file unchanged; a group BLC_SCALE_Gg
    (the removed register) is refused;
  - a UTF-8 byte order mark is kept and the first line still migrates; CRLF
    line ends are kept.
"""

import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
TOOL = os.path.join(ROOT, "tools", "migrate_profile_upd.py")
HEADER = os.path.join(ROOT, "include", "fx1_isp", "fx1_isp_csr.h")
failures = []

V107 = """# v1.07 profile
BLC_CTRL 0x1
BLC_GAIN_SEL 0x101
BLC_SCALE_G1 0x10D25   # one scale for the four colours
  BLC_SCALE_G3	70000
CCM_OFS_R 0x5
CCM_OFS_G 0xFFD
CCM_OFS_B 0x0
LSC_COEF_DATA 0x1FFFFF
"""
WANT = """# v1.07 profile
BLC_CTRL 0x1
BLC_GAIN_SEL 0x101
BLC_SCALE_G1_R 0x10D25   # one scale for the four colours
BLC_SCALE_G1_GR 0x10D25   # one scale for the four colours
BLC_SCALE_G1_GB 0x10D25   # one scale for the four colours
BLC_SCALE_G1_B 0x10D25   # one scale for the four colours
  BLC_SCALE_G3_R	70000
  BLC_SCALE_G3_GR	70000
  BLC_SCALE_G3_GB	70000
  BLC_SCALE_G3_B	70000
CCM_OFS_R 0x5
CCM_OFS_G 0xFFD
CCM_OFS_B 0x0
LSC_COEF_DATA 0x1FFFFF
"""


def check(cond, what):
    if not cond:
        failures.append(what)
        print("FAIL: " + what)


def run(*args):
    return subprocess.run([sys.executable, "-B", TOOL] + list(args), stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True)


def main():
    root = os.path.join(sys.argv[1], "migrate profile")
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(root)
    p = os.path.join(root, "p.csrw")
    open(p, "w").write(V107)

    r = run("--check", p)
    check(r.returncode == 1 and "2 BLC_SCALE_Gg" in r.stdout and open(p).read() == V107, "--check: " + r.stdout)
    r = run("--strict", p)
    check(r.returncode == 1 and "CCM_OFS_R 5" in r.stdout and "CCM_OFS_G -3" in r.stdout and open(p).read() == V107,
          "--strict leaves the file: " + r.stdout)
    r = run(p)
    check(r.returncode == 0 and "+6 writes" in r.stdout and "kept" in r.stdout, "migrate: " + r.stdout)
    got = open(p).read()
    check(got == WANT, "migrated text:\n" + got)
    names = set(re.findall(r"^#define FX1_ISP_(\w+)_OFFSET ", open(HEADER).read(), re.M))
    used = {l.split()[0] for l in got.splitlines() if l.strip() and not l.strip().startswith("#")}
    check(used <= names, "names outside the header: %s" % sorted(used - names))
    check(not any(re.fullmatch(r"BLC_SCALE_G[0-3]", n) for n in names), "the header still has BLC_SCALE_Gg")
    r = run(p)
    check(r.returncode == 0 and open(p).read() == WANT and "per-channel" not in r.stdout, "idempotent: " + r.stdout)
    r = run("--check", p)
    check(r.returncode == 0, "--check after migration: exit %d" % r.returncode)
    r = run(os.path.join(root, "missing.csrw"))
    check(r.returncode == 2, "missing file: exit %d" % r.returncode)

    # Review B/C/P/D R2: every syntax of the CCM offsets.
    def file(name, text, mode="w"):
        q = os.path.join(root, name)
        with open(q, mode, **({} if "b" in mode else {"newline": ""})) as f:
            f.write(text)
        return q

    for name, text in (("group zero", "CCM_OFS_R offset_r=0\n"), ("alias zero", "CCM_OFS_G_offset_g 0\n"),
                       ("legacy zero", "CCM_OFS_B 0x0\n")):
        q = file(name.replace(" ", "_") + ".csrw", text)
        for args in ((), ("--strict",), ("--check",)):
            r = run(*(args + (q,)))
            check(r.returncode == 0 and r.stdout == "" and open(q).read() == text,
                  "%s %s: exit %d %r" % (name, args, r.returncode, r.stdout))
    nz = "CCM_OFS_R offset_r=0x200  # +1.0 before\nCCM_OFS_G_offset_g 0xFFD\nCCM_OFS_B 0x1\nBLC_SCALE_G0 0x100\n"
    q = file("nonzero.csrw", nz)
    r = run("--strict", q)
    check(r.returncode == 1 and open(q).read() == nz and "nonzero.csrw:1: CCM_OFS_R 512:" in r.stdout
          and "nonzero.csrw:2: CCM_OFS_G -3:" in r.stdout and "nonzero.csrw:3: CCM_OFS_B 1:" in r.stdout
          and "Traceback" not in r.stdout, "group/alias/legacy nonzero --strict: " + r.stdout)
    r = run(q)
    check(r.returncode == 0 and r.stdout.count("; kept") == 3 and
          open(q).read() == nz.replace("BLC_SCALE_G0 0x100\n", "".join("BLC_SCALE_G0_%s 0x100\n" % c
                                                                       for c in ("R", "GR", "GB", "B"))),
          "nonzero kept, BLC migrated: " + r.stdout)
    for name, text, msg in (("too wide", "CCM_OFS_R offset_r=0x1000\n", ":1: CCM_OFS_R.offset_r = 0x1000 does not fit"),
                            ("unknown field", "BLC_SCALE_G2 0x100\nCCM_OFS_R bogus=1\n", ":2: CCM_OFS_R has no field"),
                            ("negative", "CCM_OFS_G -3\n", ":1:"),
                            ("unknown register", "CCM_OFS_X 0\n", ":1: unknown register or field CCM_OFS_X"),
                            ("BLC group", "BLC_SCALE_G0 scale=1\n", ":1: the removed register BLC_SCALE_G0")):
        q = file("err_" + name.replace(" ", "_") + ".csrw", text)
        r = run(q)
        check(r.returncode == 2 and msg in r.stdout and "Traceback" not in r.stdout and open(q).read() == text,
              "%s: exit %d %r" % (name, r.returncode, r.stdout))
    # A byte order mark and CRLF line ends are kept.
    q = file("bom.csrw", b"\xef\xbb\xbfBLC_SCALE_G1 0x100 # c\r\nCCM_OFS_R offset_r=0\r\n", "wb")
    r = run(q)
    want = b"\xef\xbb\xbf" + b"".join(b"BLC_SCALE_G1_%s 0x100 # c\r\n" % c for c in (b"R", b"GR", b"GB", b"B")) + \
        b"CCM_OFS_R offset_r=0\r\n"
    check(r.returncode == 0 and open(q, "rb").read() == want, "BOM/CRLF: %r %r" % (r.stdout, open(q, "rb").read()))

    print("test_migrate_profile: %d failure(s)" % len(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
