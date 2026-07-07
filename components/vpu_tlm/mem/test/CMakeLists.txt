add_executable(test_mem test_mem.cpp)

target_link_libraries(test_mem
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)
