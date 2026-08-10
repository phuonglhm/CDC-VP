# CPU model wrappers (cdc::cpu::*). Each wraps an upstream ISS behind cpu_base.
# The active backend is selected by CDC_CPU_BACKEND (riscv_vp).

# Header-only cpu_base + elf_loader interface, usable without a concrete backend.
add_library(cdc_cpu_base INTERFACE)
add_library(cdc::cpu::base ALIAS cdc_cpu_base)
target_include_directories(cdc_cpu_base
    INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(cdc_cpu_base INTERFACE SystemC::systemc)

# The CDC_CPU_BACKEND selection, built only when a platform actually binds it.
#
# The guard matters because `riscv_vp` hard-fails if third_party/riscv-vp is
# absent. TPU_V3 uses its own backend, so a TPU_V3-only build must not be made
# to require a checkout it never links.
if(CDC_BUILD_CPU_EVAL OR CDC_BUILD_CUSTOM_SOC OR CDC_BUILD_NOC_SOC
   OR CDC_BUILD_MINI_TLM)
    if(CDC_CPU_BACKEND STREQUAL "riscv_vp")
        add_subdirectory(riscv_vp)
    else()
        message(FATAL_ERROR
            "Unsupported CDC_CPU_BACKEND='${CDC_CPU_BACKEND}'. Only 'riscv_vp' is supported.")
    endif()
endif()

# The TPU_V3 runtime backend (decision record D3): the RISC-V VP++ RV32 ISS,
# which provides scalar and RVV 1.0 execution in one hart.
#
# Built alongside the CDC_CPU_BACKEND selection rather than through it, on
# purpose. CDC_CPU_BACKEND is what existing platforms bind to, and TPU_V3 needs
# a *vector* hart while they need the one they already validated against; making
# this a CDC_CPU_BACKEND value would offer every platform a swap that has not
# been qualified for it.
if(CDC_BUILD_TPU_V3_SOC)
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/riscv-vp-plusplus/vp/src/core/rv32/iss.cpp")
        add_subdirectory(riscv_vp_plusplus)
    else()
        message(STATUS
            "Skipping riscv_vp_plusplus: third_party/riscv-vp-plusplus is not "
            "present. Fetch it at the pin recorded in "
            "components/TPU_V3/docs/TPU_V3_PHASE2_AUDIT.md §1.")
    endif()
endif()
