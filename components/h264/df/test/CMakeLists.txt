cmake_minimum_required(VERSION 3.16)
project(h264_vp_df_test LANGUAGES CXX)

set(SYSTEMC_HOME "/usr/local/systemc-2.3.4")
find_package(SystemCLanguage CONFIG REQUIRED HINTS "${SYSTEMC_HOME}/lib/cmake/SystemCLanguage")

add_library(h264_vp_df STATIC 
    ../src/df_top.cpp
    ../src/df_line_mem.cpp
    ../src/df_filter.cpp
)
target_include_directories(h264_vp_df PUBLIC ../include)
target_link_libraries(h264_vp_df PUBLIC SystemC::systemc)

add_executable(test_h264_vp_df test_df.cpp)
target_link_libraries(test_h264_vp_df PRIVATE h264_vp_df)

enable_testing()
add_test(NAME h264_vp_df_unit COMMAND test_h264_vp_df)