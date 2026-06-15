add_library(adc_tlm STATIC
    src/adc_tlm.cpp
    src/adc_model.cpp)
add_library(cdc::components::adc_tlm ALIAS adc_tlm)

target_include_directories(adc_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(adc_tlm PUBLIC SystemC::systemc)

install(TARGETS adc_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
