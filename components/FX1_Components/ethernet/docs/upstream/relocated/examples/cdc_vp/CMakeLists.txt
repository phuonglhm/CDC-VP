# =============================================================================
# Integrated Circuits and Systems laboratory
# Author: Hung Nguyen <ngthung@hcmut.edu.vn>
# Module: virtual_platform.cdc_vp
# Description: RV32 platform on CDC-VP components, plus the bring-up firmware.
# Changelog:
#   2026-09-29: Initial version.
# =============================================================================
#
#   cmake -S virtual_platform -B build -DICSL_ETH_VP_CDC=ON -DCDC_VP_ROOT=/path/to/CDC-VP
#
# CDC_VP_ROOT is a checkout of github.com/phuonglhm/CDC-VP with the Bremen
# riscv-vp ISS cloned into third_party/riscv-vp (CDC-VP's own requirement).
# Linux only. The required CDC-VP components are compiled from source.

set(CDC "${CDC_VP_ROOT}")
set(BREMEN "${CDC}/third_party/riscv-vp/vp/src")
if(NOT EXISTS "${CDC}/cpu_models/riscv_vp/src/riscv_vp_wrapper.cpp")
    message(FATAL_ERROR "CDC_VP_ROOT=${CDC} is not a CDC-VP checkout")
endif()
if(NOT EXISTS "${BREMEN}/core/rv32/iss.cpp")
    message(FATAL_ERROR "Bremen riscv-vp not found: clone agra-uni-bremen/riscv-vp into "
                        "${CDC}/third_party/riscv-vp")
endif()
find_package(Boost REQUIRED)            # header-only parts, for the ISS

# ---- CDC-VP components ----------------------------------------------------------
add_library(cdc_common INTERFACE)
target_include_directories(cdc_common INTERFACE "${CDC}/components/common/include")

add_library(cdc_bus_router STATIC "${CDC}/components/bus_router/src/bus_router.cpp")
target_include_directories(cdc_bus_router PUBLIC "${CDC}/components/bus_router/include")
target_link_libraries(cdc_bus_router PUBLIC SystemC::systemc cdc_common)

add_library(cdc_memory_tlm STATIC "${CDC}/components/memory_tlm/src/memory_tlm.cpp")
target_include_directories(cdc_memory_tlm PUBLIC "${CDC}/components/memory_tlm/include")
target_link_libraries(cdc_memory_tlm PUBLIC SystemC::systemc cdc_common)

add_library(cdc_plic_tlm STATIC "${CDC}/components/plic_tlm/src/plic_tlm.cpp")
target_include_directories(cdc_plic_tlm PUBLIC "${CDC}/components/plic_tlm/include"
                                               "${CDC}/cpu_models/include")
target_link_libraries(cdc_plic_tlm PUBLIC SystemC::systemc cdc_common)

# ---- CDC-VP's Bremen RV32 CPU wrapper (cpu_models/riscv_vp/CMakeLists.txt) --------
add_subdirectory("${BREMEN}/vendor/softfloat" "${CMAKE_CURRENT_BINARY_DIR}/softfloat")
add_library(cdc_riscv_vp STATIC
    "${CDC}/cpu_models/riscv_vp/src/riscv_vp_wrapper.cpp"
    "${BREMEN}/core/rv32/iss.cpp"
    "${BREMEN}/core/rv32/syscall.cpp"
    "${BREMEN}/core/common/timer.cpp"
    "${BREMEN}/core/common/real_clint.cpp"
    "${BREMEN}/core/common/instr.cpp"
    "${BREMEN}/core/common/debug_memory.cpp"
    "${BREMEN}/core/common/rawmode.cpp")
target_include_directories(cdc_riscv_vp
    PUBLIC "${CDC}/cpu_models/include" "${CDC}/cpu_models/riscv_vp/include"
    PRIVATE "${BREMEN}" "${BREMEN}/core/rv32" "${BREMEN}/core/common"
            "${BREMEN}/vendor/softfloat/include" "${CDC}/cpu_models/riscv_vp/src"
            ${Boost_INCLUDE_DIRS})
target_compile_definitions(cdc_riscv_vp PRIVATE SC_INCLUDE_DYNAMIC_PROCESSES)
target_compile_options(cdc_riscv_vp PRIVATE -w)
target_link_libraries(cdc_riscv_vp PUBLIC SystemC::systemc PRIVATE softfloat pthread)

# ---- the platform -----------------------------------------------------------------
add_executable(riscv_eth_soc riscv_eth_soc.cpp)
target_link_libraries(riscv_eth_soc PRIVATE
    eth_mac_tlm eth_phy_tlm vp_common cdc_bus_router cdc_memory_tlm cdc_plic_tlm cdc_riscv_vp)

# ---- the bring-up firmware: the same driver source as the host platform ---------------
find_program(RISCV_GCC NAMES riscv64-unknown-elf-gcc riscv-none-elf-gcc riscv32-unknown-elf-gcc)
if(RISCV_GCC)
    set(_fw "${ICSL_ETH_VP}/fw/eth_bringup")
    set(_out "${CMAKE_CURRENT_BINARY_DIR}/eth_bringup")
    file(MAKE_DIRECTORY "${_out}")
    # rv32imac, ilp32, freestanding: no libc and no libgcc helpers needed
    add_custom_command(
        OUTPUT "${_out}/eth_bringup.elf"
        COMMAND "${RISCV_GCC}" -march=rv32imac_zicsr -mabi=ilp32 -mcmodel=medany -O2 -g
                -ffreestanding -nostdlib -nostartfiles -fno-builtin
                -ffunction-sections -fdata-sections -Wl,--gc-sections
                -T "${_fw}/linker.ld" -I "${ICSL_ETH_VP}/sw/include"
                "${_fw}/startup.S" "${_fw}/main.c" "${ICSL_ETH_VP}/sw/src/eth_drv.c"
                -o "${_out}/eth_bringup.elf"
        DEPENDS "${_fw}/startup.S" "${_fw}/main.c" "${_fw}/linker.ld"
                "${ICSL_ETH_VP}/sw/src/eth_drv.c" "${ICSL_ETH_VP}/sw/include/eth_drv.h"
                "${ICSL_ETH_VP}/sw/include/eth_regs.h"
        COMMENT "RV32 bring-up firmware")
    add_custom_target(eth_bringup_fw ALL DEPENDS "${_out}/eth_bringup.elf")

    if(ICSL_ETH_VP_TESTS)
        add_test(NAME riscv_eth_bringup
                 COMMAND riscv_eth_soc --fw "${_out}/eth_bringup.elf" --sim-ms 200)
        set_tests_properties(riscv_eth_bringup PROPERTIES
            LABELS "riscv;platform" TIMEOUT 600
            PASS_REGULAR_EXPRESSION "FW: ALL PASS"
            FAIL_REGULAR_EXPRESSION "FW: FAIL|FW: unexpected|SIM: TIMEOUT")
    endif()
else()
    message(WARNING "no RISC-V GCC: the bring-up firmware is not built")
endif()
