# SPDX-License-Identifier: Apache-2.0
#
# The Phase 7 composition gate. Separate from the fabric tests next door
# because it needs the CPU backend and both extracted accelerators, and those
# are exactly what `tpu_v3_neo_core` is gated on.

add_executable(test_tpu_v3_neo_core test_tpu_core.cpp)
target_link_libraries(test_tpu_v3_neo_core PRIVATE
    cdc::components::tpu_v3_neo_core)
add_test(NAME tpu_v3_neo_core COMMAND test_tpu_v3_neo_core)
set_tests_properties(tpu_v3_neo_core PROPERTIES
    LABELS "tpu_v3;integration;core"
    TIMEOUT 300)

# ── the Phase 7 pipeline gate ────────────────────────────────────────────────
#
# One firmware ELF drives every engine through MMIO. The image is built by the
# pinned cross toolchain; without it the test skips rather than fails, on the
# same terms as the Phase 2 images.
#
# **Built in the build tree, not in the source tree.** The first version put the
# ELF next to its sources, which is what `fw/TPU_V3_SoC/rvv_smoke` does, and it
# is wrong for four separate reasons: a Release and a Debug tree race for the
# same output file; a read-only source checkout cannot build at all; a stale
# gitignored ELF can be run without anyone noticing it was never rebuilt; and
# the two configurations silently share whichever one won. Phase 4.5 already
# does it the other way (`platforms/riscv_vpp_compiler_vp` builds firmware under
# `${CMAKE_CURRENT_BINARY_DIR}`); this follows it.
set(NEO_PIPELINE_FW_SRC "${CMAKE_SOURCE_DIR}/fw/TPU_V3_SoC/neo_core_pipeline")
set(NEO_PIPELINE_FW_BUILD "${CMAKE_CURRENT_BINARY_DIR}/firmware")
set(NEO_PIPELINE_TOOLDIR "/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin"
    CACHE PATH "Directory holding the riscv-none-elf cross toolchain (D4)")
set(NEO_PIPELINE_ELF "${NEO_PIPELINE_FW_BUILD}/neo_core_pipeline.elf")

set(NEO_PIPELINE_FW_SOURCES
    crt0.S main.c link.ld neo_core_map.h Makefile)

add_executable(test_tpu_v3_neo_core_pipeline test_neo_core_pipeline.cpp)
target_link_libraries(test_tpu_v3_neo_core_pipeline PRIVATE
    cdc::components::tpu_v3_neo_core)
# The firmware's own map header, so the test can assert it against address_map.h
# rather than trusting that two copies of the same constants agree.
target_include_directories(test_tpu_v3_neo_core_pipeline PRIVATE
    "${NEO_PIPELINE_FW_SRC}")

if(EXISTS "${NEO_PIPELINE_TOOLDIR}/riscv-none-elf-gcc")
    set(_neo_pipeline_copied "")
    foreach(_f IN LISTS NEO_PIPELINE_FW_SOURCES)
        add_custom_command(
            OUTPUT "${NEO_PIPELINE_FW_BUILD}/${_f}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${NEO_PIPELINE_FW_BUILD}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${NEO_PIPELINE_FW_SRC}/${_f}"
                    "${NEO_PIPELINE_FW_BUILD}/${_f}"
            DEPENDS "${NEO_PIPELINE_FW_SRC}/${_f}"
            VERBATIM)
        list(APPEND _neo_pipeline_copied "${NEO_PIPELINE_FW_BUILD}/${_f}")
    endforeach()

    add_custom_command(
        OUTPUT "${NEO_PIPELINE_ELF}"
        COMMAND ${CMAKE_COMMAND} -E env "TOOLDIR=${NEO_PIPELINE_TOOLDIR}"
                make -C "${NEO_PIPELINE_FW_BUILD}"
        DEPENDS ${_neo_pipeline_copied}
        COMMENT "Building the NEO-CORE pipeline firmware image"
        VERBATIM)
    add_custom_target(neo_core_pipeline_image DEPENDS "${NEO_PIPELINE_ELF}")
    add_dependencies(test_tpu_v3_neo_core_pipeline neo_core_pipeline_image)
else()
    message(STATUS
        "RV32GCV cross toolchain not found at ${NEO_PIPELINE_TOOLDIR}; "
        "tpu_v3_neo_core_pipeline will skip")
endif()

add_test(NAME tpu_v3_neo_core_pipeline
    COMMAND test_tpu_v3_neo_core_pipeline "${NEO_PIPELINE_ELF}")
set_tests_properties(tpu_v3_neo_core_pipeline PROPERTIES
    LABELS "tpu_v3;integration;core;golden"
    SKIP_RETURN_CODE 77
    TIMEOUT 600)

