"""System smoke test for the riscv_cpu_eval platform (Step 2).

A real RISC-V core (mariusmm RV32IMAC) executes the bare-metal firmware and prints
through uart_tlm. Skips if the firmware ELF has not been cross-compiled yet.
"""

from pathlib import Path

import pexpect
import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
HELLO_REL = "fw/hello_baremetal_riscv/hello.elf"
TIMER_REL = "fw/timer_irq_riscv/timer_irq.elf"
CONFIG = "platforms/riscv_cpu_eval/configs/default.yaml"


def _skip_if_missing(rel: str, make_dir: str) -> None:
    if not (REPO_ROOT / rel).exists():
        pytest.skip(
            f"firmware not built; run `make -C {make_dir}` with the RISC-V toolchain on PATH"
        )


def test_riscv_cpu_eval_hello(vp_run):
    _skip_if_missing(HELLO_REL, "fw/hello_baremetal_riscv")

    child = vp_run("riscv_cpu_eval", config=CONFIG, args=f"--fw {HELLO_REL}")
    child.expect("Hello from RISC-V")
    child.expect(pexpect.EOF)
    child.close()
    assert child.exitstatus == 0


def test_riscv_cpu_eval_timer_irq(vp_run):
    """Gate 2: the real core takes a machine-timer interrupt and runs the handler."""
    _skip_if_missing(TIMER_REL, "fw/timer_irq_riscv")

    child = vp_run("riscv_cpu_eval", config=CONFIG, args=f"--fw {TIMER_REL}")
    child.expect("Hello from RISC-V")
    child.expect("TIMER IRQ")          # trap handler executed
    child.expect("done")               # mret returned, loop exited
    child.close()
