add_library(h264_control src/control_regs.cpp src/encoder_controller.cpp)
target_include_directories(h264_control PUBLIC include)
target_link_libraries(h264_control PUBLIC h264_sync)
