# SPDX-License-Identifier: Apache-2.0
#
# tpu_v3_core_sram: the shared SRAM of one NEO-CORE (plan §11.3).
#
# Storage and policy only. Banking, arbitration and requester attribution are
# `tpu_v3_tpu_core`'s (decision record D15), which is why this target does not
# depend on it and its test needs no arbiter.

add_library(tpu_v3_core_sram STATIC
    src/core_sram.cpp)
add_library(cdc::components::tpu_v3_core_sram ALIAS tpu_v3_core_sram)

target_include_directories(tpu_v3_core_sram
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tpu_v3_core_sram
    PUBLIC
        cdc::components::tpu_v3_native_port
        cdc::components::tpu_v3_common
        SystemC::systemc)

target_compile_features(tpu_v3_core_sram PUBLIC cxx_std_17)

install(TARGETS tpu_v3_core_sram EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(tests)
endif()
