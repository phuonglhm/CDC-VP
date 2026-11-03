#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Convert symbolic CSR writes into little-endian {offset, value} uint32 pairs.
set -euo pipefail

if [[ $# -eq 1 && ( "$1" == "--help" || "$1" == "-h" ) ]]; then
    echo "Usage: $0 PATH/TO/profile.csrw"
    echo "Creates or replaces PATH/TO/profile.bin. Does not update sample job.bin."
    exit 0
fi
if [[ $# -ne 1 ]]; then
    echo "Usage: $0 PATH/TO/profile.csrw" >&2
    exit 2
fi

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
header="${here}/../../sources/VP_FX1_V3.0/include/fx1_isp/fx1_isp_csr.h"
unset PYTHONHOME PYTHONPATH
exec /usr/bin/python3 - "$1" "${header}" <<'PY'
import os
from pathlib import Path
import re
import stat
import struct
import sys
import tempfile


def convert():
    source = Path(sys.argv[1]).expanduser()
    header = Path(sys.argv[2])
    if source.suffix.lower() != ".csrw":
        raise ValueError("input must have the .csrw extension")
    offsets = {
        name: int(value, 16)
        for name, value in re.findall(
            r"^#define\s+FX1_ISP_(\w+)_OFFSET\s+(0x[0-9A-Fa-f]+)u\b",
            header.read_text(encoding="utf-8"), re.MULTILINE
        )
    }
    if not offsets:
        raise ValueError(f"no CSR offsets found in {header}")
    data = bytearray()
    for number, line in enumerate(source.read_text(encoding="utf-8-sig").splitlines(), 1):
        fields = line.split("#", 1)[0].split()
        if not fields:
            continue
        location = f"{source}:{number}"
        if len(fields) != 2:
            raise ValueError(f"{location}: expected <REGISTER> <VALUE>")
        name, text = fields
        if name not in offsets:
            raise ValueError(f"{location}: unknown CSR {name}")
        try:
            value = int(text, 0)
        except ValueError:
            raise ValueError(f"{location}: invalid integer {text!r}") from None
        if not 0 <= value <= 0xFFFFFFFF:
            raise ValueError(f"{location}: value must fit uint32")
        data.extend(struct.pack("<II", offsets[name], value))
    if not data:
        raise ValueError(f"{source}: no CSR writes")

    target = source.with_suffix(".bin")
    old_size = target.stat().st_size if target.exists() else None
    mode = stat.S_IMODE(target.stat().st_mode) if target.exists() else 0o644
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=target.parent, prefix="." + target.name + ".",
                                         delete=False) as output:
            temporary = output.name
            output.write(data)
            os.fchmod(output.fileno(), mode)
        os.replace(temporary, target)
        temporary = None
    finally:
        if temporary is not None:
            os.unlink(temporary)
    print(f"OK: {source} -> {target} ({len(data) // 8} CSR writes, {len(data)} bytes)")
    if old_size is not None and old_size != len(data):
        print("WARNING: CSR count changed; regenerate job.bin for every sample using this profile.",
              file=sys.stderr)
    else:
        print("Sample job.bin must declare the same CSR write count; this script does not update it.")


try:
    convert()
except (OSError, UnicodeError, ValueError) as error:
    print(f"csrw_to_bin: {error}", file=sys.stderr)
    sys.exit(1)
PY
