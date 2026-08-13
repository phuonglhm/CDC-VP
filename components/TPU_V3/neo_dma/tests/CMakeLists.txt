# SPDX-License-Identifier: Apache-2.0

add_executable(test_tpu_v3_neo_dma test_neo_dma.cpp)
target_link_libraries(test_tpu_v3_neo_dma PRIVATE
    cdc::components::tpu_v3_neo_dma
    cdc::components::tpu_v3_tpu_core)
add_test(NAME tpu_v3_neo_dma COMMAND test_tpu_v3_neo_dma)
set_tests_properties(tpu_v3_neo_dma PROPERTIES
    LABELS "tpu_v3;unit;dma"
    TIMEOUT 300)

# The reuse-boundary gate. It reads the DMA's own sources with comments
# stripped and the symbols the compiler emitted, so a dependency arriving
# through a transitive header cannot slip past a source scan and a copy-pasted
# register map cannot slip past a symbol scan.
add_test(
    NAME neo_dma_independence
    COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
            -DARCHIVE=$<TARGET_FILE:tpu_v3_neo_dma>
            -DNM=${CMAKE_NM}
            "-DLINK_LIBRARIES=$<TARGET_PROPERTY:tpu_v3_neo_dma,LINK_LIBRARIES>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_dma_independence.cmake")
set_tests_properties(neo_dma_independence PROPERTIES
    LABELS "tpu_v3;dma;provenance"
    TIMEOUT 120)
