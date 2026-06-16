# Author: hoangv11
# Verified by: quannh107

add_library(spi_tlm STATIC
    src/spi_tlm.cpp)
add_library(cdc::components::spi_tlm ALIAS spi_tlm)

target_include_directories(spi_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(spi_tlm PUBLIC SystemC::systemc)

install(TARGETS spi_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()
