include(CTest)

file(GLOB VPU_FETCH_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/fetch/src/*.cpp
)

file(GLOB VPU_MEM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/mem/src/*.cpp
)

add_library(video_encoder_tlm STATIC
    ${VPU_MEM_SOURCES}
    ${VPU_FETCH_SOURCES}
    common/src/frame.cpp
    common/src/simple_memory.cpp
    src/mode_decision.cpp
    src/prediction_to_rec_packet.cpp
    src/rec_backend_bridges.cpp
    src/video_encoder_to_rec.cpp
    src/video_encoder_full_tlm.cpp
    src/video_encoder_tlm.cpp

    rec/src/rec_intra.cpp
    rec/src/rec_mc.cpp
    rec/src/rec_tq.cpp
    rec/src/rec_inv_tq.cpp
    rec/src/res_buffer.cpp
    rec/src/rec_memory.cpp
    rec/src/top.cpp

    db/src/db_bs.cpp
    db/src/db_mv.cpp
    db/src/db_filter_sao.cpp
    db/src/top.cpp

    cabac/src/cabac.cpp
    cabac/src/top.cpp

    prei/src/prei.cpp
    posi/src/posi.cpp
    ime/src/ime.cpp
    fme/src/fme.cpp
)

target_include_directories(video_encoder_tlm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mem/include
        ${CMAKE_CURRENT_SOURCE_DIR}/fetch/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/rec/include
        ${CMAKE_CURRENT_SOURCE_DIR}/db/include
        ${CMAKE_CURRENT_SOURCE_DIR}/cabac/include
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
add_subdirectory(rec/test)
add_subdirectory(db/test)
add_subdirectory(cabac/test)
add_subdirectory(fetch/test)

add_subdirectory(mem/test)
add_subdirectory(tests)
