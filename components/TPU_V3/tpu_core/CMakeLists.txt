# SPDX-License-Identifier: Apache-2.0
#
# tpu_v3_tpu_core: the three NEO-CORE interconnect planes of decision record
# D15 — a 32-bit AXI4-Lite control fabric, a native banked local-SRAM data
# fabric, and the bidirectional external bridge between them and the
# chip/global path.
#
# `tpu_core` itself — the composition that owns a hart, an SRAM, a DMA, a
# matrix engine and a transform engine — is Phase 7. The directory carries the
# name it will have then rather than being renamed later.

add_library(tpu_v3_tpu_core STATIC
    src/neo_local_sram_fabric.cpp
    src/neo_control_fabric.cpp
    src/core_registers.cpp
    src/neo_external_bridge.cpp)
add_library(cdc::components::tpu_v3_tpu_core ALIAS tpu_v3_tpu_core)

target_include_directories(tpu_v3_tpu_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tpu_v3_tpu_core
    PUBLIC
        cdc::components::tpu_v3_core_sram
        cdc::components::tpu_v3_common
        SystemC::systemc)

target_compile_features(tpu_v3_tpu_core PUBLIC cxx_std_17)

install(TARGETS tpu_v3_tpu_core EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(tests)
endif()
