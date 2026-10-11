# Real coding modules; test assembly explicitly selects the non-codec EC payload.
add_library(h264_coding_modules STATIC
  ../../tq/src/tq_top.cpp ../../tq/src/ftq.cpp ../../tq/src/itq.cpp ../../tq/src/transpose_ram.cpp
  ../../ec/src/ec_top.cpp ../../ec/src/cavlc.cpp ../../ec/src/cabac.cpp
  ../../ec/src/slice_encoder.cpp ../../ec/src/exp_golomb.cpp ../../ec/src/syntax_builder.cpp ../../ec/src/nal_formatter.cpp
  ../../df/src/df_top.cpp ../../df/src/df_filter.cpp ../../df/src/df_line_mem.cpp
  ../../mem/src/memory_map.cpp ../../mem/src/internal_memory.cpp ../../mem/src/external_memory.cpp)
target_include_directories(h264_coding_modules PUBLIC ../../tq/include ../../ec/include ../../df/include ../../mem/include)
target_link_libraries(h264_coding_modules PUBLIC h264_shared)
add_executable(coding_reconstruction_tb coding_reconstruction_tb.cpp)
add_executable(full_pipeline_communication_tb ../full_pipeline_communication_tb.cpp)
foreach(target coding_reconstruction_tb full_pipeline_communication_tb)
  target_link_libraries(${target} PRIVATE h264_coding_modules h264_top h264_platform h264_intra_tlm h264_inter_tlm)
  foreach(width 32 64 128)
    foreach(latency 0 50)
      add_test(NAME ${target}_${width}_${latency} COMMAND ${target} ${width} ${latency})
      set_tests_properties(${target}_${width}_${latency} PROPERTIES TIMEOUT 30 LABELS "communication;non-codec")
    endforeach()
  endforeach()
  foreach(scenario source_fault nal_fault reference_fault reset_active)
    add_test(NAME ${target}_${scenario} COMMAND ${target} 32 50 ${scenario})
    set_tests_properties(${target}_${scenario} PROPERTIES TIMEOUT 30 LABELS "communication;recovery;non-codec")
  endforeach()
  add_test(NAME ${target}_bad_expected COMMAND ${target} 32 50 bad_expected)
  set_tests_properties(${target}_bad_expected PROPERTIES TIMEOUT 30 LABELS "communication;scoreboard")
endforeach()
foreach(module tq ec df mem)
  add_executable(nguyen_${module}_original_tb ../../${module}/test/test_${module}.cpp)
  target_link_libraries(nguyen_${module}_original_tb PRIVATE h264_coding_modules)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nguyen_${module}_original_tb PRIVATE -UNDEBUG)
  endif()
  add_test(NAME nguyen_${module}_original COMMAND nguyen_${module}_original_tb)
  set_tests_properties(nguyen_${module}_original PROPERTIES TIMEOUT 30 LABELS "coding-original")
endforeach()

add_executable(full_pipeline_behavior_tb ../full_pipeline_behavior_tb.cpp)
target_link_libraries(full_pipeline_behavior_tb PRIVATE h264_coding_modules h264_top h264_platform h264_intra_tlm h264_inter_tlm)
foreach(width 32 64 128)
  foreach(scenario normal filter wrap filter_toggle)
    add_test(NAME behavior_${scenario}_${width} COMMAND full_pipeline_behavior_tb ${width} 50 ${scenario})
    set_tests_properties(behavior_${scenario}_${width} PROPERTIES TIMEOUT 30 LABELS "behavior;non-codec")
  endforeach()
endforeach()
foreach(scenario capacity live_config early_reference source_fault nal_fault reference_fault
    reset_prediction reset_tq reset_ec reset_df reset_eos reset_nal_write reset_reference_write reset_sw
    error_tq error_ec error_df error_eos)
  add_test(NAME behavior_${scenario} COMMAND full_pipeline_behavior_tb 32 50 ${scenario})
  set_tests_properties(behavior_${scenario} PROPERTIES TIMEOUT 30 LABELS "behavior;recovery;non-codec")
endforeach()

