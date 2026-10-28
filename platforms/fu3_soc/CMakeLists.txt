# FU3 SoC platform (CDC_BUILD_FU3_SOC): the FU3 bus (components/FU3_Components/bus)
# with the on-chip memories (T04), both CPUs (T05/T06), the CRM and SYSCSR (T07)
# integrated and every other enabled target bound to a not-integrated responder
# until tasks T08-T17 integrate the models; see README.md.
add_library(fu3_soc_core STATIC
    src/fu3_soc_top.cpp
    src/fu3_memory.cpp
    src/not_integrated_target.cpp
    src/gated_target.cpp
    src/fu3_peripherals.cpp
    src/fu3_pinmux.cpp
    src/vp_abi_regs.cpp
    src/image_loader.cpp
    src/gdb_server.cpp
    src/self_test.cpp)
target_include_directories(fu3_soc_core PUBLIC src include)
target_compile_options(fu3_soc_core PRIVATE -Wall -Wextra)
target_link_libraries(fu3_soc_core PUBLIC SystemC::systemc fu3::bus_system fu3::cpu fu3::dbg fu3::crm fu3::syscsr fu3::plic fu3::timer fu3::wdt fu3::uart fu3::spi fu3::i2c fu3::gpio fu3::io_pad fu3::qspi fu3::hyperram fu3::aes fu3::adc fu3::rng fu3::mailbox cdc::components::fu3_ap_dma)

add_executable(vp_fu3_soc src/main.cpp)

# `vp_fu3_soc --version`: package name, CDC-VP revision and tree state at
# configure time, build type and compiler (cmake/build_info.h.in).
set(FU3_SOC_PACKAGE_NAME "VP_FU3_SOC" CACHE STRING "Package name the vp_fu3_soc binary reports")
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
                OUTPUT_VARIABLE FU3_SOC_GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" status --porcelain --untracked-files=no
                OUTPUT_VARIABLE _fu3_git_status OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
                RESULT_VARIABLE _fu3_git_rc)
if(NOT FU3_SOC_GIT_REVISION OR NOT _fu3_git_rc EQUAL 0)
    set(FU3_SOC_GIT_REVISION "unknown")
    set(FU3_SOC_GIT_STATE "unknown")
elseif(_fu3_git_status)
    set(FU3_SOC_GIT_STATE "dirty")
else()
    set(FU3_SOC_GIT_STATE "clean")
