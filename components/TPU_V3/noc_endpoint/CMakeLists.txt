# SPDX-License-Identifier: Apache-2.0
#
# Phase 9: the chip's NoC endpoint.
#
# Deliberately free of any dependency on `noc_interconnect`. The endpoint is
# four TLM sockets and a chunking rule; placement, the manager port and the D1
# owner mapping are the platform's business. Keeping the dependency out means
# the endpoint's own gate needs no mesh, and it cannot quietly acquire a habit
# of reaching into the interconnect for something it should have been told.

add_library(tpu_v3_noc_endpoint STATIC src/chip_noc_endpoint.cpp)
add_library(cdc::components::tpu_v3_noc_endpoint ALIAS tpu_v3_noc_endpoint)

target_include_directories(tpu_v3_noc_endpoint
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tpu_v3_noc_endpoint
    PUBLIC
        cdc::components::tpu_v3_tpu_core
        cdc::components::tpu_v3_common
        SystemC::systemc)

target_compile_features(tpu_v3_noc_endpoint PUBLIC cxx_std_17)

install(TARGETS tpu_v3_noc_endpoint EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(tests)
endif()
