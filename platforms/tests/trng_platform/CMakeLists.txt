#author: linhtk55-fpt

add_executable(trng_platform
    src/main.cpp
    src/trng_platform_top.cpp)

target_include_directories(trng_platform PRIVATE src)
target_compile_definitions(trng_platform PRIVATE CDC_CPU_BACKEND_${CDC_CPU_BACKEND})
target_link_libraries(trng_platform PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart_tlm
    cdc::components::clint_tlm
    cdc::components::plic_tlm
    cdc::components::trng_tlm
    cdc::cpu::${CDC_CPU_BACKEND})

add_custom_command(TARGET trng_platform POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:trng_platform>/configs)

cdc_make_portable(trng_platform)
cdc_package_platform(trng_platform)
