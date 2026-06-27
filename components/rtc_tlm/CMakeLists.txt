add_library(rtc_tlm STATIC
    src/rtc_tlm.cpp
    src/rtc_model.cpp)
add_library(cdc::components::rtc_tlm ALIAS rtc_tlm)

target_include_directories(rtc_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(rtc_tlm PUBLIC SystemC::systemc)

install(TARGETS rtc_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
