#!/usr/bin/env python3
"""Host check of fx1_stress_freertos_cli_eth_camera (cli_eth_camera.txt): the two camera
sessions under `eth load` background traffic.

  check_cli_eth_camera.py --golden camera_isp_golden.h CONSOLE_LOG
  check_cli_eth_camera.py --golden camera_isp_golden.h --selftest

Under the Ethernet load the CSI-2 receiver may drop sensor frames (cmd_camera.c: a CPU
that does not serve the sides in time makes the receiver drop; the capture takes the
next N frames and counts the drops). Which sensor frames a capture gets depends on
firmware timing, which code layout changes (DDR3 timing is address dependent): a fixed
frame number in the expectation tests timing, not correctness. This check requires,
for each session (RAW12 4 lanes, `camera capture 4`; RAW10 2 lanes, `camera capture 2`):

  - `camera start: PASS` (EDEV or any start failure fails), and exactly N capture lines
    before `camera capture: N/N frames, drops D` and `camera capture: PASS`;
  - every frame: RAW `raw ok`, FRAME_ID == the RAW frame number (pixel (0,0)) == the
    golden frame named, and its NV12 CRC equal to the golden header's CRC of that frame
    number for the session's depth (checked here, not only by the firmware);
  - FRAME_IDs strictly increasing (no duplicate, no reorder), ISP seq strictly
    increasing; the 64x32 geometry;
and of the run: `eth load: ... payload errors 0` + PASS, `eth cycle: 5/5`, `poweroff: 0
failed`, `RESULT PASS`, no `camera start: FAIL` anywhere. Exit 0 when it holds.
"""
import argparse
import re
import sys

CAPTURE = re.compile(r"^\s*capture (\d+) FRAME_ID (\d+) side (\d) isp seq (\d+) (zero-copy|staged) out (\d+) "
                     r"(\d+)x(\d+) raw frame (\d+) raw (ok|\S+) crc 0x([0-9a-f]+) = golden frame (\d+)\s*$")
ANY_CAPTURE = re.compile(r"^\s*capture \d+ FRAME_ID")
SESSIONS = (("RAW12", 4, "raw12"), ("RAW10", 2, "raw10"))


def golden_crcs(path):
    text = open(path).read()
    out = {}
    for key in ("raw12", "raw10"):
        m = re.search(r"cam_isp_crc_%s\[(\d+)\][^=]*=\s*\{([^}]*)\}" % key, text)
        if not m:
            raise ValueError("%s: no cam_isp_crc_%s" % (path, key))
        vals = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)", m.group(2))]
        if len(vals) != int(m.group(1)):
            raise ValueError("%s: cam_isp_crc_%s has %d values, declared %s" % (path, key, len(vals), m.group(1)))
        out[key] = vals
    return out


def check(text, golden):
    """The list of problems ([] when the log passes)."""
    bad = []
    lines = text.splitlines()
    if any("camera start: FAIL" in l or re.search(r"camera start: E[A-Z]+", l) for l in lines):
        bad.append("a camera start failed: " + next(l.strip() for l in lines if "camera start: E" in l or "camera start: FAIL" in l))
    # Session k: from its `camera capture N` command to its summary line.
    starts = [i for i, l in enumerate(lines) if re.search(r"camera capture \d+\s*$", l)]
    if len(starts) != len(SESSIONS):
        bad.append("%d `camera capture` command(s), expected %d" % (len(starts), len(SESSIONS)))
        return bad
    for (depth, n, key), s in zip(SESSIONS, starts):
        want_cmd = "camera capture %d" % n
        if not lines[s].rstrip().endswith(want_cmd):
            bad.append("%s session: command %r, expected %r" % (depth, lines[s].strip(), want_cmd))
            continue
        end = next((i for i in range(s + 1, len(lines)) if lines[i].startswith("camera capture:")), None)
        if end is None:
            bad.append("%s session: no `camera capture:` summary" % depth)
            continue
        summary = re.match(r"camera capture: (\d+)/(\d+) frames, drops (\d+)", lines[end])
        if not summary or (int(summary.group(1)), int(summary.group(2))) != (n, n):
            bad.append("%s session: %r, expected %d/%d frames" % (depth, lines[end].strip(), n, n))
        if end + 1 >= len(lines) or lines[end + 1].strip() != "camera capture: PASS":
            bad.append("%s session: no `camera capture: PASS` after the summary" % depth)
        body = [l for l in lines[s + 1:end] if ANY_CAPTURE.match(l)]
        if len(body) != n:
            bad.append("%s session: %d capture line(s), expected %d" % (depth, len(body), n))
        ids, seqs = [], []
        for l in body:
            m = CAPTURE.match(l)
            if not m:
                bad.append("%s session: capture line not as expected: %r" % (depth, l.strip()))
                continue
            fid, seq, w, h, raw_n, raw, crc, gold = (int(m.group(2)), int(m.group(4)), int(m.group(7)), int(m.group(8)),
                                                    int(m.group(9)), m.group(10), int(m.group(11), 16), int(m.group(12)))
            ids.append(fid)
            seqs.append(seq)
            if (w, h) != (64, 32):
                bad.append("%s FRAME_ID %d: %dx%d, expected 64x32" % (depth, fid, w, h))
            if raw != "ok":
                bad.append("%s FRAME_ID %d: raw %s" % (depth, fid, raw))
            if not (fid == raw_n == gold):
                bad.append("%s FRAME_ID %d: raw frame %d, golden frame %d (must all agree)" % (depth, fid, raw_n, gold))
            if not 1 <= gold <= len(golden[key]):
                bad.append("%s FRAME_ID %d: golden frame %d outside 1..%d" % (depth, fid, gold, len(golden[key])))
            elif crc != golden[key][gold - 1]:
                bad.append("%s FRAME_ID %d: crc 0x%08x, golden header 0x%08x" % (depth, fid, crc, golden[key][gold - 1]))
        if any(b <= a for a, b in zip(ids, ids[1:])):
            bad.append("%s session: FRAME_IDs %s not strictly increasing (duplicate or reorder)" % (depth, ids))
        if any(b <= a for a, b in zip(seqs, seqs[1:])):
            bad.append("%s session: ISP seq %s not strictly increasing" % (depth, seqs))
        print("%s: FRAME_IDs %s, %s" % (depth, ids, lines[end].strip()))
    text1 = "\n".join(lines)
    for what, pat in (("eth load with payload errors 0", r"eth load: sent [1-9][0-9]*, received [1-9][0-9]*, payload errors 0"),
                      ("eth load: PASS", r"eth load: PASS"), ("eth cycle: 5/5", r"eth cycle: 5/5"),
                      ("poweroff: 0 failed", r"poweroff: 0 failed"), ("RESULT PASS", r"RESULT PASS")):
        if not re.search(pat, text1):
            bad.append("missing: " + what)
    return bad


