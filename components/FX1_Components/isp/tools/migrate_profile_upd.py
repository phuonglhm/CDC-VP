#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Migrate a CSR v1.07 profile (.csrw) to the VP CSR extension 0x20261109.

    migrate_profile_upd.py [--check] [--strict] FILE.csrw [...]

docs/ISP_CSR_EXTENSION_UPD.md. In place, line by line (comments, blank lines
and the order of the writes are kept):

  ISP-UPD-002  `BLC_SCALE_Gg V` (g = 0..3; the scale shared by the four
               colours, removed) becomes the four lines
               `BLC_SCALE_Gg_R V`, `_GR V`, `_GB V`, `_B V`: equal per-channel
               scales give the same output.
  ISP-UPD-007  CCM_OFS_R/G/B are now in output pixel levels (code 1 adds one
               level; it was 1/512). A nonzero offset cannot be converted
               without changing the picture, so it is kept and reported;
               --strict makes it an error. CCM lines are decoded with
               csrw_codec.py (any syntax: `CCM_OFS_R V`, `CCM_OFS_R_offset_r V`,
               `CCM_OFS_R offset_r=V`); a malformed one is an error.
  ISP-UPD-001  LSC_COEF_DATA codes keep their meaning (UQ4.18 extends UQ3.18
               upwards); nothing to do.

--check: change nothing, exit 1 if a file needs the BLC migration.
The number of writes changes (+3 per BLC scale): regenerate the matching
profile.bin and job.bin (profile_count) after migrating an SDK sample.
A UTF-8 byte order mark at the start of the file is kept. Python standard
library only (+ csrw_codec.py and the generated fx1_isp_csr.h, for CCM lines).
Exit status 0 on success, 1 on a file that needs migration (--check) or a
nonzero CCM offset (--strict), 2 for usage or I/O errors, or a line that
cannot be read (file:line; that file and the ones after it are left as they
are).
"""

import argparse
import os
import re
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import csrw_codec  # noqa: E402  (ISP-UPD-008: one reader of the profile syntax)

# The removed v1.07 register (not in the extended map): `BLC_SCALE_Gg VALUE` only.
SCALE = re.compile(r"^(\s*)BLC_SCALE_G([0-3])(\s+)([^\s#]+)(.*)$", re.ASCII)  # \s: C isspace, as the readers
SCALE_HEAD = re.compile(r"^\s*BLC_SCALE_G[0-3](?:\s|$)", re.ASCII)
LINE = re.compile(r"[^\n]*\n|[^\n]+\Z")  # lines end at '\n' only, as the readers (csrw_codec.lines)
BOM = "\ufeff"


class MigrateError(ValueError):
    """A line the migration cannot read; the message starts with file:line."""


def migrate(text, path="<profile>", cmap=None):
    """(new text, BLC lines migrated, nonzero CCM offsets as (line, channel, code)).
    cmap: a csrw_codec.CsrMap, loaded on the first CCM line when None."""
    out, migrated, ccm = [], 0, []
    bom = text.startswith(BOM)
    lines = LINE.findall(text[len(BOM) if bom else 0:])
    for n, line in enumerate(lines, 1):
        body = line.rstrip("\r\n")
        eol = line[len(body):]
        code_part = body.split("#", 1)[0]
        where = "%s:%d" % (path, n)
        m = SCALE.match(body)
        if m and len(csrw_codec.tokens_of(code_part)) == 2 and "=" not in code_part:
            indent, g, sep, value, rest = m.groups()
            for c in ("R", "GR", "GB", "B"):
                out.append("%sBLC_SCALE_G%s_%s%s%s%s%s" % (indent, g, c, sep, value, rest, eol or "\n"))
            migrated += 1
            continue
        if SCALE_HEAD.match(code_part):
            raise MigrateError("%s: the removed register BLC_SCALE_G%s takes one value: 'BLC_SCALE_Gg VALUE'"
                               % (where, csrw_codec.tokens_of(code_part)[0][-1]))
        tokens = csrw_codec.tokens_of(code_part)
        if tokens and tokens[0].startswith("CCM_OFS_"):
            try:
                if cmap is None:
                    cmap = csrw_codec.CsrMap()
                reg, value = csrw_codec.parse_line(cmap, code_part, where)
            except csrw_codec.CsrwError as e:
                raise MigrateError(str(e))
            ch = reg.name[len("CCM_OFS_"):]
            f = reg.field("offset_" + ch.lower())
            if f is None:
                raise MigrateError("%s: %s is not a CCM offset register" % (where, reg.name))
            code = (value >> f.lsb) & ((1 << f.width) - 1)
            if code:
                sign = 1 << (f.width - 1)
                ccm.append((n, ch, code - 2 * sign if code & sign else code))
        out.append(line)
    return (BOM if bom else "") + "".join(out), migrated, ccm


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("files", nargs="+")
    ap.add_argument("--check", action="store_true", help="report only; exit 1 if a file needs migration")
    ap.add_argument("--strict", action="store_true", help="a nonzero CCM offset is an error")
    a = ap.parse_args()
    status = 0
    for path in a.files:
        try:
            with open(path, encoding="utf-8", errors="surrogateescape", newline="") as f:  # comments: any bytes
                text = f.read()
        except OSError as e:
            print("migrate_profile_upd: %s" % e, file=sys.stderr)
            return 2
        try:
            new, migrated, ccm = migrate(text, path)
        except MigrateError as e:
            print("migrate_profile_upd: %s (nothing changed)" % e, file=sys.stderr)
            return 2
        for n, ch, code in ccm:
            print("%s:%d: CCM_OFS_%s %d: now %+d output level(s) (was %+.4f before ISP-UPD-007)%s"
                  % (path, n, ch, code, code, code / 512.0, "" if a.strict else "; kept"))
            if a.strict:
                status = 1
        if not migrated or (a.strict and ccm):  # --strict: a file with a nonzero offset is left unchanged
            continue
        if a.check:
            print("%s: %d BLC_SCALE_Gg line(s) to migrate" % (path, migrated))
            status = 1
            continue
        d = os.path.dirname(os.path.abspath(path))
        fd, tmp = tempfile.mkstemp(dir=d, prefix=".migrate_")
        try:
            with os.fdopen(fd, "w", encoding="utf-8", errors="surrogateescape", newline="") as f:
                f.write(new)
            os.chmod(tmp, os.stat(path).st_mode & 0o777)
            os.replace(tmp, path)
        except OSError as e:
            try:
                os.remove(tmp)
            except OSError:
                pass
            print("migrate_profile_upd: %s: %s" % (path, e), file=sys.stderr)
            return 2
        print("%s: %d BLC_SCALE_Gg line(s) -> per-channel scales (+%d writes)" % (path, migrated, 3 * migrated))
    return status


if __name__ == "__main__":
    sys.exit(main())
