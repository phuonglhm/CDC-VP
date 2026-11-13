#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Prepare an ISP RAW-sequence stream session (`run_vp.sh isp-stream`); stdlib only.

Contract: platforms/fx1_soc/docs/ISP_STREAM_CONTRACT.md. The DDR layout and the
blocks mirror fw .../fx1_map/include/fx1/fx1_isp_stream.h (checked by
demo/tests/test_stream_prep.py against the compiled header).

  isp_stream_prep.py prepare --out SESSION (--manifest M.json | --raw A.raw [B.raw ...])
                     --profile P.csrw [--repeat K] [--frames N | --loop] [--queue 1|2|4]
                     [--legacy] [--width W --height H --source-format F --source-stride S]
                     [--codec-dir DIR] [--csr-header H]
  isp_stream_prep.py reference --session SESSION --run-raw fx1_isp_run_raw [--frames N]

prepare validates the manifest (or the quick --raw form) and the profile,
normalizes every RAW once into the ISP 16-bit container, lays out the source
pool and writes SESSION/:
  manifest.resolved.json  the resolved manifest, SHA-256 of every source and container
  profile.csrw            snapshot of the profile; profile.bin its compiled writes
  pool/src_NNN.isp16      normalized sources (in_stride x height each)
  desc.bin                fx1_isp_stream_desc
  loads.txt               ADDR:FILE lines for fx1_soc --load
  reference.list          one container per line in schedule order (fx1_isp_run_raw)
  session.json            layout, schedule and identities of the session
SESSION must not exist, or be empty: nothing of the user's is removed.

reference runs the stand-alone ISP model on the same containers, profile,
mode, queue depth and physical frame count (one session, temporal state
kept) and writes SESSION/reference.json: the SHA-256 of every frame with its
identity. A loop session has no reference (no frame count).

