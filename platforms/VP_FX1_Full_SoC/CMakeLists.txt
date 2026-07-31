add_executable(vp_fx1_full_soc
    src/main.cpp
    src/vp_fx1_full_soc_top.cpp)

target_include_directories(vp_fx1_full_soc PRIVATE src)
target_compile_definitions(vp_fx1_full_soc PRIVATE
    CDC_CPU_BACKEND_${CDC_CPU_BACKEND}
    CDC_ENABLE_SAURIA_NPU_V4=$<BOOL:${CDC_ENABLE_SAURIA_NPU_V4}>)
target_link_libraries(vp_fx1_full_soc PRIVATE
    SystemC::systemc
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::clint_tlm
    cdc::components::plic_tlm
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
    cdc::components::pmu_tlm
    cdc::components::dmic_tlm
    cdc::components::otp_tlm
    cdc::components::qspi_tlm
    cdc::components::flash_nor_tlm
    cdc::components::rtc_tlm
    cdc::components::adc_tlm
    cdc::components::gpio_tlm
    cdc::cpu::${CDC_CPU_BACKEND})

if(CDC_ENABLE_SAURIA_NPU_V4)
    target_link_libraries(vp_fx1_full_soc PRIVATE
        cdc::components::npu_tlm)
endif()

add_custom_command(TARGET vp_fx1_full_soc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:vp_fx1_full_soc>/configs)

cdc_make_portable(vp_fx1_full_soc)
cdc_package_platform(vp_fx1_full_soc)
