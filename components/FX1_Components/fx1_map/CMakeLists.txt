# cdc::components::fx1_map — header-only FX1 memory map, interrupt map and
# timer constants. One source for the platform, the bus defaults and firmware.
add_library(fx1_map INTERFACE)
add_library(cdc::components::fx1_map ALIAS fx1_map)
target_include_directories(fx1_map INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

install(TARGETS fx1_map EXPORT cdc-components-targets)
install(DIRECTORY include/ DESTINATION include)
