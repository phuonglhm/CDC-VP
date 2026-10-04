#!/usr/bin/env python3
"""Convert a HasNpuTop program directory into the stream that the firmware replay engine executes (npu_has_fw.h).

Input : <insts dir>/mmio.txt, dram_init.bin, dram_golden.bin   (tools/fe/fe_emit_insts.py, tools/fe/fe_vit_full.py,
        tools/has/make_rce_gate.py; shipped programs are in the data package)
Output: <out>.nhp      stream of little-endian 32-bit words: register writes, host steps, and after every instruction
                       or host step the CRC-32 of its expected output (taken from dram_golden.bin), so that firmware
                       can check the result without holding the golden image
        <out>.S        (--asm) assembly file that places the stream and dram_init.bin into two sections of the
                       firmware ELF with .incbin (symbols npu_has_stream, npu_has_image)

usage: python3 make_fw_program.py <insts dir> <out prefix> [--count N] [--asm]
       --count N : only the first N instructions (host steps before the last one included)

The output region of each instruction is computed exactly as tools/has/wrapper/test_npu_tlm_core_rtl.cpp does.
"""
import argparse
import os
import struct
import zlib

MAGIC, VERSION = 0x3150484E, 1
REC_END, REC_WRITE, REC_HOST, REC_CHECK = 0, 1, 2, 3
PUSH_A, EXT_BASE, EXT_END = 0x40000310, 0x4000046C, 0x400004E8
LN_MAGIC = 0x31504E4C


def ln_out_int16(init, addr):
    """16-bit output flag of an LNP1 parameter block: word 0 magic, word 1 H, then 19 int64 scalars, the last one."""
    if addr + 8 + 19 * 8 > len(init) or struct.unpack_from("<I", init, addr)[0] != LN_MAGIC:
        return False
    return struct.unpack_from("<q", init, addr + 8 + 18 * 8)[0] != 0


def out_bytes(op, sticky, ext, init):
    pack = sticky.get(0x454, 0)
    mode = (pack >> 24) & 0xFF if pack & 0xFFFF0000 else pack
    if op == 0x12:                                   # GEMM_FUSED: OUT_C x OUT_H x OUT_W
        return ext[3] * ext[4] * ext[5]
    if op == 0x15 and mode == 0:                     # ELEM_WISE ADD: LEN
        return sticky.get(0x450, 0)
    if op == 0x15:                                   # MAX_POOL / AVG_POOL
        k, pd, sd = ext[23], ext[24], sticky.get(0x424, 1)
        return ext[0] * ((ext[1] + 2 * pd - k) // sd + 1) * ((ext[2] + 2 * pd - k) // sd + 1)
    if op == 0x13:                                   # FUSED_ATTN: [NQ][D]
        return (ext[28] or sticky.get(0x450, 0)) * sticky.get(0x458, 0)
    if op == 0x14:                                   # LAYERNORM: [rows][H], int16 when the block asks for it
        return ext[28] * sticky.get(0x450, 0) * (2 if ln_out_int16(init, ext[29]) else 1)
    raise SystemExit("opcode 0x%02x is not supported by this converter" % op)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("insts_dir")
    ap.add_argument("out_prefix")
    ap.add_argument("--count", type=int, default=-1)
    ap.add_argument("--asm", action="store_true")
    a = ap.parse_args()

    init = open(os.path.join(a.insts_dir, "dram_init.bin"), "rb").read()
    gold = open(os.path.join(a.insts_dir, "dram_golden.bin"), "rb").read()
    words, sticky, ext = [], {}, [0] * 31
    n_ins = n_host = 0

    def check(off, n):
        if off + n > len(gold):
            raise SystemExit("output region 0x%x + %d is outside the golden image" % (off, n))
        words.extend((REC_CHECK, off, n, zlib.crc32(gold[off:off + n]) & 0xFFFFFFFF))

    for line in open(os.path.join(a.insts_dir, "mmio.txt")):
        if a.count >= 0 and n_ins >= a.count:
            break
        line = line.strip()
        if not line or line[0] == "#":
            continue
        tok = line.split()
        if tok[0] == "H":
            if tok[1] != "upsample2x":
                raise SystemExit("unsupported host step: " + line)
            kv = dict(t.split("=") for t in tok[2:])
            src, dst, c, h, w = (int(kv[k], 0) for k in ("in", "out", "c", "h", "w"))
            words.extend((REC_HOST, src, dst, c, h, w))
            check(dst, c * 2 * h * 2 * w)
            n_host += 1
            continue
        if tok[0] != "W":
            raise SystemExit("unknown line: " + line)
        addr, val = int(tok[1], 16), int(tok[2], 16)
        if EXT_BASE <= addr < EXT_END:
            ext[(addr - EXT_BASE) // 4] = val
        elif 0x40000400 <= addr <= 0x40000468:
            sticky[addr & 0xFFF] = val
        words.extend((REC_WRITE, addr, val))
        if addr == PUSH_A:
            check(sticky.get(0x408, 0), out_bytes(val & 0xFF, sticky, ext, init))
            ext = [0] * 31
            n_ins += 1
    words.append(REC_END)
    if a.count >= 0 and n_ins < a.count:
        raise SystemExit("only %d instructions in the program" % n_ins)

    hdr = [MAGIC, VERSION, 6 + len(words), len(init), n_ins, n_host]
    blob = struct.pack("<%dI" % (6 + len(words)), *(hdr + words))
    with open(a.out_prefix + ".nhp", "wb") as f:
        f.write(blob)
    print("%s.nhp: %d instructions, %d host steps, %d bytes; image %d bytes" % (a.out_prefix, n_ins, n_host, len(blob), len(init)))
    if a.asm:
        with open(a.out_prefix + ".S", "w", newline="\n") as f:
            f.write("/* Generated by make_fw_program.py: program stream and DRAM image for the firmware ELF. */\n"
                    "    .section .npu_has_stream, \"a\"\n    .balign 4\n    .globl npu_has_stream\nnpu_has_stream:\n"
                    "    .incbin \"%s\"\n"
                    "    .section .npu_has_image, \"aw\"\n    .balign 4096\n    .globl npu_has_image\nnpu_has_image:\n"
                    "    .incbin \"%s\"\n    .balign 4096\n    .globl npu_has_image_end\nnpu_has_image_end:\n"
                    % (os.path.abspath(a.out_prefix + ".nhp").replace("\\", "/"),
                       os.path.abspath(os.path.join(a.insts_dir, "dram_init.bin")).replace("\\", "/")))
        print("%s.S written" % a.out_prefix)


if __name__ == "__main__":
    main()
