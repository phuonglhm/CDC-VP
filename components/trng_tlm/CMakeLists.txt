add_library(trng_tlm STATIC
    src/trng_tlm.cpp
    src/trng_model.cpp)
add_library(cdc::components::trng_tlm ALIAS trng_tlm)

target_include_directories(trng_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(trng_tlm PUBLIC SystemC::systemc)

install(TARGETS trng_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()
