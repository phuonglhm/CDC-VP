add_executable(test_bus_router test_bus_router.cpp)
target_link_libraries(test_bus_router PRIVATE
    cdc::components::bus_router
    cdc::components::memory_tlm
    SystemC::systemc)
target_include_directories(test_bus_router PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME bus_router COMMAND test_bus_router)