Exit status: 0 done, 2 input or usage error (the message names the problem),
3 the reference run failed.
"""
import argparse
import array
import hashlib
import json
import os
import random
import struct
import subprocess
import sys
import tempfile

# ---- fx1_memory_map.h / fx1_isp_stream.h mirror --------------------------------
DDR_BASE, DDR_SIZE = 0x80000000, 0x20000000
FW_SHARED_BASE, FW_RAW_BUF_BASE, FW_YUV_BUF_BASE = 0x80500000, 0x81000000, 0x88000000
FW_TEST_AREA_BASE = 0x90000000
DESC_ADDR = FW_SHARED_BASE + 0x100000
STATUS_ADDR = FW_SHARED_BASE + 0x100100
CONTROL_ADDR = FW_SHARED_BASE + 0x100200
RECORD_ADDR = FW_SHARED_BASE + 0x100400
RECORD_BYTES = 128
PROFILE_ADDR = FW_SHARED_BASE + 0x110000
PROFILE_MAX = 32768
CHAIN_ADDR, CHAIN_PITCH = FW_SHARED_BASE + 0x200000, 0x50000
SLOTS, SLOT_BYTES = 4, 0x1000000
POOL_BASE, POOL_END, POOL_ALIGN = FW_TEST_AREA_BASE + 0x200000, DDR_BASE + DDR_SIZE, 0x1000
DMA_CHANNEL, DMA_CHUNK = 1, 1008
DESC_MAGIC, STATUS_MAGIC, CONTROL_MAGIC, RECORD_MAGIC = 0x53315846, 0x54315846, 0x43315846, 0x46315846
VERSION = 1
FLAG_LOOP = 0x1
MODE_PROFILE, MODE_LEGACY = 0, 1
STATES = ("IDLE", "PREPARED", "RUNNING", "DRAINING", "STOPPED", "ERROR")
STOP_REASONS = ("NONE", "DONE", "USER", "LIMIT", "ERROR")
ERRORS = ("NONE", "DESC", "BUSY", "PROFILE", "CONFIG", "COPY", "ISP", "TIMEOUT", "HOST")
REQ_NONE, REQ_STOP = 0, 1
REASON_USER, REASON_LIMIT = 1, 2
REC_FREE, REC_PUBLISHED = 0, 1
COLORIMETRY_VALID = 1 << 31

DESC_FIELDS = ("magic version size flags session_lo session_hi width height bayer depth in_stride "
               "frame_bytes source_count source_base source_pitch repeat frames_lo frames_hi queue mode "
               "profile_count profile_id out_stride frame_budget_us").split() + ["reserved%d" % i for i in range(8)]
STATUS_FIELDS = ("magic version state stop_reason error detail session_lo session_hi submitted_lo submitted_hi "
                 "completed_lo completed_hi cancelled failed out_width out_height out_stride colorimetry dma_err "
                 "idma_frames odma_frames publish_seq elapsed_ticks").split() + ["reserved%d" % i for i in range(9)]
CONTROL_FIELDS = "magic version request reason session_lo session_hi".split() + ["reserved%d" % i for i in range(10)]
RECORD_FIELDS = ("magic state seq ack_seq session_lo session_hi ordinal_lo ordinal_hi source_index repeat_index "
                 "driver_seq tag frame_id profile_id ts sof_ts eof_ts slot y_addr uv_addr width height stride "
                 "colorimetry status").split() + ["reserved%d" % i for i in range(7)]
SIZES = {"desc": 128, "status": 128, "control": 64, "record": RECORD_BYTES}

# ---- sources (tools/raw_fixture.py formats, stdlib) ---------------------------------
# name: (bits that must be zero in the source word, right shift to the ISP's 12-bit scale)
FORMATS = {
    "le16_raw10_msb": (0x003F, 4),   # RAW10 in bits [15:6] (the "Image Source" set, the samples)
    "le16_raw10_has": (0xF003, 0),   # RAW10 in bits [11:2] (HAS contract)
    "le16_raw12_has": (0xF000, 0),   # RAW12 in bits [11:0] (HAS contract)
}
RAW10_SOURCES = ("le16_raw10_msb", "le16_raw10_has")
DEPTHS = {"raw12": 0, "raw10": 1}    # COMMON_INPUT_FORMAT.DEPTH
BAYER = {"RGGB": 0, "GRBG": 1, "GBRG": 2, "BGGR": 3}
MANIFEST_KEYS = {"version", "width", "height", "bayer", "source_format", "source_stride", "input_depth", "frames"}
QUICK_FORMAT = "le16_raw10_msb"      # the samples' layout (run_vp.sh isp FILE.raw)
QUICK_WIDTH, QUICK_HEIGHT = 2688, 1520
# fx1_isp_set_geometry() (driver) and MAS v1.07 §6.6.3.4/5: the container is even, 2..3842 x 2..2162
# (the phase overscan included); the active image after the Bayer phase trim is 32..3840 x 32..2160.
MAX_W, MAX_H = 3842, 2162
MIN_ACTIVE, MAX_ACTIVE_W, MAX_ACTIVE_H = 32, 3840, 2160
FRAME_BUDGET_US = 2000000            # simulated time per frame (the demo's 1 s DONE budget, x2)


class PrepError(Exception):
    """An input is not usable; the message says which and why."""


def align(v, a):
    return (v + a - 1) // a * a


def active_geometry(width, height, bayer):
    """The active image the ISP processes: the container less the Bayer phase trim (the driver's formula)."""
    return (width - (bayer & 1)) & ~1, (height - ((bayer >> 1) & 1)) & ~1


def read_bytes(path):
    """Every input file is read exactly once, through here: what is checked is what is written."""
    with open(path, "rb") as f:
        return f.read()


def bad_samples(bad, nbytes):
    """(count, first index) of the non-zero 16-bit lanes of `bad`, in linear time."""
    raw = bad.to_bytes(nbytes, "little")
    lanes = array.array("H")
    lanes.frombytes(raw)
    first = (nbytes - len(raw.lstrip(b"\0"))) // 2   # little endian: the first non-zero byte is the low end
    return len(lanes) - lanes.count(0), first


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def pack_block(fields, values, size):
    unknown = set(values) - set(fields)
    if unknown:
        raise PrepError("unknown block fields: %s" % ", ".join(sorted(unknown)))
    data = struct.pack("<%dI" % len(fields), *[values.get(f, 0) & 0xFFFFFFFF for f in fields])
    assert len(data) == size, (len(data), size)
    return data


def unpack_block(fields, data, size, what):
    if len(data) < size:
        raise PrepError("%s: %d bytes, expected %d" % (what, len(data), size))
    return dict(zip(fields, struct.unpack_from("<%dI" % len(fields), data)))


def normalize(data, fmt, width, height, src_stride, depth, in_stride, where):
    """The ISP container (in_stride x height bytes, LE16 right-aligned) of one source frame."""
    zero, shift = FORMATS[fmt]
    if depth == DEPTHS["raw10"]:
        if fmt not in RAW10_SOURCES:
            raise PrepError("%s: input_depth raw10 needs a RAW10 source (%s), not %s"
                            % (where, ", ".join(RAW10_SOURCES), fmt))
        # native RAW10 in bits [9:0]: two more shifts. The two low bits of the 12-bit scale are
        # already declared zero by both RAW10 source formats (their masks hold 0x3 << shift).
        shift += 2
    if len(data) != src_stride * height:
        raise PrepError("%s: %d bytes, expected %d (stride %d x height %d)"
                        % (where, len(data), src_stride * height, src_stride, height))
    active = 2 * width
    rows = data if src_stride == active else b"".join(
        data[y * src_stride:y * src_stride + active] for y in range(height))
    n = len(rows) // 2
    # The whole frame as one integer: a lane shift is exact because the bits
    # shifted in from the next sample are its own bits declared zero.
    x = int.from_bytes(rows, "little")
    bad = x & int.from_bytes(struct.pack("<H", zero) * n, "little")
    if bad:
        count, first = bad_samples(bad, len(rows))
        raise PrepError("%s: %d sample%s bits 0x%04X set (first at x=%d, y=%d), which format %s%s declares zero"
                        % (where, count, " has" if count == 1 else "s have", zero, first % width, first // width,
                           fmt, " (as RAW10 native)" if depth else ""))
    out = (x >> shift).to_bytes(len(rows), "little")
    if in_stride == active:
        return out
    pad = bytes(in_stride - active)
    return b"".join(out[y * active:(y + 1) * active] + pad for y in range(height))


# ---- profile ---------------------------------------------------------------------
def default_paths(codec_dir, header):
    here = os.path.dirname(os.path.abspath(__file__))
    repo_isp = os.path.join(here, "..", "..", "..", "components", "FX1_Components", "isp")
    if not codec_dir:
        codec_dir = here if os.path.isfile(os.path.join(here, "csrw_codec.py")) else os.path.join(repo_isp, "tools")
    if not header:
        for h in (os.path.join(here, "..", "..", "sources", "VP_FX1_V3.0", "include", "fx1_isp", "fx1_isp_csr.h"),
                  os.path.join(repo_isp, "include", "fx1_isp", "fx1_isp_csr.h")):
            if os.path.isfile(h):
                header = h
                break
    return os.path.abspath(codec_dir), header and os.path.abspath(header)


def load_codec(codec_dir):
    sys.dont_write_bytecode = True
    if codec_dir not in sys.path:
        sys.path.insert(0, codec_dir)
    try:
        import csrw_codec  # noqa: E402
    except ImportError as e:
        raise PrepError("csrw_codec.py not found in %s (%s)" % (codec_dir, e))
    return csrw_codec


def profile_facts(path, codec, cmap, data=None):
    """Compiled writes and the input settings the profile itself writes (last write wins).

    With `data`, those bytes are compiled (through a private copy, as the codec reads files) and
    `path` only names them in messages: the caller's snapshot is exactly what is compiled."""
    try:
        if data is None:
            stream = codec.read(path, cmap)
        else:
            with tempfile.TemporaryDirectory(prefix="isp_stream_prep_") as d:
                tmp = os.path.join(d, "profile.csrw")
                with open(tmp, "wb") as f:
                    f.write(data)
                try:
                    stream = codec.read(tmp, cmap)
                except codec.CsrwError as e:
                    raise codec.CsrwError(str(e).replace(tmp, path))
    except codec.CsrwError as e:
        raise PrepError(str(e))
    if not stream:
        raise PrepError("%s: no CSR writes" % path)
    if len(stream) > PROFILE_MAX:
        raise PrepError("%s: %d writes, at most %d fit the stream profile area" % (path, len(stream), PROFILE_MAX))
    last = {}
    for n, reg, value in stream:
        last[reg.name] = (n, reg, value)

    def field(name, fname):
        if name not in last:
            return None
        n, reg, value = last[name]
        # Field names keep the CSR's case (COMMON_BAYER pattern, COMMON_INPUT_FORMAT DEPTH).
        f = next((x for x in reg.fields if x.name.lower() == fname), None)
        if f is None:
            raise PrepError("CSR header: %s has no field %s" % (name, fname))
        return n, (value & f.mask) >> f.lsb

    names = {v: k for k, v in BAYER.items()}
    facts = {"writes": len(stream), "bin": codec.to_bin([(reg, v) for _, reg, v in stream])}
    b = field("COMMON_BAYER", "pattern")
    facts["bayer"] = b and (b[0], names.get(b[1], "code %d" % b[1]))
    d = field("COMMON_INPUT_FORMAT", "depth")
    facts["depth"] = d and (d[0], d[1])
    for reg in ("COMMON_FRAME_WIDTH", "COMMON_FRAME_HEIGHT"):
        facts[reg] = (last[reg][0], last[reg][2]) if reg in last else None
    return facts


# ---- manifest ----------------------------------------------------------------------
def load_manifest(path):
    try:
        with open(path, encoding="utf-8") as f:
            m = json.load(f)
    except (OSError, ValueError) as e:
        raise PrepError("manifest %s: %s" % (path, e))
    if not isinstance(m, dict):
        raise PrepError("manifest %s: not a JSON object" % path)
    extra, missing = set(m) - MANIFEST_KEYS, {"version", "width", "height", "bayer", "source_format",
                                               "input_depth", "frames"} - set(m)
    if extra:
        raise PrepError("manifest %s: unknown keys %s" % (path, ", ".join(sorted(extra))))
    if missing:
        raise PrepError("manifest %s: missing keys %s" % (path, ", ".join(sorted(missing))))
    if m["version"] != 1:
        raise PrepError("manifest %s: version %r, this tool reads version 1" % (path, m["version"]))
    base = os.path.dirname(os.path.abspath(path))
    frames = m["frames"]
    if not isinstance(frames, list) or not frames or not all(isinstance(p, str) and p for p in frames):
        raise PrepError("manifest %s: frames must be a non-empty list of file paths" % path)
    m = dict(m)
    m["frames"] = [p if os.path.isabs(p) else os.path.join(base, p) for p in frames]
    return m


def check_manifest(m, where):
    for k in ("width", "height"):
        if not isinstance(m[k], int) or isinstance(m[k], bool):
            raise PrepError("%s: %s must be an integer" % (where, k))
    w, h = m["width"], m["height"]
    if not (2 <= w <= MAX_W and 2 <= h <= MAX_H) or w % 2 or h % 2:
        raise PrepError("%s: geometry %dx%d invalid (even, 2..%d x 2..%d)" % (where, w, h, MAX_W, MAX_H))
    if m["bayer"] not in BAYER:
        raise PrepError("%s: bayer %r, one of %s" % (where, m["bayer"], ", ".join(BAYER)))
    aw, ah = active_geometry(w, h, BAYER[m["bayer"]])
    if not (MIN_ACTIVE <= aw <= MAX_ACTIVE_W and MIN_ACTIVE <= ah <= MAX_ACTIVE_H):
        raise PrepError("%s: geometry %dx%d %s leaves an active image of %dx%d after the Bayer phase trim; "
                        "the ISP needs %d..%d x %d..%d" % (where, w, h, m["bayer"], aw, ah, MIN_ACTIVE, MAX_ACTIVE_W,
                                                          MIN_ACTIVE, MAX_ACTIVE_H))
    if m["source_format"] not in FORMATS:
        raise PrepError("%s: source_format %r, one of %s" % (where, m["source_format"], ", ".join(sorted(FORMATS))))
    if m["input_depth"] not in DEPTHS:
        raise PrepError("%s: input_depth %r, raw12 or raw10" % (where, m["input_depth"]))
    if m["input_depth"] == "raw10" and m["source_format"] not in RAW10_SOURCES:
        raise PrepError("%s: input_depth raw10 needs a RAW10 source_format (%s)" % (where, ", ".join(RAW10_SOURCES)))
    s = m.get("source_stride", 2 * w)
    if not isinstance(s, int) or isinstance(s, bool) or s < 2 * w or s % 2:
        raise PrepError("%s: source_stride %r invalid for width %d (even, >= %d)" % (where, s, w, 2 * w))
    m["source_stride"] = s
    for p in m["frames"]:
        if not os.path.isfile(p):
            raise PrepError("%s: RAW file not found: %s" % (where, p))
        size = os.path.getsize(p)
        if size != s * h:
            raise PrepError("%s: %s: %d bytes, expected %d (stride %d x height %d)" % (where, p, size, s * h, s, h))
    return m


def check_against_profile(m, facts, profile):
    b = facts["bayer"]
    if b is None:
        raise PrepError("%s: no COMMON_BAYER write; the profile must state the Bayer phase "
                        "(e.g. COMMON_BAYER pattern=%d for %s)" % (profile, BAYER[m["bayer"]], m["bayer"]))
    if b[1] != m["bayer"]:
        raise PrepError("%s:%d: COMMON_BAYER is %s, the sequence is %s" % (profile, b[0], b[1], m["bayer"]))
    d = facts["depth"]
    depth = d[1] if d else 0
    if depth != DEPTHS[m["input_depth"]]:
        where = "%s:%d: COMMON_INPUT_FORMAT depth=%d" % (profile, d[0], depth) if d else \
            "%s: no COMMON_INPUT_FORMAT write (depth 0, RAW12)" % profile
        raise PrepError("%s, the sequence is input_depth %s (depth %d)" % (where, m["input_depth"], DEPTHS[m["input_depth"]]))
    for reg, want in (("COMMON_FRAME_WIDTH", m["width"]), ("COMMON_FRAME_HEIGHT", m["height"])):
        if facts[reg] and facts[reg][1] != want:
            raise PrepError("%s:%d: %s %d, the sequence is %d" % (profile, facts[reg][0], reg, facts[reg][1], want))


def quick_manifest(raws, facts, profile, a):
    if facts["bayer"] is None:
        raise PrepError("%s: no COMMON_BAYER write: --raw takes the Bayer phase from the profile "
                        "(use --manifest to state it)" % profile)
    depth = facts["depth"][1] if facts["depth"] else 0
    return {"version": 1, "width": a.width or QUICK_WIDTH, "height": a.height or QUICK_HEIGHT,
            "bayer": facts["bayer"][1], "source_format": a.source_format or QUICK_FORMAT,
            "source_stride": a.source_stride or 2 * (a.width or QUICK_WIDTH),
            "input_depth": {v: k for k, v in DEPTHS.items()}.get(depth, "raw12"),
            "frames": [os.path.abspath(p) for p in raws]}


# ---- schedule and layout ---------------------------------------------------------------
def identity(n, sources, repeat):
    """(source index, repeat index) of physical frame n (0-based)."""
    return (n // repeat) % sources, n % repeat


def layout(m, sources):
    w, h = m["width"], m["height"]
    in_stride = align(2 * w, 16)
    frame_bytes = in_stride * h
    if frame_bytes > SLOT_BYTES:
        raise PrepError("a %dx%d frame (%d bytes) does not fit a %d-byte input slot" % (w, h, frame_bytes, SLOT_BYTES))
    pitch = align(frame_bytes, POOL_ALIGN)
    pool_end = POOL_BASE + sources * pitch
    if pool_end > POOL_END:
        fit = (POOL_END - POOL_BASE) // pitch
        raise PrepError("%d sources of %d bytes need %d MiB of source pool; %d MiB (%d such frames) are available"
                        % (sources, pitch, (sources * pitch) >> 20, (POOL_END - POOL_BASE) >> 20, fit))
    # The output is at most the active image (the resizer only reduces); its stride is the one
    # fx1_isp_run_raw programs, align16(output width).
    aw, ah = active_geometry(w, h, BAYER[m.get("bayer", "RGGB")])
    out_stride = align(aw, 16)
    y_bytes = out_stride * ah
    uv_offset = align(y_bytes, 0x100000)
    if uv_offset + y_bytes // 2 > SLOT_BYTES:
        raise PrepError("NV12 of %d rows at stride %d does not fit an output slot" % (ah, out_stride))
    chain = -(-frame_bytes // DMA_CHUNK)
    if chain * 16 > CHAIN_PITCH:
        raise PrepError("a %d-byte frame needs %d SYS_DMA commands; a chain holds %d" % (frame_bytes, chain, CHAIN_PITCH // 16))
    return {"in_stride": in_stride, "frame_bytes": frame_bytes, "source_pitch": pitch, "pool_base": POOL_BASE,
            "pool_end": pool_end, "out_stride": out_stride, "uv_offset": uv_offset, "dma_commands": chain,
            "in_slots": [FW_RAW_BUF_BASE + i * SLOT_BYTES for i in range(SLOTS)],
            "out_slots": [FW_YUV_BUF_BASE + i * SLOT_BYTES for i in range(SLOTS)]}


def new_dir(path):
    if os.path.exists(path):
        if not os.path.isdir(path) or os.listdir(path):
            raise PrepError("%s exists and is not an empty directory (nothing is removed: give a new path)" % path)
    else:
        os.makedirs(path)


def write_atomic(path, data, mode="wb"):
    tmp = path + ".tmp"
    with open(tmp, mode) as f:
        f.write(data)
    os.replace(tmp, path)


def prepare(a):
    codec_dir, header = default_paths(a.codec_dir, a.csr_header)
    if not header:
        raise PrepError("CSR header fx1_isp_csr.h not found (give --csr-header)")
    codec = load_codec(codec_dir)
    cmap = codec.CsrMap(header)
    if a.repeat < 1:
        raise PrepError("--repeat %d: K must be >= 1" % a.repeat)
    if a.queue not in (1, 2, 4):
        raise PrepError("--queue %d: 1, 2 or 4" % a.queue)
    if a.frames is not None and a.frames < 1:
        raise PrepError("--frames %d: N must be >= 1" % a.frames)
    profile = os.path.abspath(a.profile)
    if not profile.endswith(".csrw") or not os.path.isfile(profile):
        raise PrepError("profile %s: not a .csrw file" % a.profile)
    profile_text = read_bytes(profile)            # read once: the snapshot, the compile and the hash use these bytes
    facts = profile_facts(profile, codec, cmap, profile_text)
    if a.manifest:
        if a.width or a.height or a.source_format or a.source_stride:
            raise PrepError("--width/--height/--source-format/--source-stride belong to the --raw form")
        m = load_manifest(a.manifest)
        where = "manifest %s" % a.manifest
    else:
        m = quick_manifest(a.raw, facts, profile, a)
        where = "--raw"
    format_defaulted = not a.manifest and not a.source_format
    m = check_manifest(m, where)
    check_against_profile(m, facts, profile)
    sources = len(m["frames"])
    lay = layout(m, sources)
    frames = 0 if a.loop else (a.frames if a.frames is not None else sources * a.repeat)

    depth = DEPTHS[m["input_depth"]]

    # Every source is read once, checked and converted before anything is written: a refused input leaves
    # no session behind, and a file saved meanwhile cannot replace what was checked. The containers wait in
    # memory, at most the pool budget (layout() has checked it).
    converted = []
    quiet = []
    for k, p in enumerate(m["frames"]):
        data = read_bytes(p)
        cont = normalize(data, m["source_format"], m["width"], m["height"], m["source_stride"], depth,
                         lay["in_stride"], "source %d (%s)" % (k, p))
        if m["source_format"] == "le16_raw10_msb" and not int.from_bytes(data, "little") & int.from_bytes(
                b"\x00\xF0" * (len(data) // 2), "little"):
            quiet.append(k)
        converted.append((p, sha256_bytes(data), cont))
        del data
    if quiet:
        # The zero-bit check is no packing detection: a right-aligned RAW12/RAW10 file passes it as
        # le16_raw10_msb and comes out 16x darker. Say so; a really dark scene stays allowed.
        sys.stderr.write("warning: source(s) %s: every sample is below 0x1000, i.e. under 64/1023 as le16_raw10_msb%s; "
                         "a right-aligned RAW12/RAW10 file looks like this: check the source format\n"
                         % (", ".join(map(str, quiet)), " (the --raw default)" if format_defaulted else ""))
    new_dir(a.out)
    os.makedirs(os.path.join(a.out, "pool"))
    src_info, loads = [], []
    for k, (p, raw_sha, cont) in enumerate(converted):
        out = os.path.join(a.out, "pool", "src_%03d.isp16" % k)
        write_atomic(out, cont)
        addr = POOL_BASE + k * lay["source_pitch"]
        loads.append((addr, out))
        src_info.append({"index": k, "path": p, "name": os.path.basename(p), "sha256": raw_sha,
                         "container": os.path.relpath(out, a.out), "container_sha256": sha256_bytes(cont),
                         "address": "0x%08X" % addr})
    profile_snapshot = os.path.join(a.out, "profile.csrw")
    write_atomic(profile_snapshot, profile_text)
    if sha256_bytes(open(profile_snapshot, "rb").read()) != sha256_bytes(profile_text):
        raise PrepError("profile snapshot differs from %s" % profile)
    write_atomic(os.path.join(a.out, "profile.bin"), facts["bin"])
    session = random.SystemRandom().getrandbits(64) or 1
    desc = {"magic": DESC_MAGIC, "version": VERSION, "size": SIZES["desc"], "flags": FLAG_LOOP if a.loop else 0,
            "session_lo": session & 0xFFFFFFFF, "session_hi": session >> 32, "width": m["width"],
            "height": m["height"], "bayer": BAYER[m["bayer"]], "depth": depth, "in_stride": lay["in_stride"],
            "frame_bytes": lay["frame_bytes"], "source_count": sources, "source_base": POOL_BASE,
            "source_pitch": lay["source_pitch"], "repeat": a.repeat, "frames_lo": frames & 0xFFFFFFFF,
            "frames_hi": frames >> 32, "queue": a.queue, "mode": MODE_LEGACY if a.legacy else MODE_PROFILE,
            "profile_count": facts["writes"], "profile_id": 1, "out_stride": lay["out_stride"],
            "frame_budget_us": FRAME_BUDGET_US}
    write_atomic(os.path.join(a.out, "desc.bin"), pack_block(DESC_FIELDS, desc, SIZES["desc"]))
    loads = [(DESC_ADDR, os.path.join(a.out, "desc.bin")),
             (PROFILE_ADDR, os.path.join(a.out, "profile.bin"))] + loads
    write_atomic(os.path.join(a.out, "loads.txt"), "".join("0x%08X:%s\n" % (ad, p) for ad, p in loads), "w")
    cycle = sources * a.repeat
    write_atomic(os.path.join(a.out, "reference.list"),
                 "".join(os.path.join(a.out, src_info[identity(n, sources, a.repeat)[0]]["container"]) + "\n"
                         for n in range(cycle)), "w")
    resolved = dict(m)
    resolved["frames"] = [s["path"] for s in src_info]
    write_atomic(os.path.join(a.out, "manifest.resolved.json"), json.dumps(resolved, indent=2) + "\n", "w")
    active = active_geometry(m["width"], m["height"], BAYER[m["bayer"]])
    info = {"tool": "isp_stream_prep.py", "session_id": "%016x" % session, "mode": "legacy" if a.legacy else "profile",
            "profile": {"path": profile, "sha256": sha256_bytes(profile_text), "writes": facts["writes"],
                        "bin_sha256": sha256_bytes(facts["bin"])},
            "input": {"form": "manifest" if a.manifest else "raw", "manifest": a.manifest and os.path.abspath(a.manifest),
                      "width": m["width"], "height": m["height"], "bayer": m["bayer"],
                      "source_format": m["source_format"], "source_format_defaulted": format_defaulted,
                      "source_stride": m["source_stride"], "input_depth": m["input_depth"],
                      "active_width": active[0], "active_height": active[1]},
            "schedule": {"sources": sources, "repeat": a.repeat, "frames": frames, "loop": bool(a.loop),
                         "queue": a.queue},
            "layout": dict(lay, **{"desc": "0x%08X" % DESC_ADDR, "profile": "0x%08X" % PROFILE_ADDR}),
            "sources": src_info}
    for k in ("pool_base", "pool_end"):
        info["layout"][k] = "0x%08X" % info["layout"][k]
    info["layout"]["in_slots"] = ["0x%08X" % v for v in lay["in_slots"]]
    info["layout"]["out_slots"] = ["0x%08X" % v for v in lay["out_slots"]]
    write_atomic(os.path.join(a.out, "session.json"), json.dumps(info, indent=2) + "\n", "w")
    print("isp-stream session %s: %d source(s) %dx%d %s %s, repeat %d, %s, queue %d, %s mode, profile %d writes"
          % (info["session_id"], sources, m["width"], m["height"], m["bayer"], m["input_depth"], a.repeat,
             "loop" if a.loop else "%d frame(s)" % frames, a.queue, info["mode"], facts["writes"]))
    print("source format %s%s, active image %dx%d" % (m["source_format"], " (the --raw default: give --source-format "
          "for other files)" if format_defaulted else "", active[0], active[1]))
    print("source pool 0x%08X..0x%08X (%d MiB of %d)" % (POOL_BASE, lay["pool_end"], (lay["pool_end"] - POOL_BASE) >> 20,
                                                        (POOL_END - POOL_BASE) >> 20))
    return 0


def reference(a):
    path = os.path.join(a.session, "session.json")
    if not os.path.isfile(path):
        raise PrepError("%s: not a session (no session.json; run prepare first)" % a.session)
    info = json.load(open(path, encoding="utf-8"))
    sch = info["schedule"]
    if sch["loop"] and not a.frames:
        raise PrepError("a loop session has no frame count: give --frames N for a reference")
    frames = a.frames or sch["frames"]
    out = os.path.join(a.session, "reference")
    new_dir(out)
    # The pool holds DDR images (rows at in_stride, so the firmware copies one linear chain);
    # fx1_isp_run_raw reads packed 2 x width rows and lays them out at the same align16 stride
    # itself. When the two differ, the model gets the pool containers with the padding cut.
    w, h, in_stride = info["input"]["width"], info["input"]["height"], info["layout"]["in_stride"]
    inputs = os.path.join(a.session, "reference.list")
    packed = None
    if in_stride != 2 * w:
        packed = os.path.join(out, "packed")
        os.makedirs(packed)
        names = {}
        lines = []
        for src in open(inputs, encoding="utf-8").read().splitlines():
            if src not in names:
                data = read_bytes(src)
                rows = [data[y * in_stride:(y + 1) * in_stride] for y in range(h)]
                if any(r[2 * w:].strip(b"\0") for r in rows):
                    raise PrepError("%s: non-zero row padding (not a container of this session)" % src)
                names[src] = os.path.join(packed, os.path.basename(src))
                write_atomic(names[src], b"".join(r[:2 * w] for r in rows))
            lines.append(names[src])
        inputs = os.path.join(out, "inputs.list")
        write_atomic(inputs, "".join(l + "\n" for l in lines), "w")
    cmd = [a.run_raw, "--input-list", inputs,
           "--width", str(info["input"]["width"]), "--height", str(info["input"]["height"]),
           "--profile", os.path.join(a.session, "profile.csrw"), "--out", os.path.join(out, "ref"),
           "--frames", str(frames), "--queue", str(sch["queue"]), "--per-frame"]
    if info["mode"] == "profile":
        cmd.append("--profile-mode")
    with open(os.path.join(out, "run_raw.log"), "w") as log:
        rc = subprocess.call(cmd, stdout=log, stderr=subprocess.STDOUT)
    txt = {}
    path = os.path.join(out, "ref.txt")
    if os.path.isfile(path):
        for line in open(path, encoding="utf-8"):
            k, _, v = line.strip().partition(" ")
            txt[k] = v
    if rc != 0 or int(txt.get("completed", -1)) != frames:
        print("reference: fx1_isp_run_raw exit %d, %s of %d frame(s) completed (%s)"
              % (rc, txt.get("completed", "?"), frames, os.path.join(out, "run_raw.log")), file=sys.stderr)
        return 3
    srcs = info["sources"]
    per = []
    for n in range(frames):
        k, r = identity(n, sch["sources"], sch["repeat"])
        col = txt.get("frame_colorimetry_%d" % n, "").split()
        per.append({"ordinal": n, "source_index": k, "repeat_index": r, "name": srcs[k]["name"],
                    "sha256": txt["frame_sha256_%d" % n],
                    "frame_id": int(col[1]) if len(col) > 1 else None})
    ref = {"session_id": info["session_id"], "command": cmd, "width": int(txt["width"]), "height": int(txt["height"]),
           "csc_std": int(txt.get("csc_std", -1)), "csc_full_range": int(txt.get("csc_full_range", -1)),
           "dma_err": txt.get("dma_err"), "frames": per}
    write_atomic(os.path.join(a.session, "reference.json"), json.dumps(ref, indent=2) + "\n", "w")
    if packed:
        for f in os.listdir(packed):   # derived from the pool, as large: not kept
            os.remove(os.path.join(packed, f))
        os.rmdir(packed)
    for f in os.listdir(out):   # keep the hashes, drop the per-frame NV12 of the model
        if f.endswith(".nv12") and not a.keep_nv12:
            os.remove(os.path.join(out, f))
    print("reference: %d frame(s), %dx%d, last %s" % (frames, ref["width"], ref["height"], per[-1]["sha256"][:12]))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("prepare")
    p.add_argument("--out", required=True)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--manifest")
    g.add_argument("--raw", nargs="+")
    p.add_argument("--profile", required=True)
    p.add_argument("--repeat", type=int, default=1)
    f = p.add_mutually_exclusive_group()
    f.add_argument("--frames", type=int)
    f.add_argument("--loop", action="store_true")
    p.add_argument("--queue", type=int, default=1)
    p.add_argument("--legacy", action="store_true")
    p.add_argument("--width", type=int)
    p.add_argument("--height", type=int)
    p.add_argument("--source-format", choices=sorted(FORMATS))
    p.add_argument("--source-stride", type=int)
    p.add_argument("--codec-dir")
    p.add_argument("--csr-header")
    p.set_defaults(func=prepare)
    r = sub.add_parser("reference")
    r.add_argument("--session", required=True)
    r.add_argument("--run-raw", required=True)
    r.add_argument("--frames", type=int)
    r.add_argument("--keep-nv12", action="store_true")
    r.set_defaults(func=reference)
    a = ap.parse_args(argv)
    try:
        return a.func(a)
    except PrepError as e:
        print("isp-stream: %s" % e, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
