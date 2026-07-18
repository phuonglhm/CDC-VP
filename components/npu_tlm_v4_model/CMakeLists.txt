if(NOT EXISTS "${SAURIA_NPU_ROOT}/npu_top.h")
    message(FATAL_ERROR
        "CDC_ENABLE_SAURIA_NPU_V4=ON requires the private SAURIA model, but "
        "npu_top.h was not found at SAURIA_NPU_ROOT='${SAURIA_NPU_ROOT}'. "
        "Set -DSAURIA_NPU_ROOT=<path-containing-npu_top.h>.")
endif()

add_library(npu_tlm_v4_model STATIC
    src/npu_tlm_v4_model.cpp)
add_library(cdc::components::npu_tlm_v4_model ALIAS npu_tlm_v4_model)

target_include_directories(npu_tlm_v4_model
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${SAURIA_NPU_ROOT}")

target_link_libraries(npu_tlm_v4_model PUBLIC SystemC::systemc)

install(TARGETS npu_tlm_v4_model EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
