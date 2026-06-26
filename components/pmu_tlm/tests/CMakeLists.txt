add_executable(test_pmu_tlm
    test_pmu_tlm.cpp)
target_link_libraries(test_pmu_tlm PRIVATE
    cdc::components::pmu_tlm
    SystemC::systemc)
add_test(NAME pmu_tlm COMMAND test_pmu_tlm)
