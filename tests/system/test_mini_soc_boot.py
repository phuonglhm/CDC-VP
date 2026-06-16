"""System smoke test for the mini_soc platform (Step 1)."""

import pexpect


def test_mini_soc_boot(vp_run):
    child = vp_run("mini_soc", config="platforms/mini_soc/configs/default.yaml")

    child.expect("Hello from mini SoC")
    child.expect("I2C IRQ fired")

    child.expect(pexpect.EOF)
    child.close()
    assert child.exitstatus == 0
