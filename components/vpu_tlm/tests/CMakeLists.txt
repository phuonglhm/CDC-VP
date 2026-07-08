add_executable(test_mode_decision
    test_mode_decision.cpp
)

target_link_libraries(test_mode_decision
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_mode_decision COMMAND test_mode_decision)

add_executable(test_prediction_to_rec_packet
    test_prediction_to_rec_packet.cpp
)

target_link_libraries(test_prediction_to_rec_packet
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_prediction_to_rec_packet COMMAND test_prediction_to_rec_packet)

add_executable(test_video_encoder_tlm
    test_video_encoder_tlm.cpp
)

target_link_libraries(test_video_encoder_tlm
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_video_encoder_tlm COMMAND test_video_encoder_tlm)

add_executable(test_video_encoder_rec_integration
    test_video_encoder_rec_integration.cpp
)

target_link_libraries(test_video_encoder_rec_integration
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_video_encoder_rec_integration COMMAND test_video_encoder_rec_integration)

add_executable(test_video_encoder_full_tlm
    test_video_encoder_full_tlm.cpp
)

target_link_libraries(test_video_encoder_full_tlm
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_video_encoder_full_tlm COMMAND test_video_encoder_full_tlm)
