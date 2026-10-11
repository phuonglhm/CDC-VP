cmake_minimum_required(VERSION 3.16)
project(h264_vp_tq_test LANGUAGES CXX)

set(SYSTEMC_HOME "/usr/local/systemc-2.3.4")
find_package(SystemCLanguage CONFIG REQUIRED
    HINTS "${SYSTEMC_HOME}/lib/cmake/SystemCLanguage"
)

add_library(h264_vp_tq STATIC
    ../src/tq_top.cpp
    ../src/ftq.cpp
    ../src/itq.cpp
    ../src/transpose_ram.cpp
)

target_compile_features(h264_vp_tq PUBLIC cxx_std_17)
target_include_directories(h264_vp_tq PUBLIC ../include ../../interfaces/include)

target_link_libraries(h264_vp_tq PUBLIC SystemC::systemc)

add_executable(test_h264_vp_tq test_tq.cpp)

target_link_libraries(test_h264_vp_tq PRIVATE h264_vp_tq)

enable_testing()
add_test(NAME h264_vp_tq_unit COMMAND test_h264_vp_tq)
