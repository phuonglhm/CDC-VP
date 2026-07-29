# SPDX-License-Identifier: Apache-2.0

set(_sauria_v42_required_files
    npu_top.h
    npu_profile.h
    sauria_targets.h
    driver/libsauria_cfg.h
    instrumentation/perf_counters.h)

foreach(_sauria_file IN LISTS _sauria_v42_required_files)
    if(NOT EXISTS "${SAURIA_NPU_ROOT}/${_sauria_file}")
        message(FATAL_ERROR
            "CDC_ENABLE_SAURIA_NPU_V4=ON requires the SAURIA V4.2 model, but "
            "'${_sauria_file}' was not found at "
            "SAURIA_NPU_ROOT='${SAURIA_NPU_ROOT}'. "
            "Set -DSAURIA_NPU_ROOT=<path-to-v4.2_model>.")
    endif()
endforeach()

add_library(npu_tlm_v4_model STATIC
    src/npu_tlm_v4_model.cpp)
add_library(cdc::components::npu_tlm_v4_model ALIAS npu_tlm_v4_model)

target_include_directories(npu_tlm_v4_model
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${SAURIA_NPU_ROOT}"
        "${SAURIA_NPU_ROOT}/driver"
        "${SAURIA_NPU_ROOT}/instrumentation")

target_link_libraries(npu_tlm_v4_model PUBLIC SystemC::systemc)

install(TARGETS npu_tlm_v4_model EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
