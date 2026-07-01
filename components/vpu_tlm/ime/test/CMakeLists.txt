add_executable(test_ime
    test_ime.cpp
)

target_link_libraries(test_ime
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)
