# SPDX-License-Identifier: Apache-2.0

add_executable(test_sauria_profile test_sauria_profile.cpp)
target_link_libraries(test_sauria_profile PRIVATE
    cdc::components::tpu_v3_sauria_matrix)
target_compile_features(test_sauria_profile PRIVATE cxx_std_17)

add_test(NAME tpu_v3_sauria_profile COMMAND test_sauria_profile)
set_tests_properties(tpu_v3_sauria_profile PROPERTIES
    LABELS "tpu_v3;sauria;unit"
    TIMEOUT 60)

# The two-instance hygiene gate (decision record D17). It compiles the *patched*
# Sauria array at the pinned geometry, elaborates two of them, and requires that
# no `trace_sysc/` directory appears — the observable consequence of the
# function-local `static std::ofstream` objects being compiled out.
add_test(NAME tpu_v3_sauria_two_instance_hygiene
    COMMAND ${CMAKE_COMMAND}
            -DBINARY=$<TARGET_FILE:test_sauria_two_instances>
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/hygiene_run
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_hygiene_gate.cmake")
set_tests_properties(tpu_v3_sauria_two_instance_hygiene PROPERTIES
    LABELS "tpu_v3;sauria;boundary"
    TIMEOUT 300)

add_test(NAME tpu_v3_sauria_static_state_hygiene
    COMMAND ${CMAKE_COMMAND}
            -DBINARY=$<TARGET_FILE:test_gemm_adapter>
            -DNM=${CMAKE_NM}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/static_state_gate.cmake")
set_tests_properties(tpu_v3_sauria_static_state_hygiene PROPERTIES
    LABELS "tpu_v3;sauria;boundary"
    TIMEOUT 60)

add_executable(test_sauria_two_instances test_sauria_two_instances.cpp)
target_link_libraries(test_sauria_two_instances PRIVATE
    cdc::components::tpu_v3_sauria_matrix
    SystemC::systemc)

add_executable(test_tile_staging_store test_tile_staging_store.cpp)
target_link_libraries(test_tile_staging_store PRIVATE
    cdc::components::tpu_v3_sauria_matrix
    SystemC::systemc)

add_test(NAME tpu_v3_sauria_tile_staging COMMAND test_tile_staging_store)
set_tests_properties(tpu_v3_sauria_tile_staging PROPERTIES
    LABELS "tpu_v3;sauria;unit"
    TIMEOUT 120)

add_executable(test_gemm_config test_gemm_config.cpp)
target_link_libraries(test_gemm_config PRIVATE
    cdc::components::tpu_v3_sauria_matrix)

add_test(NAME tpu_v3_sauria_gemm_config COMMAND test_gemm_config)
set_tests_properties(tpu_v3_sauria_gemm_config PROPERTIES
    LABELS "tpu_v3;sauria;unit"
    TIMEOUT 60)

# The composition elaborates. Reaching `sc_start` proves all 429 generated port
# bindings resolved, which is also what keeps the audit's closure finding
# honest: a kept module still needing an excluded one would leave a signal with
# no driver and no binding.
add_executable(test_matrix_composition test_matrix_composition.cpp)
target_link_libraries(test_matrix_composition PRIVATE
    cdc::components::tpu_v3_sauria_matrix
    SystemC::systemc)

add_test(NAME tpu_v3_sauria_composition
    COMMAND ${CMAKE_COMMAND}
            -DBINARY=$<TARGET_FILE:test_matrix_composition>
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/composition_run
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_hygiene_gate.cmake")
set_tests_properties(tpu_v3_sauria_composition PROPERTIES
    LABELS "tpu_v3;sauria;boundary"
    TIMEOUT 600)

# Every internal signal of the composition has a driver. Source-level, because
# SystemC diagnoses an unbound *port* and says nothing about an unwritten signal —
# see the script for the eight signals this caught, four of them the array's
# operand inputs.
add_test(NAME tpu_v3_sauria_driver_coverage
    COMMAND ${CMAKE_COMMAND}
            -DHEADER=${CMAKE_CURRENT_SOURCE_DIR}/../include/tpu_v3/sauria/matrix_composition.h
            -P "${CMAKE_CURRENT_SOURCE_DIR}/driver_coverage_gate.cmake")
set_tests_properties(tpu_v3_sauria_driver_coverage PROPERTIES
    LABELS "tpu_v3;sauria;boundary"
    TIMEOUT 60)

# The engine computes a real GEMM, against the pinned golden case. Everything
# between "operands staged" and "results staged" is under test here; the fabric
# path is `test_gemm_adapter`.
add_executable(test_gemm_staged test_gemm_staged.cpp)
target_link_libraries(test_gemm_staged PRIVATE
    cdc::components::tpu_v3_sauria_matrix
    SystemC::systemc)
target_compile_definitions(test_gemm_staged PRIVATE
    TPU_V3_SAURIA_CASE_DIR="${TPU_V3_SAURIA_WORK}/npu_demo_clean/cases/demo_gemm_64x64")

add_test(NAME tpu_v3_sauria_gemm_staged
    COMMAND ${CMAKE_COMMAND}
            -DBINARY=$<TARGET_FILE:test_gemm_staged>
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/gemm_staged_run
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_hygiene_gate.cmake")
set_tests_properties(tpu_v3_sauria_gemm_staged PROPERTIES
    LABELS "tpu_v3;sauria;golden"
    TIMEOUT 1800)

# The differential: the same job, the same oracle, over `neo_local_sram_if`.
add_executable(test_gemm_adapter test_gemm_adapter.cpp)
target_link_libraries(test_gemm_adapter PRIVATE
    cdc::components::tpu_v3_sauria_matrix
    cdc::components::tpu_v3_tpu_core
    SystemC::systemc)
target_compile_definitions(test_gemm_adapter PRIVATE
    TPU_V3_SAURIA_CASE_DIR="${TPU_V3_SAURIA_WORK}/npu_demo_clean/cases/demo_gemm_64x64")

add_test(NAME tpu_v3_sauria_gemm_adapter
    COMMAND ${CMAKE_COMMAND}
            -DBINARY=$<TARGET_FILE:test_gemm_adapter>
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/gemm_adapter_run
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_hygiene_gate.cmake")
set_tests_properties(tpu_v3_sauria_gemm_adapter PROPERTIES
    LABELS "tpu_v3;sauria;golden"
    TIMEOUT 1800)

add_executable(test_config_loader test_config_loader.cpp)
target_link_libraries(test_config_loader PRIVATE
    cdc::components::tpu_v3_sauria_matrix)

add_test(NAME tpu_v3_sauria_config_loader COMMAND test_config_loader)
set_tests_properties(tpu_v3_sauria_config_loader PROPERTIES
    LABELS "tpu_v3;sauria;unit"
    TIMEOUT 60)

add_executable(test_sa_control test_sa_control.cpp)
target_link_libraries(test_sa_control PRIVATE
    cdc::components::tpu_v3_sauria_matrix
    SystemC::systemc)

add_test(NAME tpu_v3_sauria_control COMMAND test_sa_control)
set_tests_properties(tpu_v3_sauria_control PROPERTIES
    LABELS "tpu_v3;sauria;unit"
    TIMEOUT 60)

add_executable(test_adapter_epoch test_adapter_epoch.cpp)
target_link_libraries(test_adapter_epoch PRIVATE
    cdc::components::tpu_v3_sauria_matrix
    SystemC::systemc)

add_test(NAME tpu_v3_sauria_adapter_epoch COMMAND test_adapter_epoch)
set_tests_properties(tpu_v3_sauria_adapter_epoch PROPERTIES
    LABELS "tpu_v3;sauria;boundary"
    TIMEOUT 60)

add_test(NAME tpu_v3_sauria_dependency_closure
    COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
            -DBINARY=$<TARGET_FILE:test_gemm_adapter>
            -DNM=${CMAKE_NM}
            "-DLINK_LIBRARIES=$<TARGET_PROPERTY:tpu_v3_sauria_matrix,LINK_LIBRARIES>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/dependency_closure_gate.cmake")
set_tests_properties(tpu_v3_sauria_dependency_closure PROPERTIES
    LABELS "tpu_v3;sauria;provenance"
    TIMEOUT 60)

add_executable(test_two_adapters test_two_adapters.cpp)
target_link_libraries(test_two_adapters PRIVATE
    cdc::components::tpu_v3_sauria_matrix
    SystemC::systemc)

add_test(NAME tpu_v3_sauria_two_adapters COMMAND test_two_adapters)
set_tests_properties(tpu_v3_sauria_two_adapters PROPERTIES
    LABELS "tpu_v3;sauria;boundary"
    TIMEOUT 60)
