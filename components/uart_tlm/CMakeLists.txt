add_library(uart_tlm STATIC
    src/uart_tlm.cpp)
add_library(cdc::components::uart_tlm ALIAS uart_tlm)

target_include_directories(uart_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(uart_tlm PUBLIC SystemC::systemc)

install(TARGETS uart_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
