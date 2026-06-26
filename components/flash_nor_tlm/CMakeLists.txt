add_library(flash_nor_tlm STATIC
    src/flash_nor_tlm.cpp)
add_library(cdc::components::flash_nor_tlm ALIAS flash_nor_tlm)

target_include_directories(flash_nor_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(flash_nor_tlm PUBLIC SystemC::systemc)

install(TARGETS flash_nor_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
