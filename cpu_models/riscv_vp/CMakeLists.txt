# cdc::cpu::riscv_vp — cpu_base wrapper around the Bremen riscv-vp rv32 ISS.
# Builds the minimal upstream subset: vendored softfloat + the needed core-common
# sources + rv32 (iss, syscall). Deliberately avoids gdb-mc / platforms / boost
# iostreams (we use our own elf_loader, not Bremen's).

set(BREMEN ${CMAKE_SOURCE_DIR}/third_party/riscv-vp/vp/src)

if(NOT EXISTS "${BREMEN}/core/rv32/iss.cpp")
    message(FATAL_ERROR
        "Bremen riscv-vp not found at ${BREMEN}. Clone agra-uni-bremen/riscv-vp "
        "into third_party/riscv-vp before building riscv_vp.")
endif()

# Vendored Berkeley SoftFloat (its own CMakeLists sets the right defines/includes).
add_subdirectory(${BREMEN}/vendor/softfloat ${CMAKE_CURRENT_BINARY_DIR}/softfloat)

add_library(riscv_vp STATIC
    src/riscv_vp_wrapper.cpp
    ${BREMEN}/core/rv32/iss.cpp
    ${BREMEN}/core/rv32/syscall.cpp
    ${BREMEN}/core/common/timer.cpp
    ${BREMEN}/core/common/real_clint.cpp
    ${BREMEN}/core/common/instr.cpp
    ${BREMEN}/core/common/debug_memory.cpp
    ${BREMEN}/core/common/rawmode.cpp)
add_library(cdc::cpu::riscv_vp ALIAS riscv_vp)

target_include_directories(riscv_vp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/cpu_models/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${BREMEN}                         # for "core/common/..." and "util/..." includes
        ${BREMEN}/core/rv32
        ${BREMEN}/core/common
        ${BREMEN}/vendor/softfloat/include
        src)

target_compile_definitions(riscv_vp PRIVATE SC_INCLUDE_DYNAMIC_PROCESSES)
target_compile_options(riscv_vp PRIVATE -w)   # silence third-party warnings

target_link_libraries(riscv_vp
    PUBLIC  SystemC::systemc
    PRIVATE softfloat pthread)
