#author: linhtk55-fpt

add_library(timer_tlm STATIC
    src/timer.cpp)
add_library(cdc::components::timer_tlm ALIAS timer_tlm)
add_library(cdc::components::timer2_tlm ALIAS timer_tlm)

target_include_directories(timer_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(timer_tlm PUBLIC SystemC::systemc)

install(TARGETS timer_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()
