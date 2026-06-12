"""Shared pytest fixtures for cdc-vp platform smoke tests."""

from __future__ import annotations

from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent


def _find_binary(platform: str) -> Path:
    """Locate a built platform binary, preferring the debug build."""
    candidates = [
        REPO_ROOT / "build" / preset / "platforms" / platform / platform
        for preset in ("debug", "release")
    ]
    for path in candidates:
        if path.exists():
            return path
    pytest.skip(
        f"{platform} binary not built; run "
        f"`cmake --preset debug && cmake --build --preset debug` first"
    )


@pytest.fixture
def vp_run():
    """Return a callable that spawns a platform binary under pexpect."""
    pexpect = pytest.importorskip("pexpect")

    def _run(platform: str, config: str | None = None, args: str = "", timeout: int = 60):
        binary = _find_binary(platform)
        cmd = str(binary)
        if config is not None:
            cmd += f" -c {config}"
        if args:
            cmd += f" {args}"
        return pexpect.spawn(cmd, timeout=timeout, encoding="utf-8", cwd=str(REPO_ROOT))

    return _run