# ── D27 standalone one-core baseline ───────────────────────────────────────
#
# Reuse the verified Phase 7 firmware and harness, but compile both views for
# chip-index 0/core-index 0. The indices only retain the existing absolute map;
# this target instantiates no `tpu_chip`, endpoint or NoC.
set(NEO_STANDALONE_FW_BUILD
    "${CMAKE_CURRENT_BINARY_DIR}/standalone_firmware")
set(NEO_STANDALONE_ELF
    "${NEO_STANDALONE_FW_BUILD}/neo_core_standalone_pipeline.elf")
set(NEO_STANDALONE_MANIFEST
    "${CMAKE_CURRENT_BINARY_DIR}/neo_core_microbench_manifest.json")

add_executable(neo_core_microbench test_neo_core_pipeline.cpp)
target_link_libraries(neo_core_microbench PRIVATE
    cdc::components::tpu_v3_neo_core)
target_include_directories(neo_core_microbench PRIVATE
    "${NEO_PIPELINE_FW_SRC}")
target_compile_definitions(neo_core_microbench PRIVATE
    NEO_PIPELINE_CHIP_ID=0u
    NEO_PIPELINE_CORE_ID=0u)

# A configure-time dependency-graph guard catches someone directly adding a
# retained chip/NoC target. The post-build symbol guard below catches a hidden
# or renamed link edge that reaches actual chip/NoC implementation code.
get_target_property(_d27_harness_links neo_core_microbench LINK_LIBRARIES)
get_target_property(_d27_core_links tpu_v3_neo_core LINK_LIBRARIES)
string(JOIN ";" _d27_dependency_evidence
    "${_d27_harness_links}" "${_d27_core_links}")
if(_d27_dependency_evidence MATCHES
   "(tpu_v3_tpu_chip|tpu_v3_chip_fabric|tpu_v3_noc_endpoint|floo_noc|noc_interconnect)")
    message(FATAL_ERROR
        "D27 standalone dependency closure contains chip/NoC code: "
        "${_d27_dependency_evidence}")
endif()

add_custom_command(TARGET neo_core_microbench POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            "-DBINARY=$<TARGET_FILE:neo_core_microbench>"
            "-DNM=${CMAKE_NM}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_standalone_dependencies.cmake"
    COMMENT "Checking the D27 standalone binary for chip/NoC dependencies"
    VERBATIM)

if(EXISTS "${NEO_PIPELINE_TOOLDIR}/riscv-none-elf-gcc")
    set(_neo_standalone_copied "")
    foreach(_f IN LISTS NEO_PIPELINE_FW_SOURCES)
        add_custom_command(
            OUTPUT "${NEO_STANDALONE_FW_BUILD}/${_f}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${NEO_STANDALONE_FW_BUILD}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${NEO_PIPELINE_FW_SRC}/${_f}"
                    "${NEO_STANDALONE_FW_BUILD}/${_f}"
            DEPENDS "${NEO_PIPELINE_FW_SRC}/${_f}"
            VERBATIM)
        list(APPEND _neo_standalone_copied
             "${NEO_STANDALONE_FW_BUILD}/${_f}")
    endforeach()

    add_custom_command(
        OUTPUT "${NEO_STANDALONE_ELF}"
        COMMAND ${CMAKE_COMMAND} -E env "TOOLDIR=${NEO_PIPELINE_TOOLDIR}"
                make -C "${NEO_STANDALONE_FW_BUILD}"
                PIPELINE_CHIP_ID=0 PIPELINE_CORE_ID=0
                ELF=neo_core_standalone_pipeline.elf
        DEPENDS ${_neo_standalone_copied}
        COMMENT "Building the D27 standalone NEO-CORE pipeline image"
        VERBATIM)
    add_custom_target(neo_core_standalone_pipeline_image
        DEPENDS "${NEO_STANDALONE_ELF}")
    add_dependencies(neo_core_microbench
        neo_core_standalone_pipeline_image)
else()
    message(STATUS
        "RV32GCV cross toolchain not found at ${NEO_PIPELINE_TOOLDIR}; "
        "tpu_v3_neo_core_d27_baseline will skip")
endif()

add_test(NAME tpu_v3_neo_core_d27_baseline
    COMMAND neo_core_microbench "${NEO_STANDALONE_ELF}"
            --manifest "${NEO_STANDALONE_MANIFEST}")
set_tests_properties(tpu_v3_neo_core_d27_baseline PROPERTIES
    LABELS "tpu_v3;integration;core;golden;d27;standalone"
    SKIP_RETURN_CODE 77
    TIMEOUT 600)

add_test(NAME tpu_v3_neo_core_d27_dependency_guard_control
    COMMAND ${CMAKE_COMMAND}
            "-DFORCED_SYMBOLS=cdc::components::tpu_v3::chip::tpu_chip::tpu_chip"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_standalone_dependencies.cmake")
set_tests_properties(tpu_v3_neo_core_d27_dependency_guard_control PROPERTIES
    LABELS "tpu_v3;integration;core;d27;standalone;negative_control"
    WILL_FAIL TRUE
    TIMEOUT 30)
