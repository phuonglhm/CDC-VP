cmake_minimum_required(VERSION 3.16)

# Video Encoder TLM component

add_library(video_encoder_tlm STATIC
    src/frame.cpp
    src/prei.cpp
    src/posi.cpp
    src/ime.cpp
    src/fme.cpp
    src/mode_decision.cpp
    src/rec.cpp
    src/cabac.cpp
    src/video_encoder_tlm.cpp
)

target_include_directories(video_encoder_tlm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(video_encoder_tlm
    PUBLIC
        SystemC::systemc
)

add_library(cdc::components::video_encoder_tlm ALIAS video_encoder_tlm)

add_subdirectory(tests)
