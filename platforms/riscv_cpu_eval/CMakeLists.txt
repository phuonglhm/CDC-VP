add_executable(riscv_cpu_eval
    src/main.cpp
    src/riscv_cpu_eval_top.cpp)

target_include_directories(riscv_cpu_eval PRIVATE src)
target_compile_definitions(riscv_cpu_eval PRIVATE CDC_CPU_BACKEND_${CDC_CPU_BACKEND})
target_link_libraries(riscv_cpu_eval PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart_tlm
    cdc::components::timer_tlm
    cdc::cpu::${CDC_CPU_BACKEND})   # swap CPU backend via CMake flag

add_custom_command(TARGET riscv_cpu_eval POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:riscv_cpu_eval>/configs)

# Self-contained, copy-anywhere artifact (bundles libsystemc.so, rpath=$ORIGIN).
cdc_make_portable(riscv_cpu_eval)
cdc_package_platform(riscv_cpu_eval)
