add_executable(isp_run isp_run.cpp)
target_link_libraries(isp_run PRIVATE
    cdc::components::isp_tlm
    cdc::components::memory_tlm
    SystemC::systemc)
target_include_directories(isp_run PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
