#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

"""Protocol controls for the host-assisted live NoC dashboard client."""

from __future__ import annotations

import importlib.util
import json
import pathlib
import socket
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True


def load(path: pathlib.Path):
    spec = importlib.util.spec_from_file_location("noc_cli_tested", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    client = load(pathlib.Path(sys.argv[1]).resolve())
    protocol = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8")
    dashboard = pathlib.Path(sys.argv[3]).resolve()
    dashboard_contract = load(pathlib.Path(sys.argv[4]).resolve())
    if 'CDC_NOC_DASHBOARD_REQUEST_BODY "FLOONOC_DASHBOARD_V1"' not in protocol:
        print("firmware/platform protocol body changed without the host client",
              file=sys.stderr)
        return 1

    marker = b"\x1eFLOONOC_DASHBOARD_V1\x1f\n"
    if client.MARKER != marker:
        print("host marker does not match firmware/platform marker",
              file=sys.stderr)
        return 1

    # Every possible split point must still suppress exactly one marker and
    # preserve the byte stream on either side.
    framed = b"before\n" + marker + b"after\n"
    for split in range(len(framed) + 1):
        filter_ = client.MarkerFilter()
        events = filter_.feed(framed[:split])
        events += filter_.feed(framed[split:])
        events.append(("data", filter_.flush()))
        visible = b"".join(payload for kind, payload in events
                           if kind == "data")
        requests = sum(kind == "dashboard" for kind, _ in events)
        if visible != b"before\nafter\n" or requests != 1:
            print(f"marker split {split} was decoded incorrectly",
                  file=sys.stderr)
            return 1

    filter_ = client.MarkerFilter()
    events = filter_.feed(marker + marker)
    if sum(kind == "dashboard" for kind, _ in events) != 2:
        print("back-to-back dashboard requests were not both detected",
              file=sys.stderr)
        return 1

    # Exercise the real TCP client, atomic file publication and renderer
    # ordering. Compare byte-for-byte with direct use of the canonical
    # dashboard, so the CLI cannot silently substitute a shortened report.
    with tempfile.TemporaryDirectory(prefix="noc_cli_test.") as temporary:
        root = pathlib.Path(temporary)
        metrics = root / "metrics.json"
        fixture_json = json.dumps(dashboard_contract.fixture())
        metrics.write_text(fixture_json, encoding="utf-8")
        direct = subprocess.run(
            [sys.executable, str(dashboard), str(metrics)],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if direct.returncode != 0:
            print(direct.stderr.decode(errors="replace"), file=sys.stderr)
            return 1

        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        process = subprocess.Popen(
            [
                sys.executable,
                str(pathlib.Path(sys.argv[1]).resolve()),
                "--port", str(port),
                "--metrics", str(metrics),
                "--dashboard", str(dashboard),
                "--snapshot-timeout", "2",
            ],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        connection, _ = listener.accept()
        connection.sendall(b"before\n")
        time.sleep(0.1)
        unpublished = root / "metrics.json.tmp"
        unpublished.write_text(fixture_json, encoding="utf-8")
        unpublished.replace(metrics)
        connection.sendall(marker + b"after\n")
        connection.close()
        listener.close()
        stdout, stderr = process.communicate(timeout=5)
        if process.returncode != 0:
            print(stderr.decode(errors="replace"), file=sys.stderr)
            return 1
        expected = b"before\n" + direct.stdout + b"after\n"
        if stdout != expected:
            print(f"host client output ordering mismatch: {stdout!r}",
                  file=sys.stderr)
            return 1

    print("PASS: host-assisted NoC dashboard UART protocol")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
