# SPDX-License-Identifier: Apache-2.0
#
# TPU_V3 SoC platform. See components/TPU_V3/docs/TPU_V3_IMPLEMENTATION_PLAN.md.

add_executable(tpu_v3_soc
    src/main.cpp
    src/config_loader.cpp
    src/tpu_v3_soc_top.cpp)

target_include_directories(tpu_v3_soc PRIVATE src)

# Recorded in the binary so `--version` can name the build that produced a
# result. The full SHA, not the abbreviated one: the manifest records a second
# revision queried at *package* time, and the two are only comparable if they
# are the same kind of string. Them differing is itself informative — it means
# the tree moved between building the binary and packaging it.
execute_process(
    COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE TPU_V3_GIT_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT TPU_V3_GIT_REVISION)
    set(TPU_V3_GIT_REVISION "unknown")
endif()

target_compile_definitions(tpu_v3_soc PRIVATE
    TPU_V3_SOC_BUILD_TYPE="$<IF:$<BOOL:${CMAKE_BUILD_TYPE}>,${CMAKE_BUILD_TYPE},unspecified>"
    TPU_V3_SOC_GIT_REVISION="${TPU_V3_GIT_REVISION}"
    TPU_V3_SOC_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
    # The backend compiled into *this* binary. The platform refuses a
    # configuration asking for anything else, so the manifest, `--version` and
    # the runtime cannot disagree about which MXU produced a result.
    TPU_V3_SOC_MXU_BACKEND="${TPU_V3_MXU_BACKEND}")

target_link_libraries(tpu_v3_soc PRIVATE
    SystemC::systemc
    cdc::components::tpu_v3_common)

add_custom_command(TARGET tpu_v3_soc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:tpu_v3_soc>/configs
    VERBATIM)

cdc_make_portable(tpu_v3_soc)
cdc_package_platform(tpu_v3_soc)

# ── package contents beyond the base bundle ──────────────────────────────────
#
# `cdc_package_platform` copies the binary, the SystemC runtime and configs.
# A distributable bundle also needs the licence texts of everything statically
# linked into it and a record of what it was built from. Phase 12 adds firmware
# to the same list; the directory is created now so the layout does not change
# when it does.

set(TPU_V3_PACKAGE_DIR "${CDC_PACKAGE_ROOT}/tpu_v3_soc")

# Kept in sync with docs/TPU_V3_PHASE0_AUDIT.md §4. When Spike is actually
# linked (Phase 2), flip TPU_V3_SPIKE_LINKED and add its licence below.
set(TPU_V3_SPIKE_REVISION "16c0b60119f65a648643cf5d41e4e38e871f0bad"
    CACHE STRING "Pinned riscv-isa-sim revision (docs/TPU_V3_PHASE0_AUDIT.md §4)")
set(TPU_V3_SPIKE_LINKED FALSE)

# CDC_ENABLE_SAURIA_NPU_V4 is a repository-wide option: it may build Sauria
# for another platform without putting Sauria into this executable. Keep that
# global state separate from the claims about this binary. Phase 9 may change
# the latter two values only after the backend and redistribution gates pass.
set(TPU_V3_SAURIA_LINKED FALSE)
set(TPU_V3_SAURIA_SELECTABLE FALSE)

# RISC-V VP++ is built as a component but is not yet linked into this
# executable; the platform instantiates no core until Phase 5. Recorded the same
# way as Spike: the manifest states what the binary contains, not what the tree
# can build.
set(TPU_V3_VPP_LINKED FALSE)

# If the VP++ component was skipped (no checkout), the manifest still has to say
# something true rather than emit empty fields.
foreach(_vpp_var RISCV_VP_PLUSPLUS_REVISION RISCV_VP_PLUSPLUS_BACKPORT
                 RISCV_VP_PLUSPLUS_PATCH_NAME RISCV_VP_PLUSPLUS_PATCH_SHA256)
    if(NOT DEFINED ${_vpp_var})
        set(${_vpp_var} "unknown")
    endif()
