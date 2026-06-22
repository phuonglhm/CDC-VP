add_library(dmic_tlm STATIC
    src/dmic.cpp)
add_library(cdc::components::dmic_tlm ALIAS dmic_tlm)

target_include_directories(dmic_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(dmic_tlm PUBLIC SystemC::systemc)

install(TARGETS dmic_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(src)
endif()
