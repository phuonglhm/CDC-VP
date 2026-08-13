# SPDX-License-Identifier: Apache-2.0
#
# tpu_v3_neo_dma: the independent NEO DMA (plan §11.5, decision record D14).
#
# The link list is part of the contract, not an incidental detail. This target
# depends on the native-port *interface* and the address map, and on nothing
# else: not on the shared PL330-style DMA component, not on the NPU model, not
# on `tpu_v3_tpu_core` whose fabric it reaches through `neo_local_sram_if`
# alone — and not on `tpu_v3_core_sram`, the storage it is forbidden to reach.
# That last one was a real edge until the interface was split out of the SRAM
# library: the code never touched the storage, but the build graph said it
# depended on it, and a dependency nobody needs is the one a later change
# quietly starts using. `neo_dma_independence` re-checks the sources, the built
# archive and this list.

add_library(tpu_v3_neo_dma STATIC
    src/neo_dma.cpp)
add_library(cdc::components::tpu_v3_neo_dma ALIAS tpu_v3_neo_dma)

target_include_directories(tpu_v3_neo_dma
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tpu_v3_neo_dma
    PUBLIC
        cdc::components::tpu_v3_native_port
        cdc::components::tpu_v3_common
        SystemC::systemc)

target_compile_features(tpu_v3_neo_dma PUBLIC cxx_std_17)

install(TARGETS tpu_v3_neo_dma EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(tests)
endif()
