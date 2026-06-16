add_executable(timer_platform
    src/main.cpp
    src/timer_platform_top.cpp)

target_include_directories(timer_platform PRIVATE src)
target_compile_definitions(timer_platform PRIVATE CDC_CPU_BACKEND_${CDC_CPU_BACKEND})
target_link_libraries(timer_platform PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart_tlm
    cdc::components::clint_tlm
    cdc::components::plic_tlm
    cdc::components::timer_tlm
    cdc::cpu::${CDC_CPU_BACKEND})

add_custom_command(TARGET timer_platform POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:timer_platform>/configs)

cdc_make_portable(timer_platform)
cdc_package_platform(timer_platform)
