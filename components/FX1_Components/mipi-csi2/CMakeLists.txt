cmake_minimum_required(VERSION 3.16)
# MIPI CSI-2 RX component: detailed D-PHY RX model (fx1_mipi_dphy), the
# D-PHY wrapper/startup fixture and the sensor BFM (fx1_csi2_bfm).
# Configured from FX1_Components when FX1_BUILD_MIPI_CSI2=ON, or standalone.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(fx1_mipi_csi2 LANGUAGES C CXX)
  include(CTest)
  set(_csi2_tests_default ${BUILD_TESTING})
elseif(DEFINED CDC_BUILD_TESTS)
  set(_csi2_tests_default ${CDC_BUILD_TESTS})
else()
  set(_csi2_tests_default OFF)
endif()

if(NOT TARGET SystemC::systemc)
  if(SYSTEMC_HOME)
    list(PREPEND CMAKE_PREFIX_PATH "${SYSTEMC_HOME}")
  elseif(DEFINED ENV{SYSTEMC_HOME})
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{SYSTEMC_HOME}")
  endif()
  find_package(SystemCLanguage CONFIG QUIET)
  if(NOT TARGET SystemC::systemc)
    find_path(SYSTEMC_INCLUDE_DIR NAMES systemc HINTS "${SYSTEMC_HOME}/include" "$ENV{SYSTEMC_HOME}/include")
    find_library(SYSTEMC_LIBRARY NAMES systemc HINTS "${SYSTEMC_HOME}/lib" "${SYSTEMC_HOME}/lib64" "$ENV{SYSTEMC_HOME}/lib")
    if(NOT SYSTEMC_INCLUDE_DIR OR NOT SYSTEMC_LIBRARY)
      message(FATAL_ERROR "SystemC not found. Set SYSTEMC_HOME or SYSTEMC_INCLUDE_DIR and SYSTEMC_LIBRARY.")
    endif()
    find_package(Threads REQUIRED)
    add_library(SystemC::systemc UNKNOWN IMPORTED)
    set_target_properties(SystemC::systemc PROPERTIES IMPORTED_LOCATION "${SYSTEMC_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES Threads::Threads)
  endif()
endif()

# --- fx1_mipi_dphy: the generated PHY model, compiled as shipped ------------
# src/sources.txt is the authoritative list; vl_rt.cpp and the hand-written LP
# receiver are added as documented in README.md. No unity build: helpers in
# different generated TUs share names.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/sources.txt" _dphy_rel_sources REGEX "\\.cpp")
set(_dphy_sources "${CMAKE_CURRENT_SOURCE_DIR}/include/vl/vl_rt.cpp")
foreach(_s IN LISTS _dphy_rel_sources)
  string(STRIP "${_s}" _s)
  list(APPEND _dphy_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/${_s}")
endforeach()
file(GLOB _dphy_manual_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/manual/*.cpp")
list(APPEND _dphy_sources ${_dphy_manual_sources})

add_library(fx1_mipi_dphy STATIC ${_dphy_sources})
add_library(cdc::components::fx1_mipi_dphy ALIAS fx1_mipi_dphy)
target_compile_features(fx1_mipi_dphy PUBLIC cxx_std_17)
# SYSTEM: the generated model headers are not warning-clean and are not edited.
target_include_directories(fx1_mipi_dphy SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src/common
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rx_core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pll
  ${CMAKE_CURRENT_SOURCE_DIR}/src/top
  ${CMAKE_CURRENT_SOURCE_DIR}/src/manual)
target_link_libraries(fx1_mipi_dphy PUBLIC SystemC::systemc)
# The netlist TUs are very large; -O1 keeps compile time and memory bounded
# (README.md). Placed after the build-type flags on the command line, so it
# overrides -O2/-O3 for this target only.
set(FX1_MIPI_DPHY_OPT "-O1" CACHE STRING "Optimisation flag for the generated D-PHY netlist")
target_compile_options(fx1_mipi_dphy PRIVATE ${FX1_MIPI_DPHY_OPT})

# --- fx1_csi2_bfm: sensor-side BFM (packet builder, lane distribution) -----
add_library(fx1_csi2_bfm STATIC bfm/src/csi2_packet.cpp)
add_library(cdc::components::fx1_csi2_bfm ALIAS fx1_csi2_bfm)
target_compile_features(fx1_csi2_bfm PUBLIC cxx_std_17)
target_include_directories(fx1_csi2_bfm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bfm/include> $<INSTALL_INTERFACE:include>)
if(NOT MSVC)
  target_compile_options(fx1_csi2_bfm PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- fx1_mipi_dphy_wrapper: port binding, tie-offs and startup fixture ------
add_library(fx1_mipi_dphy_wrapper STATIC phy/src/dphy_rx_wrapper.cpp phy/src/dphy_adapter.cpp phy/src/dphy_startup.cpp)
add_library(cdc::components::fx1_mipi_dphy_wrapper ALIAS fx1_mipi_dphy_wrapper)
target_include_directories(fx1_mipi_dphy_wrapper PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/phy/include> $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/phy/src)
target_link_libraries(fx1_mipi_dphy_wrapper PUBLIC fx1_mipi_dphy)
if(NOT MSVC)
  target_compile_options(fx1_mipi_dphy_wrapper PRIVATE -Wall -Wextra)
endif()

# --- fx1_csi2_dphy_bfm: pad-level D-PHY transmitter (needs the vl runtime) --
add_library(fx1_csi2_dphy_bfm STATIC bfm/src/dphy_tx_bfm.cpp)
add_library(cdc::components::fx1_csi2_dphy_bfm ALIAS fx1_csi2_dphy_bfm)
target_link_libraries(fx1_csi2_dphy_bfm PUBLIC fx1_csi2_bfm fx1_mipi_dphy)
if(NOT MSVC)
  target_compile_options(fx1_csi2_dphy_bfm PRIVATE -Wall -Wextra)
endif()

# --- fx1_csi2_regs: C99 register map (generated from the v1.4 workbook) -----
# I2C controller model (components/i2c_tlm): the CDC-VP tree provides it;
# a standalone build of this component compiles it here.
if(NOT TARGET cdc::components::i2c_tlm)
  set(_i2c_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../i2c_tlm")
  add_library(fx1_csi2_i2c_tlm STATIC "${_i2c_dir}/src/i2c.cpp")
  target_include_directories(fx1_csi2_i2c_tlm PUBLIC "${_i2c_dir}/include")
  target_link_libraries(fx1_csi2_i2c_tlm PUBLIC SystemC::systemc)
  add_library(cdc::components::i2c_tlm ALIAS fx1_csi2_i2c_tlm)
endif()

# CCS camera sensor model (M5a): I2C target driving the D-PHY transmit BFM.
add_library(fx1_csi2_sensor STATIC sensor/src/ccs_sensor.cpp)
add_library(cdc::components::fx1_csi2_sensor ALIAS fx1_csi2_sensor)
target_include_directories(fx1_csi2_sensor PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sensor/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(fx1_csi2_sensor PUBLIC fx1_csi2_dphy_bfm cdc::components::i2c_tlm)
if(NOT MSVC)
  target_compile_options(fx1_csi2_sensor PRIVATE -Wall -Wextra)
endif()

add_library(fx1_csi2_regs INTERFACE)
target_include_directories(fx1_csi2_regs INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/driver/include> $<INSTALL_INTERFACE:include>)

# --- fx1_csi2_drv: C99 reference driver (no SystemC) -----------------------
enable_language(C)
add_library(fx1_csi2_drv STATIC driver/src/fx1_csi2_drv.c)
add_library(cdc::components::fx1_csi2_drv ALIAS fx1_csi2_drv)
target_link_libraries(fx1_csi2_drv PUBLIC fx1_csi2_regs)
set_target_properties(fx1_csi2_drv PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(NOT MSVC)
  target_compile_options(fx1_csi2_drv PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- fx1_csi2_controller: CSR, reset sequencer, IRQ, PHY adapter (M2) -------
# Receive datapath cores without SystemC (unit-testable on their own).
add_library(fx1_csi2_rx_core STATIC controller/src/csi2_protocol.cpp controller/src/lane_management.cpp
  controller/src/packet_analyzer.cpp controller/src/pixel_path.cpp controller/src/stream_out.cpp
  controller/src/frame_control.cpp controller/src/sys_datapath.cpp controller/src/axis_monitor.cpp controller/src/dma_engine.cpp)
target_compile_features(fx1_csi2_rx_core PUBLIC cxx_std_17)
target_include_directories(fx1_csi2_rx_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/controller/include> $<INSTALL_INTERFACE:include>)
if(NOT MSVC)
  target_compile_options(fx1_csi2_rx_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fx1_csi2_controller STATIC controller/src/csi2_rx_controller.cpp)
add_library(cdc::components::fx1_csi2_controller ALIAS fx1_csi2_controller)
target_include_directories(fx1_csi2_controller PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/controller/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(fx1_csi2_controller PUBLIC fx1_csi2_regs fx1_csi2_rx_core fx1_mipi_dphy_wrapper)
if(NOT MSVC)
  target_compile_options(fx1_csi2_controller PRIVATE -Wall -Wextra)
endif()

# --- tests -----------------------------------------------------------------
option(FX1_MIPI_CSI2_BUILD_TESTS "Build MIPI CSI-2 / D-PHY regressions" ${_csi2_tests_default})
if(FX1_MIPI_CSI2_BUILD_TESTS)
  enable_testing()
  add_executable(test_fx1_csi2_packet tests/test_csi2_packet.cpp)
  target_link_libraries(test_fx1_csi2_packet PRIVATE fx1_csi2_bfm)
  foreach(case ecc crc packets pack lanes)
    add_test(NAME fx1_csi2_packet_${case} COMMAND test_fx1_csi2_packet ${case})
    set_tests_properties(fx1_csi2_packet_${case} PROPERTIES TIMEOUT 10 LABELS "fx1_csi2;fx1_csi2_unit")
  endforeach()

  add_executable(test_fx1_dphy_startup tests/test_phy_startup.cpp)
  target_link_libraries(test_fx1_dphy_startup PRIVATE fx1_mipi_dphy_wrapper)
  add_test(NAME fx1_dphy_elaborate COMMAND test_fx1_dphy_startup elaborate)
  set_tests_properties(fx1_dphy_elaborate PROPERTIES TIMEOUT 60 LABELS "fx1_csi2;fx1_csi2_unit")
  foreach(case startup reset config_fault)
    add_test(NAME fx1_dphy_${case} COMMAND test_fx1_dphy_startup ${case})
    set_tests_properties(fx1_dphy_${case} PROPERTIES TIMEOUT 600 LABELS "fx1_csi2;fx1_phy_slow")
  endforeach()

  add_executable(test_fx1_csi2_lane_packet tests/test_csi2_lane_packet.cpp)
  target_link_libraries(test_fx1_csi2_lane_packet PRIVATE fx1_csi2_rx_core fx1_csi2_bfm)
  foreach(case lm_merge lm_abort lm_lanes lm_reset_states pa_lines pa_ecc pa_payload_errors pa_routing pa_frames
               pa_short_header pa_missing_footer pa_order)
    add_test(NAME fx1_csi2_rx_${case} COMMAND test_fx1_csi2_lane_packet ${case})
    set_tests_properties(fx1_csi2_rx_${case} PROPERTIES TIMEOUT 60 LABELS "fx1_csi2;fx1_csi2_unit")
  endforeach()

  add_executable(test_fx1_csi2_sys_datapath tests/test_csi2_sys_datapath.cpp)
  target_link_libraries(test_fx1_csi2_sys_datapath PRIVATE fx1_csi2_rx_core fx1_csi2_bfm)
  foreach(case b2p_raw10 b2p_raw12 b2p_wc_align lb_fwft fb_formats axis_skid axis_abort tpg_pattern tpg_timing
               pipe_frame_raw10 pipe_frame_raw12 pipe_crc_drop pipe_lb_full pipe_lb_overflow pipe_cdc pipe_wc_align
               pipe_tpg fc_checks ctl_core_disable ctl_unit ctl_core_pulse pipe_zero_wc mux_unit pipe_quiescent
               pipe_bench)
    add_test(NAME fx1_csi2_sys_${case} COMMAND test_fx1_csi2_sys_datapath ${case})
    set_tests_properties(fx1_csi2_sys_${case} PROPERTIES TIMEOUT 60 LABELS "fx1_csi2;fx1_csi2_unit")
  endforeach()

  add_executable(test_fx1_csi2_dma tests/test_csi2_dma.cpp)
  target_link_libraries(test_fx1_csi2_dma PRIVATE fx1_csi2_rx_core fx1_csi2_bfm)
  foreach(case frame_raw10 frame_raw12 frame_raw10_to16 frame_raw12_to16 bursts pingpong layout bounds
               claim_refused cfg_latched overflow back_to_back ctl_drop stream_backlog)
    add_test(NAME fx1_csi2_dma_${case} COMMAND test_fx1_csi2_dma ${case})
    set_tests_properties(fx1_csi2_dma_${case} PROPERTIES TIMEOUT 120 LABELS "fx1_csi2;fx1_csi2_unit")
  endforeach()

  add_executable(test_fx1_csi2_sensor tests/test_csi2_sensor.cpp)
  target_include_directories(test_fx1_csi2_sensor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(test_fx1_csi2_sensor PRIVATE fx1_csi2_controller fx1_csi2_drv fx1_csi2_sensor)
  foreach(case stream raw12_2lane stream_noncontinuous controls raw_file reset_mid_frame restart_before_fe raw_short raw_partial raw_mode_change raw_load_errors)
    add_test(NAME fx1_csi2_sensor_${case} COMMAND test_fx1_csi2_sensor ${case} ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(fx1_csi2_sensor_${case} PROPERTIES TIMEOUT 600 LABELS "fx1_csi2;fx1_phy_slow")
  endforeach()

  add_executable(test_fx1_csi2_dma_ctrl tests/test_csi2_dma_ctrl.cpp)
  target_include_directories(test_fx1_csi2_dma_ctrl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(test_fx1_csi2_dma_ctrl PRIVATE fx1_csi2_controller fx1_csi2_drv fx1_csi2_bfm)
  foreach(case soft_reset_delayed full_reset_delayed resetn_delayed abort_delayed_p0 abort_delayed_p1
               disable_delayed core_disable_open core_disable_after_fe slverr_first slverr_mid slverr_last
               decerr_mid_p0 slverr_partial late_response driver_api)
    add_test(NAME fx1_csi2_dmactl_${case} COMMAND test_fx1_csi2_dma_ctrl ${case})
    set_tests_properties(fx1_csi2_dmactl_${case} PROPERTIES TIMEOUT 120 LABELS "fx1_csi2;fx1_csi2_unit")
  endforeach()

  # P13: PLL gating of the D-PHY model, checked by delta-cycle count.
  add_executable(test_fx1_dphy_pll_gate tests/test_dphy_pll_gate.cpp)
  target_link_libraries(test_fx1_dphy_pll_gate PRIVATE fx1_mipi_dphy_wrapper)
  foreach(case on off)
    add_test(NAME fx1_dphy_pll_gate_${case} COMMAND test_fx1_dphy_pll_gate ${case})
    set_tests_properties(fx1_dphy_pll_gate_${case} PROPERTIES TIMEOUT 300 LABELS "fx1_csi2;fx1_phy_slow")
  endforeach()

  add_executable(test_fx1_csi2_rx_phy tests/test_csi2_rx_phy.cpp)
  target_link_libraries(test_fx1_csi2_rx_phy PRIVATE fx1_csi2_controller fx1_csi2_dphy_bfm fx1_csi2_drv)
  target_include_directories(test_fx1_csi2_rx_phy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  foreach(case frame_raw10_4lane frame_raw12_2lane frame_raw10_1lane errors unarmed skew
               soft_reset_aligned soft_reset_latency soft_reset_noclk stall_drop tpg
               core_pulse_lat0 core_pulse_lat1 core_pulse_lat20 zero_wc dma_sensor dma_tpg dma_stream_backlog drv_camera drv_recover)
    add_test(NAME fx1_csi2_rxphy_${case} COMMAND test_fx1_csi2_rx_phy ${case})
    set_tests_properties(fx1_csi2_rxphy_${case} PROPERTIES TIMEOUT 600 LABELS "fx1_csi2;fx1_phy_slow")
  endforeach()
  # Throughput benchmark: label fx1_bench, outside every fx1_csi2* label (ctest -L is a regex).
  add_test(NAME fx1_csi2_rxphy_bench_640x480 COMMAND test_fx1_csi2_rx_phy bench_640x480)
  set_tests_properties(fx1_csi2_rxphy_bench_640x480 PROPERTIES TIMEOUT 3600 LABELS "fx1_bench")

  add_executable(test_fx1_csi2_controller tests/test_csi2_controller.cpp)
  target_include_directories(test_fx1_csi2_controller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(test_fx1_csi2_controller PRIVATE fx1_csi2_controller fx1_csi2_dphy_bfm)
  foreach(case csr_reset csr_access csr_partial csr_reserved irq counters gsp timestamp soft_reset_scope
               reset_drain reset_drain_resetn full_reset ownership hw_guard)
    add_test(NAME fx1_csi2_ctrl_${case} COMMAND test_fx1_csi2_controller ${case})
    set_tests_properties(fx1_csi2_ctrl_${case} PROPERTIES TIMEOUT 120 LABELS "fx1_csi2;fx1_csi2_unit")
  endforeach()
  foreach(case phy_bringup phy_events)
    add_test(NAME fx1_csi2_ctrl_${case} COMMAND test_fx1_csi2_controller ${case})
    set_tests_properties(fx1_csi2_ctrl_${case} PROPERTIES TIMEOUT 600 LABELS "fx1_csi2;fx1_phy_slow")
  endforeach()

  # Driver timeout arithmetic against a fake HAL (no SystemC, fast).
  add_executable(test_fx1_csi2_driver_timeout tests/test_csi2_driver_timeout.cpp)
  target_link_libraries(test_fx1_csi2_driver_timeout PRIVATE fx1_csi2_drv)
  foreach(case zero below_poll max_poll10 max_poll_big poll_zero abort_in_budget abort_phase2_late abort_zero
               abort_below_poll phy_start_late phy_start_in_budget)
    add_test(NAME fx1_csi2_drv_timeout_${case} COMMAND test_fx1_csi2_driver_timeout ${case})
    set_tests_properties(fx1_csi2_drv_timeout_${case} PROPERTIES TIMEOUT 60 LABELS "fx1_csi2;fx1_csi2_unit")
  endforeach()

  add_executable(test_fx1_csi2_driver tests/test_csi2_driver.cpp)
  target_link_libraries(test_fx1_csi2_driver PRIVATE fx1_csi2_controller fx1_csi2_drv fx1_csi2_dphy_bfm)
  foreach(case probe probe_bad_version timeouts irq_ts_gsp)
    add_test(NAME fx1_csi2_drv_${case} COMMAND test_fx1_csi2_driver ${case})
    set_tests_properties(fx1_csi2_drv_${case} PROPERTIES TIMEOUT 120 LABELS "fx1_csi2;fx1_csi2_unit")
  endforeach()
  foreach(case bringup4 lanes_max2 no_sensor)
    add_test(NAME fx1_csi2_drv_${case} COMMAND test_fx1_csi2_driver ${case})
    set_tests_properties(fx1_csi2_drv_${case} PROPERTIES TIMEOUT 600 LABELS "fx1_csi2;fx1_phy_slow")
  endforeach()

  add_executable(test_fx1_dphy_hs tests/test_phy_hs_receive.cpp)
  target_link_libraries(test_fx1_dphy_hs PRIVATE fx1_mipi_dphy_wrapper fx1_csi2_dphy_bfm)
  foreach(case 1lane 2lane 4lane skew lead4 lead4_link sot_err1 sot_err2 corrupt multi phase)
    add_test(NAME fx1_dphy_hs_${case} COMMAND test_fx1_dphy_hs ${case})
    set_tests_properties(fx1_dphy_hs_${case} PROPERTIES TIMEOUT 600 LABELS "fx1_csi2;fx1_phy_slow")
  endforeach()
endif()

# One target that builds every library and test of this component; needed in
# the CDC-VP tree, where the directory is EXCLUDE_FROM_ALL.
set(_csi2_all_deps fx1_mipi_dphy fx1_mipi_dphy_wrapper fx1_csi2_bfm fx1_csi2_dphy_bfm fx1_csi2_rx_core fx1_csi2_controller fx1_csi2_drv
    fx1_csi2_sensor)
if(FX1_MIPI_CSI2_BUILD_TESTS)
  list(APPEND _csi2_all_deps test_fx1_csi2_packet test_fx1_dphy_startup test_fx1_dphy_hs test_fx1_csi2_controller test_fx1_csi2_driver test_fx1_csi2_driver_timeout
       test_fx1_csi2_lane_packet test_fx1_csi2_sys_datapath test_fx1_csi2_dma test_fx1_csi2_dma_ctrl test_fx1_csi2_sensor test_fx1_dphy_pll_gate test_fx1_csi2_rx_phy)
endif()
add_custom_target(fx1_mipi_csi2_all)
# Executables of the fast tests (label fx1_csi2_unit, no HS traffic through the
# detailed PHY): what the fx1_soc CI gate builds and runs (M5b).
if(FX1_MIPI_CSI2_BUILD_TESTS)
  add_custom_target(fx1_csi2_unit_tests)
  add_dependencies(fx1_csi2_unit_tests test_fx1_csi2_packet test_fx1_dphy_startup test_fx1_csi2_lane_packet
                   test_fx1_csi2_sys_datapath test_fx1_csi2_dma test_fx1_csi2_dma_ctrl test_fx1_csi2_controller
                   test_fx1_csi2_driver_timeout test_fx1_csi2_driver)
endif()
add_dependencies(fx1_mipi_csi2_all ${_csi2_all_deps})
