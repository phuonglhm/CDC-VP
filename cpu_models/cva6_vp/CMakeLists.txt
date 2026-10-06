file(GLOB SOFTFLOAT_SOURCES 
    "${CMAKE_SOURCE_DIR}/third_party/softfloat/source/*.c"
    "${CMAKE_SOURCE_DIR}/third_party/softfloat/source/RISCV/*.c"
)

list(FILTER SOFTFLOAT_SOURCES EXCLUDE REGEX ".*M\\.c$")

add_library(cva6_softfloat STATIC ${SOFTFLOAT_SOURCES})

target_include_directories(cva6_softfloat PUBLIC
    "${CMAKE_SOURCE_DIR}/third_party/softfloat/build/Linux-x86_64-GCC"
    "${CMAKE_SOURCE_DIR}/third_party/softfloat/source/RISCV"
    "${CMAKE_SOURCE_DIR}/third_party/softfloat/source/include"
    "${CMAKE_SOURCE_DIR}/third_party" 
)

target_compile_definitions(cva6_softfloat PUBLIC
    SOFTFLOAT_ROUND_ODD
    INLINE_LEVEL=5
    SOFTFLOAT_FAST_DIV32TO16
    SOFTFLOAT_FAST_DIV64TO32
    SOFTFLOAT_FAST_INT64
)

add_library(cdc_cpu_cva6 STATIC
    src/cva6_wrapper.cpp
)

target_include_directories(cdc_cpu_cva6 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
 
    ${CMAKE_SOURCE_DIR}/third_party

    ${CMAKE_SOURCE_DIR}/third_party/riscv-vp/vp/src
    ${CMAKE_SOURCE_DIR}/third_party/riscv-vp/vp/src/core/common

    ${CMAKE_SOURCE_DIR}/third_party/softfloat/source/include
)

target_link_libraries(cdc_cpu_cva6 PUBLIC 
    SystemC::systemc 
    riscv_vp 
    cva6_softfloat
)