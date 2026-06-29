add_executable(test_video_encoder_tlm test_video_encoder_tlm.cpp)
target_link_libraries(test_video_encoder_tlm PRIVATE
    cdc::components::video_encoder_tlm
    SystemC::systemc)
target_include_directories(test_video_encoder_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME video_encoder_tlm COMMAND test_video_encoder_tlm)
