#!/usr/bin/env python3
"""Build one NPU gate program for test_fx1_npu: the drop's generator writes the
program (mmio.txt, dram_init.bin), the drop's independent numpy oracle
(tools/has/ref_program.py) writes the reference image. The generator's own
dram_golden.bin is never used.

usage: make_gate.py <model root> <kind> <out dir> [<host_trace_gen> <host_trace_replay> <base>...]
  kind: ref_mp pad neo ref avgpool nd_big slice_concat yolov8m dma_tensor vit_heads

With the two trace tools of the drop (tools/has/host_trace_gen.cpp,
host_trace_replay.cpp), also writes out/neo_window_<base>.txt for each window
base: the program as the CPU drives neo_lite, every instruction the hardware
runs as given through the 82-word register file and the doorbell (0x17), with
the system addresses of a window at <base> (host_trace_replay --emit-neo-mmio
--neo-dram-add <base>).

Exit 0 when out/mmio.txt, out/dram_init.bin and out/oracle_golden.bin exist
and the oracle checked every instruction (it reports MISMATCH against the
initial image, which is expected: only the reference it writes is used).
"""
import os
import re
import subprocess
import sys

GENERATORS = {
    "ref_mp": ["tools/has/make_ref_gate_mp.py"],
    "pad": ["tools/has/make_pad_gate.py"],
    "neo": ["tools/has/make_neo_window_gate.py"],
    # plan N4: more instruction forms of the drop's own check programs
    "ref": ["tools/has/make_ref_gate.py"],                   # GEMM_FUSED 3x3 / residual / without extension registers
    "avgpool": ["tools/has/make_avgpool_gate.py"],           # ELEM_WISE AVG_POOL
    "nd_big": ["tools/has/make_dma_nd_big_gate.py"],         # ELEM_WISE slice / concat larger than SRAM-A
    "slice_concat": ["tools/has/make_ref_gate_dma_slice_concat.py"],  # 4-D HasDma slice / concat / tile blocks
    "yolov8m": ["tools/has/make_yolov8m_full_gate.py"],      # 117 YOLOv8m steps at 64 x 64, channels / 8
    "dma_tensor": ["tools/has/make_dma_tensor_gate.py"],     # DMA_TENSOR COPY (outside the FX1 ABI)
    "vit_heads": ["tools/has/make_vit_heads_gate.py", "--heads", "2", "--tokens", "32"],  # FUSED_ATTN (outside the ABI)
}
# Programs of instructions outside the FX1 ABI: only the program is made (the
# clone must refuse every push: test_fx1_npu gate_refused); no reference image.
PROGRAM_ONLY = {"nd_big", "vit_heads"}


def main():
    if len(sys.argv) < 4 or len(sys.argv) == 5 or sys.argv[2] not in GENERATORS:
        print(__doc__)
        return 2
    root, kind, out = sys.argv[1], sys.argv[2], os.path.abspath(sys.argv[3])
    os.makedirs(out, exist_ok=True)
    gen = [sys.executable, os.path.join(root, GENERATORS[kind][0]), out] + GENERATORS[kind][1:]
    r = subprocess.run(gen, cwd=root, capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    sys.stderr.write(r.stderr)
    if r.returncode != 0:
        print(f"make_gate: generator failed ({r.returncode})")
        return 1
    if kind in PROGRAM_ONLY:
        ok = os.path.exists(os.path.join(out, "mmio.txt")) and os.path.exists(os.path.join(out, "dram_init.bin"))
        print("make_gate: %s: program only (outside the FX1 ABI)%s" % (kind, "" if ok else ": MISSING FILES"))
        return 0 if ok else 1
    golden = os.path.join(out, "oracle_golden.bin")
    if os.path.exists(golden):
        os.remove(golden)
    ref = [sys.executable, os.path.join(root, "tools/has/ref_program.py"), out,
           "--image", os.path.join(out, "dram_init.bin"), "--write-ref", golden]
    r = subprocess.run(ref, cwd=root, capture_output=True, text=True)
    sys.stdout.write(r.stdout[-2000:])
    m = re.search(r"checked (\d+): match \d+, mismatch \d+; not checked (\d+); refused or in error (\d+)", r.stdout)
    if not os.path.exists(golden) or not m:
        print("make_gate: the oracle wrote no reference image")
        return 1
    checked, not_checked, refused = (int(x) for x in m.groups())
    if checked == 0 or not_checked or refused:
        print(f"make_gate: oracle checked {checked}, not checked {not_checked}, refused {refused}")
        return 1
    if os.path.getsize(golden) != os.path.getsize(os.path.join(out, "dram_init.bin")):
        print("make_gate: reference and initial image differ in size")
        return 1
    print(f"make_gate: {kind}: {checked} instructions, reference {golden}")
    if len(sys.argv) > 5:
        trace_gen, trace_replay, bases = sys.argv[4], sys.argv[5], sys.argv[6:]
        trace = os.path.join(out, "program.trace")
        env = {k: v for k, v in os.environ.items() if not k.startswith("HAS_")}  # the drop's defaults
        r = subprocess.run([trace_gen, out], env=dict(env, HAS_HOST_TRACE=trace), capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout + r.stderr)
            print("make_gate: host_trace_gen failed")
            return 1
        for base in bases:
            path = os.path.join(out, "neo_window_%s.txt" % base)
            r = subprocess.run([trace_replay, "--emit-neo-mmio", path, "--neo-dram-add", base, trace], env=env,
                               capture_output=True, text=True)
            m = re.search(r"(\d+) of (\d+) instructions through the neo window", r.stdout)
            if r.returncode != 0 or "RESULT: PASS" not in r.stdout or not m or m.group(1) == "0":
                print(r.stdout + r.stderr)
                print("make_gate: host_trace_replay failed or sent nothing through the window")
                return 1
            print(f"make_gate: {path}: {m.group(1)} of {m.group(2)} instructions through the neo window")
    return 0


if __name__ == "__main__":
    sys.exit(main())
