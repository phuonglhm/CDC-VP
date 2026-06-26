add_executable(test_i2c_tlm
    test_i2c_tlm.cpp)
target_link_libraries(test_i2c_tlm PRIVATE
    cdc::components::i2c_tlm
    SystemC::systemc)
add_test(NAME i2c_tlm COMMAND test_i2c_tlm)
