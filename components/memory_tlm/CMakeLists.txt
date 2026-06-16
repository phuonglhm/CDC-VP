add_library(memory_tlm STATIC
    src/memory_tlm.cpp)
add_library(cdc::components::memory_tlm ALIAS memory_tlm)

target_include_directories(memory_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(memory_tlm PUBLIC SystemC::systemc)

install(TARGETS memory_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
