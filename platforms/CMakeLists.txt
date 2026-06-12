if(CDC_BUILD_MINI_TLM)
    add_subdirectory(mini_tlm)
    add_subdirectory(mini_soc)
endif()

if(CDC_BUILD_CPU_EVAL)
    add_subdirectory(riscv_cpu_eval)
endif()

if(CDC_BUILD_CUSTOM_SOC)
    add_subdirectory(riscv_custom_soc)
endif()
