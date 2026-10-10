# Compile the external implementation in this build tree; never modify CDC-VP.
set(fx1 "${H264_CDC_VP_ROOT}/components/FX1_Components")
if(NOT EXISTS "${fx1}/bus/src/bus_system.cpp")
  message(FATAL_ERROR "H264_CDC_VP_ROOT must point to CDC-VP with FX1 components")
endif()
add_library(h264_cdc_fixture STATIC
  "${fx1}/bus/src/config.cpp" "${fx1}/bus/src/router.cpp"
  "${fx1}/bus/src/apb_bridge.cpp" "${fx1}/bus/src/bus_system.cpp"
  "${fx1}/sparse_ram/src/sparse_ram.cpp"
  "${fx1}/exclusive_monitor/src/exclusive_monitor.cpp"
  "${fx1}/plic/src/plic.cpp")
target_include_directories(h264_cdc_fixture PUBLIC
  "${fx1}/bus/include" "${fx1}/fx1_map/include"
  "${fx1}/sparse_ram/include" "${fx1}/exclusive_monitor/include"
  "${fx1}/plic/include" "${H264_CDC_VP_ROOT}/cpu_models/include")
target_compile_features(h264_cdc_fixture PUBLIC cxx_std_17)
target_link_libraries(h264_cdc_fixture PUBLIC SystemC::systemc)
add_executable(h264_cdc_fx1_tb cdc_fx1_tb.cpp)
target_link_libraries(h264_cdc_fx1_tb PRIVATE h264_cdc_fixture h264_pipeline_stub h264_platform)
add_test(NAME h264_cdc_fx1 COMMAND h264_cdc_fx1_tb)
set_tests_properties(h264_cdc_fx1 PROPERTIES TIMEOUT 30 LABELS "h264;cdc-fx1;stub")
