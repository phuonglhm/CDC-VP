add_library(isp_tlm STATIC
    src/isp_tlm.cpp
    src/isp_pipeline.cpp
    src/blocks/2dnr.cpp
    src/blocks/aec.cpp
    src/blocks/awb.cpp
    src/blocks/blc.cpp
    src/blocks/bnr.cpp
    src/blocks/ccm.cpp
    src/blocks/cse.cpp
    src/blocks/demosaic.cpp
    src/blocks/dg.cpp
    src/blocks/dpc.cpp
    src/blocks/gc.cpp
    src/blocks/lsc.cpp

    src/blocks/rgb_to_yuv.cpp
    src/blocks/scale.cpp
    src/blocks/sharpen.cpp
    src/blocks/wb.cpp
    src/blocks/yuv420.cpp
)
add_library(cdc::components::isp_tlm ALIAS isp_tlm)

target_include_directories(isp_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src
        include/blocks
)

target_link_libraries(isp_tlm PUBLIC SystemC::systemc)

install(TARGETS isp_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()
