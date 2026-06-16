# CPU model wrappers (cdc::cpu::*). Each wraps an upstream ISS behind cpu_base.
# The active backend is selected by CDC_CPU_BACKEND (riscv_tlm | riscv_vp | ...).

# Header-only cpu_base + elf_loader interface, usable without a concrete backend.
add_library(cdc_cpu_base INTERFACE)
add_library(cdc::cpu::base ALIAS cdc_cpu_base)
target_include_directories(cdc_cpu_base
    INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(cdc_cpu_base INTERFACE SystemC::systemc)

if(CDC_CPU_BACKEND STREQUAL "riscv_tlm")
    add_subdirectory(riscv_tlm)
elseif(CDC_CPU_BACKEND STREQUAL "riscv_vp")
    add_subdirectory(riscv_vp)
else()
    message(FATAL_ERROR "Unknown CDC_CPU_BACKEND='${CDC_CPU_BACKEND}'")
endif()
