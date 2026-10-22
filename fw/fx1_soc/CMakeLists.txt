# FX1 SoC firmware. Included from platforms/fx1_soc when a RISC-V bare-metal
# toolchain is found (FX1_RISCV_PREFIX). Every image is compiled with the cross
# compiler through custom commands, never with the host CC/CXX, and each test
# image is registered as a ctest case that runs on the fx1_soc simulator.

set(FX1_FW_MAP_INCLUDE "${CMAKE_SOURCE_DIR}/components/FX1_Components/fx1_map/include")
set(FX1_FW_COMMON "${CMAKE_CURRENT_SOURCE_DIR}/common")
# C register headers of FX1 IPs (dma/fx1_dma_regs.h, ...).
set(FX1_FW_IP_INCLUDES "${CMAKE_SOURCE_DIR}/components/FX1_Components/dma/include")
set(FX1_FW_CC "${FX1_RISCV_PREFIX}gcc")

# CVA6 configuration of the FX1 harts (plan D6). ilp32f: the toolchain's
# multilib for rv32imafc is rv32imafc_zicsr_zaamo_zalrsc/ilp32f.
set(FX1_FW_ARCH_FLAGS -march=rv32imafc_zicsr_zifencei -mabi=ilp32f -mcmodel=medany)
# --param=min-pagesize=0: BootROM and other MMIO live at low addresses; without
# it GCC treats a constant address below 4 KiB as null + offset (-Warray-bounds).
set(FX1_FW_CFLAGS ${FX1_FW_ARCH_FLAGS} -O2 -g -std=c11 -ffreestanding -fno-builtin
    --param=min-pagesize=0
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror
    -I${FX1_FW_COMMON} -I${FX1_FW_MAP_INCLUDE} -I${FX1_FW_IP_INCLUDES})

set(FX1_FW_LDS "${CMAKE_CURRENT_BINARY_DIR}/fx1.ld")
add_custom_command(
    OUTPUT ${FX1_FW_LDS}
    COMMAND ${FX1_FW_CC} -E -P -x c -DFX1_LINKER_SCRIPT -I${FX1_FW_MAP_INCLUDE}
            ${FX1_FW_COMMON}/fx1.ld.S -o ${FX1_FW_LDS}
    DEPENDS ${FX1_FW_COMMON}/fx1.ld.S ${FX1_FW_MAP_INCLUDE}/fx1/fx1_memory_map.h
    COMMENT "fx1 fw: linker script"
    VERBATIM)

set(FX1_FW_COMMON_SOURCES ${FX1_FW_COMMON}/crt0.S ${FX1_FW_COMMON}/fx1_fw.c)
set(FX1_FW_COMMON_DEPENDS ${FX1_FW_COMMON_SOURCES} ${FX1_FW_COMMON}/fx1_fw.h
    ${FX1_FW_MAP_INCLUDE}/fx1/fx1_memory_map.h ${FX1_FW_IP_INCLUDES}/dma/fx1_dma_regs.h ${FX1_FW_LDS})

# fx1_firmware(<name> SOURCES <files...> [DEFINES d...] [INCLUDES dir...] [DEPENDS f...]
#              [CFLAGS flag...])
#   -> ${CMAKE_CURRENT_BINARY_DIR}/<name>.elf
function(fx1_firmware name)
    cmake_parse_arguments(FW "" "" "SOURCES;DEFINES;INCLUDES;DEPENDS;CFLAGS" ${ARGN})
    set(elf "${CMAKE_CURRENT_BINARY_DIR}/${name}.elf")
    set(extra "")
    foreach(d ${FW_DEFINES})
        list(APPEND extra -D${d})
    endforeach()
    foreach(i ${FW_INCLUDES})
        list(APPEND extra -I${i})
    endforeach()
    list(APPEND extra ${FW_CFLAGS})
    add_custom_command(
        OUTPUT ${elf}
        COMMAND ${FX1_FW_CC} ${FX1_FW_CFLAGS} ${extra} -nostdlib -nostartfiles
                -T ${FX1_FW_LDS} -Wl,--gc-sections -Wl,--no-warn-rwx-segments
                ${FX1_FW_COMMON_SOURCES} ${FW_SOURCES} -lgcc -o ${elf}
        DEPENDS ${FX1_FW_COMMON_DEPENDS} ${FW_SOURCES} ${FW_DEPENDS}
        COMMENT "fx1 fw: ${name}.elf"
        VERBATIM)
    add_custom_target(fx1_fw_${name} ALL DEPENDS ${elf})
endfunction()

# ── DDR3 (DDR3 VP subtask 5) ────────────────────────────────────────────────
# ddr3/: reference bring-up and test helpers (not a driver); the C register
# headers come from the DDR3 IP (components/FX1_Components/ddr3/include) and
# fx1_map (fx1_ddr_sysctrl.h).
set(FX1_FW_DDR3 "${CMAKE_CURRENT_SOURCE_DIR}/ddr3")
set(FX1_FW_DDR3_REGS "${CMAKE_SOURCE_DIR}/components/FX1_Components/ddr3/include")
set(FX1_FW_DDR3_INCLUDES ${FX1_FW_DDR3} ${FX1_FW_DDR3_REGS})
set(FX1_FW_DDR3_SOURCES ${FX1_FW_DDR3}/ddr3_fw.c ${FX1_FW_DDR3}/ddr3_probe.S)
set(FX1_FW_DDR3_DEPENDS ${FX1_FW_DDR3}/ddr3_fw.h ${FX1_FW_DDR3}/ddr3_ref_1600.h
    ${FX1_FW_DDR3_REGS}/fx1_ddr3/fx1_ddr3_regs.h ${FX1_FW_DDR3_REGS}/fx1_ddr3/provisional_encodings.h
    ${FX1_FW_MAP_INCLUDE}/fx1/fx1_ddr_sysctrl.h)

# BootROM images (--ddr-boot firmware): crt0_rom.S + fx1_rom.ld put code and
# read-only data in BootROM and data/stacks in SRAM; nothing in DDR.
set(FX1_FW_ROM_LDS "${CMAKE_CURRENT_BINARY_DIR}/fx1_rom.ld")
add_custom_command(
    OUTPUT ${FX1_FW_ROM_LDS}
    COMMAND ${FX1_FW_CC} -E -P -x c -DFX1_LINKER_SCRIPT -I${FX1_FW_MAP_INCLUDE}
            ${FX1_FW_COMMON}/fx1_rom.ld.S -o ${FX1_FW_ROM_LDS}
    DEPENDS ${FX1_FW_COMMON}/fx1_rom.ld.S ${FX1_FW_MAP_INCLUDE}/fx1/fx1_memory_map.h
    COMMENT "fx1 fw: BootROM linker script"
    VERBATIM)

# fx1_rom_firmware(<name> SOURCES <files...> [DEFINES d...] [DEPENDS f...])
#   -> ${CMAKE_CURRENT_BINARY_DIR}/<name>.elf, a BootROM image with the DDR3 helpers.
#   An image that does not fit BootROM (2 MiB) or SRAM (1 MiB) fails to link.
function(fx1_rom_firmware name)
    cmake_parse_arguments(FW "" "" "SOURCES;DEFINES;DEPENDS" ${ARGN})
    set(elf "${CMAKE_CURRENT_BINARY_DIR}/${name}.elf")
    set(extra "")
    foreach(d ${FW_DEFINES})
        list(APPEND extra -D${d})
    endforeach()
    foreach(i ${FX1_FW_DDR3_INCLUDES})
        list(APPEND extra -I${i})
    endforeach()
    add_custom_command(
        OUTPUT ${elf}
        COMMAND ${FX1_FW_CC} ${FX1_FW_CFLAGS} ${extra} -nostdlib -nostartfiles
                -T ${FX1_FW_ROM_LDS} -Wl,--gc-sections -Wl,--no-warn-rwx-segments
                ${FX1_FW_COMMON}/crt0_rom.S ${FX1_FW_COMMON}/fx1_fw.c ${FX1_FW_DDR3_SOURCES}
                ${FW_SOURCES} -lgcc -o ${elf}
        DEPENDS ${FX1_FW_COMMON}/crt0_rom.S ${FX1_FW_COMMON}/fx1_fw.c ${FX1_FW_COMMON}/fx1_fw.h
                ${FX1_FW_ROM_LDS} ${FX1_FW_MAP_INCLUDE}/fx1/fx1_memory_map.h ${FX1_FW_DDR3_SOURCES}
                ${FX1_FW_DDR3_DEPENDS} ${FW_SOURCES} ${FW_DEPENDS}
        COMMENT "fx1 fw: ${name}.elf (BootROM)"
        VERBATIM)
    add_custom_target(fx1_fw_${name} ALL DEPENDS ${elf})
