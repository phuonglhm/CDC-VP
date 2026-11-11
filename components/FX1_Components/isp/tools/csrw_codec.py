#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""FX1 ISP profile (.csrw) codec: register and field syntax -> ordered CSR writes.

    csrw_codec.py [--header fx1_isp_csr.h] check   FILE.csrw ...
    csrw_codec.py [--header fx1_isp_csr.h] explain FILE.csrw          # one line per write
    csrw_codec.py [--header fx1_isp_csr.h] emit    FILE.csrw [--style fields|legacy] [--out F]
    csrw_codec.py [--header fx1_isp_csr.h] bin     FILE.csrw OUT.bin

ISP-UPD-008 (workspace plan §14). A profile is a list of CSR writes, applied
in order; the codec never merges, sorts or deduplicates them. Every
non-blank line (after '#' comments) is ONE full 32-bit write:

  REGISTER VALUE              the whole register (the v1.07 form)
  REGISTER_field VALUE        one field; every other field of the register 0
  REGISTER f1=V1 f2=V2 ...    several fields of one register; the others 0
  PORT_TABLE VALUE            ISP-UPD-013: a table data port named with the table
                              it writes (EE_LUT_WDATA_ACTIVITY 0x8000 is
                              EE_LUT_WDATA 0x8000; TAGGED_PORTS). The name is
                              checked, not used: the last write of the port's
                              selector register earlier in the same file
                              (EE_LUT_CTRL) must select that table (lut_sel=
                              ACTIVITY), else an error. Not a CSR register.

