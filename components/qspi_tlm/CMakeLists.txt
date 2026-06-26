add_library(qspi_tlm STATIC
    src/qspi_tlm.cpp)
add_library(cdc::components::qspi_tlm ALIAS qspi_tlm)

target_include_directories(qspi_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(qspi_tlm PUBLIC SystemC::systemc)

install(TARGETS qspi_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
