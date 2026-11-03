# =============================================================================
# Integrated Circuits and Systems laboratory
# Author: Hung Nguyen <ngthung@hcmut.edu.vn>
# Module: virtual_platform.platforms.eth_soc
# Description: Host-side platforms: network nodes running the bare-metal driver.
# Changelog:
#   2026-09-29: Initial version.
# =============================================================================

# the bare-metal driver, built for the host
add_library(eth_drv STATIC "${ICSL_ETH_VP}/sw/src/eth_drv.c")
target_include_directories(eth_drv PUBLIC "${ICSL_ETH_VP}/sw/include")

add_executable(vp_eth_echo vp_eth_echo.cpp)
target_include_directories(vp_eth_echo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vp_eth_echo PRIVATE eth_mac_tlm eth_phy_tlm vp_common eth_drv)
