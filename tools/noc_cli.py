#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Interactive UART client with host-assisted FlooNoC dashboard rendering."""

from __future__ import annotations

import argparse
import os
import pathlib
import selectors
import socket
import subprocess
import sys
import time


MARKER = b"\x1eFLOONOC_DASHBOARD_V1\x1f\n"


class MarkerFilter:
    """Remove control markers while preserving arbitrary TCP chunking."""

    def __init__(self, marker: bytes = MARKER):
        if not marker:
            raise ValueError("dashboard marker must not be empty")
        self.marker = marker
        self.pending = bytearray()

    def feed(self, data: bytes) -> list[tuple[str, bytes]]:
        self.pending.extend(data)
        events: list[tuple[str, bytes]] = []
        while self.pending:
            found = self.pending.find(self.marker)
            if found >= 0:
                if found:
                    events.append(("data", bytes(self.pending[:found])))
                del self.pending[:found + len(self.marker)]
                events.append(("dashboard", b""))
                continue

            keep = 0
            maximum = min(len(self.pending), len(self.marker) - 1)
            for length in range(maximum, 0, -1):
                if self.pending[-length:] == self.marker[:length]:
                    keep = length
                    break
            emit = len(self.pending) - keep
            if emit:
                events.append(("data", bytes(self.pending[:emit])))
                del self.pending[:emit]
            break
        return events

    def flush(self) -> bytes:
        remaining = bytes(self.pending)
        self.pending.clear()
        return remaining


def file_signature(path: pathlib.Path) -> tuple[int, int, int] | None:
    try:
        stat = path.stat()
    except FileNotFoundError:
        return None
    return stat.st_ino, stat.st_mtime_ns, stat.st_size


def wait_for_snapshot(
    path: pathlib.Path,
    previous: tuple[int, int, int] | None,
    timeout_seconds: float,
) -> tuple[int, int, int]:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        current = file_signature(path)
        if current is not None and current != previous:
            return current
        time.sleep(0.02)
    raise TimeoutError(
        f"no new live metrics appeared at {path}; start noc_soc in detailed "
        "mode with --noc-metrics FILE"
    )


def render_dashboard(
    dashboard: pathlib.Path, metrics: pathlib.Path
) -> int:
    result = subprocess.run(
        [sys.executable, str(dashboard), str(metrics)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.stdout:
        sys.stdout.buffer.write(result.stdout)
        sys.stdout.buffer.flush()
    if result.returncode != 0:
        sys.stderr.buffer.write(result.stderr)
        sys.stderr.buffer.flush()
    return result.returncode


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Connect to noc_soc UART0 and render noc_dashboard requests using "
            "the live JSON atomically published by the SystemC platform."
        )
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--metrics", type=pathlib.Path, required=True)
    parser.add_argument(
        "--dashboard",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().with_name("noc_dashboard.py"),
    )
    parser.add_argument("--snapshot-timeout", type=float, default=10.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not 1 <= args.port <= 65535:
        print("noc_cli: --port must be in 1..65535", file=sys.stderr)
        return 2
    if args.snapshot_timeout <= 0:
        print("noc_cli: --snapshot-timeout must be positive", file=sys.stderr)
        return 2
    if not args.dashboard.is_file():
        print(f"noc_cli: dashboard tool not found: {args.dashboard}",
              file=sys.stderr)
        return 2

    try:
        connection = socket.create_connection((args.host, args.port), timeout=10)
    except OSError as error:
        print(f"noc_cli: cannot connect to {args.host}:{args.port}: {error}",
              file=sys.stderr)
        return 1

    connection.setblocking(False)
    # SelectSelector also accepts redirected regular-file stdin. epoll rejects
    # such descriptors with EPERM, which would make scripted CLI input fail.
    selector = selectors.SelectSelector()
    selector.register(connection, selectors.EVENT_READ, "socket")
    selector.register(sys.stdin, selectors.EVENT_READ, "stdin")
    stream_filter = MarkerFilter()
    snapshot_signature = file_signature(args.metrics)
    failed = False

    try:
        while True:
            for key, _ in selector.select():
                if key.data == "stdin":
                    chunk = os.read(sys.stdin.fileno(), 4096)
                    if not chunk:
                        selector.unregister(sys.stdin)
                        connection.shutdown(socket.SHUT_WR)
                        continue
                    connection.sendall(chunk)
                    continue

                try:
                    chunk = connection.recv(4096)
                except BlockingIOError:
                    continue
                if not chunk:
                    tail = stream_filter.flush()
                    if tail:
                        sys.stdout.buffer.write(tail)
                        sys.stdout.buffer.flush()
                    return 1 if failed else 0

                for kind, payload in stream_filter.feed(chunk):
                    if kind == "data":
                        sys.stdout.buffer.write(payload)
                        sys.stdout.buffer.flush()
                        continue
                    try:
                        snapshot_signature = wait_for_snapshot(
                            args.metrics,
                            snapshot_signature,
                            args.snapshot_timeout,
                        )
                    except TimeoutError as error:
                        print(f"\nHOST DASHBOARD ERROR: {error}", file=sys.stderr)
                        failed = True
                        continue
                    if render_dashboard(args.dashboard, args.metrics) != 0:
                        failed = True
    except KeyboardInterrupt:
        return 130
    finally:
        selector.close()
        connection.close()


if __name__ == "__main__":
    raise SystemExit(main())