# ---- self test: the check must fail on each broken log --------------------------------------
def _good(golden):
    def cap(i, fid, side, seq, mode, depth):
        return ("  capture %d FRAME_ID %d side %d isp seq %d %s out %d 64x32 raw frame %d raw ok crc 0x%08x = golden frame %d"
                % (i, fid, side, seq, mode, seq % 4, fid, golden[depth][fid - 1], fid))
    s1 = [cap(1, 1, 0, 0, "zero-copy", "raw12"), cap(3, 2, 1, 1, "zero-copy", "raw12"),
          cap(2, 3, 0, 2, "zero-copy", "raw12"), cap(5, 5, 1, 3, "zero-copy", "raw12")]
    s2 = [cap(7, 12, 0, 6, "staged", "raw10"), cap(9, 13, 1, 7, "staged", "raw10")]
    return "\n".join(["root@fx1:~# eth load start", "eth load: started", "root@fx1:~# camera start", "camera start: OK",
                      "  camera start: PASS", "root@fx1:~# camera capture 4"] + s1 +
                     ["camera capture: 4/4 frames, drops 1", "  camera capture: PASS", "root@fx1:~# camera start",
                      "camera start: OK", "  camera start: PASS", "root@fx1:~# camera capture 2"] + s2 +
                     ["camera capture: 2/2 frames, drops 1", "  camera capture: PASS",
                      "eth load: sent 18, received 18, payload errors 0, refused 0", "  eth load: PASS",
                      "eth cycle: 5/5 stop/start cycles in 5 ms", "poweroff: 0 failed check(s)", "[fx1_soc] RESULT PASS"])


def selftest(golden):
    good = _good(golden)
    crc5 = "0x%08x = golden frame 5" % golden["raw12"][4]
    cases = [
        ("wrong golden CRC", good.replace(crc5, "0x%08x = golden frame 5" % (golden["raw12"][4] ^ 1))),
        ("golden frame != FRAME_ID", good.replace("raw frame 5 raw ok crc %s" % crc5,
                                                   "raw frame 5 raw ok crc 0x%08x = golden frame 4" % golden["raw12"][3])),
        ("raw not ok", good.replace("raw frame 3 raw ok", "raw frame 3 raw bad")),
        ("missing frame (3/4)", good.replace("camera capture: 4/4 frames", "camera capture: 3/4 frames")),
        ("a capture line missing", "\n".join(l for l in good.splitlines() if "FRAME_ID 2 " not in l)),
        ("duplicate FRAME_ID", good.replace("FRAME_ID 3 side 0 isp seq 2 zero-copy out 2 64x32 raw frame 3 raw ok crc 0x%08x = golden frame 3"
                                             % golden["raw12"][2],
                                             "FRAME_ID 2 side 0 isp seq 2 zero-copy out 2 64x32 raw frame 2 raw ok crc 0x%08x = golden frame 2"
                                             % golden["raw12"][1])),
        ("camera start EDEV", good.replace("camera start: OK\n  camera start: PASS\nroot@fx1:~# camera capture 2",
                                           "camera start: EDEV\n  camera start: FAIL\nroot@fx1:~# camera capture 2")),
        ("payload errors", good.replace("payload errors 0", "payload errors 1")),
        ("no RESULT PASS", good.replace("RESULT PASS", "RESULT FAIL")),
    ]
    ok = not check(good, golden)
    print(("ok   " if ok else "FAIL ") + "a good log (frame 4 dropped, frames 1,2,3,5 + 12,13) passes")
    for name, text in cases:
        assert text != good, name
        problems = check(text, golden)
        print(("ok   " if problems else "FAIL ") + "%s fails%s" % (name, (": " + problems[0]) if problems else ""))
        ok = ok and bool(problems)
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--golden", required=True, help="fw/fx1_soc/tests/camera_isp/camera_isp_golden.h")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("log", nargs="?")
    a = ap.parse_args()
    golden = golden_crcs(a.golden)
    if a.selftest:
        return selftest(golden)
    if not a.log:
        ap.error("a console log, or --selftest")
    problems = check(open(a.log, errors="replace").read(), golden)
    for p in problems:
        print("FAIL " + p)
    print("check_cli_eth_camera: %s" % ("PASS" if not problems else "%d problem(s)" % len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
