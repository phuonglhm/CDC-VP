# FX1 SoC platform (CDC_BUILD_FX1_SOC). Host side: the simulator executable.
# Firmware side: fw/fx1_soc, built with the RISC-V cross toolchain when one is
# found, and registered as ctest cases that run on this simulator.
# DDR3 subsystem glue (DDR3 VP subtask 5): controller wiring, DDR_SYSCTRL,
# boot modes, host image/fault interface. Shared by the simulator and its test.
add_library(fx1_soc_ddr3 STATIC
    src/ddr3_subsystem.cpp
    src/ddr_sysctrl.cpp)
target_include_directories(fx1_soc_ddr3 PUBLIC src)
target_compile_options(fx1_soc_ddr3 PRIVATE -Wall -Wextra)
target_link_libraries(fx1_soc_ddr3 PUBLIC
    SystemC::systemc
    cdc::components::fx1_map
    cdc::components::fx1_ddr3)

add_executable(fx1_soc
    src/main.cpp
    src/fx1_soc_top.cpp
    src/isp_stream_bridge.cpp)

# `fx1_soc --version`: the package name, the CDC-VP revision and tree state at
# configure time, the build type and compilers (cmake/build_info.h.in).
set(FX1_SOC_PACKAGE_NAME "VP_FX1_V3.0" CACHE STRING "SDK package name the fx1_soc binary reports")
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
                OUTPUT_VARIABLE FX1_SOC_GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" status --porcelain --untracked-files=no
                OUTPUT_VARIABLE _fx1_git_status OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
                RESULT_VARIABLE _fx1_git_rc)
if(NOT FX1_SOC_GIT_REVISION OR NOT _fx1_git_rc EQUAL 0)
    set(FX1_SOC_GIT_REVISION "unknown")
    set(FX1_SOC_GIT_STATE "unknown")
elseif(_fx1_git_status)
    set(FX1_SOC_GIT_STATE "dirty")
else()
    set(FX1_SOC_GIT_STATE "clean")
