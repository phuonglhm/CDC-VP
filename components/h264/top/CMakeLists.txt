add_library(h264_top src/encoder_vp.cpp)
target_include_directories(h264_top PUBLIC include)
target_link_libraries(h264_top PUBLIC h264_control h264_dma)
