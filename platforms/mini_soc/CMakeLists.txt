# Standalone mode: this folder can be copied elsewhere and built on its own,
# pulling the components from an installed cdc-components package — the cdc-vp
# equivalent of a Fast Models .sgproj referencing prebuilt IP libraries.
# (When built as part of the main cdc-vp tree this block is skipped.)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(mini_soc CXX C)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    find_package(cdc-components REQUIRED)
endif()

add_executable(mini_soc
    src/main.cpp
    src/mini_soc_top.cpp)

target_include_directories(mini_soc PRIVATE src)
target_link_libraries(mini_soc PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart_tlm
    cdc::components::i2c_tlm)

add_custom_command(TARGET mini_soc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:mini_soc>/configs)

# Self-contained, copy-anywhere artifact (bundles libsystemc.so, rpath=$ORIGIN).
cdc_make_portable(mini_soc)
cdc_package_platform(mini_soc)