add_executable(dma_contention_tb dma_contention_tb.cpp)
target_link_libraries(dma_contention_tb PRIVATE h264_coding_modules h264_dma_transport)
foreach(width 32 64 128)
  foreach(delay 0 100)
    add_test(NAME behavior_dma_contention_${width}_${delay} COMMAND dma_contention_tb ${width} ${delay})
    set_tests_properties(behavior_dma_contention_${width}_${delay} PROPERTIES TIMEOUT 30 LABELS "behavior;dma-contention")
  endforeach()
endforeach()

add_executable(nguyen_algorithm_tb algorithm_tb.cpp)
target_link_libraries(nguyen_algorithm_tb PRIVATE h264_coding_modules)
foreach(scenario tq df ec protocol)
  add_test(NAME algorithm_${scenario} COMMAND nguyen_algorithm_tb ${scenario})
  set_tests_properties(algorithm_${scenario} PROPERTIES TIMEOUT 30 LABELS "algorithm;coding")
endforeach()
foreach(width 32 64 128)
  add_test(NAME behavior_filter_offsets_${width} COMMAND full_pipeline_behavior_tb ${width} 50 filter_offsets)
  set_tests_properties(behavior_filter_offsets_${width} PROPERTIES TIMEOUT 30 LABELS "behavior;non-codec")
endforeach()

add_executable(h264_codec_tb codec_tb.cpp)
target_link_libraries(h264_codec_tb PRIVATE h264_coding_modules)
foreach(scenario cavlc syntax)
  add_test(NAME codec_${scenario} COMMAND h264_codec_tb ${scenario})
  set_tests_properties(codec_${scenario} PROPERTIES TIMEOUT 30 LABELS "codec;algorithm")
endforeach()
find_package(Python3 QUIET COMPONENTS Interpreter)
find_program(H264_FFMPEG_EXECUTABLE ffmpeg)
if(Python3_Interpreter_FOUND AND H264_FFMPEG_EXECUTABLE)
  foreach(qp 0 26 51)
    add_test(NAME codec_decode_qp${qp}
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/decode_check.py
        $<TARGET_FILE:h264_codec_tb> ${H264_FFMPEG_EXECUTABLE}
        ${CMAKE_CURRENT_BINARY_DIR}/codec_output ${qp})
    set_tests_properties(codec_decode_qp${qp} PROPERTIES TIMEOUT 60 LABELS "codec;decoder")
  endforeach()
else()
  message(WARNING "H264 decoder validation unavailable: install Python3 and FFmpeg; codec unit tests remain enabled")
endif()

add_executable(full_pipeline_codec_tb ../full_pipeline_codec_tb.cpp)
target_link_libraries(full_pipeline_codec_tb PRIVATE h264_coding_modules h264_top h264_platform h264_intra_tlm h264_inter_tlm)
if(Python3_Interpreter_FOUND AND H264_FFMPEG_EXECUTABLE)
  foreach(width 32 64 128)
    foreach(qp 0 26 51)
      add_test(NAME full_codec_${width}_qp${qp}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/full_pipeline_decode.py
          $<TARGET_FILE:full_pipeline_codec_tb> ${H264_FFMPEG_EXECUTABLE}
          ${CMAKE_CURRENT_BINARY_DIR}/full_codec_output ${width} ${qp} normal)
      set_tests_properties(full_codec_${width}_qp${qp} PROPERTIES TIMEOUT 60 LABELS "full-codec;decoder")
    endforeach()
    foreach(scenario nal_fault reset invalid_width invalid_df_offsets syntax syntax_negative filter filter_offsets filter_negative)
      add_test(NAME full_codec_${width}_${scenario}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/full_pipeline_decode.py
          $<TARGET_FILE:full_pipeline_codec_tb> ${H264_FFMPEG_EXECUTABLE}
          ${CMAKE_CURRENT_BINARY_DIR}/full_codec_output ${width} 26 ${scenario})
      set_tests_properties(full_codec_${width}_${scenario} PROPERTIES TIMEOUT 60 LABELS "full-codec;decoder;recovery")
    endforeach()
  endforeach()
endif()
