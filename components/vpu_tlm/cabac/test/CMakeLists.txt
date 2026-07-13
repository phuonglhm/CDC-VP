file(GLOB CABAC_TEST_SRCS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(test_cabac ${CABAC_TEST_SRCS})

target_include_directories(test_cabac PRIVATE
    ${CMAKE_SOURCE_DIR}/components/vpu_tlm/common/include
    ${CMAKE_SOURCE_DIR}/components/vpu_tlm/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..                # cabac/
    ${CMAKE_CURRENT_SOURCE_DIR}/../include       # cabac/include
)

target_link_libraries(test_cabac
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_cabac COMMAND test_cabac)