endfunction()

# fx1_boot_image(<name> APP <image> [DEFINES d...]): the BootROM loader
# (boot/boot_loader.c) with the stripped DDR image <image>.elf embedded in
# BootROM. The loader initializes the DDR3 controller and starts the image.
function(fx1_boot_image name)
    cmake_parse_arguments(B "" "APP" "DEFINES" ${ARGN})
    set(app "${CMAKE_CURRENT_BINARY_DIR}/${B_APP}.elf")
    set(stripped "${CMAKE_CURRENT_BINARY_DIR}/${name}.app.elf")
    add_custom_command(
        OUTPUT ${stripped}
        COMMAND ${FX1_RISCV_PREFIX}objcopy --strip-all ${app} ${stripped}
        DEPENDS ${app}
        COMMENT "fx1 fw: ${B_APP}.elf stripped for ${name}"
        VERBATIM)
    set(blob "${CMAKE_CURRENT_BINARY_DIR}/${name}_app_image.S")
    file(WRITE ${blob}
        "/* Generated: ${B_APP}.elf embedded in BootROM for boot_loader.c. */\n"
        "    .section .app_image, \"a\"\n    .balign 8\n"
        "    .global fx1_app_image, fx1_app_image_end\n"
        "fx1_app_image:\n    .incbin \"${stripped}\"\nfx1_app_image_end:\n")
    fx1_rom_firmware(${name} SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boot/boot_loader.c ${blob}
                     DEFINES ${B_DEFINES} DEPENDS ${stripped})
endfunction()

# fx1_smoke_test(<name> [TIMEOUT_MS ms]): build tests/<name>/main.c and run it
# twice, with the default 1 us TLM quantum and with 10 us (plan C11: results
# must not depend on how the hart's time is batched).
function(fx1_smoke_test name)
    cmake_parse_arguments(T "" "TIMEOUT_MS;SOURCE" "EXTRA_SOURCES;DEFINES;INCLUDES;DEPENDS" ${ARGN})
    if(NOT T_TIMEOUT_MS)
        set(T_TIMEOUT_MS 200)
    endif()
    if(NOT T_SOURCE)
        set(T_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/tests/${name}/main.c)
    endif()
    fx1_firmware(${name} SOURCES ${T_SOURCE} ${T_EXTRA_SOURCES}
                 DEFINES ${T_DEFINES} INCLUDES ${T_INCLUDES} DEPENDS ${T_DEPENDS})
    foreach(variant "" "_q10us")
        if(variant STREQUAL "_q10us")
            set(quantum 10000)
        else()
            set(quantum 1000)
        endif()
        add_test(NAME fx1_smoke_${name}${variant}
                 COMMAND $<TARGET_FILE:fx1_soc> --fw ${CMAKE_CURRENT_BINARY_DIR}/${name}.elf
                         --timeout-ms ${T_TIMEOUT_MS} --quantum-ns ${quantum}
                         --uart-log ${CMAKE_CURRENT_BINARY_DIR}/${name}${variant}.uart.log)
        # Pass requires exit status 0 (sim-control PASS); the regex additionally
        # catches a verdict line that disagrees with the status.
        set_tests_properties(fx1_smoke_${name}${variant} PROPERTIES
            TIMEOUT 120
            LABELS "fx1;fx1_smoke"
            FAIL_REGULAR_EXPRESSION "\\[fx1_soc\\] (RESULT FAIL|RESULT TIMEOUT|ERROR)")
    endforeach()
endfunction()

# fx1_expect_test(<name> RC <status> REGEX <regex> [IMAGE <elf name>]
#                 [ARGS <simulator args...>] [LABEL <label>]): the image must
# end with exactly this simulator exit status and output. Negative tests, and
# positive tests that must also print a specific line. Without IMAGE it builds
# tests/<name>/main.c; with IMAGE it runs an image another rule builds.
function(fx1_expect_test name)
    cmake_parse_arguments(T "" "RC;REGEX;TIMEOUT_MS;IMAGE;LABEL" "ARGS" ${ARGN})
    if(NOT T_TIMEOUT_MS)
        set(T_TIMEOUT_MS 200)
    endif()
    if(NOT T_LABEL)
        set(T_LABEL fx1_smoke)
    endif()
    if(NOT T_IMAGE)
        fx1_firmware(${name} SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/${name}/main.c)
        set(T_IMAGE ${name})
    endif()
    string(REPLACE ";" "\\;" _sim_args "${T_ARGS}")
    add_test(NAME ${T_LABEL}_${name}
             COMMAND ${CMAKE_COMMAND}
                     -DSIM=$<TARGET_FILE:fx1_soc>
                     -DELF=${CMAKE_CURRENT_BINARY_DIR}/${T_IMAGE}.elf
                     "-DARGS=${_sim_args}"
                     -DTIMEOUT_MS=${T_TIMEOUT_MS}
                     -DEXPECT_RC=${T_RC}
                     "-DEXPECT_REGEX=${T_REGEX}"
                     -DLOG=${CMAKE_CURRENT_BINARY_DIR}/${name}.uart.log
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_expect.cmake)
    set_tests_properties(${T_LABEL}_${name} PROPERTIES TIMEOUT 300 LABELS "fx1;${T_LABEL}")
endfunction()

