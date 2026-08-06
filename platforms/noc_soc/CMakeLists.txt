add_executable(noc_soc
    src/main.cpp
    src/noc_soc_top.cpp)

target_include_directories(noc_soc PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/fw/common/include)
# The Step 12.7 baseline records what it was built with: the same latency in a
# Debug build means something different from a Release one.
target_compile_definitions(noc_soc PRIVATE
    NOC_SOC_BUILD_TYPE="$<IF:$<BOOL:${CMAKE_BUILD_TYPE}>,${CMAKE_BUILD_TYPE},unspecified>")
target_link_libraries(noc_soc PRIVATE
    SystemC::systemc
    cdc::components::noc_interconnect
    cdc::components::memory_tlm
    cdc::components::clint_tlm
    cdc::components::plic_tlm
    cdc::cpu::${CDC_CPU_BACKEND}
    cdc::components::uart2_tlm
    cdc::components::uart_host_tlm
    cdc::components::i2c_tlm
    cdc::components::spi_tlm
    cdc::components::timer_tlm
    cdc::components::wdt_tlm
    cdc::components::pwm_tlm
    cdc::components::dma_tlm
    cdc::components::trng_tlm
    cdc::components::clkmgr_tlm
    cdc::components::dmic_tlm
    cdc::components::otp_tlm
    cdc::components::qspi_tlm
    cdc::components::flash_nor_tlm
    cdc::components::rtc_tlm
    cdc::components::adc_tlm
    cdc::components::gpio_tlm)

add_custom_command(TARGET noc_soc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:noc_soc>/configs)

cdc_make_portable(noc_soc)
cdc_package_platform(noc_soc)

# Drainable synthetic measurement harness.  It is intentionally separate from
# noc_soc: a running CPU cannot be stopped at an arbitrary fixed-window
# boundary, while D3/D5 require injection stop, final drain and conservation.
add_executable(noc_benchmark src/noc_benchmark.cpp)
target_compile_definitions(noc_benchmark PRIVATE
    NOC_SOC_BUILD_TYPE="$<IF:$<BOOL:${CMAKE_BUILD_TYPE}>,${CMAKE_BUILD_TYPE},unspecified>")
target_link_libraries(noc_benchmark PRIVATE
    SystemC::systemc
    cdc::components::noc_interconnect)
cdc_make_portable(noc_benchmark)

# A binary bundle contains more than the component development package:
# `riscv_vp` is linked statically and the SystemC runtime is copied beside the
# executable. Ship the applicable texts and provenance with the bundle.
set(NOC_SOC_PACKAGE_DIR "${CDC_PACKAGE_ROOT}/noc_soc")
add_custom_command(TARGET noc_soc_package POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${NOC_SOC_PACKAGE_DIR}/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/LICENSE"
            "${NOC_SOC_PACKAGE_DIR}/licenses/Apache-2.0.txt"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/NOTICE"
            "${NOC_SOC_PACKAGE_DIR}/licenses/CDC-VP-NOTICE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/THIRD_PARTY.md"
            "${NOC_SOC_PACKAGE_DIR}/licenses/THIRD_PARTY.md"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/third_party/riscv-vp/LICENSE"
            "${NOC_SOC_PACKAGE_DIR}/licenses/RISC-V-VP-MIT.txt"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/components/floo_noc_model/LICENSES/SHL-0.51.txt"
            "${NOC_SOC_PACKAGE_DIR}/licenses/SHL-0.51.txt"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/components/floo_noc_model/PROVENANCE.md"
            "${NOC_SOC_PACKAGE_DIR}/licenses/FlooNoC-PROVENANCE.md"
    VERBATIM)

