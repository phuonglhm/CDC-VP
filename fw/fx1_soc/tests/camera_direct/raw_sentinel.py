#!/usr/bin/env python3
"""RAW sentinel of the direct CSI-2 -> ISP runs (review D4 F1).

The direct path must not write RAW into DDR through any master: the CSI RAW
DMA, the ISP IDMA (counted by fx1_soc --traffic-report) but also the CPU,
SYS_DMA or any other initiator, which no counter observes. The sentinel is
address based and independent of the firmware under test: the host loads a
pattern into the whole firmware RAW region (FX1_FW_RAW_BUF_BASE ..
FX1_FW_YUV_BUF_BASE, fx1/fx1_memory_map.h) with fx1_soc --load (after the DDR3
preinit, before the CPUs run) and reads it back after the run with --dump; every
byte must be unchanged, except inside the CSI side windows a concurrent RAW dump
declares (written by MIPI_DMA, checked by its counter).

The pattern is SHA-256 in counter mode: deterministic, no repeating word, so a
write of any plausible value (zero, a constant, RAW samples, a copy of another
region) changes it.

  raw_sentinel.py make FILE SIZE        write the pattern (SIZE bytes)
  raw_sentinel.py fixtures DIR          checker unit-test inputs (small region)
  raw_sentinel.py selftest DIR          pattern / check / dump removal self test

check_direct_traffic.py --raw-sentinel BASE:SIZE:PATTERN:DUMP uses check().
"""
import hashlib
import os
import sys

TAG = b"fx1 direct RAW sentinel v1"
CHUNK = 1 << 20


def pattern(offset, size):
    """Bytes [offset, offset + size) of the pattern (offset, size multiples of 32)."""
    out = bytearray()
    for i in range(offset // 32, (offset + size) // 32):
        out += hashlib.sha256(TAG + i.to_bytes(8, "little")).digest()
    return bytes(out)


def make(path, size):
    if size % 32:
        raise SystemExit(f"raw_sentinel: size {size} is not a multiple of 32")
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        for off in range(0, size, CHUNK):
            f.write(pattern(off, min(CHUNK, size - off)))
    os.replace(tmp, path)


def _ranges(base, offsets):
    """Coalesce sorted changed byte offsets into (lo, hi) address ranges."""
    out = []
    for o in offsets:
        if out and o == out[-1][1] + 1 - base:
            out[-1][1] = base + o
        else:
            out.append([base + o, base + o])
    return out


def check(base, size, pattern_path, dump_path, allowed):
    """Returns (problems, summary). `allowed`: (lo, hi) address windows where a
    change is permitted. The dump is removed afterwards (112 MiB per run; a
    later check can then never read a stale one)."""
    bad = []
    try:
        if not os.path.exists(dump_path):
            return [f"RAW sentinel: no dump {dump_path} (run without --dump?)"], ""
        if os.path.getsize(dump_path) != size:
            return [f"RAW sentinel: dump is {os.path.getsize(dump_path)} bytes, expected {size}"], ""
        if os.path.getsize(pattern_path) != size:
            return [f"RAW sentinel: pattern is {os.path.getsize(pattern_path)} bytes, expected {size}"], ""
        outside, inside, ranges = 0, 0, []
        with open(pattern_path, "rb") as p, open(dump_path, "rb") as d:
            for off in range(0, size, CHUNK):
                a, b = p.read(CHUNK), d.read(CHUNK)
                if a == b:
                    continue
                diff = [off + i for i in range(len(a)) if a[i] != b[i]]
                for o in diff:
                    if any(lo <= base + o <= hi for lo, hi in allowed):
                        inside += 1
                    else:
                        outside += 1
                        if len(ranges) < 4096:
                            ranges.append(o)
        if outside:
            rs = _ranges(base, ranges)
            shown = ", ".join(f"0x{lo:08x}-0x{hi:08x}" for lo, hi in rs[:8])
            more = f" (+{len(rs) - 8} more ranges)" if len(rs) > 8 else ""
            bad.append(f"RAW sentinel: {outside} bytes changed outside the allowed windows: {shown}{more} "
                       f"(a RAW write into DDR by some master)")
        summary = (f"RAW sentinel: 0x{base:08x}+0x{size:x} {'changed' if outside else 'unchanged'} "
                   f"outside the windows; {inside} bytes changed inside {len(allowed)} window(s)")
        return bad, summary
    finally:
        if os.path.exists(dump_path):
            os.remove(dump_path)


def fixtures(d):
    """Small region (64 KiB at 0x81000000) for the checker unit tests: one
    dump per test, named after it (check() removes the dump it read, so two
    tests must never share one; re-review D4 R1). "missing" has none."""
    size = 0x10000
    os.makedirs(d, exist_ok=True)
    pat = pattern(0, size)
    open(os.path.join(d, "pattern.bin"), "wb").write(pat)

    def dump(name, edits):
        b = bytearray(pat)
        for off, data in edits:
            b[off:off + len(data)] = data
        open(os.path.join(d, name + ".bin"), "wb").write(b)

    side = [(0x0100, bytes(range(256)))]                                # inside side 0 (0x81000000-0x81000fff)
    dump("clean", [])
    dump("cpu_write", [(0x8000, (0xDEADBEEF).to_bytes(4, "little"))])  # the reviewer's probe, scaled
    dump("zeros", [(0x2000, bytes(64))])
    dump("side_direct", side)                                           # direct only: a write is a write
    dump("side_dump", side)                                             # RAW dump: allowed inside the side
    dump("side_past_end", [(0x0100, bytes(16)), (0x1000, bytes(1))])   # 0x81001000: one past side 0
    open(os.path.join(d, "short.bin"), "wb").write(pat[:-32])
    missing = os.path.join(d, "missing.bin")
    if os.path.exists(missing):
        os.remove(missing)


def selftest(d):
    """The pattern has no repeated 32-byte block in 1 MiB, make() equals
    pattern(), check() removes the dump (a second check finds none)."""
    os.makedirs(d, exist_ok=True)
    fails = []
    p = pattern(0, CHUNK)
    blocks = {p[i:i + 32] for i in range(0, len(p), 32)}
    if len(blocks) != len(p) // 32:
        fails.append("pattern repeats a 32-byte block")
    path = os.path.join(d, "selftest_pattern.bin")
    make(path, 2 * CHUNK + 64)
    if open(path, "rb").read() != pattern(0, 2 * CHUNK + 64):
        fails.append("make() differs from pattern()")
    dump = os.path.join(d, "selftest_dump.bin")
    open(dump, "wb").write(open(path, "rb").read())
    bad, _ = check(0x81000000, 2 * CHUNK + 64, path, dump, [])
    if bad or os.path.exists(dump):
        fails.append(f"clean check: {bad}, dump left: {os.path.exists(dump)}")
    bad, _ = check(0x81000000, 2 * CHUNK + 64, path, dump, [])
    if not bad or "no dump" not in bad[0]:
        fails.append(f"second check without a dump: {bad}")
    for f in fails:
        print("FAIL:", f)
    print(f"raw_sentinel selftest: {'PASS' if not fails else 'FAIL'} ({len(fails)} failures)")
    return 1 if fails else 0


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "make":
        make(sys.argv[2], int(sys.argv[3], 0))
    elif len(sys.argv) == 3 and sys.argv[1] == "fixtures":
        fixtures(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == "selftest":
        sys.exit(selftest(sys.argv[2]))
    else:
        raise SystemExit(__doc__)
