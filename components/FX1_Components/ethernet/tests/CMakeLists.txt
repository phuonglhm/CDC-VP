    # register contract, DMA, MAC paths, MDIO, RMON, resets: CPU-independent
    add_executable(test_eth_mac_tlm test_eth_mac_tlm.cpp)
    target_link_libraries(test_eth_mac_tlm PRIVATE eth_mac_tlm eth_phy_tlm eth_vp_support)
    add_test(NAME eth_mac_tlm_unit COMMAND test_eth_mac_tlm)
    set_tests_properties(eth_mac_tlm_unit PROPERTIES LABELS "unit" TIMEOUT 120
        PASS_REGULAR_EXPRESSION "=== eth_mac_tlm: [0-9]+ checks, 0 failures ===")

    # the frame functions against the reference vectors (vectors/core.txt)
    add_executable(test_core_golden test_core_golden.cpp)
    target_link_libraries(test_core_golden PRIVATE eth_core)
    add_test(NAME eth_core_golden COMMAND test_core_golden "${ICSL_ETH_VP}/vectors/core.txt")
    set_tests_properties(eth_core_golden PROPERTIES
        LABELS "unit;golden" TIMEOUT 120
        PASS_REGULAR_EXPRESSION "=== core golden: [0-9]+ cases, 0 failures ===")

    # IP-level scenarios against the reference expectations (vectors/scenarios)
    add_executable(test_scenarios test_scenarios.cpp)
    target_link_libraries(test_scenarios PRIVATE eth_mac_tlm eth_phy_tlm eth_vp_support)
    add_test(NAME eth_mac_scenarios COMMAND test_scenarios "${ICSL_ETH_VP}/vectors/scenarios")
    set_tests_properties(eth_mac_scenarios PROPERTIES
        LABELS "scenario" TIMEOUT 120
        PASS_REGULAR_EXPRESSION "=== scenarios: [0-9]+ scenarios, [0-9]+ checks, 0 failures")

    # targeted regressions of the review findings (plan M1), one CTest per mode
    add_executable(test_eth_mac_m1 test_eth_mac_m1.cpp)
    target_link_libraries(test_eth_mac_m1 PRIVATE eth_mac_tlm eth_phy_tlm)
    add_executable(test_eth_m1_units test_eth_m1_units.cpp)
    target_link_libraries(test_eth_m1_units PRIVATE eth_mac_tlm)
    include(eth_m1_tests.cmake)

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
set_tests_properties(eth_drv_receive PROPERTIES LABELS "unit" TIMEOUT 120)

# the driver lifecycle on the MAC model (FX1 plan M3: review F02, F03, F08, F09)
add_executable(test_eth_drv_lifecycle test_eth_drv_lifecycle.cpp)
target_link_libraries(test_eth_drv_lifecycle PRIVATE eth_mac_tlm eth_phy_tlm eth_drv eth_vp_support)
foreach(_m cfg stop recover inflight irq-mdio)
    add_test(NAME eth_drv_${_m} COMMAND test_eth_drv_lifecycle ${_m})
    set_tests_properties(eth_drv_${_m} PROPERTIES LABELS "unit;m3" TIMEOUT 120
        PASS_REGULAR_EXPRESSION "=== eth_drv ${_m}: [0-9]+ checks, 0 failures ===")
endforeach()

add_test(NAME vp_eth_echo_100m COMMAND vp_eth_echo --frames 64)
add_test(NAME vp_eth_echo_10m COMMAND vp_eth_echo --frames 26 --10m)
set_tests_properties(vp_eth_echo_100m vp_eth_echo_10m PROPERTIES
    LABELS "platform" TIMEOUT 120
    PASS_REGULAR_EXPRESSION "ECHO PASS" FAIL_REGULAR_EXPRESSION "ECHO FAIL|FAIL:|SIM: TIMEOUT")
add_test(NAME vp_eth_burst_line_rate COMMAND vp_eth_echo --burst 2000)
set_tests_properties(vp_eth_burst_line_rate PROPERTIES
    LABELS "platform" TIMEOUT 120
    PASS_REGULAR_EXPRESSION "BURST PASS" FAIL_REGULAR_EXPRESSION "BURST FAIL|FAIL:|SIM: TIMEOUT")

# Inside CDC-VP (not a standalone build of this component) every test above also
# runs in the FX1 gate: labels fx1;fx1_unit (platforms/fx1_soc/ci), each with
# TIMEOUT 120 at most (the gate's per-test budget).
if(NOT CMAKE_SOURCE_DIR STREQUAL ICSL_ETH_VP)
    get_property(_eth_all_tests DIRECTORY PROPERTY TESTS)
    foreach(_t IN LISTS _eth_all_tests)
        set_property(TEST ${_t} APPEND PROPERTY LABELS fx1 fx1_unit)
    endforeach()
endif()
