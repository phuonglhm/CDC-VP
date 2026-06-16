add_executable(spi_platform
    src/main.cpp
    src/spi_platform_top.cpp)

target_include_directories(spi_platform PRIVATE src)
target_compile_definitions(spi_platform PRIVATE CDC_CPU_BACKEND_${CDC_CPU_BACKEND})

target_link_libraries(spi_platform PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart_tlm
    cdc::components::clint_tlm
    cdc::components::plic_tlm
    cdc::components::spi_tlm
    cdc::cpu::${CDC_CPU_BACKEND})

add_custom_command(TARGET spi_platform POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:spi_platform>/configs)

cdc_make_portable(spi_platform)
cdc_package_platform(spi_platform)
