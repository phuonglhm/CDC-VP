# Integration glue delegates arbitration and AXI transfers to Vinh's DMA cores.
add_library(h264_dma_transport src/dma_transport.cpp)
target_include_directories(h264_dma_transport PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>")
target_link_libraries(h264_dma_transport PUBLIC h264_dma h264_sync)
add_library(h264_top src/encoder_vp.cpp)
target_include_directories(h264_top PUBLIC include)
target_link_libraries(h264_top PUBLIC h264_control h264_dma_transport)