endif()
if(CMAKE_BUILD_TYPE)
    set(FX1_SOC_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
    set(FX1_SOC_BUILD_TYPE "none (developer build, not for packaging)")
endif()
# The NPU model drop the binary carries: SHA-256 of its package_manifest.txt
# (components/FX1_Components/npu), empty when the NPU is not built.
if(FX1_SOC_ENABLE_NPU AND FX1_NPU_MODEL_MANIFEST_SHA256)
    set(FX1_SOC_NPU_MODEL_SHA256 "${FX1_NPU_MODEL_MANIFEST_SHA256}")
else()
    set(FX1_SOC_NPU_MODEL_SHA256 "")
endif()
configure_file(cmake/build_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/fx1_soc_build_info.h @ONLY)

target_include_directories(fx1_soc PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_options(fx1_soc PRIVATE -Wall -Wextra)
target_link_libraries(fx1_soc PRIVATE
    SystemC::systemc
    cdc::cpu::riscv_vp_plusplus
    cdc::components::bus_system
    cdc::components::fx1_map
    cdc::components::fx1_sparse_ram
    cdc::components::fx1_access_guard
    cdc::components::fx1_clint
    cdc::components::fx1_plic
    cdc::components::fx1_cpu_port
    cdc::components::fx1_exclusive_monitor
    cdc::components::fx1_sim_control
    cdc::components::fx1_dma
    cdc::components::fx1_isp_tlm
    cdc::components::uart2_tlm
    cdc::components::uart_host_tlm
    cdc::components::i2c_tlm
    cdc::components::fx1_ethernet
    eth_phy_tlm
    fx1_soc_ddr3)
# --camera: present when the MIPI CSI-2 component is configured
# (-DFX1_BUILD_MIPI_CSI2=ON). Its D-PHY netlist adds ~45 s of build at -j8.
if(TARGET cdc::components::fx1_csi2_controller)
    target_link_libraries(fx1_soc PRIVATE
        cdc::components::fx1_csi2_controller
        cdc::components::fx1_mipi_dphy_wrapper
        cdc::components::fx1_csi2_dphy_bfm
        cdc::components::fx1_csi2_sensor)
    target_compile_definitions(fx1_soc PRIVATE FX1_SOC_HAS_CAMERA=1)
endif()
# NPU: present when components/FX1_Components/npu is configured
# (FX1_SOC_ENABLE_NPU=ON, the default); `fx1_soc --no-npu` removes it at run time.
if(TARGET cdc::components::fx1_npu)
    target_link_libraries(fx1_soc PRIVATE cdc::components::fx1_npu)
    target_compile_definitions(fx1_soc PRIVATE FX1_SOC_HAS_NPU=1)
endif()
# Self-contained like the other packaged platforms (cmake/modules/CdcPortable.cmake):
# RPATH=$ORIGIN, and the SystemC runtime copied beside the binary. The SDK
# package (package/package_sdk.sh, target fx1_soc_package) ships exactly that.
cdc_make_portable(fx1_soc)

# Negative integration harness (review G2-R2): real VP++ hart + silent target.
# Its firmware image and ctest registration live in fw/fx1_soc.
add_executable(test_fx1_cpu_silent_target tests/test_cpu_silent_target.cpp)
target_compile_options(test_fx1_cpu_silent_target PRIVATE -Wall -Wextra)
target_link_libraries(test_fx1_cpu_silent_target PRIVATE
    SystemC::systemc
    cdc::cpu::riscv_vp_plusplus
    cdc::components::bus_system
    cdc::components::fx1_map
    cdc::components::fx1_sparse_ram
    cdc::components::fx1_cpu_port
    cdc::components::fx1_sim_control)

# Plan C7 (review G4b-R1): a real VP++ hart with the real exclusive monitor;
# an LR whose load faults must leave no reservation for the SC that follows.
add_executable(test_fx1_faulting_lr tests/test_faulting_lr.cpp)
target_compile_options(test_fx1_faulting_lr PRIVATE -Wall -Wextra)
target_link_libraries(test_fx1_faulting_lr PRIVATE
    SystemC::systemc
    cdc::cpu::riscv_vp_plusplus
    cdc::components::fx1_cpu_port
    cdc::components::fx1_exclusive_monitor)
if(CDC_BUILD_TESTS)
    add_test(NAME fx1_faulting_lr COMMAND test_fx1_faulting_lr)
    set_tests_properties(fx1_faulting_lr PROPERTIES TIMEOUT 60 LABELS "fx1;fx1_unit")
endif()

# DDR3 subtask 5: DDR_SYSCTRL registers, P-channel and reset sequencing, the
# preinit order (scrub -> preload -> release), firmware-boot staging refusal,
# dump, the --ddr3-fault parser, and the firmware reference image
# (fw/fx1_soc/ddr3/ddr3_ref_1600.h) against the model's.
add_executable(test_fx1_ddr3_subsystem tests/test_ddr3_subsystem.cpp)
target_compile_options(test_fx1_ddr3_subsystem PRIVATE -Wall -Wextra)
target_include_directories(test_fx1_ddr3_subsystem PRIVATE ${CMAKE_SOURCE_DIR}/fw/fx1_soc/ddr3)
target_link_libraries(test_fx1_ddr3_subsystem PRIVATE fx1_soc_ddr3)
if(CDC_BUILD_TESTS)
    foreach(_case sysctrl power reset preinit firmware parser refimage wake_recovery wake_busy canonical)
        add_test(NAME fx1_ddr3_subsystem_${_case} COMMAND test_fx1_ddr3_subsystem ${_case})
        set_tests_properties(fx1_ddr3_subsystem_${_case} PROPERTIES TIMEOUT 120 LABELS "fx1;fx1_unit;ddr3")
    endforeach()
endif()

# CSI-2 M6a follow-up review R2: budget contract of the BSP I2C driver (built
# for the host with tests/hostshim/fx1_fw.h) against the real I2C0 model.
add_library(fx1_i2c_bsp_host STATIC ${CMAKE_SOURCE_DIR}/fw/fx1_soc/drivers/fx1_i2c.c)
target_include_directories(fx1_i2c_bsp_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests/hostshim
                                                   ${CMAKE_SOURCE_DIR}/fw/fx1_soc/drivers)
target_link_libraries(fx1_i2c_bsp_host PUBLIC cdc::components::fx1_map)
set_target_properties(fx1_i2c_bsp_host PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(fx1_i2c_bsp_host PRIVATE -Wall -Wextra -Werror)
add_executable(test_fx1_i2c_bsp_budget tests/test_i2c_bsp_budget.cpp)
target_compile_options(test_fx1_i2c_bsp_budget PRIVATE -Wall -Wextra)
target_link_libraries(test_fx1_i2c_bsp_budget PRIVATE fx1_i2c_bsp_host cdc::components::i2c_tlm SystemC::systemc)
if(CDC_BUILD_TESTS)
    add_test(NAME fx1_i2c_bsp_budget COMMAND test_fx1_i2c_bsp_budget)
    set_tests_properties(fx1_i2c_bsp_budget PROPERTIES TIMEOUT 120 LABELS "fx1;fx1_unit")
endif()
# Ethernet gap G6 (review): a task preempted between a status read and the
# deadline check must not abort a transaction that completed meanwhile.
add_executable(test_fx1_i2c_bsp_preempt tests/test_i2c_bsp_preempt.cpp)
target_compile_options(test_fx1_i2c_bsp_preempt PRIVATE -Wall -Wextra)
target_link_libraries(test_fx1_i2c_bsp_preempt PRIVATE fx1_i2c_bsp_host cdc::components::i2c_tlm SystemC::systemc)
if(CDC_BUILD_TESTS)
    add_test(NAME fx1_i2c_bsp_preempt COMMAND test_fx1_i2c_bsp_preempt)
    set_tests_properties(fx1_i2c_bsp_preempt PROPERTIES TIMEOUT 120 LABELS "fx1;fx1_unit")
endif()

# Ethernet review ER01-ER04: the FX1 manager (fw/fx1_soc/drivers/fx1_eth.c,
# built for the host) with an ISR thread and a service thread shaped like the
# FreeRTOS owner, on the MAC model through the driver bench of the component.
if(TARGET eth_vp_support)
    add_library(fx1_eth_host STATIC ${CMAKE_SOURCE_DIR}/fw/fx1_soc/drivers/fx1_eth.c)
    target_include_directories(fx1_eth_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests/hostshim
                                                   ${CMAKE_SOURCE_DIR}/fw/fx1_soc/drivers)
    target_link_libraries(fx1_eth_host PUBLIC eth_drv cdc::components::fx1_map)
    set_target_properties(fx1_eth_host PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_compile_options(fx1_eth_host PRIVATE -Wall -Wextra -Werror)
    add_executable(test_fx1_eth_manager tests/test_eth_manager.cpp)
    target_compile_options(test_fx1_eth_manager PRIVATE -Wall -Wextra)
    target_include_directories(test_fx1_eth_manager PRIVATE
                               ${CMAKE_SOURCE_DIR}/components/FX1_Components/ethernet/tests)
    target_link_libraries(test_fx1_eth_manager PRIVATE fx1_eth_host cdc::components::fx1_ethernet eth_phy_tlm
                                                       eth_vp_support SystemC::systemc)
    if(CDC_BUILD_TESTS)
        foreach(_m error-tx error-rx soft soft-halt wb-deadline wb-wrap stop-flush stats faulted)
            add_test(NAME fx1_eth_manager_${_m} COMMAND test_fx1_eth_manager ${_m})
            set_tests_properties(fx1_eth_manager_${_m} PROPERTIES TIMEOUT 120 LABELS "fx1;fx1_unit"
                PASS_REGULAR_EXPRESSION "=== fx1_eth manager ${_m}: [0-9]+ checks, 0 failures ===")
        endforeach()
    endif()
endif()

# Direct CSI->ISP D2a review F3: TrafficCounter counts at issue, returns and
# outstanding; the host checker of the camera_direct tests must refuse a report
# with RAW traffic issued but not returned (pending), and one in the old format.
add_executable(test_fx1_traffic_counter tests/test_traffic_counter.cpp)
target_compile_options(test_fx1_traffic_counter PRIVATE -Wall -Wextra)
target_link_libraries(test_fx1_traffic_counter PRIVATE SystemC::systemc)
if(CDC_BUILD_TESTS)
    set(_tc_dir ${CMAKE_CURRENT_BINARY_DIR}/traffic_counter)
    file(MAKE_DIRECTORY ${_tc_dir})
    add_test(NAME fx1_unit_traffic_counter COMMAND test_fx1_traffic_counter ${_tc_dir})
    set_tests_properties(fx1_unit_traffic_counter PROPERTIES TIMEOUT 60 LABELS "fx1;fx1_unit"
        PASS_REGULAR_EXPRESSION "PASS \\(0 failures\\)" FIXTURES_SETUP fx1_traffic_reports)
    find_program(FX1_PYTHON3 python3)
    if(FX1_PYTHON3)
        file(WRITE ${_tc_dir}/old_format.txt
            "MIPI_DMA reads=0 read_bytes=0 read_errors=0 writes=0 write_bytes=0 write_errors=0\n"
            "ISP_IDMA reads=0 read_bytes=0 read_errors=0 writes=0 write_bytes=0 write_errors=0\n"
            "ISP_ODMA reads=0 read_bytes=0 read_errors=0 writes=1 write_bytes=8 write_errors=0 write_lo=0x88000000 write_hi=0x88000007\n")
        set(_chk ${FX1_PYTHON3} ${CMAKE_SOURCE_DIR}/fw/fx1_soc/tests/camera_direct/check_direct_traffic.py
            --odma-lo 0x88000000 --odma-hi 0x8803ffff)
        foreach(_c "pending|1|MIPI_DMA: 1 transactions outstanding.*ISP_IDMA: 1 transactions outstanding.*MIPI_DMA: 0 reads, 1 writes issued.*ISP_IDMA: 1 reads, 0 writes issued"
                   "done|1|MIPI_DMA: 0 reads, 1 writes issued.*ISP_IDMA: 1 reads.*ISP_ODMA: 1 write errors"
                   "old_format|1|report without read_done"
                   "clean|0|direct traffic: MIPI_DMA 0 writes, ISP_IDMA 0 reads, ISP_ODMA 1 writes")
            string(REPLACE "|" ";" _c "${_c}")
            list(GET _c 0 _n)
            list(GET _c 1 _rc)
            list(GET _c 2 _re)
            string(REPLACE ";" "\\;" _cmd "${_chk};${_tc_dir}/${_n}.txt")
            add_test(NAME fx1_unit_traffic_checker_${_n}
                     COMMAND ${CMAKE_COMMAND} "-DCMD=${_cmd}" -DRC=${_rc} "-DREGEX=${_re}"
                             -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_rc.cmake)
            set_tests_properties(fx1_unit_traffic_checker_${_n} PROPERTIES TIMEOUT 60 LABELS "fx1;fx1_unit")
            if(NOT _n STREQUAL "old_format")
                set_tests_properties(fx1_unit_traffic_checker_${_n} PROPERTIES FIXTURES_REQUIRED fx1_traffic_reports)
            endif()
        endforeach()
        # D2b + review F1/F2: direct + RAW dump mode (--mipi-window per side,
        # --mipi-errors). Reports from test_fx1_csi_dma_traffic (product counter
        # and fault shim): fault (counter before the shim), lost (the refused
        # topology, shim first), clean, gap (write between the sides), cross
        # (burst past the end of a side); hand-written: IDMA read, no MIPI
        # write, a counter without windows.
        set(_cd_dir ${_tc_dir}/csi_dma)
        file(MAKE_DIRECTORY ${_cd_dir})
        set(_ok_isp "ISP_IDMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=0 write_bytes=0 write_done=0 write_errors=0 outstanding=0\n"
                    "ISP_ODMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=1 write_bytes=8 write_done=1 write_errors=0 write_lo=0x88000000 write_hi=0x88000007 outstanding=0\n")
        set(_win " windows=0x81000000-0x81000fff,0x81010000-0x81010fff outside=0")
        file(WRITE ${_cd_dir}/idma.txt
            "MIPI_DMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=2 write_bytes=16 write_done=2 write_errors=0 write_lo=0x81000000 write_hi=0x81010007 outstanding=0${_win}\n"
            "ISP_IDMA reads=1 read_bytes=8 read_done=1 read_errors=0 read_lo=0x81000000 read_hi=0x81000007 writes=0 write_bytes=0 write_done=0 write_errors=0 outstanding=0\n"
            "ISP_ODMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=1 write_bytes=8 write_done=1 write_errors=0 write_lo=0x88000000 write_hi=0x88000007 outstanding=0\n")
        file(WRITE ${_cd_dir}/none.txt
            "MIPI_DMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=0 write_bytes=0 write_done=0 write_errors=0 outstanding=0${_win}\n"
            ${_ok_isp})
        file(WRITE ${_cd_dir}/seen_mismatch.txt
            "MIPI_DMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=3 write_bytes=24 write_done=3 write_errors=1 write_lo=0x81000000 write_hi=0x81010007 outstanding=0${_win}\n"
            ${_ok_isp}
            "[fx1_soc] camera fault (injection): csi dma write-error:2, 4 MIPI_DMA write(s) seen, fired at #2 address 0x81000080 at 0 ns (error response)\n")
        # One envelope over both sides and the gap: outside 0 proves nothing (review F2).
        file(WRITE ${_cd_dir}/wrong_windows.txt
            "MIPI_DMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=3 write_bytes=24 write_done=3 write_errors=0 write_lo=0x81000000 write_hi=0x81010007 outstanding=0 windows=0x81000000-0x81010fff outside=0\n"
            ${_ok_isp})
        file(WRITE ${_cd_dir}/no_windows.txt
            "MIPI_DMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=2 write_bytes=16 write_done=2 write_errors=0 write_lo=0x81000000 write_hi=0x81010007 outstanding=0\n"
            ${_ok_isp})
        add_executable(test_fx1_csi_dma_traffic tests/test_csi_dma_traffic.cpp)
        target_compile_options(test_fx1_csi_dma_traffic PRIVATE -Wall -Wextra)
        target_link_libraries(test_fx1_csi_dma_traffic PRIVATE SystemC::systemc)
        add_test(NAME fx1_unit_csi_dma_traffic COMMAND test_fx1_csi_dma_traffic ${_cd_dir})
        set_tests_properties(fx1_unit_csi_dma_traffic PROPERTIES TIMEOUT 60 LABELS "fx1;fx1_unit"
            PASS_REGULAR_EXPRESSION "PASS \\(0 failures\\)" FIXTURES_SETUP fx1_csi_dma_reports)
        set(_chk_dump ${_chk} --mipi-window 0x81000000:0x81000fff --mipi-window 0x81010000:0x81010fff)
        # name|report|E1 = --mipi-errors 1|exit status|output pattern
        foreach(_c "dump_fault|fault|E1|0|direct traffic: MIPI_DMA 3 writes"
                   "dump_fault_unexpected|fault||1|MIPI_DMA: 1 write errors, expected 0"
                   "dump_lost|lost|E1|1|MIPI_DMA: 0 write errors, expected 1.*the fault shim saw 3 writes, the counter 2"
                   "dump_not_fired|clean|E1|1|an injected error is expected but the console has no csi dma fault report"
                   "dump_clean|clean||0|direct traffic: MIPI_DMA 3 writes, ISP_IDMA 0 reads"
                   "dump_gap|gap||1|MIPI_DMA: 1 transactions not wholly inside one RAW side window"
                   "dump_cross|cross||1|MIPI_DMA: 1 transactions not wholly inside one RAW side window"
                   "dump_seen_mismatch|seen_mismatch|E1|1|MIPI_DMA: the fault shim saw 4 writes, the counter 3"
                   "dump_wrong_windows|wrong_windows||1|MIPI_DMA: counter windows 0x81000000-0x81010fff are not 0x81000000-0x81000fff,0x81010000-0x81010fff"
                   "dump_no_windows|no_windows||1|MIPI_DMA: counter windows none are not"
                   "dump_idma|idma||1|ISP_IDMA: 1 reads, 0 writes issued"
                   "dump_none|none||1|MIPI_DMA: no write \\(RAW dump expected\\)")
            string(REPLACE "|" ";" _c "${_c}")
            list(GET _c 0 _n)
            list(GET _c 1 _f)
            list(GET _c 2 _e)
            list(GET _c 3 _rc)
            list(GET _c 4 _re)
            set(_args ${_chk_dump})
            if(_e STREQUAL "E1")
                list(APPEND _args --mipi-errors 1)
            endif()
            string(REPLACE ";" "\;" _cmd "${_args};${_cd_dir}/${_f}.txt")
            add_test(NAME fx1_unit_traffic_checker_${_n}
                     COMMAND ${CMAKE_COMMAND} "-DCMD=${_cmd}" -DRC=${_rc} "-DREGEX=${_re}"
                             -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_rc.cmake)
            set_tests_properties(fx1_unit_traffic_checker_${_n} PROPERTIES TIMEOUT 60 LABELS "fx1;fx1_unit")
            if(_f STREQUAL "fault" OR _f STREQUAL "lost" OR _f STREQUAL "clean" OR _f STREQUAL "gap" OR _f STREQUAL "cross")
                set_tests_properties(fx1_unit_traffic_checker_${_n} PROPERTIES FIXTURES_REQUIRED fx1_csi_dma_reports)
            endif()
        endforeach()
        # Review D4 F1: RAW sentinel (raw_sentinel.py; a 64 KiB region at
        # 0x81000000 here). Inputs from `raw_sentinel.py fixtures`, one dump per
        # test named after it (each check removes the dump it read: a shared one
        # made the second test fail in any order, re-review D4 R1): clean, a CPU
        # write (the reviewer's probe), zeros, a write inside side 0 (direct
        # only / RAW dump), one inside plus one byte past side 0, a short dump,
        # none. The fixture recreates them for every run.
        set(_rs_py ${CMAKE_SOURCE_DIR}/fw/fx1_soc/tests/camera_direct/raw_sentinel.py)
        set(_rs_dir ${_tc_dir}/raw_sentinel)
        file(MAKE_DIRECTORY ${_rs_dir})
        file(WRITE ${_rs_dir}/direct.txt
            "MIPI_DMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=0 write_bytes=0 write_done=0 write_errors=0 outstanding=0\n"
            ${_ok_isp})
        file(WRITE ${_rs_dir}/dump.txt
            "MIPI_DMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=2 write_bytes=16 write_done=2 write_errors=0 write_lo=0x81000000 write_hi=0x81010007 outstanding=0${_win}\n"
            ${_ok_isp})
        add_test(NAME fx1_unit_raw_sentinel_fixtures COMMAND ${FX1_PYTHON3} ${_rs_py} fixtures ${_rs_dir})
        set_tests_properties(fx1_unit_raw_sentinel_fixtures PROPERTIES TIMEOUT 60 LABELS "fx1;fx1_unit"
            FIXTURES_SETUP fx1_raw_sentinel_inputs)
        add_test(NAME fx1_unit_raw_sentinel_selftest COMMAND ${FX1_PYTHON3} ${_rs_py} selftest ${_rs_dir}/selftest)
        set_tests_properties(fx1_unit_raw_sentinel_selftest PROPERTIES TIMEOUT 120 LABELS "fx1;fx1_unit"
            PASS_REGULAR_EXPRESSION "raw_sentinel selftest: PASS \\(0 failures\\)")
        set(_rs_args --raw-base 0x81000000 --raw-size 0x10000 --raw-pattern ${_rs_dir}/pattern.bin)
        # name (= its dump)|console (direct / dump: + side windows)|exit status|output pattern
        foreach(_c "clean|direct|0|RAW sentinel: 0x81000000\\+0x10000 unchanged outside the windows"
                   "cpu_write|direct|1|RAW sentinel: 4 bytes changed outside the allowed windows: 0x81008000-0x81008003"
                   "zeros|direct|1|RAW sentinel: 64 bytes changed outside the allowed windows: 0x81002000-0x8100203f"
                   "side_direct|direct|1|RAW sentinel: 255 bytes changed outside the allowed windows: 0x81000100-"
                   "side_dump|dump|0|unchanged outside the windows. 255 bytes changed inside 2 window"
                   "side_past_end|dump|1|RAW sentinel: 1 bytes changed outside the allowed windows: 0x81001000-0x81001000"
                   "short|direct|1|RAW sentinel: dump is 65504 bytes, expected 65536"
                   "missing|direct|1|RAW sentinel: no dump")
            string(REPLACE "|" ";" _c "${_c}")
            list(GET _c 0 _n)
            list(GET _c 1 _con)
            list(GET _c 2 _rc)
            list(GET _c 3 _re)
            set(_d ${_n})
            set(_args ${_chk})
            if(_con STREQUAL "dump")
                list(APPEND _args --mipi-window 0x81000000:0x81000fff --mipi-window 0x81010000:0x81010fff)
            endif()
            list(APPEND _args ${_rs_args} --raw-dump ${_rs_dir}/${_d}.bin)
            string(REPLACE ";" "\;" _cmd "${_args};${_rs_dir}/${_con}.txt")
            add_test(NAME fx1_unit_traffic_checker_raw_${_n}
                     COMMAND ${CMAKE_COMMAND} "-DCMD=${_cmd}" -DRC=${_rc} "-DREGEX=${_re}"
                             -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_rc.cmake)
            set_tests_properties(fx1_unit_traffic_checker_raw_${_n} PROPERTIES TIMEOUT 60 LABELS "fx1;fx1_unit"
                FIXTURES_REQUIRED fx1_raw_sentinel_inputs)
        endforeach()
    endif()
endif()

# ── CI aggregate (plan P5) ─────────────────────────────────────────────────────
# `fx1_all` builds everything the FX1 tests need (simulator, harnesses, FX1
# component and VP++ backend targets, firmware images) and nothing else of the
# tree: `components/isp_tlm` is added unconditionally upstream and does not
# link today, so `cmake --build` of the default target fails for reasons
# unrelated to FX1. The list is collected once the whole tree is configured.
add_custom_target(fx1_all)
function(_fx1_collect_targets out dir)
    # A subtree added with add_subdirectory(... EXCLUDE_FROM_ALL) is opt-in
    # (e.g. the MIPI CSI-2 D-PHY netlist) and stays out of fx1_all as well.
    get_property(_excluded DIRECTORY "${dir}" PROPERTY EXCLUDE_FROM_ALL)
    if(_excluded)
        set(${out} "" PARENT_SCOPE)
        return()
    endif()
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    set(_list "")
    foreach(_t IN LISTS _targets)
        get_target_property(_type ${_t} TYPE)
        # fx1_soc_package consumes fx1_all, it is not part of it.
        if(NOT _type STREQUAL "INTERFACE_LIBRARY" AND NOT _t STREQUAL "fx1_all" AND NOT _t STREQUAL "fx1_soc_package")
            list(APPEND _list ${_t})
        endif()
    endforeach()
    foreach(_s IN LISTS _subdirs)
        _fx1_collect_targets(_sub "${_s}")
        list(APPEND _list ${_sub})
    endforeach()
    set(${out} ${_list} PARENT_SCOPE)
endfunction()
function(_fx1_finish_all)
    set(_dirs "${CMAKE_SOURCE_DIR}/components/FX1_Components"
              "${CMAKE_SOURCE_DIR}/components/uart2_tlm"
              "${CMAKE_SOURCE_DIR}/components/uart_host_tlm"
              "${CMAKE_SOURCE_DIR}/cpu_models/riscv_vp_plusplus"
              "${CMAKE_SOURCE_DIR}/platforms/fx1_soc")
    if(FX1_SOC_BUILD_FIRMWARE)
        list(APPEND _dirs "${CMAKE_SOURCE_DIR}/fw/fx1_soc")
    endif()
    set(_all "")
    foreach(_d IN LISTS _dirs)
        _fx1_collect_targets(_found "${_d}")
        list(APPEND _all ${_found})
    endforeach()
    add_dependencies(fx1_all ${_all})
endfunction()
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL _fx1_finish_all)

# ── Firmware (cross-compiled; never uses the host CC/CXX) ─────────────────────
# FX1_SOC_BUILD_FIRMWARE=ON (default) is the acceptance configuration: firmware
# and the fx1_smoke tests are mandatory, so a missing or non-working cross
# compiler is a configure error. OFF builds the simulator and unit tests only
# and is explicitly not an acceptance configuration.
option(FX1_SOC_BUILD_FIRMWARE
    "Build FX1 firmware and register fx1_smoke tests (requires a RISC-V cross toolchain)" ON)
set(FX1_RISCV_PREFIX "" CACHE STRING
    "Path prefix of the RISC-V bare-metal toolchain, e.g. /opt/toolchains/riscv-none-elf/bin/riscv-none-elf-")

if(NOT FX1_SOC_BUILD_FIRMWARE)
    message(WARNING
        "fx1_soc: FX1_SOC_BUILD_FIRMWARE=OFF - host-only build. Firmware and the "
        "fx1_smoke tests are not built; this is NOT an FX1 acceptance configuration.")
    return()
endif()

if(NOT FX1_RISCV_PREFIX)
    find_program(_fx1_riscv_gcc riscv-none-elf-gcc
        HINTS "$ENV{RISCV_HOME}/bin" /opt/toolchains/riscv-none-elf/bin
        NO_DEFAULT_PATH)
    if(NOT _fx1_riscv_gcc)
        message(FATAL_ERROR
            "fx1_soc: FX1_SOC_BUILD_FIRMWARE=ON but no riscv-none-elf-gcc was found in "
            "$RISCV_HOME/bin or /opt/toolchains/riscv-none-elf/bin. Set "
            "FX1_RISCV_PREFIX=<dir>/riscv-none-elf- or configure with "
            "-DFX1_SOC_BUILD_FIRMWARE=OFF for a host-only (non-acceptance) build.")
    endif()
    string(REGEX REPLACE "gcc$" "" _fx1_prefix "${_fx1_riscv_gcc}")
    set(FX1_RISCV_PREFIX "${_fx1_prefix}" CACHE STRING "" FORCE)
endif()

# An explicit or discovered prefix must name a compiler that runs.
execute_process(COMMAND "${FX1_RISCV_PREFIX}gcc" --version
    RESULT_VARIABLE _fx1_gcc_status OUTPUT_VARIABLE _fx1_gcc_version ERROR_QUIET)
if(NOT _fx1_gcc_status EQUAL 0)
    message(FATAL_ERROR
        "fx1_soc: FX1_RISCV_PREFIX='${FX1_RISCV_PREFIX}' does not name a working "
        "compiler (${FX1_RISCV_PREFIX}gcc --version failed). Fix the prefix or "
        "configure with -DFX1_SOC_BUILD_FIRMWARE=OFF for a host-only build.")
endif()
string(REGEX MATCH "^[^\n]*" _fx1_gcc_version "${_fx1_gcc_version}")
message(STATUS "fx1_soc: firmware toolchain ${FX1_RISCV_PREFIX}gcc (${_fx1_gcc_version})")
add_subdirectory(${CMAKE_SOURCE_DIR}/fw/fx1_soc ${CMAKE_BINARY_DIR}/fw/fx1_soc)

# ── SDK package VP_FX1_V3.0 (G6b) ────────────────────────────────────────────
# `cmake --build <dir> --target fx1_soc_package` assembles the delivery in the
# SDK repository layout under ${CDC_PACKAGE_ROOT}/VP_FX1_V3.0 (package/README
# in package_sdk.sh). The default ISP sample is the cleared D65 frame; with
# FX1_DEMO_RAW set, the outdoor1 frame is added as a second, internal-only one.
set(FX1_SOC_PACKAGE_DIR "${CDC_PACKAGE_ROOT}/VP_FX1_V3.0")
get_filename_component(FX1_RISCV_TOOLDIR "${FX1_RISCV_PREFIX}gcc" DIRECTORY)
set(_fx1_samples --sample "d65=${CMAKE_SOURCE_DIR}/components/isp_tlm/input/D65_raw_2688x1520_5376.raw")
if(FX1_DEMO_RAW AND EXISTS "${FX1_DEMO_RAW}")
    list(APPEND _fx1_samples --sample
        "outdoor1=${FX1_DEMO_RAW},internal")
endif()
add_custom_target(fx1_soc_package
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/package/package_sdk.sh" --build-dir "${CMAKE_BINARY_DIR}"
            --out "${FX1_SOC_PACKAGE_DIR}" --tooldir "${FX1_RISCV_TOOLDIR}" ${_fx1_samples}
    COMMENT "Packaging VP_FX1_V3.0 -> ${FX1_SOC_PACKAGE_DIR}"
    USES_TERMINAL
    VERBATIM)
add_dependencies(fx1_soc_package fx1_all)
if(CDC_BUILD_TESTS)
    # The distribution gate: fresh Release build, package, move, run with an
    # empty environment, rebuild the firmware from the SDK. Slow; not part of
    # run_ci.sh. Label fx1_packaging.
    add_test(NAME fx1_packaging COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/package/run_packaging_regression.sh")
    set_tests_properties(fx1_packaging PROPERTIES
        LABELS "fx1;fx1_packaging" RUN_SERIAL TRUE TIMEOUT 3600
        ENVIRONMENT "FX1_DEMO_RAW=${FX1_DEMO_RAW};PACKAGING_SYSTEMC_HOME=${SYSTEMC_HOME}")
endif()
