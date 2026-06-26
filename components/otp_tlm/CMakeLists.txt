add_library(otp_tlm STATIC
    src/otp.cpp)
add_library(cdc::components::otp_tlm ALIAS otp_tlm)

target_include_directories(otp_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(otp_tlm PUBLIC SystemC::systemc)

install(TARGETS otp_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()
