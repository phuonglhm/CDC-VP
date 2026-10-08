# FX1 SoC platform (CDC_BUILD_FX1_SOC). Host side: the simulator executable.
# Firmware side: fw/fx1_soc, built with the RISC-V cross toolchain when one is
# found, and registered as ctest cases that run on this simulator.
add_executable(fx1_soc
    src/main.cpp
    src/fx1_soc_top.cpp)

target_include_directories(fx1_soc PRIVATE src)
target_compile_options(fx1_soc PRIVATE -Wall -Wextra)
target_link_libraries(fx1_soc PRIVATE
    SystemC::systemc
    cdc::cpu::riscv_vp_plusplus
    cdc::components::bus_system
    cdc::components::fx1_map
    cdc::components::fx1_sparse_ram
    cdc::components::fx1_access_guard
    cdc::components::fx1_clint
    cdc::components::fx1_plic
    cdc::components::fx1_cpu_port
    cdc::components::fx1_exclusive_monitor
    cdc::components::fx1_sim_control
    cdc::components::fx1_dma
    cdc::components::fx1_isp_tlm
    cdc::components::uart2_tlm)

# Negative integration harness (review G2-R2): real VP++ hart + silent target.
# Its firmware image and ctest registration live in fw/fx1_soc.
add_executable(test_fx1_cpu_silent_target tests/test_cpu_silent_target.cpp)
target_compile_options(test_fx1_cpu_silent_target PRIVATE -Wall -Wextra)
target_link_libraries(test_fx1_cpu_silent_target PRIVATE
    SystemC::systemc
    cdc::cpu::riscv_vp_plusplus
    cdc::components::bus_system
    cdc::components::fx1_map
    cdc::components::fx1_sparse_ram
    cdc::components::fx1_cpu_port
    cdc::components::fx1_sim_control)

# Plan C7 (review G4b-R1): a real VP++ hart with the real exclusive monitor;
# an LR whose load faults must leave no reservation for the SC that follows.
add_executable(test_fx1_faulting_lr tests/test_faulting_lr.cpp)
target_compile_options(test_fx1_faulting_lr PRIVATE -Wall -Wextra)
target_link_libraries(test_fx1_faulting_lr PRIVATE
    SystemC::systemc
    cdc::cpu::riscv_vp_plusplus
    cdc::components::fx1_cpu_port
    cdc::components::fx1_exclusive_monitor)
if(CDC_BUILD_TESTS)
    add_test(NAME fx1_faulting_lr COMMAND test_fx1_faulting_lr)
    set_tests_properties(fx1_faulting_lr PROPERTIES TIMEOUT 60 LABELS "fx1;fx1_unit")
endif()

# ── CI aggregate (plan P5) ─────────────────────────────────────────────────────
# `fx1_all` builds everything the FX1 tests need (simulator, harnesses, FX1
# component and VP++ backend targets, firmware images) and nothing else of the
# tree: `components/isp_tlm` is added unconditionally upstream and does not
# link today, so `cmake --build` of the default target fails for reasons
# unrelated to FX1. The list is collected once the whole tree is configured.
add_custom_target(fx1_all)
function(_fx1_collect_targets out dir)
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    set(_list "")
    foreach(_t IN LISTS _targets)
        get_target_property(_type ${_t} TYPE)
        if(NOT _type STREQUAL "INTERFACE_LIBRARY" AND NOT _t STREQUAL "fx1_all")
            list(APPEND _list ${_t})
        endif()
    endforeach()
    foreach(_s IN LISTS _subdirs)
        _fx1_collect_targets(_sub "${_s}")
        list(APPEND _list ${_sub})
    endforeach()
    set(${out} ${_list} PARENT_SCOPE)
endfunction()
function(_fx1_finish_all)
    set(_dirs "${CMAKE_SOURCE_DIR}/components/FX1_Components"
              "${CMAKE_SOURCE_DIR}/cpu_models/riscv_vp_plusplus"
              "${CMAKE_SOURCE_DIR}/platforms/fx1_soc")
    if(FX1_SOC_BUILD_FIRMWARE)
        list(APPEND _dirs "${CMAKE_SOURCE_DIR}/fw/fx1_soc")
    endif()
    set(_all "")
    foreach(_d IN LISTS _dirs)
        _fx1_collect_targets(_found "${_d}")
        list(APPEND _all ${_found})
    endforeach()
    add_dependencies(fx1_all ${_all})
endfunction()
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL _fx1_finish_all)

# ── Firmware (cross-compiled; never uses the host CC/CXX) ─────────────────────
# FX1_SOC_BUILD_FIRMWARE=ON (default) is the acceptance configuration: firmware
# and the fx1_smoke tests are mandatory, so a missing or non-working cross
# compiler is a configure error. OFF builds the simulator and unit tests only
# and is explicitly not an acceptance configuration.
option(FX1_SOC_BUILD_FIRMWARE
    "Build FX1 firmware and register fx1_smoke tests (requires a RISC-V cross toolchain)" ON)
set(FX1_RISCV_PREFIX "" CACHE STRING
    "Path prefix of the RISC-V bare-metal toolchain, e.g. /opt/toolchains/riscv-none-elf/bin/riscv-none-elf-")

if(NOT FX1_SOC_BUILD_FIRMWARE)
    message(WARNING
        "fx1_soc: FX1_SOC_BUILD_FIRMWARE=OFF - host-only build. Firmware and the "
        "fx1_smoke tests are not built; this is NOT an FX1 acceptance configuration.")
    return()
endif()

if(NOT FX1_RISCV_PREFIX)
    find_program(_fx1_riscv_gcc riscv-none-elf-gcc
        HINTS "$ENV{RISCV_HOME}/bin" /opt/toolchains/riscv-none-elf/bin
        NO_DEFAULT_PATH)
    if(NOT _fx1_riscv_gcc)
        message(FATAL_ERROR
            "fx1_soc: FX1_SOC_BUILD_FIRMWARE=ON but no riscv-none-elf-gcc was found in "
            "$RISCV_HOME/bin or /opt/toolchains/riscv-none-elf/bin. Set "
            "FX1_RISCV_PREFIX=<dir>/riscv-none-elf- or configure with "
            "-DFX1_SOC_BUILD_FIRMWARE=OFF for a host-only (non-acceptance) build.")
    endif()
    string(REGEX REPLACE "gcc$" "" _fx1_prefix "${_fx1_riscv_gcc}")
    set(FX1_RISCV_PREFIX "${_fx1_prefix}" CACHE STRING "" FORCE)
endif()

# An explicit or discovered prefix must name a compiler that runs.
execute_process(COMMAND "${FX1_RISCV_PREFIX}gcc" --version
    RESULT_VARIABLE _fx1_gcc_status OUTPUT_VARIABLE _fx1_gcc_version ERROR_QUIET)
if(NOT _fx1_gcc_status EQUAL 0)
    message(FATAL_ERROR
        "fx1_soc: FX1_RISCV_PREFIX='${FX1_RISCV_PREFIX}' does not name a working "
        "compiler (${FX1_RISCV_PREFIX}gcc --version failed). Fix the prefix or "
        "configure with -DFX1_SOC_BUILD_FIRMWARE=OFF for a host-only build.")
endif()
string(REGEX MATCH "^[^\n]*" _fx1_gcc_version "${_fx1_gcc_version}")
message(STATUS "fx1_soc: firmware toolchain ${FX1_RISCV_PREFIX}gcc (${_fx1_gcc_version})")
add_subdirectory(${CMAKE_SOURCE_DIR}/fw/fx1_soc ${CMAKE_BINARY_DIR}/fw/fx1_soc)
