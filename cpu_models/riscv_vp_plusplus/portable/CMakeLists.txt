# SPDX-License-Identifier: Apache-2.0
#
# The portable-executable half of the Phase 2 gate (plan §11.2). Unlike the unit
# tests, this target is built with `cdc_make_portable`, so it carries `$ORIGIN`
# rather than the host's absolute SystemC path and gets the SystemC runtime
# copied beside it.

add_executable(rvv_runner rvv_runner.cpp)
target_link_libraries(rvv_runner PRIVATE
    cdc::cpu::riscv_vp_plusplus
    SystemC::systemc)

cdc_make_portable(rvv_runner)

add_test(NAME riscv_vp_plusplus_portable
    COMMAND ${CMAKE_COMMAND}
            -DRUNNER=$<TARGET_FILE:rvv_runner>
            -DIMAGE=${RVV_SMOKE_ELF}
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/portable_check
            -DREADELF=${CMAKE_READELF}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_portable_check.cmake")
set_tests_properties(riscv_vp_plusplus_portable PROPERTIES
    LABELS "tpu_v3;cpu;packaging"
    # The script reports a missing cross-built image by failing with this
    # marker; the toolchain is a host prerequisite, not a defect here.
    SKIP_REGULAR_EXPRESSION "__CDC_SKIP__"
    TIMEOUT 300)

if(TARGET rvv_smoke_image)
    add_dependencies(rvv_runner rvv_smoke_image)
endif()
