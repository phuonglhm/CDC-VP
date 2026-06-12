# Reusable TLM component libraries (cdc::components::*).
# Add each component as it is extracted from a platform.
# common first: other components depend on it.
add_subdirectory(common)
add_subdirectory(bus_router)
add_subdirectory(memory_tlm)
add_subdirectory(uart_tlm)
add_subdirectory(timer_tlm)
add_subdirectory(i2c_tlm)
add_subdirectory(clint_tlm)
add_subdirectory(plic_tlm)

# ── Install/export so a standalone platform can find_package(cdc-components) ──
# Mirrors how a Fast Models .sgproj references prebuilt IP libs by path.
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