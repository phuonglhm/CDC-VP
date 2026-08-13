# SPDX-License-Identifier: Apache-2.0
#
# tpu_v3_common: configuration objects and the address map.
#
# Deliberately does NOT link SystemC. A configuration object and an address
# calculation are plain C++, and keeping them that way means their tests are
# ordinary programs with an ordinary `main()` rather than SystemC elaborations.
# The TLM-facing components added from Phase 3 onwards link SystemC themselves.

add_library(tpu_v3_common STATIC
    src/address_map.cpp
    src/architecture_config.cpp
    src/sparse_memory.cpp)
add_library(cdc::components::tpu_v3_common ALIAS tpu_v3_common)

target_include_directories(tpu_v3_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_compile_features(tpu_v3_common PUBLIC cxx_std_17)

install(TARGETS tpu_v3_common EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(tests)
endif()
