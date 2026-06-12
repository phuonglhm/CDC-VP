add_library(bus_router STATIC
    src/bus_router.cpp)
add_library(cdc::components::bus_router ALIAS bus_router)

target_include_directories(bus_router
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(bus_router PUBLIC
    SystemC::systemc
    cdc::components::common)

install(TARGETS bus_router EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