endif()
if(CMAKE_BUILD_TYPE)
    set(FU3_SOC_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
    set(FU3_SOC_BUILD_TYPE "none (developer build, not for packaging)")
endif()
configure_file(cmake/build_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/fu3_soc_build_info.h @ONLY)

target_include_directories(vp_fu3_soc PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_options(vp_fu3_soc PRIVATE -Wall -Wextra)
target_link_libraries(vp_fu3_soc PRIVATE fu3_soc_core)
# RPATH=$ORIGIN and the SystemC runtime beside the binary (cmake/modules/CdcPortable.cmake).
cdc_make_portable(vp_fu3_soc)

if(CDC_BUILD_TESTS)
    add_executable(test_fu3_soc_skeleton tests/test_fu3_soc_skeleton.cpp)
    target_compile_options(test_fu3_soc_skeleton PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_skeleton PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_skeleton COMMAND test_fu3_soc_skeleton)
    add_executable(test_fu3_soc_memory tests/test_fu3_soc_memory.cpp)
    target_compile_options(test_fu3_soc_memory PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_memory PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_memory COMMAND test_fu3_soc_memory)

    # The executable itself: self-test, instance listing, and the error paths
    # FW/SW will hit first (unknown option, run without a CPU). Each checks the
    # exit status from main.cpp as well as the output (tests/expect_exit.cmake).
    function(fu3_soc_cli_test name expect_rc expect_output)
        add_test(NAME ${name} COMMAND ${CMAKE_COMMAND}
            -DCMD=$<TARGET_FILE:vp_fu3_soc> "-DARGS=${ARGN}" -DEXPECT_RC=${expect_rc}
            "-DEXPECT_OUTPUT=${expect_output}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_exit.cmake)
    endfunction()
    fu3_soc_cli_test(fu3_soc_self_test 0 "self-test: [0-9]+ checks, 0 failures -> PASS" --self-test)
    fu3_soc_cli_test(fu3_soc_list_instances 0
        "bus map: FU3 Common Spec v[0-9.]+ .*integrated.*DISABLED, external IP for R2" --list-instances)
    fu3_soc_cli_test(fu3_soc_cli_unknown_option 4 "unknown option '--no-such-option'" --no-such-option)
    fu3_soc_cli_test(fu3_soc_run_without_image 3 "no ROM image: Ibex resets at 0x0000_0080")
    # Plan T20: image and board options, checked before anything runs.
    fu3_soc_cli_test(fu3_soc_cli_bad_mode 4 "--mode: expects MODE_1 MODE_0 as two bits" --mode 2)
    fu3_soc_cli_test(fu3_soc_cli_bad_load 4 "--load: expects ADDR:FILE" --load 0x50000)
    fu3_soc_cli_test(fu3_soc_cli_missing_rom 3 "cannot read 'no_such_rom.bin'" --rom no_such_rom.bin)
    set(_t20_data ${CMAKE_CURRENT_SOURCE_DIR}/tests/data)
    fu3_soc_cli_test(fu3_soc_cli_load_mmio 3 "outside every preload region" --rom ${_t20_data}/t20_flash.bin
                     --load 0x44000000:${_t20_data}/t20_flash.bin)
    add_executable(test_fu3_soc_loader tests/test_fu3_soc_loader.cpp)
    target_compile_options(test_fu3_soc_loader PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_loader PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_loader COMMAND test_fu3_soc_loader)
    set_tests_properties(fu3_soc_cli_bad_mode fu3_soc_cli_bad_load fu3_soc_cli_missing_rom fu3_soc_cli_load_mmio
        fu3_soc_loader PROPERTIES TIMEOUT 60 LABELS "fu3;fu3_soc;fu3_cli")

    # Plan T05: firmware on each ISS, built with the RISC-V cross compiler when
    # one is found (tests/fw). The harness loads it and judges its verdict.
    add_executable(test_fu3_soc_cpu tests/test_fu3_soc_cpu.cpp)
    target_compile_options(test_fu3_soc_cpu PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_cpu PRIVATE fu3_soc_core)
    find_program(FU3_SOC_RISCV_GCC riscv-none-elf-gcc
        HINTS /opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin)
    if(FU3_SOC_RISCV_GCC)
        set(_fw ${CMAKE_CURRENT_SOURCE_DIR}/tests/fw)
        set(_fw_common -O1 -g -ffreestanding -nostdlib -nostartfiles -msmall-data-limit=0
            -Wall -Wextra -Werror -I${_fw} -I${CMAKE_CURRENT_SOURCE_DIR}/include ${_fw}/fw_trap.S ${_fw}/fw_runtime.c
            ${_fw}/fw_debug.S ${_fw}/fw_debug.c)
        add_custom_command(OUTPUT fu3_t05_cva6.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imfc_zicsr_zifencei -mabi=ilp32f ${_fw_common}
                    ${_fw}/cva6_start.S ${_fw}/cva6_tests.c -T ${_fw}/cva6.ld -o fu3_t05_cva6.elf
            DEPENDS ${_fw}/cva6_start.S ${_fw}/cva6_tests.c ${_fw}/cva6.ld ${_fw}/fw_trap.S
                    ${_fw}/fw_runtime.c ${_fw}/fw_common.h ${_fw}/fw_debug.S ${_fw}/fw_debug.c
            COMMENT "RISC-V: fu3_t05_cva6.elf")
        add_custom_command(OUTPUT fu3_t05_ibex.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32 ${_fw_common}
                    ${_fw}/ibex_rom.S ${_fw}/ibex_start.S ${_fw}/ibex_tests.c -T ${_fw}/ibex.ld -o fu3_t05_ibex.elf
            DEPENDS ${_fw}/ibex_rom.S ${_fw}/ibex_start.S ${_fw}/ibex_tests.c ${_fw}/ibex.ld ${_fw}/fw_trap.S
                    ${_fw}/fw_runtime.c ${_fw}/fw_common.h ${_fw}/fw_debug.S ${_fw}/fw_debug.c
            COMMENT "RISC-V: fu3_t05_ibex.elf")
        # T07: Ibex boot stub that sets the PLL and releases CVA6 through the CRM.
        add_custom_command(OUTPUT fu3_t07_handoff.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imc_zicsr -mabi=ilp32 -nostdlib -nostartfiles -Wall -Werror
                    ${_fw}/ibex_handoff.S -T ${_fw}/ibex.ld -o fu3_t07_handoff.elf
            DEPENDS ${_fw}/ibex_handoff.S ${_fw}/ibex.ld
            COMMENT "RISC-V: fu3_t07_handoff.elf")
        # T07: Ibex boot test through power-on, software and global reset.
        add_custom_command(OUTPUT fu3_t07_boot.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32 ${_fw_common}
                    ${_fw}/ibex_rom.S ${_fw}/ibex_start.S ${_fw}/t07_boot_tests.c ${_fw}/t07_cva6_payload.S
                    -T ${_fw}/ibex.ld -o fu3_t07_boot.elf
            DEPENDS ${_fw}/ibex_rom.S ${_fw}/ibex_start.S ${_fw}/t07_boot_tests.c ${_fw}/t07_cva6_payload.S
                    ${CMAKE_CURRENT_SOURCE_DIR}/include/fu3_vp_abi.h
                    ${_fw}/ibex.ld ${_fw}/fw_trap.S ${_fw}/fw_runtime.c ${_fw}/fw_common.h ${_fw}/fw_debug.S
                    ${_fw}/fw_debug.c
            COMMENT "RISC-V: fu3_t07_boot.elf")
        # T09 / Q03-d: CVA6 ISR on the real TIMER0 (late ISR, CMP acknowledge, re-arm).
        add_custom_command(OUTPUT fu3_t09_timer.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imfc_zicsr_zifencei -mabi=ilp32f ${_fw_common}
                    ${_fw}/cva6_start.S ${_fw}/t09_timer_tests.c -T ${_fw}/cva6.ld -o fu3_t09_timer.elf
            DEPENDS ${_fw}/cva6_start.S ${_fw}/t09_timer_tests.c ${_fw}/cva6.ld ${_fw}/fw_trap.S
                    ${_fw}/fw_runtime.c ${_fw}/fw_common.h ${_fw}/fw_debug.S ${_fw}/fw_debug.c
            COMMENT "RISC-V: fu3_t09_timer.elf")
        # FU3_VP_ABI_1 boot smoke (Ibex from reset: ABI regs, staging with the
        # debug blob, final PMP, SWIRQ, CVA6 handoff and IPI) and the VP-only
        # debug partition placeholder as a binary artifact.
        add_custom_command(OUTPUT fu3_abi1_boot.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32 ${_fw_common}
                    ${_fw}/ibex_rom.S ${_fw}/ibex_start.S ${_fw}/abi1_boot_tests.c ${_fw}/abi1_cva6_payload.S
                    ${_fw}/vp_debug_blob.S -T ${_fw}/ibex.ld -o fu3_abi1_boot.elf
            DEPENDS ${_fw}/ibex_rom.S ${_fw}/ibex_start.S ${_fw}/abi1_boot_tests.c ${_fw}/abi1_cva6_payload.S
                    ${_fw}/vp_debug_blob.S ${_fw}/ibex.ld ${_fw}/fw_trap.S ${_fw}/fw_runtime.c ${_fw}/fw_common.h
                    ${_fw}/fw_debug.S ${_fw}/fw_debug.c ${CMAKE_CURRENT_SOURCE_DIR}/include/fu3_vp_abi.h
            COMMENT "RISC-V: fu3_abi1_boot.elf")
        get_filename_component(_riscv_bin ${FU3_SOC_RISCV_GCC} DIRECTORY)
        add_custom_command(OUTPUT vp_debug_blob.bin
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32i_zicsr -mabi=ilp32 -c ${_fw}/vp_debug_blob.S -o vp_debug_blob.o
            COMMAND ${_riscv_bin}/riscv-none-elf-objcopy -O binary -j .rodata.vp_debug_blob vp_debug_blob.o vp_debug_blob.bin
            DEPENDS ${_fw}/vp_debug_blob.S
            COMMENT "RISC-V: vp_debug_blob.bin (VP-only debug partition placeholder)")
        # Plan T20: console demo run by vp_fu3_soc itself (no debug code segment,
        # so the image loader accepts it).
        add_custom_command(OUTPUT fu3_t20_console.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32 -O1 -ffreestanding -nostdlib
                    -nostartfiles -msmall-data-limit=0 -Wall -Wextra -Werror -I${_fw}
                    -I${CMAKE_CURRENT_SOURCE_DIR}/include ${_fw}/fw_trap.S ${_fw}/fw_runtime.c ${_fw}/ibex_rom.S
                    ${_fw}/ibex_start.S ${_fw}/t20_console.c -T ${_fw}/ibex.ld -o fu3_t20_console.elf
            DEPENDS ${_fw}/fw_trap.S ${_fw}/fw_runtime.c ${_fw}/fw_common.h ${_fw}/ibex_rom.S ${_fw}/ibex_start.S
                    ${_fw}/t20_console.c ${_fw}/ibex.ld ${CMAKE_CURRENT_SOURCE_DIR}/include/fu3_vp_abi.h
            COMMENT "RISC-V: fu3_t20_console.elf")
        # Plan T19: GDB test pair (no debug code segment either). Ibex: two
        # boots around a software reset, then the CVA6 handoff; CVA6: FP values.
        set(_t19_common -O1 -g -ffreestanding -nostdlib -nostartfiles -msmall-data-limit=0 -Wall -Wextra -Werror
            -I${_fw} -I${CMAKE_CURRENT_SOURCE_DIR}/include ${_fw}/fw_trap.S ${_fw}/fw_runtime.c)
        add_custom_command(OUTPUT fu3_t19_ibex.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32 ${_t19_common}
                    ${_fw}/ibex_rom.S ${_fw}/ibex_start.S ${_fw}/t19_ibex.c -T ${_fw}/ibex.ld -o fu3_t19_ibex.elf
            DEPENDS ${_fw}/fw_trap.S ${_fw}/fw_runtime.c ${_fw}/fw_common.h ${_fw}/ibex_rom.S ${_fw}/ibex_start.S
                    ${_fw}/t19_ibex.c ${_fw}/ibex.ld
            COMMENT "RISC-V: fu3_t19_ibex.elf")
        add_custom_command(OUTPUT fu3_t19_cva6.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imfc_zicsr_zifencei -mabi=ilp32f ${_t19_common}
                    ${_fw}/cva6_start.S ${_fw}/t19_cva6.c -T ${_fw}/cva6.ld -o fu3_t19_cva6.elf
            DEPENDS ${_fw}/fw_trap.S ${_fw}/fw_runtime.c ${_fw}/fw_common.h ${_fw}/cva6_start.S ${_fw}/t19_cva6.c
                    ${_fw}/cva6.ld
            COMMENT "RISC-V: fu3_t19_cva6.elf")
        # Plan T21: WFI wakes from real sources on both CPUs, a gated source,
        # SWIRQ both ways, then a WDT0 reset in the middle of their traffic.
        add_custom_command(OUTPUT fu3_t21_ibex.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32 ${_t19_common}
                    ${_fw}/ibex_rom.S ${_fw}/ibex_start.S ${_fw}/t21_ibex.c -T ${_fw}/ibex.ld -o fu3_t21_ibex.elf
            DEPENDS ${_fw}/fw_trap.S ${_fw}/fw_runtime.c ${_fw}/fw_common.h ${_fw}/ibex_rom.S ${_fw}/ibex_start.S
                    ${_fw}/t21_ibex.c ${_fw}/ibex.ld ${CMAKE_CURRENT_SOURCE_DIR}/include/fu3_vp_abi.h
            COMMENT "RISC-V: fu3_t21_ibex.elf")
        add_custom_command(OUTPUT fu3_t21_cva6.elf
            COMMAND ${FU3_SOC_RISCV_GCC} -march=rv32imfc_zicsr_zifencei -mabi=ilp32f ${_t19_common}
                    ${_fw}/cva6_start.S ${_fw}/t21_cva6.c -T ${_fw}/cva6.ld -o fu3_t21_cva6.elf
            DEPENDS ${_fw}/fw_trap.S ${_fw}/fw_runtime.c ${_fw}/fw_common.h ${_fw}/cva6_start.S ${_fw}/t21_cva6.c
                    ${_fw}/cva6.ld ${CMAKE_CURRENT_SOURCE_DIR}/include/fu3_vp_abi.h
            COMMENT "RISC-V: fu3_t21_cva6.elf")
        add_custom_target(fu3_soc_t05_firmware ALL
            DEPENDS fu3_t05_cva6.elf fu3_t05_ibex.elf fu3_t07_handoff.elf fu3_t07_boot.elf fu3_t09_timer.elf
                    fu3_abi1_boot.elf vp_debug_blob.bin fu3_t20_console.elf
                    fu3_t19_ibex.elf fu3_t19_cva6.elf fu3_t21_ibex.elf fu3_t21_cva6.elf)
        add_executable(test_fu3_soc_boot tests/test_fu3_soc_boot.cpp)
        target_compile_options(test_fu3_soc_boot PRIVATE -Wall -Wextra)
        target_link_libraries(test_fu3_soc_boot PRIVATE fu3_soc_core)
        add_test(NAME fu3_soc_boot COMMAND test_fu3_soc_boot ${CMAKE_CURRENT_BINARY_DIR}/fu3_t07_boot.elf)
        # The same runs with a small global quantum (default 1 us) give the same verdicts.
        add_test(NAME fu3_soc_boot_q100 COMMAND test_fu3_soc_boot -q 100 ${CMAKE_CURRENT_BINARY_DIR}/fu3_t07_boot.elf)
        set_tests_properties(fu3_soc_boot fu3_soc_boot_q100 PROPERTIES TIMEOUT 120 LABELS "fu3;fu3_soc;fu3_cpu")
        add_test(NAME fu3_soc_cpu_cva6 COMMAND test_fu3_soc_cpu cva6 ${CMAKE_CURRENT_BINARY_DIR}/fu3_t05_cva6.elf
                 ${CMAKE_CURRENT_BINARY_DIR}/fu3_t07_handoff.elf)
        add_test(NAME fu3_soc_cpu_ibex COMMAND test_fu3_soc_cpu ibex ${CMAKE_CURRENT_BINARY_DIR}/fu3_t05_ibex.elf)
        add_test(NAME fu3_soc_cpu_cva6_q10 COMMAND test_fu3_soc_cpu -q 10 cva6
                 ${CMAKE_CURRENT_BINARY_DIR}/fu3_t05_cva6.elf ${CMAKE_CURRENT_BINARY_DIR}/fu3_t07_handoff.elf)
        add_test(NAME fu3_soc_cpu_ibex_q10 COMMAND test_fu3_soc_cpu -q 10 ibex ${CMAKE_CURRENT_BINARY_DIR}/fu3_t05_ibex.elf)
        add_test(NAME fu3_soc_timer_irq COMMAND test_fu3_soc_cpu cva6 ${CMAKE_CURRENT_BINARY_DIR}/fu3_t09_timer.elf
                 ${CMAKE_CURRENT_BINARY_DIR}/fu3_t07_handoff.elf)
        add_test(NAME fu3_soc_timer_irq_q10 COMMAND test_fu3_soc_cpu -q 10 cva6
                 ${CMAKE_CURRENT_BINARY_DIR}/fu3_t09_timer.elf ${CMAKE_CURRENT_BINARY_DIR}/fu3_t07_handoff.elf)
        add_test(NAME fu3_soc_abi1_boot COMMAND test_fu3_soc_cpu ibex ${CMAKE_CURRENT_BINARY_DIR}/fu3_abi1_boot.elf)
        add_test(NAME fu3_soc_abi1_boot_q10 COMMAND test_fu3_soc_cpu -q 10 ibex ${CMAKE_CURRENT_BINARY_DIR}/fu3_abi1_boot.elf)
        # Review ABI1-R01: DEBUG_EN high from power-on (board), released at 1 ms,
        # after the firmware reached its wait (about 0.5 ms).
        add_test(NAME fu3_soc_abi1_boot_debug_en COMMAND test_fu3_soc_cpu -d 1000 ibex ${CMAKE_CURRENT_BINARY_DIR}/fu3_abi1_boot.elf)
        set_tests_properties(fu3_soc_cpu_cva6 fu3_soc_cpu_ibex fu3_soc_cpu_cva6_q10 fu3_soc_cpu_ibex_q10
            fu3_soc_timer_irq fu3_soc_timer_irq_q10 fu3_soc_abi1_boot fu3_soc_abi1_boot_q10 fu3_soc_abi1_boot_debug_en
            PROPERTIES TIMEOUT 120 LABELS "fu3;fu3_soc;fu3_cpu")

        # Plan T20: the console demo through the command line (straps, console
        # input, NOR through XIP, dump of a DSRAM signature) and the verdicts.
        set(_t20 ${CMAKE_CURRENT_BINARY_DIR}/fu3_t20_console.elf)
        add_test(NAME fu3_soc_cli_console COMMAND ${CMAKE_COMMAND} -DCMD=$<TARGET_FILE:vp_fu3_soc>
            "-DARGS=--rom;${_t20};--mode;10;--boot-pin;1;--uart-rx-file;${_t20_data}/t20_rx.txt;--uart-rx-delay-us;400;--flash;${_t20_data}/t20_flash.bin;--dump;0x0103f000:4:${CMAKE_CURRENT_BINARY_DIR}/t20_signature.bin;--expect-uart;PASS;--fail-uart;FAIL;--timeout-ms;20"
            -DEXPECT_RC=0
            "-DEXPECT_OUTPUT=straps MODE=10 BOOT=1 DEBUG_EN=0 valid=1.*reset cause 0x00000001.*rx: hello vp.*flash 0x44332211.*\\[UART0\\] PASS.*vp_fu3_soc: PASS \\(console printed \"PASS\"\\)"
            -DCHECK_FILE=${CMAKE_CURRENT_BINARY_DIR}/t20_signature.bin -DCHECK_HEX=20eeffc0
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_exit.cmake)
        fu3_soc_cli_test(fu3_soc_cli_debug_en 0 "straps MODE=00 BOOT=0 DEBUG_EN=1 valid=1"
                         --rom ${_t20} --debug-en 1 --expect-uart PASS --timeout-ms 20)
        fu3_soc_cli_test(fu3_soc_cli_debug_en_release 0 "DEBUG_EN=1 \\(released at 50 us\\).*straps MODE=00 BOOT=0 DEBUG_EN=0"
                         --rom ${_t20} --debug-en 1 --debug-en-release-us 50 --expect-uart PASS --timeout-ms 20)
        # --elf alone (its ROM segment counts as the ROM) and a NOR offset.
        fu3_soc_cli_test(fu3_soc_cli_elf_flash_offset 0 "flash 0x2211ffff.*vp_fu3_soc: PASS"
                         --elf ${_t20} --flash ${_t20_data}/t20_flash.bin@2 --expect-uart PASS --timeout-ms 20)
        # Review R01: the fail text wins, also when it follows the expected one
        # in the same console line (verdict judged on the output up to the stop).
        fu3_soc_cli_test(fu3_soc_cli_pass_then_fail 1 "rx: PASS FAIL.*vp_fu3_soc: FAIL \\(console printed \"FAIL\"\\)"
                         --rom ${_t20} --uart-rx-file ${_t20_data}/t20_rx_pass_fail.txt --uart-rx-delay-us 400
                         --expect-uart PASS --fail-uart FAIL --timeout-ms 20)
        fu3_soc_cli_test(fu3_soc_cli_fail_only 1 "rx: FAIL.*vp_fu3_soc: FAIL \\(console printed \"FAIL\"\\)"
                         --rom ${_t20} --uart-rx-file ${_t20_data}/t20_rx_fail.txt --uart-rx-delay-us 400
                         --expect-uart PASS --fail-uart FAIL --timeout-ms 20)
        fu3_soc_cli_test(fu3_soc_cli_timeout 2 "TIMEOUT \\(console did not print \"NEVER\" within 2 ms\\)"
                         --rom ${_t20} --expect-uart NEVER --timeout-ms 2)
        fu3_soc_cli_test(fu3_soc_cli_fail_uart 1 "FAIL \\(console printed \"reset cause\"\\)"
                         --rom ${_t20} --fail-uart "reset cause" --expect-uart PASS)
        fu3_soc_cli_test(fu3_soc_cli_debug_partition 3 "segment 3 \\[0x00060000, 0x000600c8\\) touches the Ibex debug partition"
                         --rom ${CMAKE_CURRENT_BINARY_DIR}/fu3_abi1_boot.elf)
        fu3_soc_cli_test(fu3_soc_cli_cva6_as_rom 3 "touches the CVA6 debug partition"
                         --rom ${CMAKE_CURRENT_BINARY_DIR}/fu3_t05_cva6.elf)
        # Plan T21: wakes and a watchdog reset during CPU traffic, run by the CLI;
        # no warning may appear (an access the reset catches is not an error).
        add_test(NAME fu3_soc_wake_reset COMMAND ${CMAKE_COMMAND} -DCMD=$<TARGET_FILE:vp_fu3_soc>
            "-DARGS=--rom;${CMAKE_CURRENT_BINARY_DIR}/fu3_t21_ibex.elf;--elf;${CMAKE_CURRENT_BINARY_DIR}/fu3_t21_cva6.elf;--expect-uart;PASS;--fail-uart;FAIL;--timeout-ms;50"
            -DEXPECT_RC=0
            "-DEXPECT_OUTPUT=ibex wfi timer1 ok.*ibex gated timer1 no wake ok.*cva6 wfi 0x80000007.*ibex wfi swirq ok.*wdt armed.*T21 boot 2.*reset cause 0x00000009.*vp_fu3_soc: PASS"
            "-DFORBID_OUTPUT=Warning|Error"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_exit.cmake)
        set_tests_properties(fu3_soc_wake_reset PROPERTIES TIMEOUT 120 LABELS "fu3;fu3_soc;fu3_t21;fu3_cpu")
        # Plan T19: the GDB server over its remote protocol (tests/gdb_rsp_test.py).
        find_program(FU3_SOC_PYTHON3 python3 HINTS /usr/bin)
        if(FU3_SOC_PYTHON3)
            foreach(_case session kill nowait gdb checksum)
                add_test(NAME fu3_soc_gdb_rsp_${_case} COMMAND ${FU3_SOC_PYTHON3} -I
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/gdb_rsp_test.py $<TARGET_FILE:vp_fu3_soc>
                         ${CMAKE_CURRENT_BINARY_DIR} ${_riscv_bin}/riscv-none-elf-nm ${_case})
                set_tests_properties(fu3_soc_gdb_rsp_${_case} PROPERTIES TIMEOUT 180 LABELS "fu3;fu3_soc;fu3_gdb")
            endforeach()
        endif()
        set_tests_properties(fu3_soc_cli_console fu3_soc_cli_debug_en fu3_soc_cli_debug_en_release
            fu3_soc_cli_elf_flash_offset fu3_soc_cli_pass_then_fail fu3_soc_cli_fail_only fu3_soc_cli_timeout
            fu3_soc_cli_fail_uart fu3_soc_cli_debug_partition fu3_soc_cli_cva6_as_rom
            PROPERTIES TIMEOUT 60 LABELS "fu3;fu3_soc;fu3_cli")
    else()
        message(STATUS "fu3_soc: riscv-none-elf-gcc not found, CPU firmware tests (T05) are not registered")
    endif()

    # Plan T07: clock, reset and handoff through the CRM and SYSCSR.
    add_executable(test_fu3_soc_clock_reset tests/test_fu3_soc_clock_reset.cpp)
    target_compile_options(test_fu3_soc_clock_reset PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_clock_reset PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_clock_reset COMMAND test_fu3_soc_clock_reset)

    # Plans T08-T18: peripheral models, bus access and interrupt routes.
    add_executable(test_fu3_soc_peripherals tests/test_fu3_soc_peripherals.cpp)
    target_compile_options(test_fu3_soc_peripherals PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_peripherals PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_peripherals COMMAND test_fu3_soc_peripherals)
    set_tests_properties(fu3_soc_peripherals PROPERTIES TIMEOUT 120 LABELS "fu3;fu3_soc")
    # Plan T18: debugger through JTAG2AXI to the bus and the Debug Module.
    add_executable(test_fu3_soc_debug tests/test_fu3_soc_debug.cpp)
    target_compile_options(test_fu3_soc_debug PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_debug PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_debug COMMAND test_fu3_soc_debug)
    set_tests_properties(fu3_soc_debug PROPERTIES TIMEOUT 120 LABELS "fu3;fu3_soc")
    # Plan T21: connection audit (ports, sockets, nets, tie-offs).
    add_executable(test_fu3_soc_topology tests/test_fu3_soc_topology.cpp)
    target_compile_options(test_fu3_soc_topology PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_topology PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_topology COMMAND test_fu3_soc_topology)
    set_tests_properties(fu3_soc_topology PROPERTIES TIMEOUT 60 LABELS "fu3;fu3_soc;fu3_t21")
    # Plan T21: system reset while an access to each bus target is on its way.
    add_executable(test_fu3_soc_reset tests/test_fu3_soc_reset.cpp)
    target_compile_options(test_fu3_soc_reset PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_reset PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_reset_in_flight COMMAND test_fu3_soc_reset)
    set_tests_properties(fu3_soc_reset_in_flight PROPERTIES TIMEOUT 120 LABELS "fu3;fu3_soc;fu3_t21")
    # Plan T21: no spurious interrupt across reset, clock gate, peripheral reset.
    add_executable(test_fu3_soc_irq_reset tests/test_fu3_soc_irq_reset.cpp)
    target_compile_options(test_fu3_soc_irq_reset PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_irq_reset PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_irq_reset COMMAND test_fu3_soc_irq_reset)
    set_tests_properties(fu3_soc_irq_reset PROPERTIES TIMEOUT 120 LABELS "fu3;fu3_soc;fu3_t21")
    # Plan T21: debug halt during XIP loads in flight, resets while halted.
    add_executable(test_fu3_soc_halt tests/test_fu3_soc_halt.cpp)
    target_compile_options(test_fu3_soc_halt PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_halt PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_halt_in_flight COMMAND test_fu3_soc_halt)
    set_tests_properties(fu3_soc_halt_in_flight PROPERTIES TIMEOUT 120 LABELS "fu3;fu3_soc;fu3_t21")
    # Plan T22: breakpoint (trigger) and single step of both harts through JTAG and the DM.
    add_executable(test_fu3_soc_jtag_run_control tests/test_fu3_soc_jtag_run_control.cpp)
    target_compile_options(test_fu3_soc_jtag_run_control PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_jtag_run_control PRIVATE fu3_soc_core)
    add_test(NAME fu3_soc_jtag_run_control COMMAND test_fu3_soc_jtag_run_control)
    set_tests_properties(fu3_soc_jtag_run_control PROPERTIES TIMEOUT 120 LABELS "fu3;fu3_soc;fu3_t22")
    # Plan T19: DM / GDB ownership of a hart (Q04-d), also through the TCP server.
    add_executable(test_fu3_soc_gdb tests/test_fu3_soc_gdb.cpp)
    target_compile_options(test_fu3_soc_gdb PRIVATE -Wall -Wextra)
    target_link_libraries(test_fu3_soc_gdb PRIVATE fu3_soc_core)
    find_package(Threads REQUIRED)
    target_link_libraries(test_fu3_soc_gdb PRIVATE Threads::Threads)
    add_test(NAME fu3_soc_gdb_arbitration COMMAND test_fu3_soc_gdb)
    set_tests_properties(fu3_soc_gdb_arbitration PROPERTIES TIMEOUT 120 LABELS "fu3;fu3_soc;fu3_gdb")

    set_tests_properties(fu3_soc_skeleton fu3_soc_memory fu3_soc_self_test fu3_soc_list_instances fu3_soc_clock_reset
        fu3_soc_cli_unknown_option fu3_soc_run_without_image
        PROPERTIES TIMEOUT 60 LABELS "fu3;fu3_soc")
endif()
