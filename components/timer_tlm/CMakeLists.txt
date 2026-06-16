add_library(timer_tlm STATIC
    timer.cpp)
add_library(cdc::components::timer_tlm ALIAS timer_tlm)

target_include_directories(timer_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(timer_tlm PUBLIC SystemC::systemc)
add_executable(testbench testbench.cpp)
target_link_libraries(testbench PRIVATE cdc::components::timer_tlm)

install(TARGETS timer_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
# install(DIRECTORY include/ DESTINATION include)

# if(CDC_BUILD_TESTS)
#    add_subdirectory(tests)
# endif()
