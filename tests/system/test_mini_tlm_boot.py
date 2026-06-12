"""System smoke test for the mini_tlm platform (Step 1)."""

import pexpect


def test_mini_tlm_boot(vp_run):
    child = vp_run("mini_tlm", config="platforms/mini_tlm/configs/default.yaml")

    child.expect("Hello from mini VP")
    child.expect("Timer IRQ fired at 10ms")

    child.expect(pexpect.EOF)
    child.close()
    assert child.exitstatus == 0