VALUE is the field's code before the shift (decimal without a leading zero or
0x hex, non-negative, at most the field width), or in a field assignment the
name of one of the field's CSR enum values (`EE_LUT_CTRL lut_sel=ACTIVITY`,
`COMMON_BAYER pattern=BGGR`; ISP-UPD-012, the header's FX1_ISP_<REG>_<FIELD>_<NAME>
macros); a field write encodes (VALUE << lsb) & mask. Fields
that are not named are written 0 -- there is no read-modify-write: e.g.
`AEC_CTRL_en 1` then `AEC_CTRL_commit 1` is NOT `AEC_CTRL 3` (the second
write clears en); write `AEC_CTRL en=1 commit=1`. Values are register codes
(two's complement for signed fields, UQ for gains), not physical units.

Errors (file:line): unknown register/field/alias, a read-only field, a value
wider than its field, a table name that is not the selected table, a bit set outside the writable fields or on a
tied-zero bit, a field named twice, a malformed token. A `REGISTER VALUE`
line may set any bit the register defines (raw writes for tests); fields
the map marks as having no hardware are accepted with a note in `explain`.

Lines and tokens are those of the C++ reader (tests/common/vector_io.h,
review B/C/P/D R3): lines end at '\n' only ('\r\n' works, a lone '\r' does
not end a line), tokens are separated by ASCII white space only (C isspace:
space, \t, \n, \v, \f, \r; not NBSP or other Unicode spaces), a UTF-8 byte
order mark opening the file is ignored (anywhere else it is part of a token),
and bytes that are not UTF-8 are accepted in comments (names and values are
ASCII). `emit` writes UTF-8 without a byte order mark, '\n' line ends.

The register/field map is the generated header include/fx1_isp/fx1_isp_csr.h
(the same file the firmware and the SDK use; field names and access types
from its per-field comments). Python standard library only.
"""

import argparse
import os
import re
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
import glob  # noqa: E402

# The component layout (tools/ next to include/), the installed one
# (<prefix>/share/fx1_isp/tools, <prefix>/include), then the SDK's (the codec
# ships next to csrw_to_bin.sh, the header under sw/bootloader/sources/<package>).
DEFAULT_HEADERS = [os.path.join(HERE, "..", "include", "fx1_isp", "fx1_isp_csr.h"),
                   os.path.join(HERE, "..", "..", "..", "include", "fx1_isp", "fx1_isp_csr.h")] + \
    sorted(glob.glob(os.path.join(HERE, "..", "..", "sources", "*", "include", "fx1_isp", "fx1_isp_csr.h")))
WRITABLE = ("RW", "W1C", "W1S", "W1SC", "W1P")

# ISP-UPD-013: data ports that a profile may name with the table they write,
# (port, selector register, selector field): PORT_<ENUM NAME> for each named
# value of the selector field. The C++ reader has the same list
# (tests/common/vector_io.h, csrw::tagged_ports).
TAGGED_PORTS = (("EE_LUT_WDATA", "EE_LUT_CTRL", "lut_sel"),)


class CsrwError(ValueError):
    pass


class Field:
    __slots__ = ("name", "lsb", "msb", "mask", "access", "implemented", "enums")

    def __init__(self, name, lsb, msb, access, implemented):
        self.name, self.lsb, self.msb, self.access, self.implemented = name, lsb, msb, access, implemented
        self.enums = {}  # name -> code (the header's value macros of this field)
        self.mask = ((1 << (msb - lsb + 1)) - 1) << lsb  # replaced by the header's _MASK (tied-zero bits removed)

    @property
    def width(self):
        return self.msb - self.lsb + 1


class Register:
    __slots__ = ("name", "offset", "fields")

    def __init__(self, name, offset):
        self.name, self.offset, self.fields = name, offset, []

    def field(self, name):
        return next((f for f in self.fields if f.name == name), None)

    def writable_mask(self):
        m = 0
        for f in self.fields:
            if f.access in WRITABLE:
                m |= f.mask
        return m

    def defined_mask(self):
        m = 0
        for f in self.fields:
            m |= f.mask
        return m


class CsrMap:
    """Registers, fields and the REGISTER_field aliases of a generated header."""

    def __init__(self, header=None):
        path = header or next((h for h in DEFAULT_HEADERS if os.path.isfile(h)), None)
        if not path or not os.path.isfile(path):
            raise CsrwError("CSR header not found (give --header .../fx1_isp/fx1_isp_csr.h)")
        self.header = os.path.abspath(path)
        self.regs, self.by_offset, macros = {}, {}, {}
        reg = field = None
        for line in open(path, encoding="utf-8"):
            m = re.match(r"/\* 0x([0-9A-F]{4}) (\w+) \((?:CSR row|extension)", line)
            if m:
                reg = Register(m.group(2), int(m.group(1), 16))
                self.regs[reg.name] = reg
                self.by_offset[reg.offset] = reg
                field = None
                continue
            m = re.match(r"/\*   (\w+) \[(\d+):(\d+)\] (\w+)(.*)\*/", line)
            if m and reg:
                field = Field(m.group(1), int(m.group(3)), int(m.group(2)), m.group(4),
                              "no hardware" not in m.group(5))
                reg.fields.append(field)
                continue
            m = re.match(r"#define (FX1_ISP_\w+_MASK) (0x[0-9A-Fa-f]+)u", line)
            if m:
                macros[m.group(1)] = int(m.group(2), 16)
            # The field's named values follow its SHIFT/WIDTH/MASK/RESET macros.
            m = re.match(r"#define (FX1_ISP_\w+) (0x[0-9A-Fa-f]+)u\s*$", line)
            if m and field is not None:
                prefix = "FX1_ISP_%s_%s_" % (reg.name, re.sub(r"\W", "_", field.name).upper())
                if m.group(1).startswith(prefix):
                    name = m.group(1)[len(prefix):]
                    if name not in ("SHIFT", "WIDTH", "MASK", "RESET"):
                        field.enums[name] = int(m.group(2), 16)
        if not self.regs:
            raise CsrwError("%s: no register found" % path)
        self.extension = None
        m = re.search(r"#define FX1_ISP_CSR_EXTENSION (0x[0-9A-Fa-f]+)u", open(path, encoding="utf-8").read())
        if m:
            self.extension = int(m.group(1), 16)
        for r in self.regs.values():
            for f in r.fields:
                key = "FX1_ISP_%s_%s_MASK" % (r.name, re.sub(r"\W", "_", f.name).upper())
                if key in macros:
                    f.mask = macros[key]
        self.aliases = {}
        for r in self.regs.values():
            for f in r.fields:
                a = "%s_%s" % (r.name, f.name)
                if a in self.regs or a in self.aliases:
                    raise CsrwError("alias %s is ambiguous (a register or another alias)" % a)
                self.aliases[a] = (r, f)
        self.tags, self.selectors = {}, set()  # PORT_TABLE -> (port, selector, field, name, code)
        for port, sel, fname in TAGGED_PORTS:
            f = self.regs[sel].field(fname) if port in self.regs and sel in self.regs else None
            if f is None:
                continue  # a header without the port: no tagged form
            self.selectors.add(sel)
            for name, code in f.enums.items():
                t = "%s_%s" % (port, name)
                if t in self.regs or t in self.aliases or t in self.tags:
                    raise CsrwError("table name %s is ambiguous (a register or an alias)" % t)
                self.tags[t] = (self.regs[port], self.regs[sel], f, name, code)


SPACE = re.compile(r"[ \t\n\v\f\r]+")  # C isspace: what the C++ reader's operator>> skips


def lines(path):
    """The lines of a profile as the C++ reader sees them: split at '\n' only,
    one trailing '\r' dropped (it is white space there), a UTF-8 byte order
    mark at the start of the file removed, non-UTF-8 bytes kept
    (surrogateescape)."""
    with open(path, encoding="utf-8-sig", errors="surrogateescape", newline="\n") as fh:
        return [l[:-1] if l.endswith("\n") else l for l in fh]


def tokens_of(text):
    return [t for t in SPACE.split(text) if t]


LITERAL = re.compile(r"(0|[1-9][0-9]*|0[xX][0-9a-fA-F]+)")


def parse_value(text, where):
    """Decimal (no leading zero) or 0x hex, non-negative: the same literals as the
    C++ reader (tests/common/vector_io.h)."""
    if text.startswith("-"):
        raise CsrwError("%s: negative value %r (write the field's two's-complement code)" % (where, text))
    if not LITERAL.fullmatch(text):
        raise CsrwError("%s: invalid integer %r (decimal or 0x hex)" % (where, text))
    return int(text, 0)


def field_value(reg, name, text, where):
    """A field value: an integer literal, or one of the field's enum names."""
    if not text or not (text[0].isalpha() or text[0] == "_") or not text.isascii():
        return parse_value(text, where)
    f = reg.field(name)
    if f is None:
        raise CsrwError("%s: %s has no field %r (fields: %s)" % (where, reg.name, name,
                                                                ", ".join(x.name for x in reg.fields)))
    if text not in f.enums:
        raise CsrwError("%s: %s.%s: unknown value %r (%s)" % (
            where, reg.name, name, text,
            "one of " + ", ".join(f.enums) if f.enums else "the field has no named values"))
    return f.enums[text]


def encode_fields(reg, assigns, where):
    """{field: value} -> register word; every field not named is 0."""
    word, seen = 0, set()
    for name, value in assigns:
        f = reg.field(name)
        if f is None:
            raise CsrwError("%s: %s has no field %r (fields: %s)" % (where, reg.name, name,
                                                                    ", ".join(x.name for x in reg.fields)))
        if name in seen:
            raise CsrwError("%s: field %s.%s given twice" % (where, reg.name, name))
        seen.add(name)
        if f.access not in WRITABLE:
            raise CsrwError("%s: %s.%s is %s (not writable)" % (where, reg.name, name, f.access))
        if value >> f.width:
            raise CsrwError("%s: %s.%s = %#x does not fit %d bit(s)" % (where, reg.name, name, value, f.width))
        bits = (value << f.lsb)
        if bits & ~f.mask:
            raise CsrwError("%s: %s.%s = %#x sets a tied-zero bit" % (where, reg.name, name, value))
        word |= bits
    return word


def parse_line(cmap, text, where, ctx=None):
    """One profile line (comment stripped) -> (Register, value), or None for a blank line.
    ctx: the state of the file read so far ({selector register: last word});
    give the same dict for every line of one file. Without it the line is read
    as if it were the first of its file (a tagged port line is then an error)."""
    r = _parse_line(cmap, text, where, {} if ctx is None else ctx)
    if r and ctx is not None and r[0].name in cmap.selectors:
        ctx[r[0].name] = r[1]
    return r


def _parse_line(cmap, text, where, ctx):
    tokens = tokens_of(text)
    if not tokens:
        return None
    head, rest = tokens[0], tokens[1:]
    if not rest:
        raise CsrwError("%s: %s has no value" % (where, head))
    if head in cmap.regs and all("=" in t for t in rest):
        reg = cmap.regs[head]
        assigns = []
        for t in rest:
            n, _, v = t.partition("=")
            if not n or not v:
                raise CsrwError("%s: malformed field assignment %r" % (where, t))
            assigns.append((n, field_value(reg, n, v, where)))
        return reg, encode_fields(reg, assigns, where)
    if len(rest) != 1 or "=" in rest[0]:
        raise CsrwError("%s: expected 'REGISTER VALUE', 'REGISTER_field VALUE' or 'REGISTER f=V ...'" % where)
    if head not in cmap.regs and head in cmap.aliases:   # REGISTER_field VALUE (a name too)
        reg, f = cmap.aliases[head]
        return reg, encode_fields(reg, [(f.name, field_value(reg, f.name, rest[0], where))], where)
    if head not in cmap.regs and head in cmap.tags:   # PORT_TABLE VALUE (ISP-UPD-013)
        port, sel, f, name, code = cmap.tags[head]
        value = parse_value(rest[0], where)
        if value >> 32:
            raise CsrwError("%s: value %#x does not fit 32 bits" % (where, value))
        if sel.name not in ctx:
            raise CsrwError("%s: %s needs an earlier %s %s=%s in the file" % (where, head, sel.name, f.name, name))
        cur = (ctx[sel.name] & f.mask) >> f.lsb
        if cur != code:
            now = next((k for k, v in f.enums.items() if v == cur), str(cur))
            raise CsrwError("%s: %s but the last %s write has %s=%s" % (where, head, sel.name, f.name, now))
        return port, value
    value = parse_value(rest[0], where)
    if head in cmap.regs:
        reg = cmap.regs[head]
        if value >> 32:
            raise CsrwError("%s: value %#x does not fit 32 bits" % (where, value))
        return reg, value
    if head in cmap.aliases:
        reg, f = cmap.aliases[head]
        return reg, encode_fields(reg, [(f.name, value)], where)
    raise CsrwError("%s: unknown register or field %s" % (where, head))


def read(path, cmap):
    """[(line number, Register, value)] in file order."""
    out, ctx = [], {}
    for n, line in enumerate(lines(path), 1):
        r = parse_line(cmap, line.split("#", 1)[0], "%s:%d" % (path, n), ctx)
        if r:
            out.append((n,) + r)
    return out


def writes(path, cmap):
    """Canonical stream: [(register name, value)]."""
    return [(reg.name, v) for _, reg, v in read(path, cmap)]


def explain(reg, value):
    parts = []
    for f in reg.fields:
        v = (value & f.mask) >> f.lsb
        if v or f.access in WRITABLE:
            parts.append("%s=%s%s%s" % (f.name, v if f.width <= 8 else "0x%X" % v,
                                        "" if f.access == "RW" else "(%s)" % f.access,
                                        "" if f.implemented else "[no hardware]"))
    left = value & ~reg.defined_mask()
    return " ".join(parts) + (" undefined=0x%X" % left if left else "")


def _num(v, width):
    return str(v) if width <= 8 or v < 10 else "0x%X" % v


# Table/mesh data ports: one RW field written many times in a row (one write per
# entry). The field name adds nothing there, so `emit` keeps REGISTER VALUE.
# test_csrw_codec checks this list against the header (every single-field RW
# register named *_DATA / *_WDATA).
DATA_PORTS = ("LSC_COEF_DATA", "GAMMA_LUT_DATA", "GTM_LUT_DATA", "EE_LUT_WDATA", "AF_LUT_DATA",
              "TUNE_NR_LUT_DATA", "TUNE_GAMMA_DATA", "TUNE_LSC_SECTOR_DATA")


def format_write(reg, value, style="fields"):
    """One write as a profile line. 'fields': REGISTER f=V ... with the field
    names, also for a register of one field (`CSC_CTRL std=0`), so a tuning
    reader sees what each write sets: every RW field (0 included: it is part of
    the write) and every nonzero command (W1x) field. REGISTER VALUE stays for a
    table/mesh data port (DATA_PORTS), a value outside the writable fields (raw:
    nothing is dropped) and a register without a writable field. 'legacy':
    REGISTER 0xVALUE. Fields of up to 8 bits in decimal, wider ones in hex."""
    if style == "legacy" or reg.name in DATA_PORTS or value & ~reg.writable_mask():
        return "%s 0x%X" % (reg.name, value)
    if len(reg.fields) == 1:
        f = reg.fields[0]
        if f.access not in WRITABLE:
            return "%s 0x%X" % (reg.name, value)
        return "%s %s=%s" % (reg.name, f.name, _num((value & f.mask) >> f.lsb, f.width))
    parts = []
    for f in reg.fields:
        v = (value & f.mask) >> f.lsb
        if f.access not in WRITABLE:
            continue
        if v or (f.access == "RW" and f.implemented):
            parts.append("%s=%s" % (f.name, _num(v, f.width)))
    if not parts:  # all zero, no implemented RW field
        return "%s 0x0" % reg.name
    return "%s %s" % (reg.name, " ".join(parts))


def to_bin(stream):
    return b"".join(struct.pack("<II", reg.offset, v) for reg, v in stream)


def atomic_write(path, data, mode="wb"):
    d = os.path.dirname(os.path.abspath(path))
    fd, tmp = tempfile.mkstemp(dir=d, prefix=".csrw_")
    text = {} if "b" in mode else {"encoding": "utf-8", "errors": "surrogateescape", "newline": ""}
    try:
        with os.fdopen(fd, mode, **text) as f:
            f.write(data)
        os.chmod(tmp, 0o644)
        os.replace(tmp, path)
    except BaseException:
        try:
            os.remove(tmp)
        except OSError:
            pass
        raise


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--header", help="fx1_isp/fx1_isp_csr.h (default: the component's)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("check"); c.add_argument("files", nargs="+")
    e = sub.add_parser("explain"); e.add_argument("file")
    m = sub.add_parser("emit"); m.add_argument("file"); m.add_argument("--style", choices=("fields", "legacy"),
                                                                     default="fields"); m.add_argument("--out")
    b = sub.add_parser("bin"); b.add_argument("file"); b.add_argument("out")
    a = ap.parse_args()
    try:
        cmap = CsrMap(a.header)
        if a.cmd == "check":
            for f in a.files:
                print("%s: %d writes" % (f, len(read(f, cmap))))
        elif a.cmd == "explain":
            for n, reg, v in read(a.file, cmap):
                print("%5d  {0x%04X, 0x%08X}  %-24s %s" % (n, reg.offset, v, reg.name, explain(reg, v)))
        elif a.cmd == "emit":
            text = _emit(a.file, cmap, a.style)
            if a.out:
                atomic_write(a.out, text, "w")
            else:
                sys.stdout.flush()
                sys.stdout.buffer.write(text.encode("utf-8", "surrogateescape"))
        else:
            atomic_write(a.out, to_bin([(reg, v) for _, reg, v in read(a.file, cmap)]))
            print("wrote %s" % a.out)
    except (CsrwError, OSError) as err:
        print("csrw_codec: %s" % err, file=sys.stderr)
        return 1
    return 0


def _emit(path, cmap, style):
    """Rewrite a profile in `style`, keeping comments, blank lines and the order:
    the stream is the same, write for write."""
    out, ctx = [], {}
    for n, line in enumerate(lines(path), 1):
        body, sep, comment = line.partition("#")
        r = parse_line(cmap, body, "%s:%d" % (path, n), ctx)
        if not r:
            out.append(line)
            continue
        text = format_write(r[0], r[1], style)
        head = tokens_of(body)[0]
        if head in cmap.tags and style != "legacy":  # keep the table name (ISP-UPD-013)
            text = head + text[len(r[0].name):]
        out.append(text + ("  #" + comment if sep else ""))
    return "\n".join(out) + "\n"


if __name__ == "__main__":
    sys.exit(main())
