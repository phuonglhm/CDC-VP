file(GLOB VPU_FETCH_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/fetch/src/*.cpp
)

add_library(video_encoder_tlm STATIC
    ${VPU_FETCH_SOURCES}
    common/src/frame.cpp

    prei/src/prei.cpp
    posi/src/posi.cpp
    ime/src/ime.cpp
    fme/src/fme.cpp
)

target_include_directories(video_encoder_tlm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/fetch/include
        ${CMAKE_CURRENT_SOURCE_DIR}/prei/include
        ${CMAKE_CURRENT_SOURCE_DIR}/posi/include
        ${CMAKE_CURRENT_SOURCE_DIR}/ime/include
        ${CMAKE_CURRENT_SOURCE_DIR}/fme/include
)

target_link_libraries(video_encoder_tlm
    PUBLIC
        SystemC::systemc
)

add_library(cdc::components::video_encoder_tlm ALIAS video_encoder_tlm)

add_subdirectory(prei/test)
add_subdirectory(posi/test)
add_subdirectory(ime/test)
add_subdirectory(fme/test)
