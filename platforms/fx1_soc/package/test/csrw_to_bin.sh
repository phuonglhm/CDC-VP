#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Convert a profile (register or field syntax, csrw_codec.py) into
# little-endian {offset, value} uint32 pairs, one per line, in order.
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
exec /usr/bin/python3 -B - "$1" "${header}" "${here}" <<'PY'
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
    # ISP-UPD-008: register or field syntax, one line = one full-word write,
    # through the ISP's codec shipped next to this script (csrw_codec.py).
    sys.path.insert(0, sys.argv[3])
    import csrw_codec
    data = csrw_codec.to_bin([(reg, value) for _, reg, value in
                              csrw_codec.read(str(source), csrw_codec.CsrMap(str(header)))])
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
        # ISP-UPD-010: run_vp.sh gives each job the profile's own write count.
        print(f"note: the CSR count changed ({old_size // 8} -> {len(data) // 8}); run_vp.sh uses the "
              "profile's count, and the samples' goldens are those of the shipped profile.")


try:
    convert()
except (OSError, UnicodeError, ValueError) as error:
    print(f"csrw_to_bin: {error}", file=sys.stderr)
    sys.exit(1)
PY
