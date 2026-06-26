add_library(i2c_tlm STATIC
    src/i2c.cpp)
add_library(cdc::components::i2c_tlm ALIAS i2c_tlm)

target_include_directories(i2c_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(i2c_tlm PUBLIC SystemC::systemc)

install(TARGETS i2c_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()
