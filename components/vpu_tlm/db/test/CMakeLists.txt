file(GLOB DB_TEST_SRCS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(test_db ${DB_TEST_SRCS})

target_include_directories(test_db PRIVATE
    ${CMAKE_SOURCE_DIR}/components/vpu_tlm/common/include
    ${CMAKE_SOURCE_DIR}/components/vpu_tlm/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..                # db/
    ${CMAKE_CURRENT_SOURCE_DIR}/../include       # db/include
)

target_link_libraries(test_db
    PRIVATE
        video_encoder_tlm
        SystemC::systemc
)

add_test(NAME vpu_tlm.test_db COMMAND test_db)
