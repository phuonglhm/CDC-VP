add_library(cdc_cpu_cva6 STATIC
    src/cva6_wrapper.cpp
)

target_include_directories(cdc_cpu_cva6 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
 
    ${CMAKE_SOURCE_DIR}/third_party

    ${CMAKE_SOURCE_DIR}/third_party/riscv-vp-plusplus/vp/src
    ${CMAKE_SOURCE_DIR}/third_party/riscv-vp-plusplus/vp/src/core/common

    ${CMAKE_SOURCE_DIR}/third_party/riscv-vp/vp/src
    ${CMAKE_SOURCE_DIR}/third_party/riscv-vp/vp/src/core/common

    ${CMAKE_SOURCE_DIR}/third_party/softfloat/source/include
)

target_link_libraries(cdc_cpu_cva6 PUBLIC SystemC::systemc riscv_vp softfloat)