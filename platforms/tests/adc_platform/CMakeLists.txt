add_executable(adc_platform
    src/main.cpp
    src/adc_platform_top.cpp)

target_include_directories(adc_platform PRIVATE src)
target_compile_definitions(adc_platform PRIVATE CDC_CPU_BACKEND_${CDC_CPU_BACKEND})
target_link_libraries(adc_platform PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart_tlm
    cdc::components::clint_tlm
    cdc::components::plic_tlm
    cdc::components::adc_tlm
    cdc::cpu::${CDC_CPU_BACKEND})

add_custom_command(TARGET adc_platform POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:adc_platform>/configs)

cdc_make_portable(adc_platform)
cdc_package_platform(adc_platform)
