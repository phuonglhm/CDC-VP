add_library(plic_tlm STATIC
    src/plic_tlm.cpp)
add_library(cdc::components::plic_tlm ALIAS plic_tlm)

target_include_directories(plic_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/cpu_models/include>   # cpu_base.h
    PRIVATE src)

target_link_libraries(plic_tlm PUBLIC SystemC::systemc)

install(TARGETS plic_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
