# Applied to every component-owned test target, however it was declared.
#
# A helper rather than a per-target line: two targets were declared by hand and
# silently missed the flags, and the round-2 sign-off claimed otherwise because
# it checked that no warning was *printed* rather than that the flags were
# *passed*. A compiler that is not asked will not complain.
function(floo_model_test_options target_name)
    target_compile_options(${target_name} PRIVATE ${FLOO_NOC_MODEL_WARNINGS})
endfunction()

function(add_floo_model_test test_name)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE
        cdc::components::floo_noc_model
        SystemC::systemc)
    floo_model_test_options(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_floo_model_test(test_reference_model)
add_floo_model_test(test_stream_fifo)
add_floo_model_test(test_xy_route_select)
add_floo_model_test(test_wormhole_arbiter)
add_floo_model_test(test_floo_router)
add_floo_model_test(test_floo_mesh)
add_floo_model_test(test_noc_counters)
add_floo_model_test(test_noc_metrics)
add_floo_model_test(test_axi_types)
add_floo_model_test(test_axi_chimney_pack)
add_floo_model_test(test_rr_arb_tree)
add_floo_model_test(test_meta_buffer)
add_floo_model_test(test_rob_order_gate)
add_floo_model_test(test_axi_endpoint)
add_floo_model_test(test_axi_noc)
add_floo_model_test(test_axi_noc_chimney)
add_floo_model_test(test_axi_chimney_manager_response)

# The TLM wrapper is a compiled target, so its test links that instead of the
# header-only model alone.
add_executable(test_noc_interconnect test_noc_interconnect.cpp)
target_link_libraries(test_noc_interconnect PRIVATE
    cdc::components::noc_interconnect
    SystemC::systemc)
floo_model_test_options(test_noc_interconnect)
add_test(NAME test_noc_interconnect COMMAND test_noc_interconnect)

add_executable(test_noc_interconnect_stress
    test_noc_interconnect_stress.cpp)
target_link_libraries(test_noc_interconnect_stress PRIVATE
    cdc::components::noc_interconnect
    SystemC::systemc)
floo_model_test_options(test_noc_interconnect_stress)
add_test(NAME test_noc_interconnect_stress
         COMMAND test_noc_interconnect_stress)

add_executable(test_noc_interconnect_concurrency
    test_noc_interconnect_concurrency.cpp)
target_link_libraries(test_noc_interconnect_concurrency PRIVATE
    cdc::components::noc_interconnect
    SystemC::systemc)
floo_model_test_options(test_noc_interconnect_concurrency)
add_test(NAME test_noc_interconnect_concurrency
         COMMAND test_noc_interconnect_concurrency)

add_executable(test_noc_interconnect_observer
    test_noc_interconnect_observer.cpp)
target_link_libraries(test_noc_interconnect_observer PRIVATE
    cdc::components::noc_interconnect
    SystemC::systemc)
floo_model_test_options(test_noc_interconnect_observer)
add_test(NAME test_noc_interconnect_observer
         COMMAND test_noc_interconnect_observer)

add_executable(test_noc_interconnect_fast
    test_noc_interconnect_fast.cpp)
target_link_libraries(test_noc_interconnect_fast PRIVATE
    cdc::components::noc_interconnect
    SystemC::systemc)
floo_model_test_options(test_noc_interconnect_fast)
add_test(NAME test_noc_interconnect_fast
         COMMAND test_noc_interconnect_fast)

# No kernel runs in this one; it only needs `sc_main` to satisfy the linker.
add_executable(test_axi_lanes test_axi_lanes.cpp)
target_link_libraries(test_axi_lanes PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(test_axi_lanes)
add_test(NAME test_axi_lanes COMMAND test_axi_lanes)

# Two sources on purpose: this is the only test that can catch a missing
# `inline` in the header's free functions.
add_executable(test_axi_lanes_odr
    test_axi_lanes_odr.cpp
    test_axi_lanes_odr_second.cpp)
target_link_libraries(test_axi_lanes_odr PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(test_axi_lanes_odr)
add_test(NAME test_axi_lanes_odr COMMAND test_axi_lanes_odr)

# Configuration errors, rejected before any simulation. Its own executable
# because it constructs several deliberately-invalid interconnects; keeping
# them out of the main test avoids polluting that one's SystemC hierarchy.
add_executable(test_noc_interconnect_bad_config
    test_noc_interconnect_bad_config.cpp)
target_link_libraries(test_noc_interconnect_bad_config PRIVATE
    cdc::components::noc_interconnect
    SystemC::systemc)
target_compile_options(test_noc_interconnect_bad_config PRIVATE
    ${FLOO_NOC_MODEL_WARNINGS})
add_test(NAME test_noc_interconnect_bad_config
         COMMAND test_noc_interconnect_bad_config)

add_executable(route_trace_sc route_trace_sc.cpp)
target_link_libraries(route_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(route_trace_sc)

add_executable(fifo_trace_sc fifo_trace_sc.cpp)
target_link_libraries(fifo_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(fifo_trace_sc)

add_executable(arbiter_trace_sc arbiter_trace_sc.cpp)
target_link_libraries(arbiter_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(arbiter_trace_sc)

add_executable(axi_sizing_trace axi_sizing_trace.cpp)
target_link_libraries(axi_sizing_trace PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(axi_sizing_trace)

add_executable(chimney_req_trace_sc chimney_req_trace_sc.cpp)
target_link_libraries(chimney_req_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(chimney_req_trace_sc)

add_executable(chimney_rsp_trace_sc chimney_rsp_trace_sc.cpp)
target_link_libraries(chimney_rsp_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(chimney_rsp_trace_sc)

add_executable(mesh_trace_sc mesh_trace_sc.cpp)
target_link_libraries(mesh_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(mesh_trace_sc)

add_executable(chimney_rsp_timing_trace_sc chimney_rsp_timing_trace_sc.cpp)
target_link_libraries(chimney_rsp_timing_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(chimney_rsp_timing_trace_sc)

add_executable(chimney_mgr_rsp_trace_sc chimney_mgr_rsp_trace_sc.cpp)
target_link_libraries(chimney_mgr_rsp_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(chimney_mgr_rsp_trace_sc)

add_executable(chimney_timing_trace_sc chimney_timing_trace_sc.cpp)
target_link_libraries(chimney_timing_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(chimney_timing_trace_sc)

add_executable(rob_trace_sc rob_trace_sc.cpp)
target_link_libraries(rob_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(rob_trace_sc)

add_executable(router_trace_sc router_trace_sc.cpp)
target_link_libraries(router_trace_sc PRIVATE
    cdc::components::floo_noc_model
    SystemC::systemc)
floo_model_test_options(router_trace_sc)

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

add_test(
    NAME test_axi_sizing_trace
    COMMAND axi_sizing_trace
        "${CMAKE_CURRENT_SOURCE_DIR}/data/axi_sizing_configs.csv"
        "${CMAKE_CURRENT_BINARY_DIR}/axi_sizing_model.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/axi_sizing_expected.csv")

add_test(
    NAME test_chimney_req_trace_sc
    COMMAND chimney_req_trace_sc
        "${CMAKE_CURRENT_SOURCE_DIR}/data/chimney_req_stimulus.csv"
        "${CMAKE_CURRENT_BINARY_DIR}/chimney_req_model.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/chimney_req_expected.csv")

add_test(
    NAME test_chimney_rsp_trace_sc
    COMMAND chimney_rsp_trace_sc
        "${CMAKE_CURRENT_SOURCE_DIR}/data/chimney_rsp_stimulus.csv"
        "${CMAKE_CURRENT_BINARY_DIR}/chimney_rsp_model.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/chimney_rsp_expected.csv")

add_test(
    NAME test_mesh_trace_sc
    COMMAND mesh_trace_sc
        "${CMAKE_CURRENT_SOURCE_DIR}/data/mesh_stimulus.csv"
        "${CMAKE_CURRENT_BINARY_DIR}/mesh_model.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/mesh_expected.csv")

add_test(
    NAME test_chimney_rsp_timing_trace_sc
    COMMAND chimney_rsp_timing_trace_sc
        "${CMAKE_CURRENT_SOURCE_DIR}/data/chimney_rsp_timing_stimulus.csv"
        "${CMAKE_CURRENT_BINARY_DIR}/chimney_rsp_timing_model.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/chimney_rsp_timing_expected.csv")

add_test(
    NAME test_chimney_mgr_rsp_trace_sc
    COMMAND chimney_mgr_rsp_trace_sc
        "${CMAKE_CURRENT_SOURCE_DIR}/data/chimney_mgr_rsp_stimulus.csv"
        "${CMAKE_CURRENT_BINARY_DIR}/chimney_mgr_rsp_model.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/chimney_mgr_rsp_expected.csv")

add_test(
    NAME test_chimney_timing_trace_sc
    COMMAND chimney_timing_trace_sc
        "${CMAKE_CURRENT_SOURCE_DIR}/data/chimney_timing_stimulus.csv"
        "${CMAKE_CURRENT_BINARY_DIR}/chimney_timing_model.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/chimney_timing_expected.csv")

add_test(
    NAME test_rob_trace_sc
    COMMAND rob_trace_sc
        "${CMAKE_CURRENT_SOURCE_DIR}/data/rob_stimulus.csv"
        "${CMAKE_CURRENT_BINARY_DIR}/rob_model.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/rob_expected.csv")

# `OutFifoDepth = 2` is what every generated FlooNoC router has; `0` is the
# `gen_no_out_fifo` bypass. Both RTL branches are signed.
foreach(_out_depth 2 0)
    add_test(
        NAME test_router_trace_sc_d${_out_depth}
        COMMAND router_trace_sc
            "${CMAKE_CURRENT_SOURCE_DIR}/data/router_stimulus.csv"
            "${CMAKE_CURRENT_BINARY_DIR}/router_d${_out_depth}_systemc_trace.csv"
            ${_out_depth}
            "${CMAKE_CURRENT_SOURCE_DIR}/data/router_expected_d${_out_depth}.csv")
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
