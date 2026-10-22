# FX1 SoC platform (CDC_BUILD_FX1_SOC). Host side: the simulator executable.
# Firmware side: fw/fx1_soc, built with the RISC-V cross toolchain when one is
# found, and registered as ctest cases that run on this simulator.
# DDR3 subsystem glue (DDR3 VP subtask 5): controller wiring, DDR_SYSCTRL,
# boot modes, host image/fault interface. Shared by the simulator and its test.
add_library(fx1_soc_ddr3 STATIC
    src/ddr3_subsystem.cpp
    src/ddr_sysctrl.cpp)
target_include_directories(fx1_soc_ddr3 PUBLIC src)
target_compile_options(fx1_soc_ddr3 PRIVATE -Wall -Wextra)
target_link_libraries(fx1_soc_ddr3 PUBLIC
    SystemC::systemc
    cdc::components::fx1_map
    cdc::components::fx1_ddr3)

add_executable(fx1_soc
    src/main.cpp
    src/fx1_soc_top.cpp)

# `fx1_soc --version`: the package name, the CDC-VP revision and tree state at
# configure time, the build type and compilers (cmake/build_info.h.in).
set(FX1_SOC_PACKAGE_NAME "VP_FX1_V3.0" CACHE STRING "SDK package name the fx1_soc binary reports")
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
                OUTPUT_VARIABLE FX1_SOC_GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" status --porcelain --untracked-files=no
                OUTPUT_VARIABLE _fx1_git_status OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
                RESULT_VARIABLE _fx1_git_rc)
if(NOT FX1_SOC_GIT_REVISION OR NOT _fx1_git_rc EQUAL 0)
    set(FX1_SOC_GIT_REVISION "unknown")
    set(FX1_SOC_GIT_STATE "unknown")
elseif(_fx1_git_status)
    set(FX1_SOC_GIT_STATE "dirty")
else()
    set(FX1_SOC_GIT_STATE "clean")
endif()
if(CMAKE_BUILD_TYPE)
    set(FX1_SOC_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
    set(FX1_SOC_BUILD_TYPE "none (developer build, not for packaging)")
endif()
configure_file(cmake/build_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/fx1_soc_build_info.h @ONLY)

target_include_directories(fx1_soc PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/generated)
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
    cdc::components::uart2_tlm
    cdc::components::uart_host_tlm
    cdc::components::i2c_tlm
    fx1_soc_ddr3)
# --camera: present when the MIPI CSI-2 component is configured
# (-DFX1_BUILD_MIPI_CSI2=ON). Its D-PHY netlist adds ~45 s of build at -j8.
if(TARGET cdc::components::fx1_csi2_controller)
    target_link_libraries(fx1_soc PRIVATE
        cdc::components::fx1_csi2_controller
        cdc::components::fx1_mipi_dphy_wrapper
        cdc::components::fx1_csi2_dphy_bfm
        cdc::components::fx1_csi2_sensor)
    target_compile_definitions(fx1_soc PRIVATE FX1_SOC_HAS_CAMERA=1)
endif()
# Self-contained like the other packaged platforms (cmake/modules/CdcPortable.cmake):
# RPATH=$ORIGIN, and the SystemC runtime copied beside the binary. The SDK
# package (package/package_sdk.sh, target fx1_soc_package) ships exactly that.
cdc_make_portable(fx1_soc)

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

# DDR3 subtask 5: DDR_SYSCTRL registers, P-channel and reset sequencing, the
# preinit order (scrub -> preload -> release), firmware-boot staging refusal,
# dump, the --ddr3-fault parser, and the firmware reference image
# (fw/fx1_soc/ddr3/ddr3_ref_1600.h) against the model's.
add_executable(test_fx1_ddr3_subsystem tests/test_ddr3_subsystem.cpp)
target_compile_options(test_fx1_ddr3_subsystem PRIVATE -Wall -Wextra)
target_include_directories(test_fx1_ddr3_subsystem PRIVATE ${CMAKE_SOURCE_DIR}/fw/fx1_soc/ddr3)
target_link_libraries(test_fx1_ddr3_subsystem PRIVATE fx1_soc_ddr3)
if(CDC_BUILD_TESTS)
    foreach(_case sysctrl power reset preinit firmware parser refimage wake_recovery wake_busy canonical)
        add_test(NAME fx1_ddr3_subsystem_${_case} COMMAND test_fx1_ddr3_subsystem ${_case})
        set_tests_properties(fx1_ddr3_subsystem_${_case} PROPERTIES TIMEOUT 120 LABELS "fx1;fx1_unit;ddr3")
    endforeach()
endif()

