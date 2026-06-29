# Reusable TLM component libraries (cdc::components::*).
# Add each component as it is extracted from a platform.
# common first: other components depend on it.
add_subdirectory(common)
add_subdirectory(bus_router)
add_subdirectory(memory_tlm)
add_subdirectory(uart2_tlm)

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
add_subdirectory(flash_nor_tlm)
add_subdirectory(qspi_tlm)
add_subdirectory(rtc_tlm)

add_subdirectory(isp_tlm)

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
