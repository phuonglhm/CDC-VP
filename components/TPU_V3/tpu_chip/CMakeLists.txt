# SPDX-License-Identifier: Apache-2.0
#
# Phase 8: the dual-core TPU chip.
#
# Two targets, split on the same line `tpu_core` is split on. The chip-local
# fabric is a decoder and an arbiter — it needs neither a CPU backend nor an
# extracted accelerator, so a consumer of it should not have to link an
# instruction-set simulator. The composition does need all of them, and is
# built only when they are present: half a chip is not a chip.

add_library(tpu_v3_chip_fabric STATIC src/chip_local_fabric.cpp)
add_library(cdc::components::tpu_v3_chip_fabric ALIAS tpu_v3_chip_fabric)

target_include_directories(tpu_v3_chip_fabric
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tpu_v3_chip_fabric
    PUBLIC
        cdc::components::tpu_v3_tpu_core
        cdc::components::tpu_v3_common
        SystemC::systemc)

target_compile_features(tpu_v3_chip_fabric PUBLIC cxx_std_17)

install(TARGETS tpu_v3_chip_fabric EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(tests)
endif()

# ── the chip composition ─────────────────────────────────────────────────────
#
# Gated on exactly what `tpu_v3_neo_core` is gated on, because it is two of
# them. Deliberately not added to the `cdc-components-targets` export set for
# the same reason: it links the VP++ ISS, which is not in that set, and the
# redistributable artifact is the platform executable (Phase 12).
if(TARGET cdc::components::tpu_v3_neo_core)
    add_library(tpu_v3_tpu_chip STATIC src/tpu_chip.cpp)
    add_library(cdc::components::tpu_v3_tpu_chip ALIAS tpu_v3_tpu_chip)

    target_include_directories(tpu_v3_tpu_chip
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)

    target_link_libraries(tpu_v3_tpu_chip
        PUBLIC
            cdc::components::tpu_v3_chip_fabric
            cdc::components::tpu_v3_neo_core
            SystemC::systemc)

    target_compile_features(tpu_v3_tpu_chip PUBLIC cxx_std_17)

    if(CDC_BUILD_TPU_V3_TESTS)
        add_subdirectory(tests/chip)
    endif()
endif()
