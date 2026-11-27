#!/usr/bin/env python3
"""Host check of a direct CSI-2 -> ISP run (plan DIRECT_CSI_ISP D2a, D2b).

Reads the simulator console (fx1_soc --traffic-report: one line per counted
master, key=value; reads / writes are counted when ISSUED, *_done when they
returned) and requires the direct-only traffic of the contract:
  MIPI_DMA  no read, no write issued (the CSI RAW DMA stays off: no RAW in DDR)
  ISP_IDMA  no read, no write issued (the ISP takes its pixels from the stream)
  ISP_ODMA  writes issued > 0, all returned, no read, every byte inside
            [--odma-lo, --odma-hi]; write errors exactly --odma-errors
            (default 0). The counter is the master's first hop, before the
            ISP fault shim and the write guard (review BW1 F2): an injected
            ODMA error is issued traffic with an error. With the shim (console
            "[fx1_soc] isp dma fault (injection): odma-error:N, R ODMA write(s)
            seen, fired at #N ..."): R = the counter's issued writes, fired
            exactly when --odma-errors is 1
  every master: outstanding = 0 (a report taken with a transaction still in
            flight proves nothing; D2a review F3)

Direct + concurrent RAW dump (D2b, --mipi-window LO:HI, repeated per RAW
side): MIPI_DMA writes issued > 0, all returned, no read; the counter's own
windows (fx1_soc --traffic-window) are exactly the given ones and no
transaction lay outside them (checked per transaction at issue, D2b review
F2: a write into the gap between two sides or a burst past the end of one);
write errors exactly --mipi-errors (default 0; D2b review F1: the counter sits
before the VP fault shim, so an injected SLVERR is issued traffic with an
error). With the shim (console line "[fx1_soc] camera fault (injection): ...
csi dma write-error:N, R MIPI_DMA write(s) seen, fired at #N address A"):
R = the counter's issued writes, fired exactly when --mipi-errors is 1, A
inside a window. ISP_IDMA still no read and no write.

RAW sentinel (review D4 F1, raw_sentinel.py; --raw-base/--raw-size/
--raw-pattern/--raw-dump): the counters observe three masters only; the
whole firmware RAW region was loaded with the pattern before the run and
dumped after it: every byte must be unchanged (any master: CPU, SYS_DMA, ...),
except inside the --mipi-window sides of a RAW dump. The dump is removed.

  check_direct_traffic.py --odma-lo 0x88000000 --odma-hi 0x881fffff
      [--mipi-window 0x81000000:0x81000fff --mipi-window 0x81010000:0x81010fff
       [--mipi-errors 1]]
      [--raw-base 0x81000000 --raw-size 0x7000000 --raw-pattern P --raw-dump D]
      [--odma-errors 1] CONSOLE
Exit 0 pass, 1 fail.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import raw_sentinel  # noqa: E402


def parse(path):
    masters, shim, ishim = {}, None, None
    for line in open(path, errors="replace"):
        m = re.match(r"^(MIPI_DMA|ISP_IDMA|ISP_ODMA) (.*)$", line.strip())
        if m:
            d = {k: int(v, 0) for k, v in re.findall(r"(\w+)=(0x[0-9a-f]+|\d+)", m.group(2))}
            w = re.search(r"\bwindows=(\S+)", m.group(2))
            d.pop("windows", None)
            if w:
                d["windows"] = sorted(tuple(int(x, 0) for x in r.split("-")) for r in w.group(1).split(","))
            masters[m.group(1)] = d
        f = re.search(r"csi dma write-error:(\d+), (\d+) MIPI_DMA write\(s\) seen, "
                      r"(?:fired at #\d+ address (0x[0-9a-f]+)|not reached)", line)
        if f:
            shim = {"nth": int(f.group(1)), "seen": int(f.group(2)),
                    "addr": int(f.group(3), 0) if f.group(3) else None}
        g = re.search(r"isp dma fault \(injection\): odma-error:(\d+), (\d+) ODMA write\(s\) seen, "
                      r"(?:fired at #\d+ address (0x[0-9a-f]+)|not reached)", line)
        if g:
            ishim = {"nth": int(g.group(1)), "seen": int(g.group(2)),
                     "addr": int(g.group(3), 0) if g.group(3) else None}
    return masters, shim, ishim


def window(v):
    lo, hi = (int(x, 0) for x in v.split(":"))
    if hi < lo:
        raise argparse.ArgumentTypeError(f"window {v}: HI below LO")
    return (lo, hi)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--odma-lo", type=lambda v: int(v, 0), required=True)
    ap.add_argument("--odma-hi", type=lambda v: int(v, 0), required=True)
    ap.add_argument("--mipi-window", type=window, action="append", default=[])
    ap.add_argument("--mipi-errors", type=int, default=0)
    ap.add_argument("--odma-errors", type=int, default=0)
    ap.add_argument("--raw-base", type=lambda v: int(v, 0))
    ap.add_argument("--raw-size", type=lambda v: int(v, 0))
    ap.add_argument("--raw-pattern")
    ap.add_argument("--raw-dump")
    ap.add_argument("console")
    a = ap.parse_args()
    dump = bool(a.mipi_window)
    if a.mipi_errors and not dump:
        ap.error("--mipi-errors needs --mipi-window (RAW dump)")
    raw = (a.raw_base, a.raw_size, a.raw_pattern, a.raw_dump)
    if any(v is not None for v in raw) and not all(v is not None for v in raw):
        ap.error("--raw-base, --raw-size, --raw-pattern and --raw-dump go together")
    raw_bad, raw_summary = [], ""
    if a.raw_dump is not None:  # first: the dump is removed whatever the traffic verdict
        raw_bad, raw_summary = raw_sentinel.check(a.raw_base, a.raw_size, a.raw_pattern, a.raw_dump,
                                                  sorted(a.mipi_window))
    t, shim, ishim = parse(a.console)
    bad = []
    for name in ("MIPI_DMA", "ISP_IDMA", "ISP_ODMA"):
        if name not in t:
            bad.append(f"{name}: no traffic line (run without --traffic-report or --camera?)")
    if bad:
        print("\n".join(bad + raw_bad))
        return 1
    need = ("reads", "writes", "read_done", "write_done", "read_errors", "write_errors", "outstanding")
    for name in ("MIPI_DMA", "ISP_IDMA", "ISP_ODMA"):
        missing = [k for k in need if k not in t[name]]
        if missing:
            bad.append(f"{name}: report without {', '.join(missing)} (counter predates issue/outstanding counting)")
    if bad:
        print("\n".join(bad + raw_bad))
        return 1
    mipi, idma, odma = t["MIPI_DMA"], t["ISP_IDMA"], t["ISP_ODMA"]
    for name, c in (("MIPI_DMA", mipi), ("ISP_IDMA", idma), ("ISP_ODMA", odma)):
        if c["outstanding"]:
            bad.append(f"{name}: {c['outstanding']} transactions outstanding at report time")
    if not dump and (mipi["reads"] or mipi["writes"]):
        bad.append(f"MIPI_DMA: {mipi['reads']} reads, {mipi['writes']} writes issued (direct: the CSI RAW DMA must be off)")
    if dump:
        want = sorted(a.mipi_window)
        if mipi.get("windows") != want or "outside" not in mipi:
            fmt = lambda ws: "none" if ws is None else ",".join(f"0x{lo:x}-0x{hi:x}" for lo, hi in ws)
            bad.append(f"MIPI_DMA: counter windows {fmt(mipi.get('windows'))} are not {fmt(want)} "
                       f"(run without --traffic-window for the RAW sides?)")
        elif mipi["outside"]:
            bad.append(f"MIPI_DMA: {mipi['outside']} transactions not wholly inside one RAW side window")
        if mipi["writes"] == 0:
            bad.append("MIPI_DMA: no write (RAW dump expected)")
        if mipi["write_done"] != mipi["writes"]:
            bad.append(f"MIPI_DMA: {mipi['writes']} writes issued, {mipi['write_done']} returned")
        if mipi["reads"]:
            bad.append(f"MIPI_DMA: {mipi['reads']} reads (RAW dump)")
        if mipi["write_errors"] != a.mipi_errors:
            bad.append(f"MIPI_DMA: {mipi['write_errors']} write errors, expected {a.mipi_errors}")
        if mipi["writes"] and (mipi["write_lo"] < want[0][0] or mipi["write_hi"] > want[-1][1]):
            bad.append(f"MIPI_DMA: writes 0x{mipi['write_lo']:x}..0x{mipi['write_hi']:x} outside "
                       f"0x{want[0][0]:x}..0x{want[-1][1]:x} (RAW dump sides)")
        if a.mipi_errors and shim is None:
            bad.append("MIPI_DMA: an injected error is expected but the console has no csi dma fault report")
        if shim is not None:
            if shim["seen"] != mipi["writes"]:
                bad.append(f"MIPI_DMA: the fault shim saw {shim['seen']} writes, the counter {mipi['writes']} "
                           f"(issued writes lost before the counter)")
            if (shim["addr"] is not None) != (a.mipi_errors == 1):
                bad.append(f"MIPI_DMA: fault {'fired' if shim['addr'] is not None else 'not reached'}, "
                           f"{a.mipi_errors} injected error(s) expected")
            if shim["addr"] is not None and not any(lo <= shim["addr"] <= hi for lo, hi in want):
                bad.append(f"MIPI_DMA: injected error at 0x{shim['addr']:x} outside the RAW sides")
    if idma["reads"] or idma["writes"]:
        bad.append(f"ISP_IDMA: {idma['reads']} reads, {idma['writes']} writes issued (direct: the IDMA must not read)")
    if odma["writes"] == 0:
        bad.append("ISP_ODMA: no write (no NV12 output)")
    if odma["write_done"] != odma["writes"]:
        bad.append(f"ISP_ODMA: {odma['writes']} writes issued, {odma['write_done']} returned")
    if odma["write_errors"] != a.odma_errors or odma["reads"]:
        bad.append(f"ISP_ODMA: {odma['write_errors']} write errors (expected {a.odma_errors}), {odma['reads']} reads")
    if a.odma_errors and ishim is None:
        bad.append("ISP_ODMA: an injected error is expected but the console has no isp dma fault report")
    if ishim is not None:
        if ishim["seen"] != odma["writes"]:
            bad.append(f"ISP_ODMA: the fault shim saw {ishim['seen']} writes, the counter {odma['writes']} "
                       f"(issued writes lost before the counter)")
        if (ishim["addr"] is not None) != (a.odma_errors == 1):
            bad.append(f"ISP_ODMA: fault {'fired' if ishim['addr'] is not None else 'not reached'}, "
                       f"{a.odma_errors} injected error(s) expected")
    if odma["writes"] and (odma["write_lo"] < a.odma_lo or odma["write_hi"] > a.odma_hi):
        bad.append(f"ISP_ODMA: writes 0x{odma['write_lo']:x}..0x{odma['write_hi']:x} outside "
                   f"0x{a.odma_lo:x}..0x{a.odma_hi:x}")
    bad += raw_bad
    for b in bad:
        print(b)
    if raw_summary:
        print(raw_summary)
    print(f"direct traffic: MIPI_DMA {mipi['writes']} writes, ISP_IDMA {idma['reads']} reads, "
          f"ISP_ODMA {odma['writes']} writes ({odma['write_bytes']} bytes)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
