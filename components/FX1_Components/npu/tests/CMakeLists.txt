add_executable(test_fx1_npu test_fx1_npu.cpp)
target_compile_options(test_fx1_npu PRIVATE -Wall -Wextra)
target_compile_definitions(test_fx1_npu PRIVATE FX1_NPU_TEST_MODEL_HASH=0x${_fx1_npu_hash}u)
target_link_libraries(test_fx1_npu PRIVATE
    fx1_npu
    cdc::components::bus_system
    cdc::components::fx1_map
    cdc::components::fx1_exclusive_monitor
    SystemC::systemc)

# Unit and fabric cases: an independent C++ oracle (MAX_POOL, GEMM bank), no model data.
foreach(_case regs dbg gemm maxpool window unsupported refuse_model queue abort abort_running native_abort reset reset_inflight dma_error memory
              fabric_maxpool fabric_gemm fabric_timeout_abort fabric_s0_timeout_contrast)
    add_test(NAME fx1_npu_${_case} COMMAND test_fx1_npu ${_case})
    set_tests_properties(fx1_npu_${_case} PROPERTIES TIMEOUT 300 LABELS "fx1;fx1_unit;npu")
endforeach()

# Gates: programs from the drop's generators, references from its independent
# numpy oracle (tools/has/ref_program.py), replayed through the clone at a low
# and a high window, directly and over the FX1 fabric. Needs python3 + numpy
# (as the other fx1_regression tests, fw/fx1_soc/CMakeLists.txt FX1_PYTHON3).
find_program(FX1_PYTHON3 python3)
set(_fx1_npu_numpy 1)
if(FX1_PYTHON3)
    execute_process(COMMAND ${FX1_PYTHON3} -c "import numpy" RESULT_VARIABLE _fx1_npu_numpy OUTPUT_QUIET ERROR_QUIET)
endif()
if(NOT _fx1_npu_numpy EQUAL 0)
    message(STATUS "FX1 NPU: python3 with numpy not found, gate tests not registered")
else()
    # The drop's register-trace tools (plain C++, read in place) for the neo_lite
    # register-file programs of the neo gate.
    foreach(_tool host_trace_gen host_trace_replay)
        add_executable(fx1_npu_${_tool} ${FX1_NPU_MODEL_ROOT}/tools/has/${_tool}.cpp)
        target_include_directories(fx1_npu_${_tool} SYSTEM PRIVATE ${FX1_NPU_MODEL_ROOT} ${FX1_NPU_MODEL_ROOT}/driver)
        target_compile_features(fx1_npu_${_tool} PRIVATE cxx_std_17)
    endforeach()
    foreach(_gate ref_mp pad neo)
        set(_dir ${CMAKE_CURRENT_BINARY_DIR}/gates/${_gate})
        set(_trace_args)
        if(_gate STREQUAL "neo")
            set(_trace_args $<TARGET_FILE:fx1_npu_host_trace_gen> $<TARGET_FILE:fx1_npu_host_trace_replay> 80900000 8E000000)
        endif()
        add_test(NAME fx1_npu_gate_${_gate}_make
                 COMMAND ${FX1_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/make_gate.py ${FX1_NPU_MODEL_ROOT} ${_gate} ${_dir} ${_trace_args})
        set_tests_properties(fx1_npu_gate_${_gate}_make PROPERTIES TIMEOUT 300 LABELS "fx1;fx1_regression;npu"
                             FIXTURES_SETUP fx1_npu_gate_${_gate})
        add_test(NAME fx1_npu_gate_${_gate}_low_direct
                 COMMAND test_fx1_npu gate ${_dir} ${_dir}/oracle_golden.bin 0x80900000 direct)
        add_test(NAME fx1_npu_gate_${_gate}_high_fabric
                 COMMAND test_fx1_npu gate ${_dir} ${_dir}/oracle_golden.bin 0x8E000000 fabric)
        set_tests_properties(fx1_npu_gate_${_gate}_low_direct fx1_npu_gate_${_gate}_high_fabric PROPERTIES
                             TIMEOUT 600 LABELS "fx1;fx1_regression;npu" FIXTURES_REQUIRED fx1_npu_gate_${_gate})
    endforeach()
    # Plan N1 DFC dispatch: the neo gate through the 82-word register file and the doorbell.
    set(_dir ${CMAKE_CURRENT_BINARY_DIR}/gates/neo)
    add_test(NAME fx1_npu_gate_neo_window_low_direct
             COMMAND test_fx1_npu gate_neo ${_dir}/neo_window_80900000.txt ${_dir} ${_dir}/oracle_golden.bin 0x80900000 direct)
    add_test(NAME fx1_npu_gate_neo_window_high_fabric
             COMMAND test_fx1_npu gate_neo ${_dir}/neo_window_8E000000.txt ${_dir} ${_dir}/oracle_golden.bin 0x8E000000 fabric)
    # Review N1 R2 on the neo path: reset during the first doorbell's last copy-back.
    add_test(NAME fx1_npu_gate_neo_window_reset_low_direct
             COMMAND test_fx1_npu gate_neo_reset ${_dir}/neo_window_80900000.txt ${_dir} ${_dir}/oracle_golden.bin 0x80900000 direct)
    set_tests_properties(fx1_npu_gate_neo_window_low_direct fx1_npu_gate_neo_window_high_fabric
                         fx1_npu_gate_neo_window_reset_low_direct PROPERTIES
                         TIMEOUT 600 LABELS "fx1;fx1_regression;npu" FIXTURES_REQUIRED fx1_npu_gate_neo)
endif()
