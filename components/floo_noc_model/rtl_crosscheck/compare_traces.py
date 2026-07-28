#!/usr/bin/env python3
# SPDX-License-Identifier: SHL-0.51

from pathlib import Path
import sys


def normalized_lines(path: Path) -> list[str]:
    return [
        line.strip()
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print(
            f"usage: {sys.argv[0]} <systemc-trace.csv> <rtl-trace.csv> [label]",
            file=sys.stderr,
        )
        return 2

    systemc_path = Path(sys.argv[1])
    rtl_path = Path(sys.argv[2])
    label = sys.argv[3] if len(sys.argv) == 4 else "route-select"
    systemc_lines = normalized_lines(systemc_path)
    rtl_lines = normalized_lines(rtl_path)

    if systemc_lines == rtl_lines:
        print(f"{label} cross-check PASS: {len(systemc_lines) - 1} cycles match")
        return 0

    print(f"{label} cross-check FAIL", file=sys.stderr)
    count = max(len(systemc_lines), len(rtl_lines))
    for index in range(count):
        systemc_line = (
            systemc_lines[index] if index < len(systemc_lines) else "<missing>"
        )
        rtl_line = rtl_lines[index] if index < len(rtl_lines) else "<missing>"
        if systemc_line != rtl_line:
            print(
                f"line {index + 1}: "
                f"SystemC='{systemc_line}' RTL='{rtl_line}'",
                file=sys.stderr,
            )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
