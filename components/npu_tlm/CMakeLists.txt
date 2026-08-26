# SPDX-License-Identifier: Apache-2.0

set(_sauria_model_root "${SAURIA_NPU_ROOT}")
if(NOT _sauria_model_root)
    set(_sauria_model_root "${CMAKE_CURRENT_SOURCE_DIR}/models/v4.4_model_25Aug")
endif()

set(_sauria_v44_required_files
    npu_top.h
    npu_profile.h
    sauria_targets.h
    config_regs.h
    control/instruction_decoder.h
    control/sauria_dma.h
    driver/libsauria_cfg.h
    driver/sauria_run.h
    instrumentation/perf_counters.h)

foreach(_sauria_file IN LISTS _sauria_v44_required_files)
    if(NOT EXISTS "${_sauria_model_root}/${_sauria_file}")
        message(FATAL_ERROR
            "CDC_ENABLE_SAURIA_NPU_V4=ON requires the SAURIA V4.4 model, but "
            "'${_sauria_file}' was not found at "
            "SAURIA_NPU_ROOT='${_sauria_model_root}'. "
            "Set -DSAURIA_NPU_ROOT=<path-to-v4.4-model> to override the "
            "bundled model.")
    endif()
endforeach()

add_library(npu_tlm STATIC
    src/npu_tlm.cpp)
add_library(cdc::components::npu_tlm ALIAS npu_tlm)

target_include_directories(npu_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${_sauria_model_root}"
        "${_sauria_model_root}/driver"
        "${_sauria_model_root}/instrumentation")

target_link_libraries(npu_tlm PUBLIC SystemC::systemc)
target_compile_features(npu_tlm PUBLIC cxx_std_17)

install(TARGETS npu_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
