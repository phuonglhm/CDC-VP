# Reusable TLM component libraries (cdc::components::*).
# Add each component as it is extracted from a platform.
# common first: other components depend on it.
add_subdirectory(common)
add_subdirectory(bus_router)
add_subdirectory(memory_tlm)
add_subdirectory(uart2_tlm)
add_subdirectory(uart_host_tlm)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/i2c_tlm/CMakeLists.txt")
   add_subdirectory(i2c_tlm)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/timer_tlm/CMakeLists.txt")
   add_subdirectory(timer_tlm)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/otp_tlm/CMakeLists.txt")
   add_subdirectory(otp_tlm)
endif()

add_subdirectory(adc_tlm)
add_subdirectory(gpio_tlm)
add_subdirectory(clkmgr_tlm)
add_subdirectory(spi_tlm)
add_subdirectory(clint_tlm)
add_subdirectory(plic_tlm)
add_subdirectory(pwm_tlm)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pmu_tlm/CMakeLists.txt")
   add_subdirectory(pmu_tlm)
endif()

add_subdirectory(wdt_tlm)
add_subdirectory(dma_tlm)
add_subdirectory(trng_tlm)
add_subdirectory(dmic_tlm)
if(CDC_ENABLE_SAURIA_NPU_V4)
   add_subdirectory(npu_tlm)
endif()
add_subdirectory(vpu_tlm)
add_subdirectory(flash_nor_tlm)
add_subdirectory(qspi_tlm)
add_subdirectory(rtc_tlm)

# The FlooNoC model plus its TLM wrapper. The model itself is header-only; the
# wrapper is what a platform links to use the NoC in place of `bus_router`.
# Its own standalone tests are off here: the top-level build has its own test
# switch, and the model's cross-check suite is run from the component directory.
set(FLOO_NOC_MODEL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(floo_noc_model)

# TPU_V3 reusable components. The SoC switch brings in the common Phase 1-4
# tree, while either extracted accelerator may also be built and gated on its
# own. All three switches default OFF, so existing platforms remain unaffected.
if(CDC_BUILD_TPU_V3_SOC OR CDC_BUILD_TPU_V3_SAURIA_MATRIX
   OR CDC_BUILD_TPU_V3_IMAGE_TRANSFORM)
   add_subdirectory(TPU_V3)
endif()

 add_subdirectory(isp_tlm)  # TEMP: WIP, missing headers — re-enable when it builds

# FX1 components (bus, memory map, CLINT, sparse RAM, sim-control, ...). Only
# configured for the FX1 SoC so the default build is unchanged.
if(CDC_BUILD_FX1_SOC)
   add_subdirectory(FX1_Components)
endif()

# Every FU3 model, for platforms/fu3_soc. A function, so that BUILD_TESTING
# follows CDC_BUILD_TESTS for the FU3 models only (several of them gate their
# tests on BUILD_TESTING, which other components set in the cache). bus comes
# first: the peripherals only pull in their own copy of it when fu3::bus_system
# does not exist. IO_PAD is not listed: GPIO adds it (fu3::io_pad) and a second
# add_subdirectory would define its targets twice.
function(cdc_add_fu3_components)
   set(BUILD_TESTING ${CDC_BUILD_TESTS})
   add_subdirectory(FU3_Components/bus)
   add_subdirectory(FU3_Components/GPIO)
   add_subdirectory(FU3_Components/QSPI)
   add_subdirectory(FU3_Components/AES-256)
   add_subdirectory(FU3_Components/cpu)
   add_subdirectory(FU3_Components/ADC)
   add_subdirectory(FU3_Components/DMA0/AP_DMA)
   add_subdirectory(FU3_Components/crm)
   add_subdirectory(FU3_Components/dbg)
   add_subdirectory(FU3_Components/hyperram)
   add_subdirectory(FU3_Components/i2c)
   add_subdirectory(FU3_Components/mailbox)
   add_subdirectory(FU3_Components/plic)
   add_subdirectory(FU3_Components/rng)
   add_subdirectory(FU3_Components/spi)
   add_subdirectory(FU3_Components/syscsr)
   add_subdirectory(FU3_Components/timer)
   add_subdirectory(FU3_Components/uart)
   add_subdirectory(FU3_Components/wdt)
   # Every FU3 model test carries the "fu3" label, so that `ctest -L fu3` runs
   # the component tests together with the platform's (review T21: several
   # component directories set no label and dropped out of the FU3 regression).
   get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY SUBDIRECTORIES)
   foreach(dir IN LISTS dirs)
      if(dir MATCHES "/FU3_Components/")
         cdc_label_fu3_tests(${dir})
      endif()
   endforeach()
endfunction()

function(cdc_label_fu3_tests dir)
   get_property(tests DIRECTORY ${dir} PROPERTY TESTS)
   if(tests)
      set_property(TEST ${tests} DIRECTORY ${dir} APPEND PROPERTY LABELS fu3)
   endif()
   get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
   foreach(sub IN LISTS subdirs)
      cdc_label_fu3_tests(${sub})
   endforeach()
endfunction()

if(CDC_BUILD_FU3_SOC)
   cdc_add_fu3_components()
endif()

# ── Install/export so a standalone platform can find_package(cdc-components) ──
install(EXPORT cdc-components-targets
    NAMESPACE cdc::components::
    DESTINATION lib/cmake/cdc-components
    FILE cdc-components-targets.cmake)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/cdc-components-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cdc-components-config.cmake"
    INSTALL_DESTINATION lib/cmake/cdc-components)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cdc-components-config.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/modules/CdcPortable.cmake"
    DESTINATION lib/cmake/cdc-components)
