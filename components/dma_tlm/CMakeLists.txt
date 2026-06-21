#Author: QuanNH107

add_library(dma_tlm STATIC
    src/dma_tlm.cpp)
add_library(cdc::components::dma_tlm ALIAS dma_tlm)

target_include_directories(dma_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(dma_tlm PUBLIC SystemC::systemc)

install(TARGETS dma_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()
