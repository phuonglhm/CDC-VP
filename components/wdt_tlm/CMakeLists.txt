add_library(wdt_tlm STATIC
    src/wdt_tlm.cpp
    src/wdt_model.cpp)
add_library(cdc::components::wdt_tlm ALIAS wdt_tlm)

target_include_directories(wdt_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(wdt_tlm PUBLIC SystemC::systemc)

install(TARGETS wdt_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()
