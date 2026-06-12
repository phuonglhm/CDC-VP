add_executable(mini_tlm
    src/main.cpp
    src/mini_tlm_top.cpp)

target_include_directories(mini_tlm PRIVATE src)
target_link_libraries(mini_tlm PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart_tlm
    cdc::components::timer_tlm)

add_custom_command(TARGET mini_tlm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:mini_tlm>/configs)

# Self-contained, copy-anywhere artifact (bundles libsystemc.so, rpath=$ORIGIN).
cdc_make_portable(mini_tlm)
cdc_package_platform(mini_tlm)
