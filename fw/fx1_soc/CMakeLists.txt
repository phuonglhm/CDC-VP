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

# fx1_firmware(<name> SOURCES <files...> [DEFINES d...] [INCLUDES dir...] [DEPENDS f...])
#   -> ${CMAKE_CURRENT_BINARY_DIR}/<name>.elf
function(fx1_firmware name)
    cmake_parse_arguments(FW "" "" "SOURCES;DEFINES;INCLUDES;DEPENDS" ${ARGN})
    set(elf "${CMAKE_CURRENT_BINARY_DIR}/${name}.elf")
    set(extra "")
    foreach(d ${FW_DEFINES})
        list(APPEND extra -D${d})
    endforeach()
    foreach(i ${FW_INCLUDES})
        list(APPEND extra -I${i})
    endforeach()
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
        endif()
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
else()
    fx1_firmware(hello_2hart SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/hello_2hart/main.c)
endif()
