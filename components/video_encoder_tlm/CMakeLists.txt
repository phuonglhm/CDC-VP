add_library(video_encoder_tlm STATIC
    src/frame.cpp
    src/prei.cpp
    src/posi.cpp
    src/ime.cpp
    src/fme.cpp
    src/mode_decision.cpp
    src/rec.cpp
    src/cabac.cpp
    src/video_encoder_tlm.cpp)
add_library(cdc::components::video_encoder_tlm ALIAS video_encoder_tlm)

target_include_directories(video_encoder_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(video_encoder_tlm PUBLIC SystemC::systemc)

install(TARGETS video_encoder_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
