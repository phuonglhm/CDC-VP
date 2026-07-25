function(add_floo_model_test test_name)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE
        cdc::components::floo_noc_model
        SystemC::systemc)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_floo_model_test(test_reference_model)
add_floo_model_test(test_ready_valid_fifo)
add_floo_model_test(test_xy_route_select)
add_floo_model_test(test_wormhole_arbiter)
add_floo_model_test(test_floo_router)
add_floo_model_test(test_floo_mesh)

add_executable(route_trace_sc route_trace_sc.cpp)
target_link_libraries(route_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)

set(_route_trace_actual
    "${CMAKE_CURRENT_BINARY_DIR}/route_select_systemc_trace.csv")
add_test(
    NAME test_route_trace_sc
    COMMAND route_trace_sc
        "${CMAKE_CURRENT_SOURCE_DIR}/data/route_select_stimulus.csv"
        "${_route_trace_actual}"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/route_select_expected.csv")
