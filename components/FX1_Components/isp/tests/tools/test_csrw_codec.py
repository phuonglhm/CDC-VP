#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Profile field syntax (ISP-UPD-008): tools/csrw_codec.py and the model's reader.

    test_csrw_codec.py CSRW_DUMP OUT_DIR

  - grammar: REGISTER VALUE, REGISTER_field VALUE, REGISTER f=V ...; one line
    is one full-word write, unnamed fields 0, no read-modify-write, order kept;
  - the plan's cases: Gamma enable/confirm, AEC en+commit (and the
    two-line trap), BLC G5 + scale, LSC begin/validate, EE select/confirm,
    AWB grid, fields of 1/2/3/12/18/22/32 bits, LSC exact 8.0, CCM -3;
  - errors with file:line: unknown names, RO field, value too wide, field
    given twice, tied-zero bit, negative/malformed literal, missing value,
    trailing tokens, a field of another register;
  - the Python codec and the C++ reader (CSRW_DUMP) give the same stream, or
    both an error, for every case, every committed profile/vector and the
    migrated samples;
  - emit: every sample and vector profile re-emitted in field syntax keeps
    its stream write for write; the shipped samples are in field syntax and
    equal their v1.07-syntax streams (binary check in the SDK gate);
  - bin: little-endian {offset, value}; an error leaves an existing .bin;
  - review B/C/P/D R3: lines and tokens as the C++ reader: a UTF-8 byte order
    mark opening the file (before a register, an alias, a group or a comment,
    with CRLF) is ignored by both readers, the model's frames.csrw reader
    too; one elsewhere is an error on the same line; ASCII white space only
    (NBSP, U+001C are not separators), '\n' ends a line (a lone '\r' does
    not), non-UTF-8 bytes in a comment are accepted and kept by emit.