# CSI-2 M6a follow-up review R2: budget contract of the BSP I2C driver (built
# for the host with tests/hostshim/fx1_fw.h) against the real I2C0 model.
add_library(fx1_i2c_bsp_host STATIC ${CMAKE_SOURCE_DIR}/fw/fx1_soc/drivers/fx1_i2c.c)
target_include_directories(fx1_i2c_bsp_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests/hostshim
                                                   ${CMAKE_SOURCE_DIR}/fw/fx1_soc/drivers)
set_target_properties(fx1_i2c_bsp_host PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(fx1_i2c_bsp_host PRIVATE -Wall -Wextra -Werror)
add_executable(test_fx1_i2c_bsp_budget tests/test_i2c_bsp_budget.cpp)
target_compile_options(test_fx1_i2c_bsp_budget PRIVATE -Wall -Wextra)
target_link_libraries(test_fx1_i2c_bsp_budget PRIVATE fx1_i2c_bsp_host cdc::components::i2c_tlm SystemC::systemc)
if(CDC_BUILD_TESTS)
    add_test(NAME fx1_i2c_bsp_budget COMMAND test_fx1_i2c_bsp_budget)
    set_tests_properties(fx1_i2c_bsp_budget PROPERTIES TIMEOUT 120 LABELS "fx1;fx1_unit")
endif()

# ── CI aggregate (plan P5) ─────────────────────────────────────────────────────
# `fx1_all` builds everything the FX1 tests need (simulator, harnesses, FX1
# component and VP++ backend targets, firmware images) and nothing else of the
# tree: `components/isp_tlm` is added unconditionally upstream and does not
# link today, so `cmake --build` of the default target fails for reasons
# unrelated to FX1. The list is collected once the whole tree is configured.
add_custom_target(fx1_all)
function(_fx1_collect_targets out dir)
    # A subtree added with add_subdirectory(... EXCLUDE_FROM_ALL) is opt-in
    # (e.g. the MIPI CSI-2 D-PHY netlist) and stays out of fx1_all as well.
    get_property(_excluded DIRECTORY "${dir}" PROPERTY EXCLUDE_FROM_ALL)
    if(_excluded)
        set(${out} "" PARENT_SCOPE)
        return()
    endif()
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    set(_list "")
    foreach(_t IN LISTS _targets)
        get_target_property(_type ${_t} TYPE)
        # fx1_soc_package consumes fx1_all, it is not part of it.
        if(NOT _type STREQUAL "INTERFACE_LIBRARY" AND NOT _t STREQUAL "fx1_all" AND NOT _t STREQUAL "fx1_soc_package")
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
              "${CMAKE_SOURCE_DIR}/components/uart2_tlm"
              "${CMAKE_SOURCE_DIR}/components/uart_host_tlm"
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

# ── SDK package VP_FX1_V3.0 (G6b) ────────────────────────────────────────────
# `cmake --build <dir> --target fx1_soc_package` assembles the delivery in the
# SDK repository layout under ${CDC_PACKAGE_ROOT}/VP_FX1_V3.0 (package/README
# in package_sdk.sh). The default ISP sample is the cleared D65 frame; with
# FX1_DEMO_RAW set, the outdoor1 frame is added as a second, internal-only one.
set(FX1_SOC_PACKAGE_DIR "${CDC_PACKAGE_ROOT}/VP_FX1_V3.0")
get_filename_component(FX1_RISCV_TOOLDIR "${FX1_RISCV_PREFIX}gcc" DIRECTORY)
set(_fx1_samples --sample "d65=${CMAKE_SOURCE_DIR}/components/isp_tlm/input/D65_raw_2688x1520_5376.raw")
if(FX1_DEMO_RAW AND EXISTS "${FX1_DEMO_RAW}")
    list(APPEND _fx1_samples --sample
        "outdoor1=${FX1_DEMO_RAW},internal")
endif()
add_custom_target(fx1_soc_package
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/package/package_sdk.sh" --build-dir "${CMAKE_BINARY_DIR}"
            --out "${FX1_SOC_PACKAGE_DIR}" --tooldir "${FX1_RISCV_TOOLDIR}" ${_fx1_samples}
    COMMENT "Packaging VP_FX1_V3.0 -> ${FX1_SOC_PACKAGE_DIR}"
    USES_TERMINAL
    VERBATIM)
add_dependencies(fx1_soc_package fx1_all)
if(CDC_BUILD_TESTS)
    # The distribution gate: fresh Release build, package, move, run with an
    # empty environment, rebuild the firmware from the SDK. Slow; not part of
    # run_ci.sh. Label fx1_packaging.
    add_test(NAME fx1_packaging COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/package/run_packaging_regression.sh")
    set_tests_properties(fx1_packaging PROPERTIES
        LABELS "fx1;fx1_packaging" RUN_SERIAL TRUE TIMEOUT 3600
        ENVIRONMENT "FX1_DEMO_RAW=${FX1_DEMO_RAW};PACKAGING_SYSTEMC_HOME=${SYSTEMC_HOME}")
endif()
