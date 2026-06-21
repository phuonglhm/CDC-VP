#Author: QuanNH107

add_executable(test_dma_tlm
    test_dma_tlm.cpp)
target_link_libraries(test_dma_tlm PRIVATE
    cdc::components::dma_tlm
    SystemC::systemc)
add_test(NAME dma_tlm COMMAND test_dma_tlm)
