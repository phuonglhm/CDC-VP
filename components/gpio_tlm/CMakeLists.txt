add_library(gpio_tlm STATIC
    src/gpio_tlm.cpp)
add_library(cdc::components::gpio_tlm ALIAS gpio_tlm)

target_include_directories(gpio_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(gpio_tlm PUBLIC SystemC::systemc)

install(TARGETS gpio_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
