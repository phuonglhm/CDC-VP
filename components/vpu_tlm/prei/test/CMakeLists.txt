add_executable(test_prei
    test_prei.cpp
)

target_link_libraries(test_prei
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_prei COMMAND test_prei)
