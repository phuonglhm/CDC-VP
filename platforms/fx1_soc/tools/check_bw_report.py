#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check an fx1_soc --bw-report event log (plan CSI_ISP_BW1, schema fx1-bw/1).

  check_bw_report.py check REPORT [--dump-dir DIR] [--uart-log LOG]
                     [--golden camera_isp_golden.h] [--direct-only] [--min-good N]
                     [--expect-dma-errors MASTER:N ...] [--expect-stall MASTER:N:US]
                     [--console CONSOLE]
      The report is complete (header first, footer last, complete=1); every
      DMA issue has exactly one response, not earlier; with --direct-only no
      MIPI_DMA write and no ISP_IDMA read was issued. Every good capture
      (CSI outcome ok, stream boundary, ISP output DONE, retired OK) has:
      received RAW payload = W*H*bits/8 (bits from its data type), AXI4-Stream
      useful bytes = 2*W*H with H TLAST beats and one SOF beat, ODMA bytes =
      1.5*W*H, first write issue < last write response <= DONE, the last write
      response equal to the latest ODMA response time of its planes in the DMA
      log; with --dump-dir its NV12 dump exists, has 1.5*W*H bytes and, when
      the UART log names its FRAME_ID with a CRC, the same CRC-32. The ODMA
      writes of a good output (issued between its first write and its end,
      each transaction wholly inside one of its planes; one starting in a
      plane and running past it fails) add up to its record's bytes and to
      1.5*W*H, their count is the record's, every response is OK. DMA error
      responses: none, unless --expect-dma-errors gives the exact count per
      master (review BW1 F2/F3). With --console (run_expect.cmake appends it as
      the last argument) and an ISP fault shim line, the shim saw exactly the
      master's transactions in the report, and the fired one is in the report
      at its address and its issue time (the counter is the master's first
      hop); --expect-stall MASTER:N:US also requires its response at least US
      after its issue. With
      --golden, the CRC-32 of the golden table of its bits for its FRAME_ID
      (the sensor's frame number since its reset, frames the table covers).
      At least --min-good good captures (default 1).

  check_bw_report.py selftest REPORT [check options]
      Sensitivity of `check` on a clean report: it must PASS, and each of
      these copies must FAIL: footer removed, one DMA response removed, every
      ODMA issue's bytes set to 1, every ODMA response set to an error, one
      ODMA write moved to run past the end of its plane, one capture's AXIS
      bytes short by 8, and (with --dump-dir) one NV12 byte flipped.

  check_bw_report.py same-run UART_A UART_B CONSOLE_A CONSOLE_B
      Two runs of the same stimulus (observers on / off): identical UART logs
      and the same [sim-control] verdict line (simulated time included).

Expected values come from the geometry and from the firmware's own CRC lines,
not from the recorder. Exit 0 when every check holds, 1 otherwise (all
failures listed), 2 for bad usage.
"""
import json
import re
import sys
import zlib
from pathlib import Path

DT_BITS = {0x2B: 10, 0x2C: 12}


def load(path):
    events = []
    for n, line in enumerate(Path(path).read_text().splitlines(), 1):
        if not line.strip():
            continue
        try:
            events.append(json.loads(line))
        except json.JSONDecodeError as e:
            raise SystemExit(f"FAIL {path}:{n}: not JSON ({e})")
    return events


def check(args):
    report = args[0]
    opts = {"dump": None, "uart": None, "direct": False, "min_good": 1, "golden": None, "errors": {},
            "stall": None, "console": None}
    i = 1
    while i < len(args):
        a = args[i]
        if a == "--dump-dir":
            opts["dump"], i = args[i + 1], i + 2
        elif a == "--uart-log":
            opts["uart"], i = args[i + 1], i + 2
        elif a == "--golden":
            opts["golden"], i = args[i + 1], i + 2
        elif a == "--direct-only":
            opts["direct"], i = True, i + 1
        elif a == "--min-good":
            opts["min_good"], i = int(args[i + 1]), i + 2
        elif a == "--expect-dma-errors":
            m, _, n = args[i + 1].partition(":")
            opts["errors"][m], i = int(n), i + 2
        elif a == "--expect-stall":
            m, n, us = args[i + 1].split(":")
            opts["stall"], i = (m, int(n), int(us)), i + 2
        elif a == "--console":
            opts["console"], i = args[i + 1], i + 2
        elif not a.startswith("--") and i == len(args) - 1:
            opts["console"], i = a, i + 1  # run_expect.cmake appends the console log
        else:
            raise SystemExit(2)
    ev = load(report)
    bad = []
    if not ev or ev[0].get("ev") != "header" or ev[0].get("schema") != "fx1-bw/1":
        bad.append("no fx1-bw/1 header on the first line")
    if not ev or ev[-1].get("ev") != "footer" or ev[-1].get("complete") != 1:
        bad.append("incomplete report: no footer with complete=1 on the last line")
    if bad:
        return bad, 0

    # DMA: pairing, order, direct-only.
    issues, resps = {}, {}
    for e in ev:
        if e["ev"] != "dma":
            continue
        k = (e["m"], e["id"])
        tgt = issues if e["ph"] == "issue" else resps
        if k in tgt:
            bad.append(f"DMA {k} {e['ph']} reported twice")
        tgt[k] = e
    for k, i_ in issues.items():
        r = resps.get(k)
        if r is None:
            bad.append(f"DMA {k} issued, no response")
        elif r["t"] < i_["t"]:
            bad.append(f"DMA {k} response at {r['t']} before its issue at {i_['t']}")
    for k in resps:
        if k not in issues:
            bad.append(f"DMA {k} response without an issue")
    res_fs = ev[0].get("time_resolution_fs", 1)
    # DMA status: no error response unless expected, exactly as many as expected.
    for m in ("MIPI_DMA", "ISP_IDMA", "ISP_ODMA"):
        n_err = sum(1 for (mm, _), r in resps.items() if mm == m and r.get("ok") != 1)
        want = opts["errors"].get(m, 0)
        if n_err != want:
            bad.append(f"{m}: {n_err} DMA error response(s), expected {want}")
    # The ISP fault shim's own report (console): it sits after the counter.
    if opts["console"] and Path(opts["console"]).exists():
        text = Path(opts["console"]).read_text(errors="replace")
        f = re.search(r"isp dma fault \(injection\): (idma-error|odma-error|idma-stall):(\d+)(?::\d+)?, (\d+) "
                      r"(IDMA read|ODMA write)\(s\) seen, (?:fired at #\d+ address (0x[0-9a-f]+) at (\d+) ns|not reached)",
                      text)
        if f:
            m = "ISP_IDMA" if f.group(4) == "IDMA read" else "ISP_ODMA"
            want_w = 1 if m == "ISP_ODMA" else 0
            mine = sorted((i_ for (mm, _), i_ in issues.items() if mm == m and i_["w"] == want_w), key=lambda x: x["id"])
            if int(f.group(3)) != len(mine):
                bad.append(f"{m}: the fault shim saw {f.group(3)} transaction(s), the report has {len(mine)} "
                           f"(lost before the counter)")
            nth = int(f.group(2))
            if f.group(5) and len(mine) >= nth:
                x = mine[nth - 1]
                t_ns = x["t"] * res_fs // 1000000
                if int(x["a"], 16) != int(f.group(5), 16) or t_ns != int(f.group(6)):
                    bad.append(f"{m}: transaction #{nth} in the report at {x['a']} {t_ns} ns, the shim fired at "
                               f"{f.group(5)} {f.group(6)} ns (issue time not the master's)")
                r = resps.get((m, x["id"]))
                if f.group(1).endswith("error") and (r is None or r.get("ok") != 0):
                    bad.append(f"{m}: the injected error of #{nth} is not an error response in the report")
    if opts["stall"]:
        m, nth, us = opts["stall"]
        mine = sorted((i_ for (mm, _), i_ in issues.items() if mm == m), key=lambda x: x["id"])
        if len(mine) < nth:
            bad.append(f"{m}: stall #{nth} expected, {len(mine)} transaction(s) in the report")
        else:
            x = mine[nth - 1]
            r = resps.get((m, x["id"]))
            held = (r["t"] - x["t"]) * res_fs * 1e-9 if r else 0
            if r is None or held < us:
                bad.append(f"{m}: #{nth} held {held:.3f} us between issue and response, at least {us} us expected")
    if opts["direct"]:
        mipi_w = sum(i_["n"] for (m, _), i_ in issues.items() if m == "MIPI_DMA" and i_["w"])
        idma_r = sum(i_["n"] for (m, _), i_ in issues.items() if m == "ISP_IDMA" and not i_["w"])
        if mipi_w:
            bad.append(f"direct-only: MIPI_DMA wrote {mipi_w} bytes")
        if idma_r:
            bad.append(f"direct-only: ISP_IDMA read {idma_r} bytes")

    # Captures.
    by = {}
    def cap(seq):
        return by.setdefault(seq, {})
    for e in ev:
        t = e["ev"]
        if t == "capture_start":
            cap(e["seq"])["start"] = e
        elif t == "capture_end":
            cap(e["seq"])["end"] = e
        elif t == "capture_stream_end":
            cap(e["seq"])["boundary"] = e
        elif t == "axis_capture":
            cap(e["seq"])["axis"] = e
        elif t == "rx_frame" and e["seq"]:
            cap(e["seq"])["rx"] = e
        elif t == "isp_admit":
            cap(e["seq"])["admit"] = e
        elif t == "output" and e["direct"] and e["seq"]:
            cap(e["seq"]).setdefault("outputs", []).append(e)
        elif t == "retire" and e["direct"] and e["seq"]:
            cap(e["seq"])["retire"] = e

    uart_crc = {}
    if opts["uart"]:
        for line in Path(opts["uart"]).read_text(errors="replace").splitlines():
            m = re.search(r"FRAME_ID (\d+) .* crc (0x[0-9a-f]+) = golden", line)
            if m:
                uart_crc[int(m.group(1))] = int(m.group(2), 16)

    golden = {}
    if opts["golden"]:
        text = Path(opts["golden"]).read_text()
        for bits in (12, 10):
            m = re.search(r"cam_isp_crc_raw%d\[\d+\][^{]*\{([^}]*)\}" % bits, text)
            if not m:
                bad.append(f"golden table of RAW{bits} not found in {opts['golden']}")
                continue
            golden[bits] = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]+)", m.group(1))]
    odma = [(issues[k], resps.get(k)) for k in issues if k[0] == "ISP_ODMA"]
    good = 0
    crc_checked = 0
    for seq, c in sorted(by.items()):
        outs = [o for o in c.get("outputs", []) if o["how"] == "done"]
        if not (c.get("end", {}).get("outcome") == "ok" and "boundary" in c and outs and
                c.get("retire", {}).get("ok") == 1):
            continue
        good += 1
        o = outs[0]
        w, h = o["w"], o["h"]
        p = w * h
        rx, ax = c.get("rx"), c.get("axis")
        if rx is None:
            bad.append(f"seq {seq}: no rx_frame")
        else:
            bits = DT_BITS.get(int(rx["dt"], 16))
            if bits is None:
                bad.append(f"seq {seq}: data type {rx['dt']} not RAW10/12")
            elif rx["image_bytes"] != p * bits // 8:
                bad.append(f"seq {seq}: received RAW{bits} payload {rx['image_bytes']} != {p * bits // 8}")
            if rx["crc_bad"] or rx["truncated"] or rx["unclassified"] or rx["not_forwarded_bytes"]:
                bad.append(f"seq {seq}: good capture with packet errors {rx}")
        if ax is None:
            bad.append(f"seq {seq}: no axis_capture")
        else:
            if ax["useful_bytes"] != 2 * p:
                bad.append(f"seq {seq}: AXIS useful bytes {ax['useful_bytes']} != {2 * p}")
            if ax["tlast_beats"] != h or ax["sof_beats"] != 1 or ax["abort_beats"] or ax["error_beats"]:
                bad.append(f"seq {seq}: AXIS beats tlast {ax['tlast_beats']} sof {ax['sof_beats']} "
                           f"abort {ax['abort_beats']} error {ax['error_beats']}")
            if ax["accepted_beats"] != ((w + 3) // 4) * h:
                bad.append(f"seq {seq}: AXIS accepted beats {ax['accepted_beats']} != {((w + 3) // 4) * h}")
        if o["write_bytes"] != p * 3 // 2:
            bad.append(f"seq {seq}: ODMA bytes {o['write_bytes']} != {p * 3 // 2}")
        tf, tl, td = o.get("t_first_write_issue"), o.get("t_last_write_response"), o["t_end"]
        if tf is None or not (tf < tl <= td):
            bad.append(f"seq {seq}: output times first {tf} last {tl} done {td} not ordered")
        else:
            # This output's ODMA writes in the DMA log: issued between its first
            # write and its end, starting in one of its planes (review BW1 F3).
            spans = [(int(o["y_base"], 16), o["y_stride"] * h), (int(o["uv_base"], 16), o["uv_stride"] * (h // 2))]
            last = None
            n_w = b_w = 0
            for i_, r in odma:
                a = int(i_["a"], 16)
                if not (i_["w"] and tf <= i_["t"] <= td):
                    continue
                plane = next(((lo, n) for lo, n in spans if lo <= a < lo + n), None)
                if plane is None:
                    continue
                n_w += 1
                b_w += i_["n"]
                if a + i_["n"] > plane[0] + plane[1]:
                    bad.append(f"seq {seq}: ODMA write {i_['a']} + {i_['n']} runs past its plane")
                if r is None or r.get("ok") != 1:
                    bad.append(f"seq {seq}: ODMA write {i_['a']} of a good output without an OK response")
                if r:
                    last = r["t"] if last is None or r["t"] > last else last
            if last != tl:
                bad.append(f"seq {seq}: last write response {tl} != DMA log {last}")
            if n_w != o["writes"]:
                bad.append(f"seq {seq}: {o['writes']} writes reported, {n_w} in the DMA log")
            if b_w != o["write_bytes"] or b_w != p * 3 // 2:
                bad.append(f"seq {seq}: ODMA bytes in the DMA log {b_w}, record {o['write_bytes']}, "
                           f"active NV12 {p * 3 // 2}")
        adm = c.get("admit")
        if adm is None or adm["tag"] != o["tag"] or c["retire"]["tag"] != o["tag"]:
            bad.append(f"seq {seq}: TAG not consistent across admission / output / retirement")
        if opts["dump"]:
            if "dump" not in o:
                bad.append(f"seq {seq}: no NV12 dump")
            else:
                f = Path(opts["dump"]) / o["dump"]
                data = f.read_bytes() if f.exists() else b""
                if len(data) != p * 3 // 2:
                    bad.append(f"seq {seq}: dump {o['dump']} has {len(data)} bytes")
                fid = c["start"]["frame_id"]
                bits = DT_BITS.get(int(rx["dt"], 16)) if rx else None
                table = golden.get(bits, [])
                if golden and 1 <= fid <= len(table):
                    crc_checked += 1
                    if zlib.crc32(data) != table[fid - 1]:
                        bad.append(f"seq {seq}: dump CRC 0x{zlib.crc32(data):08x} != golden RAW{bits} frame {fid} "
                                   f"0x{table[fid - 1]:08x}")
                elif golden:
                    bad.append(f"seq {seq}: FRAME_ID {fid} beyond the golden table ({len(table)} frames)")
                if fid in uart_crc:
                    crc_checked += 1
                    if zlib.crc32(data) != uart_crc[fid]:
                        bad.append(f"seq {seq}: dump CRC 0x{zlib.crc32(data):08x} != firmware 0x{uart_crc[fid]:08x}")
    if good < opts["min_good"]:
        bad.append(f"{good} good capture(s), expected at least {opts['min_good']}")
    if opts["dump"] and (uart_crc or golden) and not crc_checked:
        bad.append("no dump matched a firmware CRC line")
    return bad, good, crc_checked


def selftest(args):
    import shutil
    import tempfile
    report = args[0]
    rest = list(args[1:])
    res = check([report] + rest)
    out = [("clean", not res[0])]
    ev_lines = Path(report).read_text().splitlines()
    evs = [json.loads(l) for l in ev_lines]

    def variant(name, edit, dump_edit=None):
        with tempfile.TemporaryDirectory() as tmp:
            lines = [json.loads(l) for l in ev_lines]
            lines = edit(lines)
            p = Path(tmp) / "bw.jsonl"
            p.write_text("\n".join(json.dumps(x, separators=(",", ":")) for x in lines) + "\n")
            r = list(rest)
            if dump_edit and "--dump-dir" in r:
                d = Path(tmp) / "nv12"
                shutil.copytree(r[r.index("--dump-dir") + 1], d)
                dump_edit(d)
                r[r.index("--dump-dir") + 1] = str(d)
            got = check([str(p)] + r)
            out.append((name, bool(got[0])))

    variant("no footer", lambda l: l[:-1])
    def drop_resp(l):
        k = next(i for i, x in enumerate(l) if x["ev"] == "dma" and x["ph"] == "resp")
        return l[:k] + l[k + 1:]
    variant("one response removed", drop_resp)
    def odma_bytes(l):
        for x in l:
            if x["ev"] == "dma" and x["ph"] == "issue" and x["m"] == "ISP_ODMA":
                x["n"] = 1
        return l
    variant("ODMA issue bytes = 1", odma_bytes)
    def odma_err(l):
        for x in l:
            if x["ev"] == "dma" and x["ph"] == "resp" and x["m"] == "ISP_ODMA":
                x["ok"] = 0
        return l
    variant("ODMA responses = error", odma_err)
    def past_plane(l):
        outs = [x for x in l if x["ev"] == "output" and x["how"] == "done"]
        o = outs[0]
        end = int(o["y_base"], 16) + o["y_stride"] * o["h"]
        last = max((x for x in l if x["ev"] == "dma" and x["ph"] == "issue" and x["m"] == "ISP_ODMA" and x["w"] and
                    int(x["a"], 16) < end and int(x["a"], 16) >= int(o["y_base"], 16) and
                    o["t_first_write_issue"] <= x["t"] <= o["t_end"]), key=lambda x: int(x["a"], 16))
        last["a"] = "0x%x" % (end - last["n"] // 2)  # starts inside the Y plane, ends past it
        return l
    variant("ODMA write past its plane", past_plane)
    def axis_short(l):
        x = next(x for x in l if x["ev"] == "axis_capture" and x.get("useful_bytes"))
        x["useful_bytes"] -= 8
        return l
    variant("AXIS bytes - 8", axis_short)
    if "--dump-dir" in rest:
        def flip(d):
            f = sorted(d.iterdir())[0]
            b = bytearray(f.read_bytes())
            b[7] ^= 1
            f.write_bytes(bytes(b))
        variant("NV12 byte flipped", lambda l: l, flip)
    ok = out[0][1] and all(failed for _, failed in out[1:])
    for name, flag in out:
        print(f"  {name}: {'PASS' if name == 'clean' and flag else 'FAIL as required' if name != 'clean' and flag else 'NOT AS REQUIRED'}")
    return ok


def same_run(a, b, ca, cb):
    bad = []
    if Path(a).read_bytes() != Path(b).read_bytes():
        bad.append(f"UART logs differ: {a} {b}")
    va = re.findall(r"\[sim-control\] .*", Path(ca).read_text(errors="replace"))
    vb = re.findall(r"\[sim-control\] .*", Path(cb).read_text(errors="replace"))
    if not va or va != vb:
        bad.append(f"verdict lines differ: {va} / {vb}")
    return bad


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    if sys.argv[1] == "check":
        res = check(sys.argv[2:])
        bad, good = res[0], res[1]
        crc = res[2] if len(res) > 2 else 0
        for b in bad:
            print("FAIL", b)
        if not bad:
            print(f"check_bw_report: PASS ({good} good capture(s), {crc} dump CRC(s) checked)")
        return 1 if bad else 0
    if sys.argv[1] == "selftest":
        ok = selftest(sys.argv[2:])
        print(f"check_bw_report: selftest {'PASS' if ok else 'FAIL'}")
        return 0 if ok else 1
    if sys.argv[1] == "same-run" and len(sys.argv) == 6:
        bad = same_run(*sys.argv[2:6])
        for b in bad:
            print("FAIL", b)
        if not bad:
            print("check_bw_report: same-run PASS (UART logs and verdict identical)")
        return 1 if bad else 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
