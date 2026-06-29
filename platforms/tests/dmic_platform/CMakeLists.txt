add_executable(dmic_platform
    src/main.cpp
    src/dmic_platform_top.cpp)

target_include_directories(dmic_platform PRIVATE src)
target_compile_definitions(dmic_platform PRIVATE CDC_CPU_BACKEND_${CDC_CPU_BACKEND})
target_link_libraries(dmic_platform PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart2_tlm
    cdc::components::clint_tlm
    cdc::components::plic_tlm
    cdc::components::dmic_tlm
    cdc::cpu::${CDC_CPU_BACKEND})

add_custom_command(TARGET dmic_platform POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:dmic_platform>/configs)

cdc_make_portable(dmic_platform)
cdc_package_platform(dmic_platform)
