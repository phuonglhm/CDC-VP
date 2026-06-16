"""System smoke test for the ADC IP test platform on the Bremen backend."""

from pathlib import Path

import pexpect
import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
BINARY = REPO_ROOT / "build" / "bremen" / "platforms" / "tests" / "adc_platform" / "adc_platform"
FW = REPO_ROOT / "fw" / "adc_irq_riscv" / "adc_irq.elf"
CONFIG = "platforms/tests/adc_platform/configs/default.yaml"


def test_adc_platform_irq():
    if not BINARY.exists():
        pytest.skip(
            "Bremen adc_platform not built; run "
            "`cmake -S . -B build/bremen -G Ninja -DCDC_CPU_BACKEND=riscv_vp && cmake --build build/bremen`"
        )
    if not FW.exists():
        pytest.skip("ADC firmware not built; run `make -C fw/adc_irq_riscv`")

    cmd = f"{BINARY} -c {CONFIG} --fw {FW} --sim-ms 5"
    child = pexpect.spawn(cmd, timeout=120, encoding="utf-8", cwd=str(REPO_ROOT))

    child.expect("adc_platform config:")
    child.expect("irq map: ADC0 -> PLIC source 1 -> MEIP")
    child.expect("ADC platform start")
    child.expect("WRITE ADC_INTR_ENABLE \\[0x1006000C\\] <= 0x00000001")
    child.expect("WRITE ADC_CONTROL \\[0x10060000\\] <= 0x00000003")
    child.expect("READ  PLIC_CLAIM \\[0x0C200004\\] => 0x00000001")
    child.expect("READ  ADC_DATA \\[0x10060008\\] => 0x00000[0-9A-F]{3}")
    child.expect("ADC IRQ")
    child.expect("ADC sample=0x00000[0-9A-F]{3}")
    child.expect("WRITE PLIC_CLAIM \\[0x0C200004\\] <= 0x00000001")
    child.expect("READ  ADC_STATUS \\[0x10060004\\] => 0x00000000")
    child.expect("ADC PASS")
    child.close()
