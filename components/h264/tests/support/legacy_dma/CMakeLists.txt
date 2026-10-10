# Previous Huy DMA model: tests only, not installed or linked into EncoderVp.
add_library(h264_legacy_dma src/dma_arbiter.cpp src/dma_bridge.cpp)
target_include_directories(h264_legacy_dma PUBLIC include)
target_link_libraries(h264_legacy_dma PUBLIC h264_sync)
add_executable(h264_legacy_dma_tb test/legacy_dma_tb.cpp)
target_link_libraries(h264_legacy_dma_tb PRIVATE h264_legacy_dma h264_platform)
add_test(NAME h264_legacy_priority_fairness COMMAND h264_legacy_dma_tb)
set_tests_properties(h264_legacy_priority_fairness PROPERTIES TIMEOUT 20 LABELS "legacy;test-only")
