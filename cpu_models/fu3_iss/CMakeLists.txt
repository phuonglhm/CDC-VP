# fu3_iss: the FU3 fork of the Bremen riscv-vp RV32 ISS (third_party/riscv-vp,
# upstream 48b2f5877b2368cc466fb0da155db349e676c0b0, MIT, see LICENSE.riscv-vp).
#
# Only the files FU3 has to change are copied here (src/core/...); everything
# else (trap.h, fp.h, irq_if.h, util/, softfloat) still comes from the untouched
# third_party checkout. The copies live in namespace fu3_rv32, so a binary that
# also links the shared upstream `riscv_vp` target sees no duplicate symbols.
# See README.md for the list of FU3 changes.

set(FU3_ISS_UPSTREAM "${CMAKE_SOURCE_DIR}/third_party/riscv-vp/vp/src")
if(NOT EXISTS "${FU3_ISS_UPSTREAM}/core/common/trap.h")
    message(FATAL_ERROR "fu3_iss needs the riscv-vp checkout at ${FU3_ISS_UPSTREAM}")
endif()
if(NOT TARGET softfloat)
    message(FATAL_ERROR "fu3_iss needs the riscv-vp vendored softfloat target (cpu_models/riscv_vp)")
endif()

add_library(fu3_iss STATIC
    src/core/rv32/iss.cpp
    src/core/common/instr.cpp)
add_library(cdc::cpu::fu3_iss ALIAS fu3_iss)

# The fork's directory comes first, so "core/common/instr.h" and the rv32
# headers resolve to the FU3 copies; the rest falls through to upstream.
target_include_directories(fu3_iss
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/rv32
        ${FU3_ISS_UPSTREAM}
        ${FU3_ISS_UPSTREAM}/core/common)

target_compile_definitions(fu3_iss PRIVATE SC_INCLUDE_DYNAMIC_PROCESSES)
target_compile_options(fu3_iss PRIVATE -w)   # upstream code; FU3 edits are reviewed by diff

target_link_libraries(fu3_iss
    PUBLIC  SystemC::systemc softfloat
    PRIVATE pthread)

if(CDC_BUILD_TESTS)
    add_executable(test_fu3_iss_edges tests/test_fu3_iss_edges.cpp)
    target_link_libraries(test_fu3_iss_edges PRIVATE fu3_iss)
    add_test(NAME fu3_iss_edges COMMAND test_fu3_iss_edges)

    # Both ISS backends in one binary, linked normally (no --whole-archive).
    add_executable(test_fu3_iss_mixed_backends tests/test_mixed_backends.cpp)
    target_link_libraries(test_fu3_iss_mixed_backends PRIVATE cdc_cpu_cva6 riscv_vp)
    add_test(NAME fu3_iss_mixed_backends COMMAND test_fu3_iss_mixed_backends)

    set_tests_properties(fu3_iss_edges fu3_iss_mixed_backends PROPERTIES TIMEOUT 60 LABELS "fu3;fu3_cpu")
endif()
