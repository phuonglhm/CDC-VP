set(REC_TEST_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/testbench.cpp
)

add_executable(test_rec ${REC_TEST_SRCS})

target_include_directories(test_rec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..                # rec/
    ${CMAKE_CURRENT_SOURCE_DIR}/../include       # rec/include
    ${CMAKE_SOURCE_DIR}/components/vpu_tlm/common/include
    ${CMAKE_SOURCE_DIR}/components/vpu_tlm/include
)

target_link_libraries(test_rec
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_rec COMMAND test_rec)
