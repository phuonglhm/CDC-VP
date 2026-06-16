add_library(uart2_tlm STATIC
    src/uart.cpp)
add_library(cdc::components::uart2_tlm ALIAS uart2_tlm)

target_include_directories(uart2_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/includes>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(uart2_tlm PUBLIC SystemC::systemc)

install(TARGETS uart2_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY includes/ DESTINATION include)