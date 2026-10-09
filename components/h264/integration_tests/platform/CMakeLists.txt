add_library(h264_platform memory/src/ddr_memory.cpp host/src/host_driver.cpp)
target_include_directories(h264_platform PUBLIC memory/include host/include)
target_link_libraries(h264_platform PUBLIC h264_sync h264_control)
