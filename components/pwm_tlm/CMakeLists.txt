add_library(pwm_tlm STATIC
    src/pwm.cpp)
add_library(cdc::components::pwm_tlm ALIAS pwm_tlm)

target_include_directories(pwm_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(pwm_tlm PUBLIC SystemC::systemc)

install(TARGETS pwm_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()
