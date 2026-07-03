add_library(isp_tlm STATIC
    core/src/isp_tlm.cpp
    pipeline/src/isp_pipeline.cpp
    blocks/2dnr/src/2dnr.cpp
    blocks/aec/src/aec.cpp
    blocks/awb/src/awb.cpp
    blocks/blc/src/blc.cpp
    blocks/bnr/src/bnr.cpp
    blocks/ccm/src/ccm.cpp
    blocks/cse/src/cse.cpp
    blocks/demosaic/src/demosaic.cpp
    blocks/dg/src/dg.cpp
    blocks/dpc/src/dpc.cpp
    blocks/gc/src/gc.cpp
    blocks/lsc/src/lsc.cpp

    blocks/rgb_to_yuv/src/rgb_to_yuv.cpp
    blocks/scale/src/scale.cpp
    blocks/sharpen/src/sharpen.cpp
    blocks/wb/src/wb.cpp
    blocks/yuv420/src/yuv420.cpp
)
add_library(cdc::components::isp_tlm ALIAS isp_tlm)

target_include_directories(isp_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pipeline/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/2dnr/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/aec/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/awb/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/blc/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/bnr/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/ccm/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/cse/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/demosaic/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/dg/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/dpc/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/gc/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/lsc/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/rgb_to_yuv/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/scale/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/sharpen/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/wb/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blocks/yuv420/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        core/src
)

target_link_libraries(isp_tlm PUBLIC SystemC::systemc)

install(TARGETS isp_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY core/include/ DESTINATION include)

if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()
