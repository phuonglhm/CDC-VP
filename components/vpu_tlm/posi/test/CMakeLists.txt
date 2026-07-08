add_executable(test_posi
    test_posi.cpp
)

target_link_libraries(test_posi
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_posi COMMAND test_posi)
