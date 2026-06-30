add_executable(test_video_encoder_tlm
    test_video_encoder_tlm.cpp
)

target_link_libraries(test_video_encoder_tlm
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

target_include_directories(test_video_encoder_tlm
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
