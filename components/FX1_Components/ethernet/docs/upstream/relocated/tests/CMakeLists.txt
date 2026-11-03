# =============================================================================
# Integrated Circuits and Systems laboratory
# Author: Hung Nguyen <ngthung@hcmut.edu.vn>
# Module: virtual_platform.tests
# Description: Platform and end-to-end regressions of the virtual prototype.
# Changelog:
#   2026-09-29: Initial version.
# =============================================================================
#
# Labels:
#   unit      the MAC component on its own (register contract, behaviour,
#   platform  the bare-metal driver on two network nodes
#   riscv     RV32 firmware on CDC-VP (Linux, cdc_vp/run_in_docker.sh)
#
#   ctest --test-dir virtual_platform/build -L "unit|scenario|platform" --output-on-failure

add_executable(test_eth_drv test_eth_drv.c)
target_link_libraries(test_eth_drv PRIVATE eth_drv)
add_test(NAME eth_drv_receive COMMAND test_eth_drv)
set_tests_properties(eth_drv_receive PROPERTIES LABELS "unit")

add_test(NAME vp_eth_echo_100m COMMAND vp_eth_echo --frames 64)
add_test(NAME vp_eth_echo_10m COMMAND vp_eth_echo --frames 26 --10m)
set_tests_properties(vp_eth_echo_100m vp_eth_echo_10m PROPERTIES
    LABELS "platform" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "ECHO PASS" FAIL_REGULAR_EXPRESSION "ECHO FAIL|FAIL:|SIM: TIMEOUT")
add_test(NAME vp_eth_burst_line_rate COMMAND vp_eth_echo --burst 2000)
set_tests_properties(vp_eth_burst_line_rate PROPERTIES
    LABELS "platform" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "BURST PASS" FAIL_REGULAR_EXPRESSION "BURST FAIL|FAIL:|SIM: TIMEOUT")
