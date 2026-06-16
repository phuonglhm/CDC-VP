"""System smoke test for the riscv_custom_soc platform (Step 3).

A custom RISC-V SoC takes a CLINT timer interrupt and a PLIC external interrupt
(sourced from the I2C peripheral). Skips if the firmware ELF is not built.
"""

from pathlib import Path

import pexpect
import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
FW_REL = "fw/soc_irq_riscv/soc_irq.elf"
CONFIG = "platforms/riscv_custom_soc/configs/default.yaml"


def test_riscv_custom_soc_interrupts(vp_run):
    if not (REPO_ROOT / FW_REL).exists():
        pytest.skip(
            "firmware not built; run `make -C fw/soc_irq_riscv` with the RISC-V toolchain on PATH"
        )

    child = vp_run("riscv_custom_soc", config=CONFIG, args=f"--fw {FW_REL}")

    child.expect("Hello from custom SoC")
    child.expect("EXT IRQ")   # PLIC external interrupt (I2C source)
    child.expect("TIMER")     # CLINT timer interrupt
    child.expect("done")
    child.close()
