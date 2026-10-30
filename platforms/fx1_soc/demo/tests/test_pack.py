#!/usr/bin/env python3
"""run_isp_demo.py --pack-only on small synthetic dumps (review G6a R2).

One 4x4 job packs to a 24-byte NV12 and a PNG; every damaged result block,
dump, job or preview must make the script exit 2 and leave no platform.nv12
or platform.png behind. No simulator is needed: the dumps are written here
the way fx1_soc --dump writes them. Python stdlib only, like isp_pack.py.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "..", "run_isp_demo.py")
JOB_MAGIC, RESULT_MAGIC = 0x4A315846, 0x52315846
W = H = 4
STRIDE = 16                       # align(W, 16), what the script computes
Y_LEN, UV_LEN = STRIDE * H, STRIDE * (H // 2)
CH2_STRIDE = 16                   # align(3 W, 16)
PACK = os.path.join(HERE, "..", "isp_pack.py")
AUTO = os.path.join(HERE, "..", "..", "package", "test", "isp_auto.py")
CH1 = {"y1.bin": bytes(range(Y_LEN)), "uv1.bin": bytes(UV_LEN)}
CH2 = {"rgb2.bin": bytes(CH2_STRIDE * H)}


def job(version=2, **kw):
    """Version 2 (96 bytes: mode 0 profile, outputs CH0) or version 1 (64 bytes, legacy)."""
    f = dict(width=W, height=H, in_stride=16, out_stride=STRIDE, profile_count=1, frames=1, mode=0, outputs=1)
    f.update(kw)
    head = struct.pack("<12I", JOB_MAGIC, version, f["width"], f["height"], 3, 0x81000000, f["in_stride"],
                       0x88000000, 0x88100000, f["out_stride"], f["profile_count"], f["frames"])
    if version == 1:
        return head + struct.pack("<4I", 0, 0, 0, 0)
    return head + struct.pack("<12I", f["mode"], f["outputs"], 1, 0x88200000, 0x88300000, STRIDE, 0x88400000,
                              CH2_STRIDE, 0, 0, 0, 0)


def result(**kw):
    f = dict(magic=RESULT_MAGIC, status=0, out_width=W, out_height=H, out_stride=STRIDE, frames_done=1,
             idma_frames=1, odma_frames=1, dma_err=2, aec=1, awb=1, af=1, ticks=0,
             mode=0, outputs=1, profile_id=1, tune_error=0, ch1_w=0, ch1_h=0, ch2_w=0, ch2_h=0)
    f.update(kw)
    return struct.pack("<21I", *f.values())


def case(name, expect, files=None, drop=(), png_dir=False, profile=None, tool="demo", args=(), want_files=()):
    d = tempfile.mkdtemp(prefix="fx1_pack_")
    try:
        content = {"job.bin": job(), "result.bin": result(),
                   "y.bin": bytes(range(Y_LEN)), "uv.bin": bytes(128 + i % 64 for i in range(UV_LEN))}
        content.update(files or {})
        for n, data in content.items():
            if n not in drop:
                open(os.path.join(d, n), "wb").write(data)
        with open(os.path.join(d, "p.csrw"), "w") as f:
            f.write("DMA_CTRL 0x0\n")
        if png_dir:
            os.mkdir(os.path.join(d, "platform.png"))   # the preview cannot write its output
        else:
            open(os.path.join(d, "platform.png"), "wb").write(b"stale")
        cmd = ([sys.executable, SCRIPT, "--pack-only", "--no-reference", "--out", d,
                "--profile", profile or os.path.join(d, "p.csrw")] if tool == "demo"
               else [sys.executable, PACK, d])  # the SDK's entrypoint (run_vp.sh)
        r = subprocess.run(cmd + list(args), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        nv12, png = os.path.join(d, "platform.nv12"), os.path.join(d, "platform.png")
        if expect == 0:
            ok = (r.returncode == 0 and os.path.getsize(nv12) == W * H * 3 // 2 and os.path.isfile(png)
                  and open(png, "rb").read(8) == b"\x89PNG\r\n\x1a\n"
                  and all(os.path.isfile(os.path.join(d, n)) and os.path.getsize(os.path.join(d, n)) == size
                          for n, size in want_files))
        else:
            ok = (r.returncode == expect and not os.path.exists(nv12)
                  and (png_dir or not os.path.exists(png))
                  and not any(os.path.exists(os.path.join(d, n)) for n in ("platform_ch1.nv12", "platform_ch2.rgb")))
        last = (r.stdout.strip().splitlines() or [""])[-1]
        print("%-4s %-44s exit %d  %s" % ("ok" if ok else "FAIL", name, r.returncode, last))
        return ok
    finally:
        shutil.rmtree(d)


def auto_cases():
    """isp_auto.py prepare (SDK `run_vp.sh isp auto`): version 1 and 2 jobs keep
    their size, the frame count grows by the probe, a version-2 job turns legacy."""
    ok_all = []
    for version in (1, 2):
        d = tempfile.mkdtemp(prefix="fx1_auto_")
        try:
            src = os.path.join(d, "job.bin")
            open(src, "wb").write(job(version=version, frames=3))
            env = dict(os.environ, PYTHONPATH=os.path.dirname(PACK))
            r = subprocess.run([sys.executable, AUTO, "prepare", src, src], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, env=env)
            data = open(src, "rb").read()
            words = struct.unpack("<%dI" % (len(data) // 4), data)
            ok = (r.returncode == 0 and len(data) == (64 if version == 1 else 96) and words[11] == 4
                  and (version == 1 or words[12] == 1))
            print("%-4s %-44s exit %d  %s" % ("ok" if ok else "FAIL", "isp_auto prepare, job version %d" % version,
                                               r.returncode, (r.stdout.strip().splitlines() or [""])[-1]))
            ok_all.append(ok)
        finally:
            shutil.rmtree(d)
    return ok_all


def main():
    default_profile = os.path.join(HERE, "..", "..", "..", "..", "components", "FX1_Components", "isp", "samples",
                                   "profiles", "outdoor1_bggr_2688x1520_full.csrw")
    results = [
        case("valid 4x4 job", 0),
        case("Y and UV dumps of 1 byte (review repro)", 2, {"y.bin": b"\0", "uv.bin": b"\0"}),
        case("Y dump one byte short", 2, {"y.bin": bytes(Y_LEN - 1)}),
        case("UV dump one byte long", 2, {"uv.bin": bytes(UV_LEN + 1)}),
        case("result block truncated", 2, {"result.bin": result()[:48]}),
        case("result magic wrong", 2, {"result.bin": result(magic=0)}),
        case("result status failed", 2, {"result.bin": result(status=1)}),
        case("stride below width", 2, {"result.bin": result(out_stride=2)}),
        case("odd output height", 2, {"result.bin": result(out_height=3)}),
        case("zero output width", 2, {"result.bin": result(out_width=0)}),
        case("output larger than input", 2, {"result.bin": result(out_width=8)}),
        case("no frame completed", 2, {"result.bin": result(frames_done=0, odma_frames=0)}),
        case("job.bin missing", 2, drop=("job.bin",)),
        case("job.bin of another version", 2, {"job.bin": job()[:4] + struct.pack("<I", 9) + job()[8:]}),
        # Version 2 (V107-10): profile mode by default; version 1 still packs, as legacy.
        case("version-1 job, legacy result", 0, {"job.bin": job(version=1), "result.bin": result(mode=1, profile_id=0)}),
        case("legacy job, legacy result", 0, {"job.bin": job(mode=1), "result.bin": result(mode=1, profile_id=0)}),
        case("profile job, firmware ran legacy", 2, {"result.bin": result(mode=1, profile_id=0)}),
        case("profile job, profile ID not active", 2, {"result.bin": result(profile_id=0)}),
        case("legacy job, firmware ran profile mode", 2, {"job.bin": job(mode=1), "result.bin": result()}),
        case("outputs differ from the job's", 2, {"result.bin": result(outputs=3)}),
        # Review V107-10 completion R2: the CH1 / CH2 dumps and geometry are checked.
        case("CH1 + CH2 valid", 0, dict(CH1, **CH2, **{"job.bin": job(outputs=7),
                                                     "result.bin": result(outputs=7, ch1_w=W, ch1_h=H, ch2_w=W, ch2_h=H)}),
             want_files=(("platform_ch1.nv12", W * H * 3 // 2), ("platform_ch2.rgb", 3 * W * H))),
        case("CH1 dumps empty", 2, {"job.bin": job(outputs=3), "result.bin": result(outputs=3, ch1_w=W, ch1_h=H),
                                    "y1.bin": b"", "uv1.bin": b""}),
        case("CH1 UV dump short", 2, dict(CH1, **{"job.bin": job(outputs=3), "uv1.bin": bytes(UV_LEN - 1),
                                                  "result.bin": result(outputs=3, ch1_w=W, ch1_h=H)})),
        case("CH1 dump missing", 2, {"job.bin": job(outputs=3), "result.bin": result(outputs=3, ch1_w=W, ch1_h=H)}),
        case("CH1 geometry zero", 2, dict(CH1, **{"job.bin": job(outputs=3), "result.bin": result(outputs=3)})),
        case("CH2 dump short", 2, {"job.bin": job(outputs=5), "rgb2.bin": bytes(CH2_STRIDE * H - 1),
                                   "result.bin": result(outputs=5, ch2_w=W, ch2_h=H)}),
        case("CH2 wider than its stride", 2, dict(CH2, **{"job.bin": job(outputs=5),
                                                          "result.bin": result(outputs=5, ch2_w=W + 2, ch2_h=H)})),
        # Review V107-10 completion R3: the SDK entrypoint isp_pack.py checks the identity too.
        case("isp_pack: valid", 0, tool="pack"),
        case("isp_pack: profile job, legacy result", 2, {"result.bin": result(mode=1, profile_id=0)}, tool="pack"),
        case("isp_pack: legacy job, profile result", 2, {"job.bin": job(mode=1)}, tool="pack"),
        case("isp_pack: outputs differ", 2, {"result.bin": result(outputs=3)}, tool="pack"),
        case("isp_pack: profile ID differs", 2, {"result.bin": result(profile_id=7)}, tool="pack"),
        case("isp_pack: --expect-mode legacy, legacy result", 0, {"result.bin": result(mode=1, profile_id=0)},
             tool="pack", args=("--expect-mode", "legacy")),
        case("isp_pack: --expect-mode legacy, profile result", 2, tool="pack", args=("--expect-mode", "legacy")),
        case("isp_pack: CH1 dumps empty", 2, {"job.bin": job(outputs=3), "result.bin": result(outputs=3, ch1_w=W, ch1_h=H),
                                              "y1.bin": b"", "uv1.bin": b""}, tool="pack"),
        case("profile refused by the firmware", 2, {"result.bin": result(status=2, tune_error=0x4)}),
        case("job mode out of range", 2, {"job.bin": job(mode=5)}),
        case("job outputs without CH0", 2, {"job.bin": job(outputs=2)}),
        case("--profile differs from the job's", 2, profile=default_profile),
        case("preview cannot write the PNG", 2, png_dir=True),
    ]
    results += auto_cases()
    print("%d/%d pack cases pass" % (sum(results), len(results)))
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
