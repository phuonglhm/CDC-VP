# SPDX-License-Identifier: Apache-2.0

add_executable(test_im2col_contract test_im2col_contract.cpp)
target_link_libraries(test_im2col_contract PRIVATE
    cdc::components::tpu_v3_image_transform)
target_include_directories(test_im2col_contract SYSTEM PRIVATE
    "${TPU_V3_TRANSFORM_ROOT}"
    "${TPU_V3_TRANSFORM_ROOT}/driver")
add_test(NAME tpu_v3_transform_im2col_contract COMMAND test_im2col_contract)
set_tests_properties(tpu_v3_transform_im2col_contract PROPERTIES
    LABELS "tpu_v3;transform;golden"
    TIMEOUT 120)

add_executable(test_image_transform test_image_transform.cpp)
target_link_libraries(test_image_transform PRIVATE
    cdc::components::tpu_v3_image_transform
    cdc::components::tpu_v3_tpu_core)
add_test(NAME tpu_v3_image_transform COMMAND test_image_transform)
set_tests_properties(tpu_v3_image_transform PROPERTIES
    LABELS "tpu_v3;transform;unit;boundary"
    TIMEOUT 300)

add_test(NAME tpu_v3_transform_independence
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
        -DARCHIVE=$<TARGET_FILE:tpu_v3_image_transform>
        -DNM=${CMAKE_NM}
        "-DLINK_LIBRARIES=$<TARGET_PROPERTY:tpu_v3_image_transform,LINK_LIBRARIES>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_transform_independence.cmake")
set_tests_properties(tpu_v3_transform_independence PROPERTIES
    LABELS "tpu_v3;transform;provenance"
    TIMEOUT 120)
