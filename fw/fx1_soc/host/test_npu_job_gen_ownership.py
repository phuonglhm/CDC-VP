#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""fx1_unit_npu_job_gen_ownership: npu_job_gen only replaces what it owns (review N5 R1, R3).

  test_npu_job_gen_ownership.py <npu_job_gen> <reference fixtures dir> <work dir>

Cases (each in its own directory under <work dir>):
  - a new directory: every fixture, byte for byte the reference, plus the marker;
  - a second run into it, with a user note at its root and in a fixture directory,
    a user fixture directory, and a stale golden_99.bin: notes and user fixture
    kept, the stale golden gone, the fixtures again byte for byte the reference;
  - a foreign non-empty directory (no marker): exit 2, nothing written or removed;
  - <out> a regular file, and a fixture path that is a regular file in a marked
    directory: exit 2, nothing written or removed;
  - links to targets outside <out> (R3), in a directory the tool made: a symbolic
    link at job.bin, at fixture.json, a dangling one at a golden name, at the
    marker, and a fixture directory that is a link: exit 2, nothing written,
    the link and its target unchanged (a dangling target not created); a hard
    link at job.bin and at the marker: exit 0, the other name keeps its content.
"""
import hashlib
import os
import shutil
import subprocess
import sys

failures = 0
MARKER = ".npu_job_gen"


def check(ok, what):
    global failures
    print("  [%s] %s" % (" ok " if ok else "FAIL", what))
    if not ok:
        failures += 1


def tree(d):
    """{relative path: sha256, or 'link -> target'}; links are recorded, not followed."""
    out = {}
    for root, dirs, files in os.walk(d):
        for f in files + [x for x in dirs if os.path.islink(os.path.join(root, x))]:
            p = os.path.join(root, f)
            if os.path.islink(p):
                out[os.path.relpath(p, d)] = "link -> " + os.readlink(p)
                continue
            with open(p, "rb") as fh:
                out[os.path.relpath(p, d)] = hashlib.sha256(fh.read()).hexdigest()
    return out


def read(p):
    with open(p, "rb") as f:
        return f.read().decode("latin-1")


def same_as_ref(out, ref):
    """Every file of ref (but .stamp / the marker) equal in out."""
    r = {k: v for k, v in tree(ref).items() if os.path.basename(k) not in (".stamp", MARKER)}
    o = tree(out)
    return all(o.get(k) == v for k, v in r.items()), len(r)


def main():
    gen, ref, work = sys.argv[1:4]
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    run = lambda out: subprocess.run([gen, out], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    fresh = os.path.join(work, "fresh")
    p = run(fresh)
    ok, n = same_as_ref(fresh, ref)
    check(p.returncode == 0 and ok and os.path.isfile(os.path.join(fresh, MARKER)),
          "new directory: %d files byte for byte the reference, marker written" % n)

    note_root = os.path.join(fresh, "my_notes.txt")
    note_fx = os.path.join(fresh, "chain", "tuning_notes.txt")
    mine = os.path.join(fresh, "my_job")
    stale = os.path.join(fresh, "chain", "golden_99.bin")
    for path, text in ((note_root, "mine\n"), (note_fx, "mine too\n"), (os.path.join(mine, "job.bin"), "my job\n"),
                       (stale, "stale\n")):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)
    p = run(fresh)
    ok, _ = same_as_ref(fresh, ref)
    kept = all(os.path.isfile(x) for x in (note_root, note_fx, os.path.join(mine, "job.bin")))
    check(p.returncode == 0 and ok, "second run into its own directory: fixtures again byte for byte the reference")
    check(kept and open(note_root).read() == "mine\n" and open(os.path.join(mine, "job.bin")).read() == "my job\n",
          "user notes (root, inside a fixture) and a user fixture directory kept")
    check(not os.path.exists(stale), "a stale golden_<n>.bin of the tool is removed")

    foreign = os.path.join(work, "foreign")
    os.makedirs(os.path.join(foreign, "chain"))
    for rel in ("user_notes.txt", "chain/job.bin"):
        with open(os.path.join(foreign, rel), "w") as f:
            f.write("user's\n")
    before = tree(foreign)
    p = run(foreign)
    check(p.returncode == 2 and "not made by npu_job_gen" in p.stdout and tree(foreign) == before,
          "foreign non-empty directory: exit 2, nothing written or removed (%s)" % p.stdout.strip()[:80])

    afile = os.path.join(work, "a_file")
    with open(afile, "w") as f:
        f.write("x\n")
    p = run(afile)
    check(p.returncode == 2 and open(afile).read() == "x\n", "output path is a file: exit 2, untouched")

    marked = os.path.join(work, "marked")
    os.makedirs(marked)
    shutil.copy(os.path.join(fresh, MARKER), marked)
    with open(os.path.join(marked, "chain"), "w") as f:
        f.write("a file named like a fixture\n")
    before = tree(marked)
    p = run(marked)
    check(p.returncode == 2 and tree(marked) == before and not os.path.exists(os.path.join(marked, "gemm")),
          "a fixture path that is a file: exit 2 before any write")

    # Review N5 R3: links to targets outside the output directory.
    outside = os.path.join(work, "outside")
    os.makedirs(outside)
    n = [0]

    def marked_dir():
        n[0] += 1
        d = os.path.join(work, "links%d" % n[0])
        p = run(d)
        if p.returncode != 0:
            print(p.stdout)
        return d

    def target(name, text):
        t = os.path.join(outside, name)
        with open(t, "w") as f:
            f.write(text)
        return t

    def refused_link(rel, tgt, what, make_target=True):
        d = marked_dir()
        text = "user-owned %s outside the output\n" % rel
        if make_target:
            t = target(tgt, text)
        else:
            t = os.path.join(outside, tgt)
        link = os.path.join(d, rel)
        if os.path.lexists(link):
            os.unlink(link)
        os.symlink(t, link)
        before = tree(d)
        p = run(d)
        tgt_ok = read(t) == text if make_target else not os.path.lexists(t)
        check(p.returncode == 2 and "not write through links" in p.stdout and tree(d) == before and tgt_ok
              and os.path.islink(link), "%s: exit 2, nothing written, link and target unchanged (%s)"
              % (what, p.stdout.strip().splitlines()[-1][:90] if p.stdout.strip() else "no output"))

    refused_link("chain/job.bin", "job_backup.bin", "chain/job.bin a symbolic link outside")
    refused_link("gemm/fixture.json", "fixture_backup.json", "gemm/fixture.json (a text output) a symbolic link outside")
    refused_link("chain/golden_7.bin", "never_created.bin", "a dangling symbolic link at a golden name", make_target=False)
    refused_link(MARKER, "marker_notes.txt", "the marker a symbolic link outside")

    d = marked_dir()
    tdir = os.path.join(outside, "chain_dir")
    shutil.copytree(os.path.join(d, "chain"), tdir)
    shutil.rmtree(os.path.join(d, "chain"))
    os.symlink(tdir, os.path.join(d, "chain"))
    t_before, before = tree(tdir), tree(d)
    p = run(d)
    check(p.returncode == 2 and tree(tdir) == t_before and tree(d) == before,
          "a fixture directory that is a symbolic link: exit 2, nothing written in it")

    for rel, what in (("chain/job.bin", "chain/job.bin"), (MARKER, "the marker")):
        d = marked_dir()
        t = target("hard_" + os.path.basename(rel), "user-owned, hard-linked\n")
        os.unlink(os.path.join(d, rel))
        os.link(t, os.path.join(d, rel))
        p = run(d)
        ok, _ = same_as_ref(d, ref)
        check(p.returncode == 0 and ok and read(t) == "user-owned, hard-linked\n"
              and os.stat(t).st_nlink == 1, "%s a hard link: exit 0, a new file, the other name unchanged" % what)

    print("RESULT %s (%d failure%s)" % ("FAIL" if failures else "PASS", failures, "" if failures == 1 else "s"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