"""

import glob
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ISP = os.path.join(HERE, "..", "..")
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(ISP, "tools"))
import csrw_codec as codec  # noqa: E402

failures = []


def check(cond, what):
    if not cond:
        failures.append(what)
        print("FAIL: " + what)


def main():
    dump, out = sys.argv[1], os.path.join(sys.argv[2], "csrw codec")
    os.makedirs(out, exist_ok=True)
    m = codec.CsrMap()
    check(m.extension == 0x20261109 and len(m.regs) == 621, "map: %d registers, extension %r" % (len(m.regs), m.extension))

    def py(path):
        try:
            return ["%s 0x%08X" % (r, v) for r, v in codec.writes(path, m)], None
        except codec.CsrwError as e:
            return None, str(e)

    def cxx(path):
        r = subprocess.run([dump, path], capture_output=True, text=True, errors="surrogateescape")
        return (r.stdout.split("\n")[:-1], None) if r.returncode == 0 else (None, r.stderr.strip())

    def case(name, text, want=None, error=None):
        p = os.path.join(out, name + ".csrw")
        if isinstance(text, bytes):
            open(p, "wb").write(text)
        else:
            open(p, "w", newline="").write(text)
        a, ea = py(p)
        b, eb = cxx(p)
        if error:
            check(a is None and b is None and error in (ea or "") and error in (eb or ""),
                  "%s: both must fail with %r (python %r, c++ %r)" % (name, error, ea, eb))
            check(ea is not None and eb is not None and ea.split(": ", 1)[0] == eb.split(": ", 1)[0],
                  "%s: same file:line (python %r, c++ %r)" % (name, ea, eb))
        else:
            check(a == b and a is not None, "%s: python %r / %r, c++ %r / %r" % (name, a, ea, b, eb))
            if want is not None:
                check(a == ["%s 0x%08X" % w for w in want], "%s: %r, expected %r" % (name, a, want))

    # Forms and the plan's examples.
    case("legacy", "GAMMA_CTRL 0x100\nGAMMA_CTRL 0x001\n", [("GAMMA_CTRL", 0x100), ("GAMMA_CTRL", 1)])
    case("gamma_alias", "GAMMA_CTRL_lut_load_confirm 1\nGAMMA_CTRL_en 1\n", [("GAMMA_CTRL", 0x100), ("GAMMA_CTRL", 1)])
    case("aec_group", "AEC_CTRL en=1 commit=1\n", [("AEC_CTRL", 3)])
    case("aec_two_lines_are_two_writes", "AEC_CTRL_en 1\nAEC_CTRL_commit 1\n", [("AEC_CTRL", 1), ("AEC_CTRL", 2)])
    case("blc_g5_scale", "BLC_GAIN_SEL gain_level=5 range_scale_en=1\n", [("BLC_GAIN_SEL", 0x105)])
    case("lsc_begin_validate", "LSC_LOAD_CTRL load_profile=1 load_begin=1\nLSC_LOAD_CTRL load_validate=1\n",
         [("LSC_LOAD_CTRL", 0x101), ("LSC_LOAD_CTRL", 0x200)])
    case("ee_select_confirm", "EE_CTRL en=1 profile_sel=2 profile_wr_sel=1\nEE_LUT_CTRL lut_sel=3 profile_load_confirm=1\n",
         [("EE_CTRL", 1 | (2 << 2) | (1 << 4)), ("EE_LUT_CTRL", (3 << 1) | (1 << 8))])
    case("awb_grid", "AWB_CTRL en=1 num_zone_x=16 num_zone_y=16\n", [("AWB_CTRL", 0x1021)])
    case("lsc_exact8_22bit", "LSC_COEF_DATA_coef_data 0x200000\nLSC_COEF_DATA 0x3FFFFF\n",
         [("LSC_COEF_DATA", 0x200000), ("LSC_COEF_DATA", 0x3FFFFF)])
    case("scale_18bit", "BLC_SCALE_G7_B_scale_g7_b 0x3FFFF\n", [("BLC_SCALE_G7_B", 0x3FFFF)])
    case("ccm_minus3_code", "CCM_OFS_G_offset_g 0xFFD\n", [("CCM_OFS_G", 0xFFD)])
    case("field_32bit", "AEC_CONTEXT_ID_context_id 0xFFFFFFFF\n", [("AEC_CONTEXT_ID", 0xFFFFFFFF)])
    case("comments_blank_crlf", "# head\r\n\r\nGAMMA_CTRL en=1  # on\r\n   \r\n", [("GAMMA_CTRL", 1)])
    case("zero_group", "AEC_CTRL en=0\n", [("AEC_CTRL", 0)])
    # Errors.
    case("err_unknown", "FOO_BAR 1\n", error="unknown register or field FOO_BAR")
    case("err_unknown_field", "AEC_CTRL en=1 bogus=1\n", error="has no field")
    case("err_ro", "AEC_STATUS busy=1\n", error="not writable")
    case("err_ro_alias", "AEC_STATUS_busy 1\n", error="not writable")
    case("w1c_alias", "AEC_STATUS_stat_done 1\n", [("AEC_STATUS", 2)])
    case("err_too_wide", "GAMMA_CTRL_lut_load_confirm 0x100\n", error="does not fit")
    case("err_twice", "AEC_CTRL en=1 en=1\n", error="given twice")
    case("tied_zero_ok", "IDMA_BUF_VALID_idma_buf_valid 0xF\n", [("IDMA_BUF_VALID", 0xF)])
    case("err_tied_zero", "IDMA_BUF_VALID_idma_buf_valid 0x10\n", error="tied-zero")
    case("err_negative", "CCM_OFS_G_offset_g -3\n", error="negative")
    case("err_literal", "GAMMA_CTRL 010\n", error="invalid integer")
    case("err_literal_hex", "GAMMA_CTRL 0xZZ\n", error="invalid integer")
    case("err_no_value", "GAMMA_CTRL\n", error="has no value")
    case("err_trailing", "GAMMA_CTRL 1 2\n", error="expected")
    case("err_mixed", "GAMMA_CTRL en=1 0x100\n", error="expected")
    case("err_other_register_field", "AEC_CTRL lut_load_confirm=1\n", error="has no field")
    case("err_assign_form", "AEC_CTRL en=\n", error="malformed")
    case("err_legacy_33bit", "GAMMA_CTRL 0x100000000\n", error="32 bits")
    case("err_line_number", "GAMMA_CTRL 1\n\nFOO 1\n", error=":3:")
    # Review B/C/P/D R3: the byte order mark, white space and line ends of the C++ reader.
    bom = b"\xef\xbb\xbf"
    case("bom_register", bom + b"GAMMA_CTRL 0x100\n", [("GAMMA_CTRL", 0x100)])
    case("bom_alias", bom + b"GAMMA_CTRL_en 1\n", [("GAMMA_CTRL", 1)])
    case("bom_group_crlf", bom + b"AEC_CTRL en=1 commit=1\r\nGAMMA_CTRL_en 1\r\n", [("AEC_CTRL", 3), ("GAMMA_CTRL", 1)])
    case("bom_comment_crlf", bom + b"# tuning\r\nGAMMA_CTRL en=1\r\n", [("GAMMA_CTRL", 1)])
    case("bom_only", bom, [])
    case("err_bom_line2", b"GAMMA_CTRL 1\n" + bom + b"GAMMA_CTRL 1\n", error=":2: unknown register or field")
    case("err_double_bom", bom + bom + b"GAMMA_CTRL 1\n", error=":1: unknown register or field")
    case("err_nbsp", b"GAMMA_CTRL\xc2\xa01\n", error=":1:")
    case("err_unit_separator", b"GAMMA_CTRL\x1c1\n", error=":1:")
    case("err_lone_cr", b"GAMMA_CTRL 1\rGAMMA_CTRL 0\r", error=":1: expected")
    case("err_line_separator", "GAMMA_CTRL 1\u2028GAMMA_CTRL 0\n".encode(), error=":1: expected")
    case("form_feed_tab", b"GAMMA_CTRL\x0c\t1\n", [("GAMMA_CTRL", 1)])
    case("latin1_comment", b"GAMMA_CTRL en=1 # gi\xe1 tr\xef\nGAMMA_CTRL 0\n", [("GAMMA_CTRL", 1), ("GAMMA_CTRL", 0)])
    case("err_latin1_name", b"GAMMA_CTRL\xe1 1\n", error=":1: unknown register or field")
    r = subprocess.run([sys.executable, "-B", os.path.join(ISP, "tools", "csrw_codec.py"), "emit",
                        os.path.join(out, "latin1_comment.csrw")], capture_output=True)
    check(r.returncode == 0 and r.stdout == b"GAMMA_CTRL en=1  # gi\xe1 tr\xef\nGAMMA_CTRL en=0\n",
          "emit keeps a non-UTF-8 comment: %r %r" % (r.stdout, r.stderr))
    fr = os.path.join(out, "frames_bom.csrw")
    open(fr, "wb").write(bom + b"2 GAMMA_CTRL_en 1  # c\r\n3 AEC_CTRL en=1 commit=1\r\n")
    r = subprocess.run([dump, "--frames", fr], capture_output=True, text=True)
    check(r.returncode == 0 and r.stdout == "2 GAMMA_CTRL 0x00000001\n3 AEC_CTRL 0x00000003\n",
          "frames.csrw reader with a byte order mark: %r %r" % (r.stdout, r.stderr))

    # Every committed profile: Python = C++, and the field emit keeps the stream.
    files = sorted(glob.glob(os.path.join(ISP, "samples", "profiles", "*.csrw")) +
                   glob.glob(os.path.join(ISP, "tests", "data", "vectors", "*", "profile.csrw")) +
                   glob.glob(os.path.join(ISP, "tests", "data", "4k", "*", "profile.csrw")))
    same = emitted = 0
    for p in files:
        a, ea = py(p)
        b, eb = cxx(p)
        check(a is not None and a == b, "%s: python and c++ streams differ (%r / %r)" % (p, ea, eb))
        same += a is not None and a == b
        e = os.path.join(out, "emit_" + os.path.basename(os.path.dirname(p)) + "_" + os.path.basename(p))
        codec.atomic_write(e, codec._emit(p, m, "fields"), "w")
        check(py(e)[0] == a and cxx(e)[0] == a, "%s: emitted field syntax changes the stream" % p)
        emitted += py(e)[0] == a
    print("%d profiles: python = c++ %d, field emit identical %d" % (len(files), same, emitted))
    for p in glob.glob(os.path.join(ISP, "samples", "profiles", "*.csrw")):
        text = open(p).read()
        check(" range_scale_en=" in text and "BLC_GAIN_SEL 0x" not in text, "%s is not in field syntax" % p)

    # bin: layout, and an error keeps an existing binary.
    src = os.path.join(out, "aec_group.csrw")
    b = os.path.join(out, "x.bin")
    r = subprocess.run([sys.executable, "-B", os.path.join(ISP, "tools", "csrw_codec.py"), "bin", src, b],
                       capture_output=True, text=True)
    check(r.returncode == 0 and open(b, "rb").read() == struct.pack("<II", m.regs["AEC_CTRL"].offset, 3), "bin layout")
    r = subprocess.run([sys.executable, "-B", os.path.join(ISP, "tools", "csrw_codec.py"), "bin",
                        os.path.join(out, "err_ro.csrw"), b], capture_output=True, text=True)
    check(r.returncode == 1 and open(b, "rb").read() == struct.pack("<II", m.regs["AEC_CTRL"].offset, 3),
          "a failed compile replaced the binary")
    r = subprocess.run([sys.executable, "-B", os.path.join(ISP, "tools", "csrw_codec.py"), "explain", src],
                       capture_output=True, text=True)
    check(r.returncode == 0 and "en=1" in r.stdout and "commit=1(W1S)" in r.stdout, "explain: %r" % r.stdout)

    print("test_csrw_codec: %d failure(s)" % len(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
