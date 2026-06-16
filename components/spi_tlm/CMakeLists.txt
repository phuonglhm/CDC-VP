add_library(spi_tlm STATIC
    spi.cpp)
add_library(cdc::components::spi_tlm ALIAS spi_tlm)

target_include_directories(spi_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(spi_tlm PUBLIC SystemC::systemc)

install(TARGETS spi_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(FILES spi.h DESTINATION include)
