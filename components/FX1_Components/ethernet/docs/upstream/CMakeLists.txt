# =============================================================================
# Integrated Circuits and Systems laboratory
# Author: Hung Nguyen <ngthung@hcmut.edu.vn>
# Module: virtual_platform
# Description: SystemC/TLM-2.0 virtual prototype of the 10/100 Ethernet MAC.
# Changelog:
#   2026-09-29: Initial version.
# =============================================================================
#
#   cmake -S virtual_platform -B virtual_platform/build -G Ninja -DSYSTEMC_HOME=/opt/systemc-2.3.4
#   cmake --build virtual_platform/build
#   ctest --test-dir virtual_platform/build --output-on-failure
#
# The MAC component (components/eth_mac_tlm) depends on SystemC/TLM only, so
# it can also be added to another platform build -- CDC-VP's, for one.
cmake_minimum_required(VERSION 3.21)

project(icsl_eth_vp
    VERSION 1.0.6
    LANGUAGES C CXX
    DESCRIPTION "SystemC/TLM-2.0 virtual prototype of the ICSL 10/100 Ethernet MAC")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ICSL_ETH_VP_TESTS "Build the unit tests and the regression" ON)
option(ICSL_ETH_VP_CDC "Build the RV32 platform on CDC-VP components (Linux; needs CDC_VP_ROOT)" OFF)
set(CDC_VP_ROOT "" CACHE PATH "CDC-VP checkout with third_party/riscv-vp, for ICSL_ETH_VP_CDC")

# ---- SystemC ----------------------------------------------------------------
# Same convention as CDC-VP: an Accellera install prefix in SYSTEMC_HOME.
if(NOT TARGET SystemC::systemc)
    set(SYSTEMC_HOME "$ENV{SYSTEMC_HOME}" CACHE PATH "SystemC installation prefix")
    find_path(SYSTEMC_INCLUDE_DIR NAMES systemc systemc.h HINTS "${SYSTEMC_HOME}/include")
    find_library(SYSTEMC_LIBRARY NAMES systemc
                 HINTS "${SYSTEMC_HOME}/lib64" "${SYSTEMC_HOME}/lib" "${SYSTEMC_HOME}/lib-mingw64")
    if(NOT SYSTEMC_INCLUDE_DIR OR NOT SYSTEMC_LIBRARY)
        message(FATAL_ERROR "SystemC not found: set SYSTEMC_HOME to an Accellera SystemC "
                            "install prefix (virtual_platform/scripts/install_systemc.sh builds one).")
    endif()
    add_library(SystemC::systemc UNKNOWN IMPORTED GLOBAL)
    set_target_properties(SystemC::systemc PROPERTIES
        IMPORTED_LOCATION "${SYSTEMC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_INCLUDE_DIR}")
    find_package(Threads REQUIRED)
    set_property(TARGET SystemC::systemc APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES Threads::Threads)
    get_filename_component(_systemc_libdir "${SYSTEMC_LIBRARY}" DIRECTORY)
    set(CMAKE_BUILD_RPATH "${_systemc_libdir}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()
if(MINGW)
    # self-contained executables: several MinGW runtimes are often on PATH
    # (Git for Windows ships its own), and the wrong libstdc++ fails to load
    add_link_options(-static)
endif()

set(ICSL_ETH_VP "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")

if(ICSL_ETH_VP_TESTS)
    enable_testing()
endif()

# generic platform pieces (router, memory, UART, test-status device), header-only
add_library(vp_common INTERFACE)
target_include_directories(vp_common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/components/common/include")
target_link_libraries(vp_common INTERFACE SystemC::systemc)

# the PHY and link partner, header-only
add_library(eth_phy_tlm INTERFACE)
target_include_directories(eth_phy_tlm INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/components/eth_phy_tlm/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/eth_mac_tlm/include")
target_link_libraries(eth_phy_tlm INTERFACE SystemC::systemc)

add_subdirectory(components/eth_mac_tlm)
add_subdirectory(platforms/eth_soc)
if(ICSL_ETH_VP_CDC)
    add_subdirectory(cdc_vp)
endif()
if(ICSL_ETH_VP_TESTS)
    add_subdirectory(tests)
endif()
