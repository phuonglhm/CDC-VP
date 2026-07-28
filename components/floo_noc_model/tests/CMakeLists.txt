function(add_floo_model_test test_name)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE
        cdc::components::floo_noc_model
        SystemC::systemc)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_floo_model_test(test_reference_model)
add_floo_model_test(test_stream_fifo)
add_floo_model_test(test_xy_route_select)
add_floo_model_test(test_wormhole_arbiter)
add_floo_model_test(test_floo_router)
add_floo_model_test(test_floo_mesh)

add_executable(route_trace_sc route_trace_sc.cpp)
target_link_libraries(route_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)

add_executable(fifo_trace_sc fifo_trace_sc.cpp)
target_link_libraries(fifo_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)

add_executable(arbiter_trace_sc arbiter_trace_sc.cpp)
target_link_libraries(arbiter_trace_sc PRIVATE
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

# The expected FIFO traces are captured from the frozen common_cells RTL by
# rtl_crosscheck/run_stream_fifo_crosscheck.sh. They let the standalone
# regression detect a FIFO regression without Verilator; they never replace the
# RTL comparison itself.
foreach(_depth 2 4)
    add_test(
        NAME test_fifo_trace_sc_d${_depth}
        COMMAND fifo_trace_sc
            "${CMAKE_CURRENT_SOURCE_DIR}/data/stream_fifo_stimulus.csv"
            "${CMAKE_CURRENT_BINARY_DIR}/stream_fifo_d${_depth}_systemc_trace.csv"
            ${_depth}
            "${CMAKE_CURRENT_SOURCE_DIR}/data/stream_fifo_expected_d${_depth}.csv")
endforeach()

foreach(_routes 2 4 5)
    add_test(
        NAME test_arbiter_trace_sc_n${_routes}
        COMMAND arbiter_trace_sc
            "${CMAKE_CURRENT_SOURCE_DIR}/data/wormhole_arbiter_stimulus.csv"
            "${CMAKE_CURRENT_BINARY_DIR}/wormhole_arbiter_n${_routes}_systemc_trace.csv"
            ${_routes}
            "${CMAKE_CURRENT_SOURCE_DIR}/data/wormhole_arbiter_expected_n${_routes}.csv")
endforeach()
