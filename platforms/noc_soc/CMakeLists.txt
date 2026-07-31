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
endif()
