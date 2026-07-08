add_executable(test_fme
    test_fme.cpp
)

target_link_libraries(test_fme
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_fme COMMAND test_fme)
