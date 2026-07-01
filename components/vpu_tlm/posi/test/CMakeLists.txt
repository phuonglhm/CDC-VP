add_executable(test_posi
    test_posi.cpp
)

target_link_libraries(test_posi
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)
