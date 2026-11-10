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

VALUE is the field's code before the shift (decimal without a leading zero or
0x hex, non-negative, at most the field width); a field write encodes (VALUE << lsb) & mask. Fields
that are not named are written 0 -- there is no read-modify-write: e.g.
`AEC_CTRL_en 1` then `AEC_CTRL_commit 1` is NOT `AEC_CTRL 3` (the second
write clears en); write `AEC_CTRL en=1 commit=1`. Values are register codes
(two's complement for signed fields, UQ for gains), not physical units.

Errors (file:line): unknown register/field/alias, a read-only field, a value
wider than its field, a bit set outside the writable fields or on a
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


class CsrwError(ValueError):
    pass


class Field:
    __slots__ = ("name", "lsb", "msb", "mask", "access", "implemented")

    def __init__(self, name, lsb, msb, access, implemented):
        self.name, self.lsb, self.msb, self.access, self.implemented = name, lsb, msb, access, implemented
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
        reg = None
        for line in open(path, encoding="utf-8"):
            m = re.match(r"/\* 0x([0-9A-F]{4}) (\w+) \((?:CSR row|extension)", line)
            if m:
                reg = Register(m.group(2), int(m.group(1), 16))
                self.regs[reg.name] = reg
                self.by_offset[reg.offset] = reg
                continue
            m = re.match(r"/\*   (\w+) \[(\d+):(\d+)\] (\w+)(.*)\*/", line)
            if m and reg:
                reg.fields.append(Field(m.group(1), int(m.group(3)), int(m.group(2)), m.group(4),
                                        "no hardware" not in m.group(5)))
                continue
            m = re.match(r"#define (FX1_ISP_\w+_MASK) (0x[0-9A-Fa-f]+)u", line)
            if m:
                macros[m.group(1)] = int(m.group(2), 16)
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


def parse_line(cmap, text, where):
    """One profile line (comment stripped) -> (Register, value), or None for a blank line."""
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
            assigns.append((n, parse_value(v, where)))
        return reg, encode_fields(reg, assigns, where)
    if len(rest) != 1 or "=" in rest[0]:
        raise CsrwError("%s: expected 'REGISTER VALUE', 'REGISTER_field VALUE' or 'REGISTER f=V ...'" % where)
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
    out = []
    for n, line in enumerate(lines(path), 1):
        r = parse_line(cmap, line.split("#", 1)[0], "%s:%d" % (path, n))
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


def format_write(reg, value, style="fields"):
    """One write as a profile line. 'fields': a register with one field keeps
    REGISTER VALUE (it is the field); several fields -> REGISTER f=V ... with every
    RW field (0 included: it is part of the write) and every nonzero command
    (W1x) field; a value outside the writable fields stays REGISTER VALUE (raw:
    nothing is dropped). 'legacy': REGISTER 0xVALUE. Fields of up to 8 bits in
    decimal, wider ones in hex."""
    if style == "legacy" or len(reg.fields) <= 1 or value & ~reg.writable_mask():
        return "%s 0x%X" % (reg.name, value)
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
    out = []
    for n, line in enumerate(lines(path), 1):
        body, sep, comment = line.partition("#")
        r = parse_line(cmap, body, "%s:%d" % (path, n))
        if not r:
            out.append(line)
            continue
        text = format_write(r[0], r[1], style)
        out.append(text + ("  #" + comment if sep else ""))
    return "\n".join(out) + "\n"


if __name__ == "__main__":
    sys.exit(main())
