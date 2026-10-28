# CVA6/Ibex wrapper (cdc::cpu::cva6_wrapper) on the FU3 ISS fork (cpu_models/fu3_iss).
add_library(cdc_cpu_cva6 STATIC
    src/cva6_wrapper.cpp
)

target_include_directories(cdc_cpu_cva6 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

    # fu3_bus/access.h (header-only) để gắn Access cho transaction của CPU
    ${CMAKE_SOURCE_DIR}/components/FU3_Components/bus/include
)

# fu3_iss brings the ISS headers and the riscv-vp vendored softfloat. The former
# separate Berkeley softfloat build (cva6_softfloat) defined the same C symbols
# as that vendored copy, so it is no longer built.
target_link_libraries(cdc_cpu_cva6 PUBLIC
    SystemC::systemc
    fu3_iss
)