if(CDC_BUILD_TESTS)
    fx1_smoke_test(hello_2hart)
    fx1_smoke_test(clint_irq)
    fx1_smoke_test(isa_misa)
    fx1_smoke_test(irq_in_log)
    fx1_smoke_test(uart_access)
    fx1_smoke_test(plic_irq)
    fx1_smoke_test(bus_fault)
    fx1_smoke_test(dma_m2m)
    # CSI-2 M5a: camera integration. Without --camera: MIPI_CSR faults, I2C0
    # present with no device, source 7 low. With --camera (a build with
    # -DFX1_BUILD_MIPI_CSI2=ON): CSR probe, I2C0 to the sensor on source 9,
    # TPG frame through MIPI_DMA on source 7, via the reference C99 driver.
    fx1_smoke_test(camera_absent TIMEOUT_MS 20)
    if(TARGET cdc::components::fx1_csi2_controller)
        set(_csi2_drv "${CMAKE_SOURCE_DIR}/components/FX1_Components/mipi-csi2/driver")
        fx1_firmware(camera_wiring SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/camera_wiring/main.c
            ${_csi2_drv}/src/fx1_csi2_drv.c
            INCLUDES ${_csi2_drv}/include
            DEPENDS ${_csi2_drv}/include/fx1_csi2/fx1_csi2_drv.h ${_csi2_drv}/include/fx1_csi2/fx1_csi2_regs.h)
        fx1_expect_test(camera_wiring RC 0 REGEX "RESULT PASS" IMAGE camera_wiring
            ARGS --camera TIMEOUT_MS 20)
        fx1_expect_test(camera_wiring_q10us RC 0 REGEX "RESULT PASS" IMAGE camera_wiring
            ARGS --camera --quantum-ns 10000 TIMEOUT_MS 20)
        # CSI-2 M5b: camera smokes on the BSP drivers (fw/fx1_soc/drivers) and the
        # reference CSI-2 driver: FW -> I2C0 -> sensor -> D-PHY -> CSI-2 RX ->
        # MIPI_DMA -> DDR -> PLIC 7. One quantum each (the detailed PHY is slow).
        set(_drv ${CMAKE_CURRENT_SOURCE_DIR}/drivers)
        set(_cam ${CMAKE_CURRENT_SOURCE_DIR}/tests/camera)
        set(_cam_sources ${_cam}/camera.c ${_drv}/fx1_i2c.c ${_drv}/fx1_ccs_sensor.c ${_drv}/fx1_csi2_hal_mmio.c
            ${_csi2_drv}/src/fx1_csi2_drv.c)
        set(_cam_includes ${_drv} ${_cam} ${_csi2_drv}/include)
        set(_cam_depends ${_cam}/camera.h ${_drv}/fx1_i2c.h ${_drv}/fx1_ccs_sensor.h ${_drv}/fx1_csi2_hal_mmio.h
            ${_csi2_drv}/include/fx1_csi2/fx1_csi2_drv.h ${_csi2_drv}/include/fx1_csi2/fx1_csi2_regs.h)
        function(fx1_camera_test name)
            cmake_parse_arguments(C "" "SOURCE;TIMEOUT_MS" "DEFINES;ARGS" ${ARGN})
            fx1_firmware(${name} SOURCES ${C_SOURCE} ${_cam_sources} DEFINES ${C_DEFINES}
                INCLUDES ${_cam_includes} DEPENDS ${_cam_depends})
            fx1_expect_test(${name} RC 0 REGEX "RESULT PASS" IMAGE ${name}
                ARGS --camera ${C_ARGS} TIMEOUT_MS ${C_TIMEOUT_MS})
            set_tests_properties(fx1_smoke_${name} PROPERTIES TIMEOUT 900)
        endfunction()
        fx1_camera_test(camera_raw10 SOURCE ${_cam}/raw.c DEFINES CAM_BITS=10 CAM_LANES=4 CAM_RAW16=0 TIMEOUT_MS 40)
        fx1_camera_test(camera_raw12 SOURCE ${_cam}/raw.c DEFINES CAM_BITS=12 CAM_LANES=2 CAM_RAW16=1 TIMEOUT_MS 40)
        fx1_camera_test(camera_recover SOURCE ${_cam}/recover.c TIMEOUT_MS 80)
        fx1_camera_test(camera_decerr SOURCE ${_cam}/decerr.c TIMEOUT_MS 40)
        # T (us after the CPU release) inside frame 2: see tests/camera/slverr.c.
        set(FX1_CAMERA_SLVERR_T_US 6200 CACHE STRING "camera_slverr: --ddr3-fault time, inside frame 2")
        fx1_camera_test(camera_slverr SOURCE ${_cam}/slverr.c TIMEOUT_MS 40
            ARGS --ddr3-ecc off --ddr3-fault ${FX1_CAMERA_SLVERR_T_US}:write-result:1)
        fx1_firmware(camera_i2c_irq SOURCES ${_cam}/i2c_irq.c ${_drv}/fx1_i2c.c ${_drv}/fx1_ccs_sensor.c
            INCLUDES ${_drv} DEPENDS ${_drv}/fx1_i2c.h ${_drv}/fx1_ccs_sensor.h)
        fx1_expect_test(camera_i2c_irq RC 0 REGEX "RESULT PASS" IMAGE camera_i2c_irq ARGS --camera TIMEOUT_MS 20)
        # CSI-2 M6a: camera -> ISP through the camera manager (fw/fx1_soc/drivers/fx1_camera),
        # NV12 checked against camera_isp_golden.h (gen_golden.py, stand-alone ISP model).
        set(_isp_root "${CMAKE_SOURCE_DIR}/components/FX1_Components/isp")
        set(_cisp ${CMAKE_CURRENT_SOURCE_DIR}/tests/camera_isp)
        set(_cisp_sources ${_cisp}/cam_isp.c ${_drv}/fx1_camera.c ${_drv}/fx1_isp_hal_mmio.c ${_drv}/fx1_i2c.c
            ${_drv}/fx1_ccs_sensor.c ${_drv}/fx1_csi2_hal_mmio.c ${_csi2_drv}/src/fx1_csi2_drv.c
            ${_isp_root}/driver/src/fx1_isp_drv.c)
        set(_cisp_includes ${_drv} ${_cisp} ${_csi2_drv}/include ${_isp_root}/driver/include ${_isp_root}/include)
        set(_cisp_depends ${_cisp}/cam_isp.h ${_cisp}/camera_isp_golden.h ${_drv}/fx1_camera.h ${_drv}/fx1_i2c.h
            ${_drv}/fx1_ccs_sensor.h ${_drv}/fx1_csi2_hal_mmio.h ${_drv}/fx1_isp_hal_mmio.h
            ${_csi2_drv}/include/fx1_csi2/fx1_csi2_drv.h ${_isp_root}/driver/include/fx1_isp/fx1_isp_drv.h)
        function(fx1_camera_isp_test name)
            cmake_parse_arguments(C "" "SOURCE;TIMEOUT_MS;LABEL" "DEFINES;ARGS" ${ARGN})
            fx1_firmware(${name} SOURCES ${C_SOURCE} ${_cisp_sources} DEFINES ${C_DEFINES}
                INCLUDES ${_cisp_includes} DEPENDS ${_cisp_depends})
            if(NOT C_LABEL)
                set(C_LABEL fx1_smoke)
            endif()
            fx1_expect_test(${name} RC 0 REGEX "RESULT PASS" IMAGE ${name} LABEL ${C_LABEL}
                ARGS --camera ${C_ARGS} TIMEOUT_MS ${C_TIMEOUT_MS})
            set_tests_properties(${C_LABEL}_${name} PROPERTIES TIMEOUT 1800)
        endfunction()
        fx1_camera_isp_test(camera_isp_raw12 SOURCE ${_cisp}/stream.c DEFINES CAM_BITS=12 TIMEOUT_MS 80)
        fx1_camera_isp_test(camera_isp_raw10 SOURCE ${_cisp}/stream.c DEFINES CAM_BITS=10 TIMEOUT_MS 80)
        set(_i 0)
        foreach(_f csi_err backpressure odma_err timeout idma_hold idma_err sensor_stop_timeout sensor_stop_nack
                   stop_held sensor_stop_budget start_fail)
            math(EXPR _i "${_i} + 1")
            fx1_camera_isp_test(camera_isp_${_f} SOURCE ${_cisp}/faults.c DEFINES CAM_FAULT=${_i} TIMEOUT_MS 80)
        endforeach()
        # Artifacts for the host (RAW, ISP input, NV12, records), checked by
        # check_artifacts.py against the sensor pattern, the golden and
        # fx1_isp_run_raw; python3 + numpy, so fx1_regression.
        foreach(_bits 12 10)
            fx1_firmware(camera_isp_art_raw${_bits} SOURCES ${_cisp}/artifacts.c ${_cisp_sources}
                DEFINES CAM_BITS=${_bits} INCLUDES ${_cisp_includes} DEPENDS ${_cisp_depends})
        endforeach()
        fx1_firmware(camera_isp_tpg4k SOURCES ${_cisp}/tpg4k.c ${_cisp_sources}
            INCLUDES ${_cisp_includes} DEPENDS ${_cisp_depends})
        # M5b review F1/F2: driver budgets and abort recovery, polling and IRQ mode.
        fx1_firmware(camera_i2c_timeout SOURCES ${_cam}/i2c_timeout.c ${_drv}/fx1_i2c.c ${_drv}/fx1_ccs_sensor.c
            INCLUDES ${_drv} DEPENDS ${_drv}/fx1_i2c.h ${_drv}/fx1_ccs_sensor.h)
        fx1_expect_test(camera_i2c_timeout RC 0 REGEX "RESULT PASS" IMAGE camera_i2c_timeout ARGS --camera TIMEOUT_MS 60)
        # A RAW fixture that cannot hold samples is refused before the run (exit 3).
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/camera_raw_odd.raw "B")
        fx1_expect_test(camera_raw_invalid RC 3 REGEX "RAW file .*camera_raw_odd.raw has 1 bytes"
            IMAGE camera_wiring ARGS --camera --camera-raw ${CMAKE_CURRENT_BINARY_DIR}/camera_raw_odd.raw TIMEOUT_MS 20)
    endif()
    # ISP through the reference C99 driver over the generated fixture.
    set(_isp "${CMAKE_SOURCE_DIR}/components/FX1_Components/isp")
    set(_isp_dir ${CMAKE_CURRENT_SOURCE_DIR}/tests/isp_stream)
    set(_isp_sources ${_isp_dir}/isp_common.c ${_isp}/driver/src/fx1_isp_drv.c)
    set(_isp_includes ${_isp}/driver/include ${_isp}/include ${_isp_dir})
    set(_isp_depends ${_isp_dir}/isp_fixture.h ${_isp_dir}/isp_common.h
        ${_isp}/driver/include/fx1_isp/fx1_isp_drv.h ${_isp}/include/fx1_isp/fx1_isp_csr.h)
    foreach(variant basic gtm_auto backlog)
        set(_isp_defines ISP_FX_PROFILE=0 ISP_FX_BACKLOG=0)
        if(variant STREQUAL "gtm_auto")
            set(_isp_defines ISP_FX_PROFILE=1 ISP_FX_BACKLOG=0)
        elseif(variant STREQUAL "backlog")
            set(_isp_defines ISP_FX_PROFILE=0 ISP_FX_BACKLOG=1)
        endif()
        fx1_smoke_test(isp_stream_${variant} TIMEOUT_MS 1000
            SOURCE ${_isp_dir}/main.c EXTRA_SOURCES ${_isp_sources}
            DEFINES ${_isp_defines} INCLUDES ${_isp_includes} DEPENDS ${_isp_depends})
    endforeach()
    fx1_smoke_test(isp_frame_id TIMEOUT_MS 1000
        SOURCE ${_isp_dir}/frame_id.c EXTRA_SOURCES ${_isp_sources}
        INCLUDES ${_isp_includes} DEPENDS ${_isp_depends})
    # CSR/MAS 07/10 (ISP update U6): frame metadata, AWB result ownership with
    # drops, an AWB lease across a DMA soft reset, Resizer from the first
    # frame and a mode switch while streaming (648x368 frames: longer run).
    fx1_smoke_test(isp_mas0710 TIMEOUT_MS 5000
        SOURCE ${_isp_dir}/isp_mas0710.c EXTRA_SOURCES ${_isp_sources}
        INCLUDES ${_isp_includes} DEPENDS ${_isp_depends})

    # Plan C7: LR/SC reservations and AMO exclusion against the other hart,
    # SYS_DMA and ISP ODMA (platform exclusive monitor).
    set(_at_dir ${CMAKE_CURRENT_SOURCE_DIR}/tests/atomic)
    fx1_smoke_test(lrsc_masters TIMEOUT_MS 200
        SOURCE ${_at_dir}/lrsc_masters.c EXTRA_SOURCES ${_at_dir}/atomic_common.c ${_isp_sources}
        INCLUDES ${_at_dir} ${_isp_includes} DEPENDS ${_at_dir}/atomic_common.h ${_isp_depends})
    fx1_firmware(amo_dma SOURCES ${_at_dir}/amo_dma.c ${_at_dir}/atomic_common.c
        INCLUDES ${_at_dir} DEPENDS ${_at_dir}/atomic_common.h)
    fx1_firmware(spinlock_2hart SOURCES ${_at_dir}/spinlock_2hart.c
        INCLUDES ${_at_dir} DEPENDS ${_at_dir}/atomic_common.h)
    foreach(quantum 1000 10000)
        # A pass must also show that an overlap happened (see the sources).
        fx1_expect_test(amo_dma_q${quantum}ns IMAGE amo_dma RC 0 TIMEOUT_MS 200
            ARGS --quantum-ns ${quantum}
            REGEX "device writes held [1-9][0-9]*.*RESULT PASS")
        fx1_expect_test(spinlock_2hart_q${quantum}ns IMAGE spinlock_2hart RC 0 TIMEOUT_MS 2000
            ARGS --quantum-ns ${quantum} LABEL fx1_regression
            REGEX "cancelled by hart [1-9][0-9]*.*RESULT PASS")
    endforeach()
    # Plan P5: error and reset recovery through the reference drivers.
    set(_rec_dir ${CMAKE_CURRENT_SOURCE_DIR}/tests/recovery)
    fx1_smoke_test(isp_recovery TIMEOUT_MS 1000
        SOURCE ${_rec_dir}/isp_recovery.c EXTRA_SOURCES ${_isp_sources}
        INCLUDES ${_isp_includes} DEPENDS ${_isp_depends})
    fx1_smoke_test(dma_recovery TIMEOUT_MS 200 SOURCE ${_rec_dir}/dma_recovery.c)
    # ISP RAW-frame demo (platforms/fx1_soc/demo/run_isp_demo.py): host-loaded
    # RAW, profile and job descriptor; not a gate test (it needs a RAW frame).
    set(_demo_dir ${CMAKE_CURRENT_SOURCE_DIR}/demo/isp_raw_frame)
    fx1_firmware(isp_demo SOURCES ${_demo_dir}/main.c ${_isp}/driver/src/fx1_isp_drv.c
        INCLUDES ${_isp}/driver/include ${_isp}/include ${_demo_dir}
        DEPENDS ${_demo_dir}/isp_demo_job.h ${_isp}/driver/include/fx1_isp/fx1_isp_drv.h
                ${_isp}/include/fx1_isp/fx1_isp_csr.h)
    # Optional: the demo on a real RAW frame as a test, outside the gate (label
    # fx1_demo). Point FX1_DEMO_RAW at a BGGR 2688x1520 le16_raw10_msb frame
    # (e.g. the dataset's outdoor1); the NV12 must equal the stand-alone model's.
    set(FX1_DEMO_RAW "" CACHE FILEPATH "RAW frame for the optional fx1_demo_isp_raw test")
    find_program(FX1_PYTHON3 python3)
    if(FX1_DEMO_RAW AND EXISTS "${FX1_DEMO_RAW}" AND FX1_PYTHON3)
        add_test(NAME fx1_demo_isp_raw
                 COMMAND ${FX1_PYTHON3} ${CMAKE_SOURCE_DIR}/platforms/fx1_soc/demo/run_isp_demo.py
                         --raw ${FX1_DEMO_RAW} --build-dir ${CMAKE_BINARY_DIR}
                         --out ${CMAKE_CURRENT_BINARY_DIR}/isp_demo)
        set_tests_properties(fx1_demo_isp_raw PROPERTIES TIMEOUT 600 LABELS "fx1_demo"
            PASS_REGULAR_EXPRESSION "result +MATCH")
    endif()
    # G6: UART RX from the host (--uart0-rx-file), polled and interrupt-driven.
    fx1_firmware(uart_rx SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/uart_rx/main.c)
    foreach(quantum 1000 10000)
        fx1_expect_test(uart_rx_q${quantum}ns IMAGE uart_rx RC 0 TIMEOUT_MS 300
            ARGS --quantum-ns ${quantum} --uart0-rx-file ${CMAKE_CURRENT_SOURCE_DIR}/tests/uart_rx/input.txt
            REGEX "got poll: \\[poll line\\].*got irq: \\[irq line\\].*RESULT PASS")
    endforeach()
    # G6: FreeRTOS V11.2.0 + UART CLI on hart 0 (AMP), boot IP check on hart 1.
    set(_rtos_dir ${CMAKE_CURRENT_SOURCE_DIR}/freertos)
    # third_party/ is gitignored: the kernel is fetched at its pin by
    # tools/third_party/setup_third_party.sh. Check here, so a fresh clone
    # fails at configure with the fix instead of at build with a missing file.
    set(FX1_FREERTOS_KERNEL_DIR ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel CACHE PATH
        "FreeRTOS-Kernel V11.2.0 checkout for the fx1_soc FreeRTOS CLI image")
    set(_kernel ${FX1_FREERTOS_KERNEL_DIR})
    set(_kernel_version "")
    if(EXISTS ${_kernel}/include/task.h)
        file(STRINGS ${_kernel}/include/task.h _kernel_version
             REGEX "^#define tskKERNEL_VERSION_NUMBER +\"V11\\.2\\.0\"")
    endif()
    if(NOT _kernel_version OR NOT EXISTS ${_kernel}/portable/GCC/RISC-V/portASM.S)
        message(FATAL_ERROR "fx1_soc firmware: FreeRTOS-Kernel V11.2.0 not found in ${_kernel}. "
                            "Run tools/third_party/setup_third_party.sh once (it pins 0adc196, V11.2.0), "
                            "or set FX1_FREERTOS_KERNEL_DIR.")
    endif()
    set(_rtos_sources
        ${_rtos_dir}/main.c ${_rtos_dir}/rtos_bsp.c ${_rtos_dir}/rtos_libc.c ${_rtos_dir}/cli.c
        ${_rtos_dir}/hwcheck.c ${_rtos_dir}/cmd_fpu.c ${_rtos_dir}/cmd_dma.c ${_rtos_dir}/cmd_isp.c
        ${_rtos_dir}/probe.S
        ${_kernel}/tasks.c ${_kernel}/queue.c ${_kernel}/list.c
        ${_kernel}/portable/MemMang/heap_4.c
        ${_kernel}/portable/GCC/RISC-V/port.c ${_kernel}/portable/GCC/RISC-V/portASM.S
        ${_isp_dir}/isp_common.c ${_isp}/driver/src/fx1_isp_drv.c)
    set(_rtos_includes ${_rtos_dir} ${_kernel}/include ${_kernel}/portable/GCC/RISC-V
        ${_isp_includes} ${_demo_dir})
    file(GLOB _rtos_headers ${_rtos_dir}/*.h)
    set(_rtos_depends ${_rtos_headers} ${_isp_depends} ${_demo_dir}/isp_demo_job.h)
    # The upstream RV32 port casts the uint64_t mtimecmp base to a pointer
    # (port.c, vPortSetupTimerInterrupt); harmless below 4 GiB, but -Werror.
    set(_rtos_cflags -Wno-int-to-pointer-cast)
    # CSI-2 M6b: with the camera build, the `camera` commands (camera manager
    # and drivers, fw/fx1_soc/drivers) and the boot check of CSI-2 RX / I2C0 /
    # sensor; at run time without --camera they report the camera absent.
    set(_rtos_defines "")
    if(TARGET cdc::components::fx1_csi2_controller)
        set(_rdrv ${CMAKE_CURRENT_SOURCE_DIR}/drivers)
        set(_rcsi2 ${CMAKE_SOURCE_DIR}/components/FX1_Components/mipi-csi2/driver)
        set(_rcisp ${CMAKE_CURRENT_SOURCE_DIR}/tests/camera_isp)
        list(APPEND _rtos_sources ${_rtos_dir}/cmd_camera.c ${_rdrv}/fx1_camera.c ${_rdrv}/fx1_isp_hal_mmio.c
            ${_rdrv}/fx1_i2c.c ${_rdrv}/fx1_ccs_sensor.c ${_rdrv}/fx1_csi2_hal_mmio.c ${_rcsi2}/src/fx1_csi2_drv.c)
        list(APPEND _rtos_includes ${_rdrv} ${_rcsi2}/include ${_rcisp})
        file(GLOB _rdrv_headers ${_rdrv}/*.h)
        list(APPEND _rtos_depends ${_rdrv_headers} ${_rcisp}/camera_isp_golden.h
            ${_rcsi2}/include/fx1_csi2/fx1_csi2_drv.h ${_rcsi2}/include/fx1_csi2/fx1_csi2_regs.h)
        set(_rtos_defines FX1_FW_HAS_CAMERA)
    endif()
    fx1_firmware(freertos_cli SOURCES ${_rtos_sources} CFLAGS ${_rtos_cflags} DEFINES ${_rtos_defines}
        INCLUDES ${_rtos_includes} DEPENDS ${_rtos_depends})
    fx1_firmware(freertos_cli_hwfail SOURCES ${_rtos_sources} DEFINES HWCHECK_INJECT_FAILURE ${_rtos_defines}
        CFLAGS ${_rtos_cflags} INCLUDES ${_rtos_includes} DEPENDS ${_rtos_depends})
    if(TARGET cdc::components::fx1_csi2_controller)
        # Test image (M6b review F1): the I2C host lost once between configure
        # and stream-on inside `camera start`.
        # Also (M6b re-review F2): a TPG 4K frame's NV12 CRC taken as the golden
        # (minutes on the simulated CPU) for the mixed CRC / spot-check session.
        fx1_firmware(freertos_cli_camfault SOURCES ${_rtos_sources}
            DEFINES CAMERA_TEST_STREAM_ON_FAULT CAMERA_TEST_FAST_CRC ${_rtos_defines}
            CFLAGS ${_rtos_cflags} INCLUDES ${_rtos_includes} DEPENDS ${_rtos_depends})
    endif()
    # Console input at the 115200 8N1 character time (87 us), starting 60 ms
    # after CPU release. Since the harts fetch from the DDR3 controller, the
    # boot (hart 1's IP check) takes about 26 ms and a CLI line printed with
    # interrupts masked outlasts 16 RX characters at the default 10 us pacing:
    # the 16-entry RX FIFO overruns (DDR3 decision log VPL-49).
    set(_rtos_rx --uart0-rx-byte-us 87)
    foreach(quantum 1000 10000)
        fx1_expect_test(freertos_cli_q${quantum}ns IMAGE freertos_cli RC 0 TIMEOUT_MS 3000
            ARGS --quantum-ns ${quantum} --uart0-rx-file ${_rtos_dir}/tests/cli_smoke.txt
                 --uart0-rx-delay-us 60000 ${_rtos_rx}
            REGEX "total: [0-9]+ checks, 0 failures -> PASS.*root@fx1:~# .*fpu context switch: PASS.*dma copy: PASS.*isp 8-frame fixture: PASS.*nosuchcommand: unknown command.*selftest: PASS.*poweroff: 0 failed.*RESULT PASS")
    endforeach()
    # Bad md/mw input: out-of-range numbers, ranges that wrap past 4 GiB or a
    # region end, and register holes that fault on the bus must all be
    # reported by the CLI, which then keeps working (no trap, no halt).
    fx1_expect_test(freertos_cli_bad_input IMAGE freertos_cli RC 0 TIMEOUT_MS 1000
        ARGS --uart0-rx-file ${_rtos_dir}/tests/cli_bad_input.txt --uart0-rx-delay-us 60000 ${_rtos_rx}
        REGEX "md 0xfffffffc 1[\r\n]+md: address range not mapped.*md 0xfffffff0 4[\r\n]+md: address range not mapped.*md 0x100000000 1[\r\n]+usage: md.*md 99999999999 1[\r\n]+usage: md.*0x100000000[\r\n]+usage: mw.*md 0x001ffffc 2[\r\n]+md: address range not mapped.*md: bus error at 0x02008000.*md: bus error at 0x0200fffc.*md: bus error at 0x0c3ffffc.*mw: bus error at 0x02008000.*0x80000000: 0x.*0x90000000 <- 0xa5a5a5a5, reads back 0xa5a5a5a5.*0x90000000: 0xa5a5a5a5.*MTIME_LO.*poweroff: 0 failed.*RESULT PASS")
    # The boot gate: a failing IP check must stop the boot before the scheduler.
    fx1_expect_test(freertos_cli_hwcheck_fail IMAGE freertos_cli_hwfail RC 1 TIMEOUT_MS 300
        REGEX "FAIL ISP.*boot halted: hwcheck FAIL.*RESULT FAIL code=0x50")
    # CSI-2 M6b: the `camera` commands. With --camera: nominal session (probe,
    # 64x16 RAW12 4 lanes and RAW10 2 lanes against the golden, ISP ownership
    # with `isp selftest`, status, stop) and fault session (I2C host disabled ->
    # stop ESENSOR / state 3, start refused, I2C back -> stop OK, recover,
    # frames from 1 again); without --camera: absent everywhere, nothing
    # counted as failed. The TPG session (640x480 against its golden, 4K
    # metadata and RAW spot check) is a regression test.
    if(TARGET cdc::components::fx1_csi2_controller)
        set(_cam_rx --uart0-rx-delay-us 60000 ${_rtos_rx})
        fx1_expect_test(freertos_cli_camera IMAGE freertos_cli RC 0 TIMEOUT_MS 3000
            ARGS --camera --uart0-rx-file ${_rtos_dir}/tests/cli_camera.txt ${_cam_rx}
            REGEX "ok   CSI-2 RX, checks [0-9]+.*ok   I2C0 \\+ sensor, checks [0-9]+.*total: [0-9]+ checks, 0 failures -> PASS.*sensor model_id 0x0000f1c5.*camera probe: PASS.*camera start: PASS.*isp selftest: the ISP is in use by the camera.*capture 1 FRAME_ID 1 side 0 isp seq 0 zero-copy out 0 64x16 raw frame 1 raw ok crc 0x[0-9a-f]+ = golden frame 1.*capture 4 FRAME_ID 4 side 1 isp seq 3 zero-copy out 3 64x16 raw frame 4 raw ok crc 0x[0-9a-f]+ = golden frame 4.*4/4 frames, drops 0.*camera capture: PASS.*csi ok 4 err 0.*camera stop: OK.*64x16 RAW10, 2 lane.*staged out 0 64x16 raw frame [0-9]+ raw ok crc 0x[0-9a-f]+ = golden frame.*2/2 frames, drops 0.*camera capture: PASS.*camera stop: OK.*isp 8-frame fixture: PASS.*poweroff: 0 failed.*RESULT PASS")
        fx1_expect_test(freertos_cli_camera_fault IMAGE freertos_cli RC 0 TIMEOUT_MS 3000
            ARGS --camera --uart0-rx-file ${_rtos_dir}/tests/cli_camera_fault.txt ${_cam_rx}
            REGEX "camera capture: PASS.*0x10030010 <- 0x00000000.*camera stop: ESENSOR in [0-9]+ us, state stopped, sensor not confirmed.*camera: stopped, sensor not confirmed, last stop ESENSOR.*sensor errors 1.*camera start: camera is stopped, sensor not confirmed \\(camera stop first\\).*0x10030010 <- 0x00000001.*camera stop: OK in [0-9]+ us, state idle.*camera recover: OK, state running.*camera recover: PASS.*raw frame 1 raw ok crc 0x[0-9a-f]+ = golden frame 1.*raw frame 2 raw ok crc 0x[0-9a-f]+ = golden frame 2.*camera capture: PASS.*camera stop: OK.*poweroff: 0 failed.*RESULT PASS")
        fx1_expect_test(freertos_cli_camera_absent IMAGE freertos_cli RC 0 TIMEOUT_MS 3000
            ARGS --uart0-rx-file ${_rtos_dir}/tests/cli_camera_absent.txt ${_cam_rx}
            REGEX "--   CSI-2 RX: absent \\(fx1_soc without --camera\\), skipped.*ok   I2C0, checks [0-9]+.*total: [0-9]+ checks, 0 failures -> PASS.*camera: absent.*camera start: camera probe first.*camera stop: not running.*camera capture: not running.*camera: idle.*camera mode: W 8..3840.*isp 8-frame fixture: PASS.*poweroff: 0 failed.*RESULT PASS")
        fx1_expect_test(freertos_cli_camera_tpg IMAGE freertos_cli RC 0 TIMEOUT_MS 5000 LABEL fx1_regression
            ARGS --camera --uart0-rx-file ${_rtos_dir}/tests/cli_camera_tpg.txt ${_cam_rx}
            REGEX "camera probe: PASS.*640x480 RAW12, source tpg, golden yes.*640x480 raw ok crc 0x[0-9a-f]+ = golden.*640x480 raw ok crc 0x[0-9a-f]+ = golden.*2/2 frames, drops 0.*3840x2160 raw ok crc skipped \\(size\\): full golden not compared, Y spot ok UV spot ok \\(64 bytes\\).*1/1 frames, drops 0.*camera capture: PASS.*camera stop: OK.*poweroff: 0 failed.*RESULT PASS")
        # Negative: frames from a RAW file (64x16 RAW12, every sample 0x123)
        # instead of the pattern: the RAW spot check and the golden must both
        # fail, the capture is counted failed, poweroff ends with FAIL.
        fx1_expect_test(freertos_cli_camera_mismatch IMAGE freertos_cli RC 1 TIMEOUT_MS 3000 LABEL fx1_regression
            ARGS --camera --camera-raw ${_rtos_dir}/tests/cli_camera_mismatch.raw
                 --uart0-rx-file ${_rtos_dir}/tests/cli_camera_mismatch.txt ${_cam_rx}
            REGEX "camera probe: PASS.*camera start: PASS.*64x16 raw frame [0-9]+ raw BAD crc 0x[0-9a-f]+ GOLDEN MISMATCH.*64x16 raw frame [0-9]+ raw BAD crc 0x[0-9a-f]+ GOLDEN MISMATCH.*camera capture: 2/2 frames.*camera capture: FAIL.*camera stop: OK.*poweroff: 1 failed.*RESULT FAIL")
        set_tests_properties(fx1_regression_freertos_cli_camera_mismatch PROPERTIES TIMEOUT 1800)
        # M6b review F1: `camera start` fails after configure (I2C host lost
        # before stream-on, test image): the CLI cleans up (stop ESENSOR, state
        # 3, ISP released), I2C back -> stop OK, `isp selftest` PASS, start and
        # capture again against the golden. The failed start is counted.
        fx1_expect_test(freertos_cli_camera_startfault IMAGE freertos_cli_camfault RC 1 TIMEOUT_MS 3000 LABEL fx1_regression
            ARGS --camera --uart0-rx-file ${_rtos_dir}/tests/cli_camera_startfault.txt ${_cam_rx}
            REGEX "camera probe: PASS.*test fault: I2C host disabled before stream-on.*camera start: cleanup stop ESENSOR, state stopped, sensor not confirmed.*camera start: EDEV.*camera start: FAIL.*camera: stopped, sensor not confirmed, last start EDEV.*isp 8-frame fixture: PASS.*0x10030010 <- 0x00000001.*camera stop: OK in [0-9]+ us, state idle.*isp 8-frame fixture: PASS.*camera start: OK.*= golden frame 1.*= golden frame 2.*camera capture: PASS.*camera stop: OK.*poweroff: 1 failed.*RESULT FAIL")
        # M6b review F2: TPG 4K with the CRC skipped: a good frame (spot ok),
        # then ODMA Y of slot 0 moved to scratch DDR (Y not written: Y spot
        # BAD, UV ok), then UV moved (UV spot BAD, Y ok); the poisoned sample
        # positions keep the first frame's data from passing.
        fx1_expect_test(freertos_cli_camera_nv12_fault IMAGE freertos_cli RC 1 TIMEOUT_MS 5000 LABEL fx1_regression
            ARGS --camera --uart0-rx-file ${_rtos_dir}/tests/cli_camera_nv12_fault.txt ${_cam_rx}
            REGEX "3840x2160 raw ok crc skipped \\(size\\): full golden not compared, Y spot ok UV spot ok.*camera capture: PASS.*0x110030a0 <- 0x8f000000.*3840x2160 raw ok crc skipped \\(size\\): full golden not compared, Y spot BAD UV spot ok.*camera capture: FAIL.*0x110030a8 <- 0x8f800000.*3840x2160 raw ok crc skipped \\(size\\): full golden not compared, Y spot ok UV spot BAD.*camera capture: FAIL.*poweroff: 2 failed.*RESULT FAIL")
        # M6b re-review F2: CRC captures, then spot-checked ones on the same
        # ring (test image: the 4K CRC is the golden's): slot 0 went through
        # the CRC path and must come back poisoned, so a redirected UV, then
        # Y, of slot 0 is caught after the ring has wrapped.
        fx1_expect_test(freertos_cli_camera_mixed_crc IMAGE freertos_cli_camfault RC 1 TIMEOUT_MS 5000 LABEL fx1_regression
            ARGS --camera --uart0-rx-file ${_rtos_dir}/tests/cli_camera_mixed_crc.txt ${_cam_rx}
            REGEX "capture 1 FRAME_ID 1 side 0 isp seq 0 zero-copy out 0 3840x2160 raw ok crc 0xdd6da3bb = golden.*3/3 frames.*camera capture: PASS.*0x110030a8 <- 0x8f800000.*out 0 3840x2160 raw ok crc skipped \\(size\\): full golden not compared, Y spot ok UV spot BAD.*camera capture: FAIL.*camera stop: OK.*crc 0xdd6da3bb = golden.*3/3 frames.*0x110030a0 <- 0x8f000000.*out 0 3840x2160 raw ok crc skipped \\(size\\): full golden not compared, Y spot BAD UV spot ok.*camera capture: FAIL.*poweroff: 2 failed.*RESULT FAIL")
        set_tests_properties(fx1_regression_freertos_cli_camera_startfault fx1_regression_freertos_cli_camera_nv12_fault
            fx1_regression_freertos_cli_camera_mixed_crc PROPERTIES TIMEOUT 1800)
        foreach(_t freertos_cli_camera freertos_cli_camera_fault freertos_cli_camera_absent)
            set_tests_properties(fx1_smoke_${_t} PROPERTIES TIMEOUT 1800)
        endforeach()
        set_tests_properties(fx1_regression_freertos_cli_camera_tpg PROPERTIES TIMEOUT 1800)
    endif()
    # The handoff example (docs/FW_GETTING_STARTED.md): kept building and passing.
    fx1_smoke_test(example_quickstart TIMEOUT_MS 200
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/examples/quickstart/main.c)
    # Plan C12: hart 0 streams the ISP while hart 1 runs SYS_DMA copies in its
    # own DDR region; both data paths are checked.
    fx1_smoke_test(shared_mem_concurrent TIMEOUT_MS 1000
        SOURCE ${_isp_dir}/main.c
        EXTRA_SOURCES ${_isp_sources} ${CMAKE_CURRENT_SOURCE_DIR}/tests/shared_mem/hart1_dma.c
                      ${_at_dir}/atomic_common.c
        DEFINES ISP_FX_PROFILE=0 ISP_FX_BACKLOG=0 ISP_FX_HART1_DMA=1
        INCLUDES ${_isp_includes} ${_at_dir} DEPENDS ${_isp_depends} ${_at_dir}/atomic_common.h)
    # Negative control: under the upstream VP++ lock model SYS_DMA does not
    # take part, so a DMA write is lost inside an AMO. (No such control for
    # lrsc_masters: upstream drops an LR reservation after 17 instructions, so
    # its SCs fail there for an unrelated reason; mutations cover it instead.)
    fx1_expect_test(amo_dma_no_monitor IMAGE amo_dma RC 1 TIMEOUT_MS 200
        ARGS --no-exclusive-monitor REGEX "RESULT FAIL code=0x20")
    # The committed ISP fixture must still match the pinned Python reference.
    # Needs python3 + numpy, so it is a regression check and only registered
    # when they are available (the smoke tests never need Python).
    # The `python3` on PATH (the one the ISP tools use), not FindPython3's
    # newest interpreter, which may lack numpy.
    find_program(FX1_PYTHON3 python3)
    if(FX1_PYTHON3)
        execute_process(COMMAND ${FX1_PYTHON3} -c "import numpy"
                        RESULT_VARIABLE _fx1_numpy OUTPUT_QUIET ERROR_QUIET)
        if(_fx1_numpy EQUAL 0)
            add_test(NAME fx1_regression_isp_fixture_check
                     COMMAND ${FX1_PYTHON3} ${_isp_dir}/gen_fixture.py --check)
            set_tests_properties(fx1_regression_isp_fixture_check PROPERTIES
                TIMEOUT 120 LABELS "fx1;fx1_regression")
            # CSI-2 M6a: the camera -> ISP golden header is up to date, and the
            # artifacts of a RAW12 / RAW10 run check out on the host.
            if(TARGET cdc::components::fx1_csi2_controller AND TARGET fx1_isp_run_raw)
                add_test(NAME fx1_regression_camera_isp_golden_check
                         COMMAND ${FX1_PYTHON3} ${_cisp}/gen_golden.py --check
                                 --run-raw $<TARGET_FILE:fx1_isp_run_raw>)
                set_tests_properties(fx1_regression_camera_isp_golden_check PROPERTIES
                    TIMEOUT 600 LABELS "fx1;fx1_regression")
                foreach(_bits 12 10)
                    add_test(NAME fx1_regression_camera_isp_artifacts_raw${_bits}
                             COMMAND ${FX1_PYTHON3} ${_cisp}/check_artifacts.py --bits ${_bits}
                                     --sim $<TARGET_FILE:fx1_soc>
                                     --elf ${CMAKE_CURRENT_BINARY_DIR}/camera_isp_art_raw${_bits}.elf
                                     --run-raw $<TARGET_FILE:fx1_isp_run_raw>
                                     --out ${CMAKE_CURRENT_BINARY_DIR}/camera_isp_art_raw${_bits})
                    set_tests_properties(fx1_regression_camera_isp_artifacts_raw${_bits} PROPERTIES
                        TIMEOUT 1800 LABELS "fx1;fx1_regression")
                endforeach()
                # Large frames: 3840x2160 RAW12 from the CSI-2 RX TPG (stdlib only).
                add_test(NAME fx1_regression_camera_isp_tpg4k
                         COMMAND ${FX1_PYTHON3} ${_cisp}/check_tpg4k.py --sim $<TARGET_FILE:fx1_soc>
                                 --elf ${CMAKE_CURRENT_BINARY_DIR}/camera_isp_tpg4k.elf
                                 --out ${CMAKE_CURRENT_BINARY_DIR}/camera_isp_tpg4k)
                set_tests_properties(fx1_regression_camera_isp_tpg4k PROPERTIES
                    TIMEOUT 3600 LABELS "fx1;fx1_regression")
            endif()
        endif()
        # The ISP demo's pack step (isp_pack.py, stdlib only) on synthetic
        # dumps: damaged results, dumps, jobs and a failing preview must all
        # be rejected (review G6a R2).
        add_test(NAME fx1_regression_isp_demo_pack
                 COMMAND ${FX1_PYTHON3} ${CMAKE_SOURCE_DIR}/platforms/fx1_soc/demo/tests/test_pack.py)
        set_tests_properties(fx1_regression_isp_demo_pack PROPERTIES
            TIMEOUT 120 LABELS "fx1;fx1_regression")
    endif()
    # Throughput probe under both quanta (plan C11); slower, so fx1_regression.
    fx1_firmware(perf_loop SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/perf_loop/main.c)
    foreach(quantum 1000 10000)
        add_test(NAME fx1_regression_perf_loop_q${quantum}ns
                 COMMAND $<TARGET_FILE:fx1_soc> --fw ${CMAKE_CURRENT_BINARY_DIR}/perf_loop.elf
                         --timeout-ms 1000 --quantum-ns ${quantum} --quiet-uart)
        set_tests_properties(fx1_regression_perf_loop_q${quantum}ns PROPERTIES
            TIMEOUT 300 LABELS "fx1;fx1_regression"
            FAIL_REGULAR_EXPRESSION "\\[fx1_soc\\] (RESULT FAIL|RESULT TIMEOUT|ERROR)")
    endforeach()
    # A target that never answers must stop the run as an integration error,
    # not let the guest continue to PASS (review G2-R2).
    fx1_firmware(silent_read SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/silent_read/main.c)
    add_test(NAME fx1_smoke_silent_target
             COMMAND $<TARGET_FILE:test_fx1_cpu_silent_target> ${CMAKE_CURRENT_BINARY_DIR}/silent_read.elf)
    set_tests_properties(fx1_smoke_silent_target PROPERTIES TIMEOUT 120 LABELS "fx1;fx1_smoke")
    # A fault while holding the console lock must still give FAIL 0xE005.
    fx1_expect_test(trap_in_log RC 1 REGEX "RESULT FAIL code=0xe005")

    # ── DDR3 on the CPU (DDR3 VP subtask 5) ──
    set(_ddr3_dir ${CMAKE_CURRENT_SOURCE_DIR}/tests/ddr3)
    # Firmware-init boot from BootROM/SRAM: identity, commit reject, init, memory,
    # region decode miss -> fault + PLIC, power, two harts.
    fx1_rom_firmware(ddr3_fw_init SOURCES ${_ddr3_dir}/fw_init.c)
    foreach(quantum 1000 10000)
        fx1_expect_test(ddr3_fw_init_q${quantum}ns IMAGE ddr3_fw_init RC 0 TIMEOUT_MS 3000
            ARGS --ddr-boot firmware --quantum-ns ${quantum}
            REGEX "A: identity.*B: commit reject, recommit, ready.*C: word/half/byte.*D: region decode miss.*E: self-refresh.*F: two harts.*RESULT PASS")
    endforeach()
    # Errors and recovery with a host fault schedule (simulated time from CPU
    # release; the firmware gets the same schedule as defines).
    fx1_rom_firmware(ddr3_fault_recovery SOURCES ${_ddr3_dir}/fault_recovery.c
        DEFINES FAULT_CE_MS=1000 FAULT_UE_MS=1010 FAULT_TIMEOUT_MS=1020 FAULT_FATAL_MS=2000)
    fx1_expect_test(ddr3_fault_recovery IMAGE ddr3_fault_recovery RC 0 TIMEOUT_MS 4000
        ARGS --ddr-boot firmware --ddr3-fault 1000000:ce:0x80100000 --ddr3-fault 1010000:ue:0x80100040
             --ddr3-fault 1020000:read-timeout --ddr3-fault 2000000:fatal:dfi
        REGEX "B: correctable error corrected.*C: uncorrectable error -> access fault.*D: read timeout -> quarantine.*E: cold reset \\+ re-initialization.*F: fatal hold -> core reset by hart 1 -> cold reset -> re-initialization.*RESULT PASS")
    # The preinitialized boot (default) as firmware sees it.
    fx1_firmware(ddr3_preinit SOURCES ${_ddr3_dir}/preinit.c ${FX1_FW_DDR3_SOURCES}
        INCLUDES ${FX1_FW_DDR3_INCLUDES} DEFINES FAULT_CE_US=20000 FAULT_UE_US=21000
        DEPENDS ${FX1_FW_DDR3_DEPENDS})
    foreach(quantum 1000 10000)
        fx1_expect_test(ddr3_preinit_q${quantum}ns IMAGE ddr3_preinit RC 0 TIMEOUT_MS 200
            ARGS --quantum-ns ${quantum} --ddr3-fault 20000:ce:0x90000100 --ddr3-fault 21000:ue:0x90000140
            REGEX "DDR3 preinit done.*A: platform-initialized DDR3 matches.*B: both harts.*C: correctable.*D: uncorrectable.*E: self-refresh and automatic wake.*RESULT PASS")
    endforeach()
    # Review S5-R1: a failed wake must not stop simulated time; recovery by the other hart.
    fx1_rom_firmware(ddr3_power_recovery SOURCES ${_ddr3_dir}/power_recovery.c)
    fx1_expect_test(ddr3_power_recovery IMAGE ddr3_power_recovery RC 0 TIMEOUT_MS 3000
        ARGS --ddr-boot firmware
        REGEX "wake timeout -> automatic wake denied, retried with back-off -> cold reset by hart 1.*re-initialized after the failed wake.*RESULT PASS")
    fx1_rom_firmware(ddr3_wake_watchdog SOURCES ${_ddr3_dir}/wake_watchdog.c)
    fx1_expect_test(ddr3_wake_watchdog IMAGE ddr3_wake_watchdog RC 2 TIMEOUT_MS 2000
        ARGS --ddr-boot firmware
        REGEX "wake watchdog: DDR read with a failing wake.*RESULT TIMEOUT after 2000 ms")
    # Review S5-R2/R3: CPU reservations follow DDR storage, not bus names.
    fx1_rom_firmware(ddr3_atomic_lifecycle SOURCES ${_ddr3_dir}/atomic_lifecycle.c)
    fx1_expect_test(ddr3_atomic_lifecycle IMAGE ddr3_atomic_lifecycle RC 0 TIMEOUT_MS 4000
        ARGS --ddr-boot firmware
        REGEX "B: cold reset \\+ initialization voids.*C: core reset \\+ reinitialization voids.*D: CSR-driven reinitialization voids.*E: a region commit voids.*by DDR3 reset/remap [1-9].*RESULT PASS")
    fx1_firmware(ddr3_atomic_alias SOURCES ${_ddr3_dir}/atomic_alias.c ${_at_dir}/atomic_common.c
        ${FX1_FW_DDR3_SOURCES} INCLUDES ${_at_dir} ${FX1_FW_DDR3_INCLUDES}
        DEPENDS ${_at_dir}/atomic_common.h ${FX1_FW_DDR3_DEPENDS})
    foreach(quantum 1000 10000)
        fx1_expect_test(ddr3_atomic_alias_q${quantum}ns IMAGE ddr3_atomic_alias RC 0 TIMEOUT_MS 500
            ARGS --quantum-ns ${quantum}
            REGEX "A: aliases share data.*B: hart store through the alias -> SC fails.*C: DMA write through the alias -> SC fails.*D: no DMA write through the alias lost.*device writes held [1-9][0-9]*.*RESULT PASS")
    endforeach()
    # Preinit order from a BootROM entry: the CPU starts after scrub and preload.
    fx1_rom_firmware(ddr3_release_order SOURCES ${_ddr3_dir}/release_order.c)
    set(_ddr3_preload ${CMAKE_CURRENT_BINARY_DIR}/ddr3_preload.txt)
    file(WRITE ${_ddr3_preload} "FX1 DDR3 preload check: scrub -> preload -> CPU\n")
    fx1_expect_test(ddr3_release_order IMAGE ddr3_release_order RC 0 TIMEOUT_MS 200
        ARGS --load 0x80600000:${_ddr3_preload}
        REGEX "CPUs released at .*preinit order: scrub -> preload -> CPU.*RESULT PASS")
    # BootROM loader + an application copied to DDR after readiness.
    fx1_boot_image(boot_hello APP hello_2hart)
    fx1_expect_test(ddr3_boot_hello IMAGE boot_hello RC 0 TIMEOUT_MS 3000 ARGS --ddr-boot firmware
        REGEX "boot: DDR3 ready.*boot: starting the application.*Hello from FX1 hart 0.*hart 1 up.*RESULT PASS")
    fx1_boot_image(boot_freertos APP freertos_cli)
    fx1_expect_test(ddr3_boot_freertos_cli IMAGE boot_freertos RC 0 TIMEOUT_MS 5000
        ARGS --ddr-boot firmware --uart0-rx-file ${_rtos_dir}/tests/cli_smoke.txt
             --uart0-rx-delay-us 1000000 ${_rtos_rx}
        REGEX "boot: DDR3 ready.*boot: starting the application.*total: [0-9]+ checks, 0 failures -> PASS.*root@fx1:~# .*selftest: PASS.*poweroff: 0 failed.*RESULT PASS")
    # Bring-up failures are reported from BootROM/SRAM (UART + sim-control).
    fx1_boot_image(boot_hello_short_wait APP hello_2hart DEFINES BOOT_READY_TIMEOUT_MS=100)
    fx1_expect_test(ddr3_boot_ready_timeout IMAGE boot_hello_short_wait RC 1 TIMEOUT_MS 3000
        ARGS --ddr-boot firmware REGEX "DDR3 bring-up failed:.*readiness timeout.*RESULT FAIL code=0xd304")
    fx1_expect_test(ddr3_boot_init_failure IMAGE boot_hello RC 1 TIMEOUT_MS 3000
        ARGS --ddr-boot firmware --ddr3-fault 0:phy-fail
        REGEX "DDR3 bring-up failed:.*initialization failed.*RESULT FAIL code=0xd305")
    fx1_expect_test(ddr3_boot_scrub_fatal IMAGE boot_hello RC 1 TIMEOUT_MS 3000
        ARGS --ddr-boot firmware --ddr3-fault 0:fatal:scrub
        REGEX "DDR3 bring-up failed:.*fatal event.*RESULT FAIL code=0xd306")
    # A DDR image cannot be booted with --ddr-boot firmware (nothing would load it).
    fx1_expect_test(ddr3_boot_ddr_image_refused IMAGE hello_2hart RC 3 TIMEOUT_MS 200
        ARGS --ddr-boot firmware REGEX "ERROR .*with --ddr-boot firmware the DDR is not initialized")
else()
    fx1_firmware(hello_2hart SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/hello_2hart/main.c)
endif()
