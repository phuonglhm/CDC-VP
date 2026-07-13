file(GLOB FETCH_TEST_SRCS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(test_fetch ${FETCH_TEST_SRCS})

target_include_directories(test_fetch PRIVATE
    ${CMAKE_SOURCE_DIR}/components/vpu_tlm/common/include
    ${CMAKE_SOURCE_DIR}/components/vpu_tlm/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..                # fetch/
    ${CMAKE_CURRENT_SOURCE_DIR}/../include       # fetch/include
)

target_link_libraries(test_fetch
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_fetch COMMAND test_fetch)
