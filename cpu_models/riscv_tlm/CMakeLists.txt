# cdc::cpu::riscv_tlm — cpu_base wrapper around the mariusmm RISC-V-TLM core.
# Builds the needed upstream core sources (NOT its Memory/BusCtrl/Simulator/Trace/
# Timer — we use our own components) together with the thin wrapper.

set(MARIUSMM_DIR ${CMAKE_SOURCE_DIR}/third_party/RISC-V-TLM)

if(NOT EXISTS "${MARIUSMM_DIR}/src/CPU.cpp")
    message(FATAL_ERROR
        "mariusmm RISC-V-TLM not found at ${MARIUSMM_DIR}. "
        "Clone it (git submodule update --init recommended) before building riscv_tlm.")
endif()

add_library(riscv_tlm STATIC
    src/riscv_tlm_wrapper.cpp
    ${MARIUSMM_DIR}/src/CPU.cpp
    ${MARIUSMM_DIR}/src/RV32.cpp
    ${MARIUSMM_DIR}/src/BASE_ISA.cpp
    ${MARIUSMM_DIR}/src/C_extension.cpp
    ${MARIUSMM_DIR}/src/M_extension.cpp
    ${MARIUSMM_DIR}/src/A_extension.cpp
    ${MARIUSMM_DIR}/src/extension_base.cpp
    ${MARIUSMM_DIR}/src/Instruction.cpp
    ${MARIUSMM_DIR}/src/Registers.cpp
    ${MARIUSMM_DIR}/src/Performance.cpp
    ${MARIUSMM_DIR}/src/MemoryInterface.cpp)
add_library(cdc::cpu::riscv_tlm ALIAS riscv_tlm)

target_include_directories(riscv_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/cpu_models/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${MARIUSMM_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/compat   # shim: lowercase "memory.h" -> Memory.h
        ${MARIUSMM_DIR}/spdlog/include
        src)

target_compile_definitions(riscv_tlm PRIVATE SC_INCLUDE_DYNAMIC_PROCESSES)

# Upstream is third-party; silence its warnings so they don't drown ours.
target_compile_options(riscv_tlm PRIVATE -w)

target_link_libraries(riscv_tlm PUBLIC SystemC::systemc)
