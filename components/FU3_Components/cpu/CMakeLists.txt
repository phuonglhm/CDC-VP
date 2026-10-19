# FU3 CPU configuration (CVA6 + Ibex) on top of the cva6_vp wrapper.
# Header-only and built only inside the repository build (CDC_BUILD_FU3_SOC=ON), because cdc_cpu_cva6 comes from cpu_models/ and needs the top-level paths.
if(NOT TARGET cdc_cpu_cva6)
  message(FATAL_ERROR "fu3::cpu needs cdc_cpu_cva6: configure the repository root with -DCDC_BUILD_FU3_SOC=ON")
endif()

add_library(fu3_cpu INTERFACE)
add_library(fu3::cpu ALIAS fu3_cpu)

target_include_directories(fu3_cpu INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(fu3_cpu INTERFACE cdc_cpu_cva6)
