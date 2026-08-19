# SPDX-License-Identifier: Apache-2.0
#
# tpu_v3_tpu_core: the three NEO-CORE interconnect planes of decision record
# D15 — a 32-bit AXI4-Lite control fabric, a native banked local-SRAM data
# fabric, and the bidirectional external bridge between them and the
# chip/global path.
#
# Phase 7 adds `neo_hart_port`, the fourth piece: the hart has one combined
# TLM socket and the local fabric has no TLM target, so nothing could bind a
# CPU to these planes without it.
#
# `tpu_core` itself — the composition that owns a hart, an SRAM, a DMA, an MXU
# and a Transform block — is the rest of Phase 7.

add_library(tpu_v3_tpu_core STATIC
    src/neo_local_sram_fabric.cpp
    src/neo_control_fabric.cpp
    src/core_registers.cpp
    src/neo_external_bridge.cpp
    src/neo_hart_port.cpp)
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

# ── Phase 7: the NEO-CORE composition ────────────────────────────────────────
#
# A second target rather than more sources in `tpu_v3_tpu_core`, because the
# composition needs the CPU backend and both extracted accelerators while the
# fabrics need none of them. Folding them together would make every consumer of
# a fabric link an instruction-set simulator.
#
# It is built only when all three are actually present. Half a NEO-CORE is not
# a NEO-CORE: D14 freezes one MXU and one Transform block per core, and plan
# §10 already warns that "an exported target that links and does nothing is
# worse than a missing one, because a platform can depend on it and appear to
# work". A build without the pinned sources gets no target, and the platform
# report keeps saying what it does not contain.
if(CDC_BUILD_TPU_V3_SAURIA_MATRIX
   AND CDC_BUILD_TPU_V3_IMAGE_TRANSFORM
   AND TARGET cdc::cpu::riscv_vp_plusplus)

    add_library(tpu_v3_neo_core STATIC src/tpu_core.cpp)
    add_library(cdc::components::tpu_v3_neo_core ALIAS tpu_v3_neo_core)

    target_include_directories(tpu_v3_neo_core
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)

    target_link_libraries(tpu_v3_neo_core
        PUBLIC
            cdc::components::tpu_v3_tpu_core
            cdc::components::tpu_v3_neo_dma
            cdc::components::tpu_v3_sauria_matrix
            cdc::components::tpu_v3_image_transform
            cdc::cpu::riscv_vp_plusplus
            SystemC::systemc)

    target_compile_features(tpu_v3_neo_core PUBLIC cxx_std_17)

    # Deliberately not added to the `cdc-components-targets` export set. It
    # links the VP++ ISS, which is not in that set, and exporting a component
    # whose dependency is not exportable produces a config package that cannot
    # be consumed. The redistributable artifact for this composition is the
    # platform executable (Phase 12), not a static library someone links
    # against out of tree.

    if(CDC_BUILD_TPU_V3_TESTS)
        add_subdirectory(tests/neo_core)
    endif()
endif()
