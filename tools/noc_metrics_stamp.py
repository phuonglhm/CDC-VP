#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Add archival provenance to a FlooNoC metrics-v1 JSON artifact.

The simulator writes measured data and run-local configuration.  The runner
owns facts that are only available outside the process (Git state and binary
hashes), so it adds them after the run and republishes the JSON atomically.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import tempfile
from typing import Any


SCHEMA = "floo-noc-metrics-v1"


def parse_bool(value: str) -> bool:
    if value == "true":
        return True
    if value == "false":
        return False
    raise argparse.ArgumentTypeError("expected 'true' or 'false'")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="stamp external provenance into FlooNoC metrics JSON"
    )
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--git-revision", required=True)
    parser.add_argument("--git-dirty", required=True, type=parse_bool)
    parser.add_argument("--source-patch-sha256", required=True)
    parser.add_argument("--platform-binary", required=True)
    parser.add_argument("--platform-sha256", required=True)
    parser.add_argument("--firmware", required=True)
    parser.add_argument("--firmware-sha256", required=True)
    parser.add_argument("--host-cc", required=True)
    parser.add_argument("--host-cxx", required=True)
    return parser.parse_args()


def load(path: pathlib.Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as source:
        data = json.load(source)
    if not isinstance(data, dict) or data.get("schema") != SCHEMA:
        raise ValueError(f"{path}: expected schema {SCHEMA!r}")
    provenance = data.get("provenance")
    if not isinstance(provenance, dict):
        raise ValueError(f"{path}: missing provenance object")
    return data


def publish(path: pathlib.Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=path.name + ".", suffix=".tmp", dir=path.parent
    )
    temporary = pathlib.Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as output:
            json.dump(data, output, indent=2, sort_keys=False)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise


def main() -> int:
    args = parse_args()
    data = load(args.input)
    provenance = data["provenance"]
    provenance.update(
        {
            "git_revision": args.git_revision,
            "git_dirty": args.git_dirty,
            "source_patch_sha256": args.source_patch_sha256,
            "platform_binary": str(pathlib.Path(args.platform_binary).resolve()),
            "platform_sha256": args.platform_sha256,
            "firmware": str(pathlib.Path(args.firmware).resolve()),
            "firmware_sha256": args.firmware_sha256,
            "host_cc": args.host_cc,
            "host_cxx": args.host_cxx,
        }
    )
    publish(args.output, data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