# Platform-level regression. The component tests in `components/floo_noc_model`
# cannot cover this level and demonstrably did not: a synthetic write into the
# firmware's `.text` once passed all of them. Skips (77) rather than fails when
# the RISC-V cross-compiler is absent.
if(CDC_BUILD_TESTS)
    find_program(NOC_SOC_PYTHON3 NAMES python3 REQUIRED)

    add_test(
        NAME noc_soc_metrics_dashboard
        COMMAND
            ${NOC_SOC_PYTHON3}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_noc_dashboard.py
            ${CMAKE_SOURCE_DIR}/tools/noc_dashboard.py)
    set_tests_properties(noc_soc_metrics_dashboard PROPERTIES
        LABELS "metrics;dashboard")

    add_test(
        NAME noc_soc_cli_dashboard_protocol
        COMMAND
            ${NOC_SOC_PYTHON3}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_noc_cli.py
            ${CMAKE_SOURCE_DIR}/tools/noc_cli.py
            ${CMAKE_SOURCE_DIR}/fw/common/include/soc/noc_dashboard_protocol.h
            ${CMAKE_SOURCE_DIR}/tools/noc_dashboard.py
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_noc_dashboard.py)
    set_tests_properties(noc_soc_cli_dashboard_protocol PROPERTIES
        LABELS "metrics;dashboard;cli")

    add_test(
        NAME noc_soc_sweep_selection
        COMMAND
            ${NOC_SOC_PYTHON3}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_noc_sweep.py
            ${CMAKE_SOURCE_DIR}/tools/noc_sweep.py)
    set_tests_properties(noc_soc_sweep_selection PROPERTIES
        LABELS "metrics;dse")

    add_test(
        NAME noc_soc_metrics_regression
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_metrics_regression.sh)
    set_tests_properties(noc_soc_metrics_regression PROPERTIES
        ENVIRONMENT
            "NOC_BENCHMARK_BIN=$<TARGET_FILE:noc_benchmark>;NOC_DASHBOARD_TOOL=${CMAKE_SOURCE_DIR}/tools/noc_dashboard.py"
        LABELS "metrics;benchmark"
        TIMEOUT 120)

    add_test(
        NAME noc_soc_metrics_negative_controls
        COMMAND
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_metrics_negative_controls.sh)
    set_tests_properties(noc_soc_metrics_negative_controls PROPERTIES
        ENVIRONMENT
            "NOC_INTERCONNECT_LIBRARY=$<TARGET_FILE:noc_interconnect>;FLOO_SYSTEMC_HOME=${SYSTEMC_HOME};NOC_DASHBOARD_TOOL=${CMAKE_SOURCE_DIR}/tools/noc_dashboard.py;NOC_SWEEP_TOOL=${CMAKE_SOURCE_DIR}/tools/noc_sweep.py"
        LABELS "metrics;negative-controls"
        TIMEOUT 300)

    add_test(
        NAME noc_soc_firmware_regression
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_firmware_regression.sh)
    set_tests_properties(noc_soc_firmware_regression PROPERTIES
        ENVIRONMENT "NOC_SOC_BIN=$<TARGET_FILE:noc_soc>"
        SKIP_RETURN_CODE 77
        TIMEOUT 900)

    # Step 12.5 FreeRTOS gate. It drives the staged acceptance contract in
    # `fw/freertos_noc_soc/run_step_regression.sh` for levels 121, 122, 123,
    # 124 and 128, so the claim that each earlier stage stays independently
    # reproducible is checked rather than asserted. Level 128 adds the
    # interrupt-driven UART CLI. It runs the platform in explicit firmware and
    # fast-timing modes; the detailed-mode comparison is a one-off measurement,
    # not part of the regression window.
    add_test(
        NAME noc_soc_freertos_regression
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_freertos_regression.sh)
    set_tests_properties(noc_soc_freertos_regression PROPERTIES
        ENVIRONMENT "NOC_SOC_BIN=$<TARGET_FILE:noc_soc>"
        LABELS "firmware;freertos"
        SKIP_RETURN_CODE 77
        TIMEOUT 900)

    # Step 12.6 qualification of the gate above. Acceptance markers alone are
    # not evidence, so each major path is shown to fail when its defect is put
    # back. It mutates private copies under /tmp and never the working tree;
    # one control rebuilds the platform, which is why it is serialised and
    # given a longer budget than the positive regression.
    add_test(
        NAME noc_soc_freertos_negative_controls
        COMMAND
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_freertos_negative_controls.sh)
    set_tests_properties(noc_soc_freertos_negative_controls PROPERTIES
        ENVIRONMENT "NOC_SOC_BIN=$<TARGET_FILE:noc_soc>;FLOO_SYSTEMC_HOME=${SYSTEMC_HOME}"
        LABELS "negative-controls;extended;freertos"
        RUN_SERIAL TRUE
        SKIP_RETURN_CODE 77
        TIMEOUT 1800)

    # Step 12.7 measurement baseline. Deliberately NOT a threshold test: it
    # checks that every required number was produced and is well formed, and
    # archives the artifact with its provenance. A threshold invented from a
    # single sample is a future false failure. It runs the cycle-stepped
    # backend, which is the only one whose latencies may be quoted, and that
    # costs minutes rather than seconds — hence the extended label.
    add_test(
        NAME noc_soc_measurement_baseline
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_freertos_baseline.sh)
    set_tests_properties(noc_soc_measurement_baseline PROPERTIES
        ENVIRONMENT
            "NOC_SOC_BIN=$<TARGET_FILE:noc_soc>;BASELINE_DIR=${CMAKE_CURRENT_BINARY_DIR}/evidence"
        LABELS "baseline;extended;freertos"
        RUN_SERIAL TRUE
        SKIP_RETURN_CODE 77
        TIMEOUT 2400)

    # Step 10.4 distribution gate. It configures a child Release build with a
    # private install prefix and package root, so it cannot consume artifacts
    # from this build or collide with another CTest job.
    add_test(
        NAME noc_soc_packaging_regression
        COMMAND
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_packaging_regression.sh)
    set_tests_properties(noc_soc_packaging_regression PROPERTIES
        ENVIRONMENT "PACKAGING_SYSTEMC_HOME=${SYSTEMC_HOME}"
        LABELS "packaging;distribution"
        RUN_SERIAL TRUE
        SKIP_RETURN_CODE 77
        TIMEOUT 1800)
endif()
