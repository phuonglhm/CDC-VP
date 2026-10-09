add_library(h264_dma src/dma_bridge.cpp src/dma_arbiter.cpp)
target_include_directories(h264_dma PUBLIC include)
target_link_libraries(h264_dma PUBLIC h264_sync)
