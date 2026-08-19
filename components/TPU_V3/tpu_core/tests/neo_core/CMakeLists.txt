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
set(NEO_PIPELINE_FW_DIR "${CMAKE_SOURCE_DIR}/fw/TPU_V3_SoC/neo_core_pipeline")
set(NEO_PIPELINE_TOOLDIR "/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin"
    CACHE PATH "Directory holding the riscv-none-elf cross toolchain (D4)")
set(NEO_PIPELINE_ELF "${NEO_PIPELINE_FW_DIR}/neo_core_pipeline.elf")

add_executable(test_tpu_v3_neo_core_pipeline test_neo_core_pipeline.cpp)
target_link_libraries(test_tpu_v3_neo_core_pipeline PRIVATE
    cdc::components::tpu_v3_neo_core)
# The firmware's own map header, so the test can assert it against address_map.h
# rather than trusting that two copies of the same constants agree.
target_include_directories(test_tpu_v3_neo_core_pipeline PRIVATE
    "${NEO_PIPELINE_FW_DIR}")

if(EXISTS "${NEO_PIPELINE_TOOLDIR}/riscv-none-elf-gcc")
    add_custom_command(
        OUTPUT "${NEO_PIPELINE_ELF}"
        COMMAND ${CMAKE_COMMAND} -E env "TOOLDIR=${NEO_PIPELINE_TOOLDIR}"
                make -C "${NEO_PIPELINE_FW_DIR}"
        DEPENDS "${NEO_PIPELINE_FW_DIR}/crt0.S"
                "${NEO_PIPELINE_FW_DIR}/main.c"
                "${NEO_PIPELINE_FW_DIR}/link.ld"
                "${NEO_PIPELINE_FW_DIR}/neo_core_map.h"
                "${NEO_PIPELINE_FW_DIR}/Makefile"
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
