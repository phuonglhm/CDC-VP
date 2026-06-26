add_executable(riscv_custom_soc
    src/main.cpp
    src/riscv_custom_soc_top.cpp)

target_include_directories(riscv_custom_soc PRIVATE src)
target_compile_definitions(riscv_custom_soc PRIVATE CDC_CPU_BACKEND_${CDC_CPU_BACKEND})
target_link_libraries(riscv_custom_soc PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart_tlm
    cdc::components::clint_tlm
    cdc::components::plic_tlm
    cdc::components::i2c_tlm
    cdc::cpu::${CDC_CPU_BACKEND})

add_custom_command(TARGET riscv_custom_soc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:riscv_custom_soc>/configs)

cdc_make_portable(riscv_custom_soc)
cdc_package_platform(riscv_custom_soc)
