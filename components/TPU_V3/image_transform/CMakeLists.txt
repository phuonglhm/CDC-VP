# SPDX-License-Identifier: Apache-2.0

if(NOT CDC_BUILD_TPU_V3_IMAGE_TRANSFORM)
    return()
endif()

# The SystemC feeder is not copied into this component: it is not a standalone
# transform and carries SA FIFO/skew/control state. These three files are the
# evidence boundary instead. Their hashes pin the address-generation source,
# the CHW tensor layout and the cross-correlation golden semantics used by the
# extracted Im2Col adapter.
set(TPU_V3_TRANSFORM_ROOT
    "${CMAKE_SOURCE_DIR}/components/npu_tlm/models/v4.2_model"
    CACHE PATH "Pinned NPU-team v4.2 source used as Phase 6 Im2Col evidence")

set(TPU_V3_TRANSFORM_IFMAP_HASH
    "156334177cfe1682faac049ce48745d9657ed4fcd5d58eb1c0bf5bffa3692ba0")
set(TPU_V3_TRANSFORM_LAYOUT_HASH
    "d5310c80b5e283a1ae133e9ba7b05d74c3559056f2a6ebd42f607391e328c982")
set(TPU_V3_TRANSFORM_GOLDEN_HASH
    "57327c4c91c7dcc1fa2d95509fee6f83e60eea96bf80be68afbd589a67aaae3b")

set(_transform_evidence
    "data_feeder/ifmap_feeder.h|${TPU_V3_TRANSFORM_IFMAP_HASH}"
    "driver/libsauria_mem.h|${TPU_V3_TRANSFORM_LAYOUT_HASH}"
    "driver/sauria_golden.h|${TPU_V3_TRANSFORM_GOLDEN_HASH}")
foreach(_entry IN LISTS _transform_evidence)
    string(REPLACE "|" ";" _pair "${_entry}")
    list(GET _pair 0 _relative)
    list(GET _pair 1 _expected)
    set(_path "${TPU_V3_TRANSFORM_ROOT}/${_relative}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR
            "TPU_V3 Phase 6 evidence is missing: ${_path}. Set "
            "-DTPU_V3_TRANSFORM_ROOT to the pinned v4.2_model tree.")
    endif()
    file(SHA256 "${_path}" _actual)
    if(NOT _actual STREQUAL _expected)
        message(FATAL_ERROR
            "TPU_V3 Phase 6 refuses unpinned source ${_path}: expected "
            "${_expected}, got ${_actual}. Audit and explicitly rebaseline "
            "the Im2Col semantics before changing the pin.")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_path}")
endforeach()

set(TPU_V3_TRANSFORM_SOURCE_REVISION
    "v4.2-ifmap@${TPU_V3_TRANSFORM_IFMAP_HASH}")
set(TPU_V3_TRANSFORM_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    cmake/transform_provenance.h.in
    "${TPU_V3_TRANSFORM_GENERATED_DIR}/tpu_v3/transform/transform_provenance.h"
    @ONLY)

add_library(tpu_v3_image_transform STATIC
    src/im2col.cpp
    src/image_transform.cpp)
add_library(cdc::components::tpu_v3_image_transform ALIAS
    tpu_v3_image_transform)

target_include_directories(tpu_v3_image_transform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${TPU_V3_TRANSFORM_GENERATED_DIR}>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tpu_v3_image_transform
    PUBLIC
        cdc::components::tpu_v3_native_port
        cdc::components::tpu_v3_common
        SystemC::systemc)
target_compile_features(tpu_v3_image_transform PUBLIC cxx_std_17)

install(TARGETS tpu_v3_image_transform EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(tests)
endif()
