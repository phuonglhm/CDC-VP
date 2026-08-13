if(CDC_BUILD_MINI_TLM)
   if(TARGET cdc::components::timer_tlm)
      add_subdirectory(mini_tlm)
   else()
      message(STATUS "Skipping mini_tlm: cdc::components::timer_tlm is not available")
   endif()

   if(TARGET cdc::components::i2c_tlm)
      add_subdirectory(mini_soc)
   else()
      message(STATUS "Skipping mini_soc: cdc::components::i2c_tlm is not available")
   endif()
endif()

if(CDC_BUILD_CPU_EVAL)
   if(TARGET cdc::components::timer_tlm)
      add_subdirectory(riscv_cpu_eval)
   else()
      message(STATUS "Skipping riscv_cpu_eval: cdc::components::timer_tlm is not available")
   endif()
endif()

if(CDC_BUILD_CUSTOM_SOC)
   if(TARGET cdc::components::i2c_tlm)
      add_subdirectory(riscv_custom_soc)
   else()
      message(STATUS "Skipping riscv_custom_soc: cdc::components::i2c_tlm is not available")
   endif()

   add_subdirectory(VP_FX1_Full_SoC)

   add_subdirectory(tests/adc_platform)

   if(TARGET cdc::components::clkmgr_tlm)
      add_subdirectory(tests/clkmgr_platform)
   else()
      message(STATUS "Skipping clkmgr_platform: cdc::components::clkmgr_tlm is not available")
   endif()

   add_subdirectory(tests/wdt_platform)
   add_subdirectory(tests/spi_platform)
   add_subdirectory(tests/i2c_platform)
   add_subdirectory(tests/dma_platform)
   add_subdirectory(tests/dmic_platform)
   add_subdirectory(tests/pwm_platform)

   if(TARGET cdc::components::pmu_tlm)
      add_subdirectory(tests/pmu_platform)
   else()
      message(STATUS "Skipping pmu_platform: cdc::components::pmu_tlm is not available")
   endif()

   if(TARGET cdc::components::timer_tlm)
      add_subdirectory(tests/timer_platform)
   else()
      message(STATUS "Skipping timer_platform: cdc::components::timer_tlm is not available")
   endif()

   if(TARGET cdc::components::trng_tlm)
      add_subdirectory(tests/trng_platform)
   else()
      message(STATUS "Skipping trng_platform: cdc::components::trng_tlm is not available")
   endif()

   if(TARGET cdc::components::uart2_tlm)
      add_subdirectory(tests/uart_platform)
   else()
      message(STATUS "Skipping uart_platform: cdc::components::uart2_tlm is not available")
   endif()
endif()

# The FlooNoC-based SoC is kept separate from the bus_router platforms: its
# interconnect is cycle accurate, so it runs far slower and is chosen
# deliberately rather than by default.
if(CDC_BUILD_NOC_SOC)
   if(TARGET cdc::components::noc_interconnect)
      add_subdirectory(noc_soc)
   else()
      message(STATUS "Skipping noc_soc: cdc::components::noc_interconnect is not available")
   endif()
endif()

# The RISC-V VP++ Compiler Enablement VP (plan §16, Phase 4.5). Off by default,
# and deliberately not conditioned on CDC_BUILD_TPU_V3_SOC: the handoff contains
# no NoC, no accelerator and no NEO composition, so it must build without them.
if(CDC_BUILD_RISCV_VPP_COMPILER_VP)
   if(TARGET cdc::cpu::riscv_vp_plusplus)
      add_subdirectory(riscv_vpp_compiler_vp)
   else()
      message(FATAL_ERROR
          "CDC_BUILD_RISCV_VPP_COMPILER_VP=ON but cdc::cpu::riscv_vp_plusplus "
          "is not available. This platform is the VP++ hart with RAM around "
          "it; without the hart there is nothing to package. Fetch the pinned "
          "checkout with cpu_models/riscv_vp_plusplus/fetch_riscv_vp_plusplus.sh.")
   endif()
endif()

# TPU_V3 SoC. Off by default; see components/TPU_V3/docs/.
if(CDC_BUILD_TPU_V3_SOC)
   if(TARGET cdc::components::tpu_v3_common)
      add_subdirectory(TPU_V3_SoC)
   else()
      message(STATUS
          "Skipping TPU_V3_SoC: cdc::components::tpu_v3_common is not available")
   endif()
endif()
