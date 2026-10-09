cmake_minimum_required(VERSION 3.16)
project(h264_vp_ec_test LANGUAGES CXX)

set(SYSTEMC_HOME "/usr/local/systemc-2.3.4")
find_package(SystemCLanguage CONFIG REQUIRED
    HINTS "${SYSTEMC_HOME}/lib/cmake/SystemCLanguage"
)

add_library(h264_vp_ec STATIC
    ../src/ec_top.cpp
    ../src/cavlc.cpp
    ../src/cabac.cpp
    ../src/exp_golomb.cpp
    ../src/nal_formatter.cpp
    ../src/syntax_builder.cpp
)
target_include_directories(h264_vp_ec PUBLIC ../include)
target_link_libraries(h264_vp_ec PUBLIC SystemC::systemc)

add_executable(test_h264_vp_ec test_ec.cpp)

target_link_libraries(test_h264_vp_ec PRIVATE h264_vp_ec)

enable_testing()
add_test(NAME h264_vp_ec_unit COMMAND test_h264_vp_ec)