# SPDX-License-Identifier: Apache-2.0
#
# tpu_v3_native_port: the native local-data interface of a NEO-CORE, and
# nothing else.
#
# It exists so that a requester can depend on the *interface* without
# depending on the SRAM implementation behind it. `tpu_v3_neo_dma` linked
# `tpu_v3_core_sram` for one header, which meant the DMA's build graph named
# the storage it is forbidden to reach — the code was right and the dependency
# said otherwise, which is exactly the sort of gap a later change slips
# through. `neo_dma_independence` now refuses a link edge matching
# `core_sram`, so this split is enforced rather than merely intended.
#
# It is a compiled library rather than an INTERFACE one because the requester
# and status enumerations have `to_string` overloads, and a header-only target
# would leave those undefined for anyone who printed a status.

add_library(tpu_v3_native_port STATIC
    src/native_port.cpp)
add_library(cdc::components::tpu_v3_native_port ALIAS tpu_v3_native_port)

target_include_directories(tpu_v3_native_port
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tpu_v3_native_port PUBLIC SystemC::systemc)
target_compile_features(tpu_v3_native_port PUBLIC cxx_std_17)

install(TARGETS tpu_v3_native_port EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)
