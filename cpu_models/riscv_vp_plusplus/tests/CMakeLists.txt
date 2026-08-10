# SPDX-License-Identifier: Apache-2.0

add_executable(test_riscv_vp_plusplus test_riscv_vp_plusplus.cpp)
target_link_libraries(test_riscv_vp_plusplus PRIVATE
    cdc::cpu::riscv_vp_plusplus
    softfloat_vpp
    SystemC::systemc)
# The F5 check reads softfloat's global arithmetic state directly, so the test
# needs its headers. The backend keeps softfloat private on purpose: nothing
# outside the CPU model and the Phase 4 BF16 adapter should reach for it.
target_include_directories(test_riscv_vp_plusplus PRIVATE
    ${VPP}/vendor/softfloat/include)
add_test(NAME riscv_vp_plusplus_backend COMMAND test_riscv_vp_plusplus)
set_tests_properties(riscv_vp_plusplus_backend PROPERTIES
    LABELS "tpu_v3;cpu;unit"
    TIMEOUT 120)

# ── the RV32GCV smoke image, executed through the backend ────────────────────
#
# Builds the freestanding image when the cross toolchain is present, and skips
# the test otherwise rather than failing: the toolchain is a host prerequisite,
# not a defect in this repository. Same convention as the noc_soc firmware
# regression (SKIP_RETURN_CODE 77).

set(RVV_SMOKE_DIR "${CMAKE_SOURCE_DIR}/fw/TPU_V3_SoC/rvv_smoke")
set(RVV_SMOKE_TOOLDIR "/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin"
    CACHE PATH "Directory holding the riscv-none-elf cross toolchain (D4)")
set(RVV_SMOKE_ELF "${RVV_SMOKE_DIR}/rvv_smoke.elf")
set(RVV_TRAP_ELF "${RVV_SMOKE_DIR}/rvv_trap.elf")

add_executable(test_rvv_trap test_rvv_trap.cpp)
target_link_libraries(test_rvv_trap PRIVATE
    cdc::cpu::riscv_vp_plusplus
    SystemC::systemc)
target_include_directories(test_rvv_trap PRIVATE ${RVV_SMOKE_DIR})

add_executable(test_rvv_smoke test_rvv_smoke.cpp)
target_link_libraries(test_rvv_smoke PRIVATE
    cdc::cpu::riscv_vp_plusplus
    SystemC::systemc)
# The harness shares the exit protocol header with the firmware, so the two
# cannot disagree about the address or the field order.
target_include_directories(test_rvv_smoke PRIVATE ${RVV_SMOKE_DIR})

if(EXISTS "${RVV_SMOKE_TOOLDIR}/riscv-none-elf-gcc")
    # `verify` also checks the ISA attributes and the disassembly, so a build
    # that silently fell back to a scalar -march fails here rather than
    # producing a green test that executed no vector instruction.
    add_custom_command(
        OUTPUT "${RVV_SMOKE_ELF}" "${RVV_TRAP_ELF}"
        COMMAND ${CMAKE_COMMAND} -E env PATH=/usr/bin:/bin:$ENV{PATH}
                make -C "${RVV_SMOKE_DIR}" TOOLDIR=${RVV_SMOKE_TOOLDIR} all verify
        DEPENDS "${RVV_SMOKE_DIR}/main.c"
                "${RVV_SMOKE_DIR}/trap_main.c"
                "${RVV_SMOKE_DIR}/crt0.S"
                "${RVV_SMOKE_DIR}/link.ld"
                "${RVV_SMOKE_DIR}/sim_exit.h"
                "${RVV_SMOKE_DIR}/Makefile"
        COMMENT "Building the freestanding RV32GCV smoke image"
        VERBATIM)
    add_custom_target(rvv_smoke_image DEPENDS "${RVV_SMOKE_ELF}" "${RVV_TRAP_ELF}")
    add_dependencies(test_rvv_smoke rvv_smoke_image)
    add_dependencies(test_rvv_trap rvv_smoke_image)
else()
    message(STATUS
        "RV32GCV cross toolchain not found at ${RVV_SMOKE_TOOLDIR}; "
        "rvv_smoke_execution will skip.")
endif()

add_test(NAME rvv_smoke_execution
    COMMAND test_rvv_smoke "${RVV_SMOKE_ELF}")
set_tests_properties(rvv_smoke_execution PROPERTIES
    LABELS "tpu_v3;cpu;firmware"
    SKIP_RETURN_CODE 77
    TIMEOUT 300)

# D10 vector-trap gate: mid-vector fault, vstart resumption, mstatus.VS, a
# reserved vtype, and RV32 indexed access with index EEW=64.
add_test(NAME rvv_vector_trap
    COMMAND test_rvv_trap "${RVV_TRAP_ELF}")
set_tests_properties(rvv_vector_trap PROPERTIES
    LABELS "tpu_v3;cpu;firmware"
    SKIP_RETURN_CODE 77
    TIMEOUT 300)
