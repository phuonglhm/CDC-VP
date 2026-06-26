#Author: trangnm20
add_executable(test_clkmgr_tlm
    test_clkmgr_tlm.cpp)
target_link_libraries(test_clkmgr_tlm PRIVATE
    cdc::components::clkmgr_tlm
    SystemC::systemc)
target_include_directories(test_clkmgr_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME clkmgr_tlm COMMAND test_clkmgr_tlm)

add_executable(clkmgr_sim
    clkmgr_sim.cpp)
target_link_libraries(clkmgr_sim PRIVATE
    cdc::components::clkmgr_tlm
    SystemC::systemc)
