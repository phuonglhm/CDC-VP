# =============================================================================
# Integrated Circuits and Systems laboratory
# Author: Hung Nguyen <ngthung@hcmut.edu.vn>
# Module: virtual_platform.components.eth_mac_tlm
# Description: The Ethernet MAC TLM component and its unit tests.
# Changelog:
#   2026-09-29: Initial version.
# =============================================================================

# ---- bit-exact frame functions: plain C++, no SystemC --------------------------
add_library(eth_core STATIC src/core/eth_core.cpp)
target_include_directories(eth_core PUBLIC include)

# ---- the TLM component ------------------------------------------------------------
# Register offsets and masks come from sw/include/eth_regs.h, generated from the
# same workbook as src/generated/csr_table.inc.
add_library(eth_mac_tlm STATIC
    src/csr_file.cpp
    src/eth_mac_tlm.cpp
    src/eth_mac_dma.cpp
    src/eth_mac_datapath.cpp)
target_include_directories(eth_mac_tlm
    PUBLIC include "${ICSL_ETH_VP}/sw/include"
    PRIVATE src)
target_link_libraries(eth_mac_tlm PUBLIC eth_core SystemC::systemc)

if(ICSL_ETH_VP_TESTS)
    # register contract, DMA, MAC paths, MDIO, RMON, resets: CPU-independent
    add_executable(test_eth_mac_tlm tests/test_eth_mac_tlm.cpp)
    target_link_libraries(test_eth_mac_tlm PRIVATE eth_mac_tlm eth_phy_tlm vp_common)
    add_test(NAME eth_mac_tlm_unit COMMAND test_eth_mac_tlm)
    set_tests_properties(eth_mac_tlm_unit PROPERTIES LABELS "unit" TIMEOUT 300
        PASS_REGULAR_EXPRESSION "=== eth_mac_tlm: [0-9]+ checks, 0 failures ===")

    # the frame functions against the reference vectors (vectors/core.txt)
    add_executable(test_core_golden tests/test_core_golden.cpp)
    target_link_libraries(test_core_golden PRIVATE eth_core)
    add_test(NAME eth_core_golden COMMAND test_core_golden "${ICSL_ETH_VP}/vectors/core.txt")
    set_tests_properties(eth_core_golden PROPERTIES
        LABELS "unit;golden" TIMEOUT 300
        PASS_REGULAR_EXPRESSION "=== core golden: [0-9]+ cases, 0 failures ===")

    # IP-level scenarios against the reference expectations (vectors/scenarios)
    add_executable(test_scenarios tests/test_scenarios.cpp)
    target_link_libraries(test_scenarios PRIVATE eth_mac_tlm eth_phy_tlm vp_common)
    add_test(NAME eth_mac_scenarios COMMAND test_scenarios "${ICSL_ETH_VP}/vectors/scenarios")
    set_tests_properties(eth_mac_scenarios PROPERTIES
        LABELS "scenario" TIMEOUT 900
        PASS_REGULAR_EXPRESSION "=== scenarios: [0-9]+ scenarios, [0-9]+ checks, 0 failures")
endif()
