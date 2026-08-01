add_executable(noc_soc
    src/main.cpp
    src/noc_soc_top.cpp)

target_include_directories(noc_soc PRIVATE src)
target_link_libraries(noc_soc PRIVATE
    SystemC::systemc
    cdc::components::noc_interconnect
    cdc::components::memory_tlm
    cdc::components::clint_tlm
    cdc::components::plic_tlm
    cdc::cpu::${CDC_CPU_BACKEND}
    cdc::components::uart2_tlm
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
    add_test(
        NAME noc_soc_firmware_regression
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_firmware_regression.sh)
    set_tests_properties(noc_soc_firmware_regression PROPERTIES
        ENVIRONMENT "NOC_SOC_BIN=$<TARGET_FILE:noc_soc>"
        SKIP_RETURN_CODE 77
        TIMEOUT 900)

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