endforeach()

add_custom_command(TARGET tpu_v3_soc_package POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${TPU_V3_PACKAGE_DIR}/licenses"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${TPU_V3_PACKAGE_DIR}/firmware"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/LICENSE"
            "${TPU_V3_PACKAGE_DIR}/licenses/Apache-2.0.txt"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/NOTICE"
            "${TPU_V3_PACKAGE_DIR}/licenses/CDC-VP-NOTICE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/THIRD_PARTY.md"
            "${TPU_V3_PACKAGE_DIR}/licenses/THIRD_PARTY.md"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/PACKAGE_README.md"
            "${TPU_V3_PACKAGE_DIR}/README.md"
    COMMAND ${CMAKE_COMMAND}
            -DMANIFEST_PATH=${TPU_V3_PACKAGE_DIR}/BUILD_MANIFEST.json
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DBUILD_REVISION=${TPU_V3_GIT_REVISION}
            -DPLATFORM=tpu_v3_soc
            -DBUILD_TYPE=$<IF:$<BOOL:${CMAKE_BUILD_TYPE}>,${CMAKE_BUILD_TYPE},unspecified>
            -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}
            -DCXX_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}
            -DCMAKE_VERSION_USED=${CMAKE_VERSION}
            -DSYSTEMC_HOME=${SYSTEMC_HOME}
            -DMXU_BACKEND=${TPU_V3_MXU_BACKEND}
            -DSPIKE_REVISION=${TPU_V3_SPIKE_REVISION}
            -DSPIKE_LINKED=${TPU_V3_SPIKE_LINKED}
            -DVPP_BASE_REVISION=${RISCV_VP_PLUSPLUS_REVISION}
            -DVPP_BACKPORT_REVISION=${RISCV_VP_PLUSPLUS_BACKPORT}
            -DVPP_PATCH_NAME=${RISCV_VP_PLUSPLUS_PATCH_NAME}
            -DVPP_PATCH_SHA256=${RISCV_VP_PLUSPLUS_PATCH_SHA256}
            -DVPP_LINKED=${TPU_V3_VPP_LINKED}
            -DSAURIA_OPTION_ENABLED=${CDC_ENABLE_SAURIA_NPU_V4}
            -DSAURIA_LINKED=${TPU_V3_SAURIA_LINKED}
            -DSAURIA_SELECTABLE=${TPU_V3_SAURIA_SELECTABLE}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/write_build_manifest.cmake"
    VERBATIM)

# ── tests ────────────────────────────────────────────────────────────────────

if(CDC_BUILD_TPU_V3_TESTS)
    find_program(TPU_V3_PYTHON3 NAMES python3 REQUIRED)

    # Platform-level behaviour: configuration parsing, overrides, the refusals
    # that protect the frozen architecture, and the reported description. The
    # component unit tests cannot cover these because the parser deliberately
    # lives here and not in the component.
    add_test(
        NAME tpu_v3_soc_cli
        COMMAND ${TPU_V3_PYTHON3}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tpu_v3_cli.py
                $<TARGET_FILE:tpu_v3_soc>
                ${CMAKE_CURRENT_SOURCE_DIR}/configs)
    set_tests_properties(tpu_v3_soc_cli PROPERTIES
        LABELS "tpu_v3;platform"
        TIMEOUT 300)

    # Phase 1 distribution gate. Configures a child Release build with a
    # private package root, so it neither consumes artifacts from this build
    # nor collides with a concurrent CTest job, then runs the packaged binary
    # from a directory that has no access to the source or build tree.
    add_test(
        NAME tpu_v3_soc_packaging_regression
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_packaging_regression.sh)
    set_tests_properties(tpu_v3_soc_packaging_regression PROPERTIES
        ENVIRONMENT "PACKAGING_SYSTEMC_HOME=${SYSTEMC_HOME}"
        LABELS "tpu_v3;packaging;distribution"
        RUN_SERIAL TRUE
        SKIP_RETURN_CODE 77
        TIMEOUT 1800)
endif()
