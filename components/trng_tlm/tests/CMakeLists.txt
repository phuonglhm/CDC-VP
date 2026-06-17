add_executable(test_trng_tlm test_trng_tlm.cpp)
target_link_libraries(test_trng_tlm PRIVATE
    cdc::components::trng_tlm
    SystemC::systemc)
target_include_directories(test_trng_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME trng_tlm COMMAND test_trng_tlm)
