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
set(RVV_FP_ELF "${RVV_SMOKE_DIR}/rvv_fp.elf")
# The Spike differential corpus. Its own startup and linker script, because it
# is the only image that has to run on a second simulator.
set(RVV_SIG_ELF "${RVV_SMOKE_DIR}/rvv_sig.elf")
# D12/D13 conformance gate.
set(RVV_CONF_ELF "${RVV_SMOKE_DIR}/rvv_conf.elf")
# Interrupt gate.
set(RVV_IRQ_ELF "${RVV_SMOKE_DIR}/rvv_irq.elf")

add_executable(test_fp_concurrency test_fp_concurrency.cpp)
target_link_libraries(test_fp_concurrency PRIVATE
    cdc::cpu::riscv_vp_plusplus
    SystemC::systemc)
target_include_directories(test_fp_concurrency PRIVATE ${RVV_SMOKE_DIR})

add_executable(test_irq test_irq.cpp)
target_link_libraries(test_irq PRIVATE
    cdc::cpu::riscv_vp_plusplus
    SystemC::systemc)
target_include_directories(test_irq PRIVATE ${RVV_SMOKE_DIR})

add_executable(test_conformance test_conformance.cpp)
target_link_libraries(test_conformance PRIVATE
    cdc::cpu::riscv_vp_plusplus
    SystemC::systemc)
target_include_directories(test_conformance PRIVATE ${RVV_SMOKE_DIR})

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
        OUTPUT "${RVV_SMOKE_ELF}" "${RVV_TRAP_ELF}" "${RVV_FP_ELF}"
               "${RVV_SIG_ELF}" "${RVV_CONF_ELF}" "${RVV_IRQ_ELF}"
        COMMAND ${CMAKE_COMMAND} -E env PATH=/usr/bin:/bin:$ENV{PATH}
                make -C "${RVV_SMOKE_DIR}" TOOLDIR=${RVV_SMOKE_TOOLDIR} all verify
        DEPENDS "${RVV_SMOKE_DIR}/main.c"
                "${RVV_SMOKE_DIR}/trap_main.c"
                "${RVV_SMOKE_DIR}/fp_main.c"
                "${RVV_SMOKE_DIR}/sig_main.c"
                "${RVV_SMOKE_DIR}/conf_main.c"
                "${RVV_SMOKE_DIR}/irq_main.c"
                "${RVV_SMOKE_DIR}/crt0.S"
                "${RVV_SMOKE_DIR}/crt0_sig.S"
                "${RVV_SMOKE_DIR}/link.ld"
                "${RVV_SMOKE_DIR}/link_dram.ld"
                "${RVV_SMOKE_DIR}/sim_exit.h"
                "${RVV_SMOKE_DIR}/sig_layout.h"
                "${RVV_SMOKE_DIR}/Makefile"
        COMMENT "Building the freestanding RV32GCV smoke image"
        VERBATIM)
    add_custom_target(rvv_smoke_image
        DEPENDS "${RVV_SMOKE_ELF}" "${RVV_TRAP_ELF}" "${RVV_FP_ELF}"
                "${RVV_SIG_ELF}" "${RVV_CONF_ELF}" "${RVV_IRQ_ELF}")
    add_dependencies(test_rvv_smoke rvv_smoke_image)
    add_dependencies(test_rvv_trap rvv_smoke_image)
    add_dependencies(test_fp_concurrency rvv_smoke_image)
    add_dependencies(test_conformance rvv_smoke_image)
    add_dependencies(test_irq rvv_smoke_image)
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

# Interrupt gate (plan §11.2). `set_irq()` was implemented in Phase 2 and never
# executed until this test existed.
add_test(NAME interrupt_delivery
    COMMAND test_irq "${RVV_IRQ_ELF}")
set_tests_properties(interrupt_delivery PROPERTIES
    LABELS "tpu_v3;cpu;interrupts"
    SKIP_RETURN_CODE 77
    TIMEOUT 300)

# D12/D13 conformance gate. Separate from the differential test on purpose: it
# checks the things an oracle comparison cannot see — that an illegal
# instruction issues no bus request and dirties no vector state, and that each
# access origin maps to its own access-fault cause.
add_test(NAME conformance_patches
    COMMAND test_conformance "${RVV_CONF_ELF}")
set_tests_properties(conformance_patches PROPERTIES
    LABELS "tpu_v3;cpu;conformance"
    SKIP_RETURN_CODE 77
    TIMEOUT 300)

# The other half of D13's response policy: a malformed transaction must abort
# the model instead of becoming a guest fault. Its own test because the expected
# outcome is an exception, not a completed run.
add_test(NAME conformance_protocol_error
    COMMAND test_conformance "${RVV_CONF_ELF}" protocol-error)
set_tests_properties(conformance_protocol_error PROPERTIES
    LABELS "tpu_v3;cpu;conformance"
    SKIP_RETURN_CODE 77
    TIMEOUT 300)

# F5 concurrency control (audit F5): SoftFloat keeps its rounding mode and
# exception flags in process-global variables shared by every ISS instance.
# Both creation orders are run, because creation order decides which hart
# SystemC schedules first and therefore whose state is left behind.
foreach(_fp_order normal swapped)
    add_test(NAME fp_concurrency_${_fp_order}
        COMMAND test_fp_concurrency "${RVV_FP_ELF}" ${_fp_order})
    set_tests_properties(fp_concurrency_${_fp_order} PROPERTIES
        LABELS "tpu_v3;cpu;concurrency"
        SKIP_RETURN_CODE 77
        TIMEOUT 600)
endforeach()

# ── the Spike differential oracle ────────────────────────────────────────────
#
# Added from here rather than from the parent so it inherits `RVV_SMOKE_DIR`,
# `RVV_SIG_ELF` and the `rvv_smoke_image` target, all of which are defined
# above. Spike itself is never linked into anything (decision record D3); the
# subdirectory only registers a test that runs it as a child process.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../oracle" oracle)

# ── the portable-executable gate ─────────────────────────────────────────────
#
# Added from here for the same reason as the oracle: it needs `RVV_SMOKE_ELF`
# and the `rvv_smoke_image` target, both defined above.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../portable" portable)

# Decision record D19: the architectural hart reset. Runs a small hand-checked
# RV32V image to dirty the state, resets, and audits every class the contract
# names -- including the `mcycle` measurement D19 asked for rather than assumed.
add_executable(test_architectural_reset test_architectural_reset.cpp)
target_link_libraries(test_architectural_reset PRIVATE
    cdc::cpu::riscv_vp_plusplus
    SystemC::systemc)
add_test(NAME architectural_reset COMMAND test_architectural_reset)
set_tests_properties(architectural_reset PROPERTIES
    LABELS "tpu_v3;cpu;unit"
    TIMEOUT 300)
