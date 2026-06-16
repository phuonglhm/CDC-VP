add_executable(i2c_platform
    src/main.cpp
    src/i2c_platform_top.cpp
    ${CMAKE_SOURCE_DIR}/components/i2c_tlm/i2c.cpp)

target_include_directories(i2c_platform PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/components/i2c_tlm)

target_compile_definitions(i2c_platform PRIVATE CDC_CPU_BACKEND_${CDC_CPU_BACKEND})

target_link_libraries(i2c_platform PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart_tlm
    cdc::components::clint_tlm
    cdc::components::plic_tlm
    cdc::cpu::${CDC_CPU_BACKEND})

add_custom_command(TARGET i2c_platform POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:i2c_platform>/configs)

cdc_make_portable(i2c_platform)
cdc_package_platform(i2c_platform)
