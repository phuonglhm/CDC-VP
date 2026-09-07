# SPDX-License-Identifier: Apache-2.0
#
# The G2 standalone runner and the MB1–MB4 gate.
#
# The image is built in the build tree, not in the source tree, for the reasons
# the Phase 7 gate records: a Release and a Debug tree would otherwise race for
# one output file, a read-only checkout could not build at all, and a stale
# gitignored ELF could be run without anyone noticing it was never rebuilt.

set(BENCH_FW_SRC "${CMAKE_SOURCE_DIR}/fw/TPU_V3_NEO_CORE_MICROBENCH")
set(BENCH_FW_BUILD "${CMAKE_CURRENT_BINARY_DIR}/firmware")
set(BENCH_TOOLDIR "/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin"
    CACHE PATH "Directory holding the riscv-none-elf cross toolchain (D4)")
set(BENCH_RELU_ELF "${BENCH_FW_BUILD}/relu/neo_bench_relu.elf")
set(BENCH_DOT_ELF "${BENCH_FW_BUILD}/vector_dot/neo_bench_vector_dot.elf")
set(BENCH_GEMV_RVV_ELF "${BENCH_FW_BUILD}/gemv_rvv/neo_bench_gemv_rvv.elf")
set(BENCH_GEMV_MXU_ELF "${BENCH_FW_BUILD}/gemv_mxu/neo_bench_gemv_mxu.elf")
set(BENCH_GEMM_ELF "${BENCH_FW_BUILD}/gemm/neo_bench_gemm.elf")
set(BENCH_RESULTS "${CMAKE_CURRENT_BINARY_DIR}/results")
file(MAKE_DIRECTORY "${BENCH_RESULTS}")

set(BENCH_FW_COMMON crt0.S link.ld bench_map.h matrix_main.c)
set(BENCH_FW_RELU main.c Makefile)
set(BENCH_FW_DOT main.c Makefile)
set(BENCH_FW_GEMV_RVV main.c Makefile)
set(BENCH_FW_MXU Makefile)

# §8.1 wants the source revision in every row. Regenerated on every build, not
# at configure time, so it names the tree that produced the binary; the script
# appends `-dirty` when the working tree is modified, because a row produced
# from a dirty tree is not reproducible from the commit it names.
set(BENCH_REVISION_FILE "${CMAKE_CURRENT_BINARY_DIR}/source_revision.txt")
add_custom_target(neo_bench_source_revision ALL
    COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DOUTPUT_FILE=${BENCH_REVISION_FILE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/write_source_revision.cmake"
    BYPRODUCTS "${BENCH_REVISION_FILE}"
    COMMENT "Recording the source revision for benchmark result rows"
    VERBATIM)

add_executable(neo_core_bench_runner neo_core_bench_runner.cpp)
add_dependencies(neo_core_bench_runner neo_bench_source_revision)
target_link_libraries(neo_core_bench_runner PRIVATE
    cdc::components::tpu_v3_neo_core
    cdc::components::tpu_v3_common
    cdc::components::tpu_v3_microbench)
# The firmware's own map header, so the runner can assert it against
# `address_map.h` rather than trusting that two copies of the same constants
# agree. Plan §8 replaces this with a generated header in Phase 10.
target_include_directories(neo_core_bench_runner PRIVATE
    "${BENCH_FW_SRC}/common")

# ── the D27 dependency closure ───────────────────────────────────────────────
#
# Same two guards the G1 baseline uses, and deliberately the same script rather
# than a copy: a second copy would be a second thing to keep correct. The
# configure-time check catches someone adding a retained chip/NoC target; the
# post-link symbol check catches a hidden or renamed link edge that reaches
# actual chip/NoC implementation code.
get_target_property(_g2_runner_links neo_core_bench_runner LINK_LIBRARIES)
get_target_property(_g2_core_links tpu_v3_neo_core LINK_LIBRARIES)
string(JOIN ";" _g2_dependency_evidence
    "${_g2_runner_links}" "${_g2_core_links}")
if(_g2_dependency_evidence MATCHES
   "(tpu_v3_tpu_chip|tpu_v3_chip_fabric|tpu_v3_noc_endpoint|floo_noc|noc_interconnect)")
    message(FATAL_ERROR
        "G2 standalone dependency closure contains chip/NoC code: "
        "${_g2_dependency_evidence}")
endif()

set(BENCH_GUARD_SCRIPT
    "${CMAKE_SOURCE_DIR}/components/TPU_V3/tpu_core/tests/neo_core/check_standalone_dependencies.cmake")

add_custom_command(TARGET neo_core_bench_runner POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            "-DBINARY=$<TARGET_FILE:neo_core_bench_runner>"
            "-DNM=${CMAKE_NM}"
            -P "${BENCH_GUARD_SCRIPT}"
    COMMENT "Checking the G2 runner for chip/NoC dependencies"
    VERBATIM)

# ── the MB1–MB4 images ──────────────────────────────────────────────────────

if(EXISTS "${BENCH_TOOLDIR}/riscv-none-elf-gcc")
    set(_bench_copied "")
    foreach(_f IN LISTS BENCH_FW_COMMON)
        add_custom_command(
            OUTPUT "${BENCH_FW_BUILD}/common/${_f}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${BENCH_FW_BUILD}/common"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${BENCH_FW_SRC}/common/${_f}"
                    "${BENCH_FW_BUILD}/common/${_f}"
            DEPENDS "${BENCH_FW_SRC}/common/${_f}"
            VERBATIM)
        list(APPEND _bench_copied "${BENCH_FW_BUILD}/common/${_f}")
    endforeach()
    foreach(_f IN LISTS BENCH_FW_RELU)
        add_custom_command(
            OUTPUT "${BENCH_FW_BUILD}/relu/${_f}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${BENCH_FW_BUILD}/relu"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${BENCH_FW_SRC}/relu/${_f}"
                    "${BENCH_FW_BUILD}/relu/${_f}"
            DEPENDS "${BENCH_FW_SRC}/relu/${_f}"
            VERBATIM)
        list(APPEND _bench_copied "${BENCH_FW_BUILD}/relu/${_f}")
    endforeach()

    set(_bench_dot_copied "")
    foreach(_f IN LISTS BENCH_FW_DOT)
        add_custom_command(
            OUTPUT "${BENCH_FW_BUILD}/vector_dot/${_f}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${BENCH_FW_BUILD}/vector_dot"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${BENCH_FW_SRC}/vector_dot/${_f}"
                    "${BENCH_FW_BUILD}/vector_dot/${_f}"
            DEPENDS "${BENCH_FW_SRC}/vector_dot/${_f}"
            VERBATIM)
        list(APPEND _bench_dot_copied "${BENCH_FW_BUILD}/vector_dot/${_f}")
    endforeach()

    set(_bench_gemv_rvv_copied "")
    foreach(_f IN LISTS BENCH_FW_GEMV_RVV)
        add_custom_command(
            OUTPUT "${BENCH_FW_BUILD}/gemv_rvv/${_f}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${BENCH_FW_BUILD}/gemv_rvv"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${BENCH_FW_SRC}/gemv_rvv/${_f}"
                    "${BENCH_FW_BUILD}/gemv_rvv/${_f}"
            DEPENDS "${BENCH_FW_SRC}/gemv_rvv/${_f}"
            VERBATIM)
        list(APPEND _bench_gemv_rvv_copied
             "${BENCH_FW_BUILD}/gemv_rvv/${_f}")
    endforeach()

    foreach(_dir gemv_mxu gemm)
        foreach(_f IN LISTS BENCH_FW_MXU)
            add_custom_command(
                OUTPUT "${BENCH_FW_BUILD}/${_dir}/${_f}"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        "${BENCH_FW_BUILD}/${_dir}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${BENCH_FW_SRC}/${_dir}/${_f}"
                        "${BENCH_FW_BUILD}/${_dir}/${_f}"
                DEPENDS "${BENCH_FW_SRC}/${_dir}/${_f}"
                VERBATIM)
            list(APPEND _bench_${_dir}_copied
                 "${BENCH_FW_BUILD}/${_dir}/${_f}")
        endforeach()
    endforeach()

    add_custom_command(
        OUTPUT "${BENCH_RELU_ELF}"
        COMMAND ${CMAKE_COMMAND} -E env "TOOLDIR=${BENCH_TOOLDIR}"
                make -C "${BENCH_FW_BUILD}/relu"
        DEPENDS ${_bench_copied}
        COMMENT "Building the MB1 ReLU microbenchmark image"
        VERBATIM)
    add_custom_target(neo_bench_relu_image DEPENDS "${BENCH_RELU_ELF}")

    add_custom_command(
        OUTPUT "${BENCH_DOT_ELF}"
        COMMAND ${CMAKE_COMMAND} -E env "TOOLDIR=${BENCH_TOOLDIR}"
                make -C "${BENCH_FW_BUILD}/vector_dot"
        DEPENDS ${_bench_copied} ${_bench_dot_copied}
        COMMENT "Building the MB2 vector-dot microbenchmark image"
        VERBATIM)
    add_custom_target(neo_bench_vector_dot_image DEPENDS "${BENCH_DOT_ELF}")

    add_custom_command(
        OUTPUT "${BENCH_GEMV_RVV_ELF}"
        COMMAND ${CMAKE_COMMAND} -E env "TOOLDIR=${BENCH_TOOLDIR}"
                make -C "${BENCH_FW_BUILD}/gemv_rvv"
        DEPENDS ${_bench_copied} ${_bench_gemv_rvv_copied}
        COMMENT "Building the MB3 RVV GEMV microbenchmark image"
        VERBATIM)
    add_custom_target(neo_bench_gemv_rvv_image
        DEPENDS "${BENCH_GEMV_RVV_ELF}")

    add_custom_command(
        OUTPUT "${BENCH_GEMV_MXU_ELF}"
        COMMAND ${CMAKE_COMMAND} -E env "TOOLDIR=${BENCH_TOOLDIR}"
                make -C "${BENCH_FW_BUILD}/gemv_mxu"
        DEPENDS ${_bench_copied} ${_bench_gemv_mxu_copied}
        COMMENT "Building the MB3 MXU GEMV microbenchmark image"
        VERBATIM)
    add_custom_target(neo_bench_gemv_mxu_image
        DEPENDS "${BENCH_GEMV_MXU_ELF}")

    add_custom_command(
        OUTPUT "${BENCH_GEMM_ELF}"
        COMMAND ${CMAKE_COMMAND} -E env "TOOLDIR=${BENCH_TOOLDIR}"
                make -C "${BENCH_FW_BUILD}/gemm"
        DEPENDS ${_bench_copied} ${_bench_gemm_copied}
        COMMENT "Building the MB4 MXU GEMM microbenchmark image"
        VERBATIM)
    add_custom_target(neo_bench_gemm_image DEPENDS "${BENCH_GEMM_ELF}")

    add_dependencies(neo_core_bench_runner
        neo_bench_relu_image neo_bench_vector_dot_image
        neo_bench_gemv_rvv_image neo_bench_gemv_mxu_image
        neo_bench_gemm_image)
else()
    message(STATUS
        "RV32GCV cross toolchain not found at ${BENCH_TOOLDIR}; "
        "the microbenchmark tests will skip")
endif()

# ── the gate ─────────────────────────────────────────────────────────────────
#
# One CTest entry per case, per implementation, per mode: §11 requires one
# invocation per case so state cannot leak between configurations, and a single
# test looping over the ten cases would also report ten results as one.

# `--config-id reference` is checked by the runner against the plan's §7.1
# values and refused when they disagree, so this label cannot drift away from
# the configuration it names.
set(BENCH_COMMON_BASE_NOID
    --seed 20260825
    --build-type "$<CONFIG>"
    --source-revision-file "${BENCH_REVISION_FILE}")

set(BENCH_COMMON_BASE
    --seed 20260825
    --build-type "$<CONFIG>"
    --source-revision-file "${BENCH_REVISION_FILE}"
    --config-id reference)

# The MB1 image and benchmark, for the controls that do not vary them.
set(BENCH_COMMON_ARGS
    ${BENCH_COMMON_BASE}
    --elf "${BENCH_RELU_ELF}"
    --benchmark relu)

# `label` is the plan's benchmark number, `benchmark` its runner name and `elf`
# its own image: each benchmark is a separate ELF so the firmware hash in a row
# identifies the code that ran rather than a shared multiplexer.
function(add_bench_case label benchmark elf case impl mode pattern)
    set(_name
        "tpu_v3_${label}_${benchmark}_c${case}_${impl}_${mode}_${pattern}")
    add_test(NAME ${_name}
        COMMAND neo_core_bench_runner ${BENCH_COMMON_BASE}
                --elf "${elf}" --benchmark ${benchmark}
                --case ${case} --impl ${impl} --mode ${mode}
                --pattern ${pattern}
                --result "${BENCH_RESULTS}/${_name}.json")
    set_tests_properties(${_name} PROPERTIES
        LABELS "tpu_v3;integration;microbench;d27;${label};golden"
        SKIP_RETURN_CODE 77
        TIMEOUT 900)
endfunction()

# The ten frozen sizes, both implementations, both measurement modes, for each
# implemented benchmark.
foreach(_case RANGE 0 9)
    foreach(_impl scalar rvv)
        foreach(_mode kernel end_to_end)
            add_bench_case(mb1 relu "${BENCH_RELU_ELF}"
                ${_case} ${_impl} ${_mode} mixed)
            add_bench_case(mb2 vector_dot "${BENCH_DOT_ELF}"
                ${_case} ${_impl} ${_mode} mixed)
        endforeach()
    endforeach()
    foreach(_mode kernel end_to_end)
        add_bench_case(mb3 gemv_rvv "${BENCH_GEMV_RVV_ELF}"
            ${_case} rvv ${_mode} mixed)
        add_bench_case(mb3 gemv_mxu "${BENCH_GEMV_MXU_ELF}"
            ${_case} mxu ${_mode} mixed)
        add_bench_case(mb4 gemm "${BENCH_GEMM_ELF}"
            ${_case} mxu ${_mode} mixed)
    endforeach()
endforeach()

# §5.1's edge patterns, at the case that straddles the VLEN=512 lane boundary
# (N = 17). Running them at every size would multiply the matrix by five for
# coverage the mixed pattern already provides; running them at none would leave
# "all-negative, all-positive, zero and representable boundary values" as an
# unmet clause of the arithmetic baseline.
foreach(_pattern all_negative all_positive zero boundary)
    foreach(_impl scalar rvv)
        add_bench_case(mb1 relu "${BENCH_RELU_ELF}" 8 ${_impl} kernel
            ${_pattern})
        add_bench_case(mb2 vector_dot "${BENCH_DOT_ELF}" 8 ${_impl} kernel
            ${_pattern})
    endforeach()
    add_bench_case(mb3 gemv_rvv "${BENCH_GEMV_RVV_ELF}" 4 rvv kernel
        ${_pattern})
    add_bench_case(mb3 gemv_mxu "${BENCH_GEMV_MXU_ELF}" 4 mxu kernel
        ${_pattern})
    add_bench_case(mb4 gemm "${BENCH_GEMM_ELF}" 4 mxu kernel
        ${_pattern})
endforeach()

# ── negative controls ────────────────────────────────────────────────────────
#
# G2 requires a control for one arithmetic operation, one DMA leg and one
# measurement-boundary marker. Each one names the detection that must fire:
# `--expect-detect` makes the runner exit 0 only when the run failed *and*
# failed for the stated reason, so a control cannot pass because the harness
# broke in some other way. A plain WILL_FAIL would accept exactly that.

# `extra_labels` exists because a control's *subject* is not always its
# benchmark. The two model-side G3 mutations run MB1's image but are gates on
# the conservation accounting, and a `-L '^g3$'` run that skipped them would
# report the targeted gate as green while its two load-bearing controls had not
# executed.
function(add_mb1_control name extra_labels)
    add_test(NAME tpu_v3_mb1_control_${name}
        COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS} ${ARGN}
                --result "${BENCH_RESULTS}/control_${name}.json")
    set_tests_properties(tpu_v3_mb1_control_${name} PROPERTIES
        LABELS "tpu_v3;integration;microbench;d27;mb1;negative_control${extra_labels}"
        SKIP_RETURN_CODE 77
        TIMEOUT 900)
endfunction()

function(add_mb2_control name)
    add_test(NAME tpu_v3_mb2_control_${name}
        COMMAND neo_core_bench_runner ${BENCH_COMMON_BASE}
                --elf "${BENCH_DOT_ELF}" --benchmark vector_dot ${ARGN}
                --result "${BENCH_RESULTS}/mb2_control_${name}.json")
    set_tests_properties(tpu_v3_mb2_control_${name} PROPERTIES
        LABELS "tpu_v3;integration;microbench;d27;mb2;negative_control"
        SKIP_RETURN_CODE 77
        TIMEOUT 900)
endfunction()

function(add_matrix_control label benchmark elf impl name)
    add_test(NAME tpu_v3_${label}_${benchmark}_control_${name}
        COMMAND neo_core_bench_runner ${BENCH_COMMON_BASE}
                --elf "${elf}" --benchmark ${benchmark} --impl ${impl}
                ${ARGN}
                --result "${BENCH_RESULTS}/${label}_${benchmark}_control_${name}.json")
    set_tests_properties(tpu_v3_${label}_${benchmark}_control_${name} PROPERTIES
        LABELS "tpu_v3;integration;microbench;d27;${label};negative_control"
        SKIP_RETURN_CODE 77
        TIMEOUT 900)
endfunction()

# The corrupted arithmetic is in the guest: the kernel clamps to 1 instead of
# 0, so the operation itself is wrong. A control that perturbed the golden
# would prove only that the comparison compares.
add_mb1_control(arith_rvv ""
    --case 8 --impl rvv --mode kernel --inject arith
    --expect-detect golden_mismatch)
add_mb1_control(arith_scalar ""
    --case 8 --impl scalar --mode kernel --inject arith
    --expect-detect golden_mismatch)

# One DMA leg each way. Each names two detections: skipping a leg is visible
# both to the golden and to G3's byte accounting, and a control that asserted
# only the first would leave the conservation identity unproven on exactly the
# mutation it exists to catch.
#
# One DMA leg each way. Skipping DMA-in leaves core SRAM holding zeros, so the
# kernel computes the right answer for the wrong data; skipping DMA-out leaves
# the result in core SRAM, where the guest's own checksum still matches while
# the host's readback does not. The two fail in visibly different ways, which
# is what makes the end-to-end path's two halves separately proven.
add_mb1_control(dma_in_skipped ""
    --case 8 --impl rvv --mode end_to_end --inject dma_in
    --expect-detect golden_mismatch,conservation_broken)
add_mb1_control(dma_out_skipped ""
    --case 8 --impl rvv --mode end_to_end --inject dma_out
    --expect-detect golden_mismatch,conservation_broken)

# The measurement boundary. Without the closing marker there is no interval,
# and the runner must refuse the run rather than report a timing figure derived
# from a boundary that never arrived.
add_mb1_control(measurement_boundary ""
    --case 8 --impl rvv --mode kernel --inject boundary
    --expect-detect interval_not_closed)

# The two usage refusals below name the refusal they expect through
# `--expect-usage`, and pass only when the runner produces it. `WILL_FAIL`
# would accept any non-zero exit, including one caused by a typo in this file's
# own command line — a control that passes because the test is broken proves
# less than no control at all.

# The DMA controls are meaningless in kernel-only mode, and the runner says so
# rather than accepting them.
add_test(NAME tpu_v3_mb1_control_dma_fault_needs_end_to_end
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 8 --impl rvv --mode kernel --inject dma_out
            --expect-usage dma_fault_needs_end_to_end)
set_tests_properties(tpu_v3_mb1_control_dma_fault_needs_end_to_end PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;mb1;negative_control"
    TIMEOUT 120)

# The controls above are only evidence if the runner can also *refuse* to
# satisfy them. These two ask for a refusal that does not happen, and must
# fail: without them, `--expect-usage` could be a flag that always passes.
add_test(NAME tpu_v3_mb1_control_usage_expectation_bites
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 0 --impl rvv --mode kernel
            --expect-usage unimplemented_benchmark)
set_tests_properties(tpu_v3_mb1_control_usage_expectation_bites PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;negative_control"
    WILL_FAIL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

# A run whose guest never reaches the kernel must not satisfy a
# `golden_mismatch` control: the poisoned destination mismatches on its own, so
# without the "nothing else failed" half of the rule this passes while the
# mutation under test never executed. The watchdog is set far too short on
# purpose.
add_test(NAME tpu_v3_mb1_control_detection_requires_a_complete_run
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 8 --impl rvv --mode kernel --inject arith
            --expect-detect golden_mismatch --watchdog-ms 0.001)
set_tests_properties(tpu_v3_mb1_control_detection_requires_a_complete_run
    PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;mb1;negative_control"
    WILL_FAIL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

# And the mirror image: a clean run must not satisfy a control either.
add_test(NAME tpu_v3_mb1_control_detection_requires_a_fault
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 8 --impl rvv --mode kernel
            --expect-detect golden_mismatch)
set_tests_properties(tpu_v3_mb1_control_detection_requires_a_fault PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;mb1;negative_control"
    WILL_FAIL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

# A row may not label itself `reference` while running something else. The
# runner refuses the combination; this proves it does.
add_test(NAME tpu_v3_mb1_control_reference_id_requires_reference_config
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 0 --impl rvv --mode kernel --sram-capacity-bytes 65536)
set_tests_properties(tpu_v3_mb1_control_reference_id_requires_reference_config
    PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;negative_control"
    WILL_FAIL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

# ── the command line refuses what it cannot represent ────────────────────────
#
# Each asserts the *reason* through PASS_REGULAR_EXPRESSION rather than merely
# a non-zero exit: these values were all accepted once, and a `WILL_FAIL` test
# would keep passing if the refusal moved to some unrelated check.

function(add_cli_refusal name pattern)
    add_test(NAME tpu_v3_mb1_control_cli_${name}
        COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
                --case 0 --impl rvv --mode kernel ${ARGN})
    set_tests_properties(tpu_v3_mb1_control_cli_${name} PROPERTIES
        LABELS "tpu_v3;integration;microbench;d27;negative_control"
        PASS_REGULAR_EXPRESSION "${pattern}"
        TIMEOUT 120)
endfunction()

# `std::stoull` accepts a leading minus and returns the two's-complement wrap,
# so this used to run with a seed of 18446744073709551615.
add_cli_refusal(negative_seed "must be a non-negative integer" --seed -1)
# `std::stod` accepts "nan"; a NaN watchdog compares false against every bound.
add_cli_refusal(nan_watchdog "finite positive number" --watchdog-ms nan)
# Cast straight to 32 bits, this used to become a bank count of 1.
add_cli_refusal(oversized_bank_count "fits its field"
    --config-id bringup --bank-count 4294967297)

# ── MB2 negative controls ────────────────────────────────────────────────────
#
# The same four mutations as MB1, because a reduction fails differently from an
# element-wise kernel: a wrong product is invisible in a single output word
# unless the comparison and the guest checksum both look at it.
#
# The injected arithmetic adds one to every product, shifting the sum by
# exactly N. That is detectable at every frozen size — N is never zero — and it
# corrupts the operation rather than the comparison that checks it.
add_mb2_control(arith_rvv
    --case 8 --impl rvv --mode kernel --inject arith
    --expect-detect golden_mismatch)
add_mb2_control(arith_scalar
    --case 8 --impl scalar --mode kernel --inject arith
    --expect-detect golden_mismatch)
add_mb2_control(dma_in_skipped
    --case 8 --impl rvv --mode end_to_end --inject dma_in
    --expect-detect golden_mismatch,conservation_broken)
add_mb2_control(dma_out_skipped
    --case 8 --impl rvv --mode end_to_end --inject dma_out
    --expect-detect golden_mismatch,conservation_broken)
add_mb2_control(measurement_boundary
    --case 8 --impl rvv --mode kernel --inject boundary
    --expect-detect interval_not_closed)

# MB3/MB4 exercise the same independent detection paths with their own ELFs.
# `all_positive` makes the K-1 arithmetic mutation non-vacuous for every
# output element, including the rectangular GEMV cases.
foreach(_workload
        "mb3;gemv_rvv;${BENCH_GEMV_RVV_ELF};rvv"
        "mb3;gemv_mxu;${BENCH_GEMV_MXU_ELF};mxu"
        "mb4;gemm;${BENCH_GEMM_ELF};mxu")
    list(GET _workload 0 _label)
    list(GET _workload 1 _benchmark)
    list(GET _workload 2 _elf)
    list(GET _workload 3 _impl)
    add_matrix_control(${_label} ${_benchmark} "${_elf}" ${_impl} arith
        --case 4 --mode kernel --pattern all_positive --inject arith
        --expect-detect golden_mismatch)
    add_matrix_control(${_label} ${_benchmark} "${_elf}" ${_impl} dma_in_skipped
        --case 4 --mode end_to_end --inject dma_in
        --expect-detect golden_mismatch,conservation_broken)
    add_matrix_control(${_label} ${_benchmark} "${_elf}" ${_impl} dma_out_skipped
        --case 4 --mode end_to_end --inject dma_out
        --expect-detect golden_mismatch,conservation_broken)
    add_matrix_control(${_label} ${_benchmark} "${_elf}" ${_impl} boundary
        --case 4 --mode kernel --inject boundary
        --expect-detect interval_not_closed)
endforeach()

# Each benchmark has its own image, and each image refuses the other's
# identifier. Without this, a mis-wired test would run MB1's kernel and compare
# it against MB2's golden — and the mismatch would look like an arithmetic
# defect in the model rather than a wiring defect in the test.
#
# Asserted by the guest's own status code rather than by a non-zero exit: the
# run fails in several ways at once, and only this one says the image refused
# the benchmark.
add_test(NAME tpu_v3_mb2_control_wrong_image_is_refused
    COMMAND neo_core_bench_runner ${BENCH_COMMON_BASE}
            --elf "${BENCH_RELU_ELF}" --benchmark vector_dot
            --case 0 --impl rvv --mode kernel)
set_tests_properties(tpu_v3_mb2_control_wrong_image_is_refused PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;mb2;negative_control"
    PASS_REGULAR_EXPRESSION "failure status 0xbad00003"
    TIMEOUT 900)

add_test(NAME tpu_v3_mb1_control_wrong_image_is_refused
    COMMAND neo_core_bench_runner ${BENCH_COMMON_BASE}
            --elf "${BENCH_DOT_ELF}" --benchmark relu
            --case 0 --impl rvv --mode kernel)
set_tests_properties(tpu_v3_mb1_control_wrong_image_is_refused PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;mb1;negative_control"
    PASS_REGULAR_EXPRESSION "failure status 0xbad00003"
    TIMEOUT 900)

foreach(_wrong_image
        "mb3;gemv_rvv;${BENCH_GEMV_RVV_ELF};gemv_mxu;mxu"
        "mb3;gemv_mxu;${BENCH_GEMV_MXU_ELF};gemm;mxu"
        "mb4;gemm;${BENCH_GEMM_ELF};gemv_mxu;mxu")
    list(GET _wrong_image 0 _label)
    list(GET _wrong_image 1 _image_name)
    list(GET _wrong_image 2 _image)
    list(GET _wrong_image 3 _asked_benchmark)
    list(GET _wrong_image 4 _asked_impl)
    add_test(NAME tpu_v3_${_label}_${_image_name}_control_wrong_image_is_refused
        COMMAND neo_core_bench_runner ${BENCH_COMMON_BASE}
                --elf "${_image}" --benchmark ${_asked_benchmark}
                --case 0 --impl ${_asked_impl} --mode kernel)
    set_tests_properties(
        tpu_v3_${_label}_${_image_name}_control_wrong_image_is_refused PROPERTIES
        LABELS "tpu_v3;integration;microbench;d27;${_label};negative_control"
        PASS_REGULAR_EXPRESSION "failure status 0xbad00003"
        TIMEOUT 900)
endforeach()

# ── G3: model-side mutations of the counters themselves ──────────────────────
#
# The two below mutate the machine, not the check that reads it. They are the
# answer to "does this counter actually observe the thing it names", which a
# perturbation applied after sampling cannot answer.

# The kernel reads its input a second time. The arithmetic is untouched, so the
# golden still passes and `local_plane_bytes` still balances — the fabric and
# core SRAM both see the larger figure. Only the algorithm's declared traffic
# notices, which is the gap `kernel_local_bytes` exists to close.
add_mb1_control(kernel_reads_input_twice ";g3"
    --case 4 --impl rvv --mode kernel --inject extra_read
    --expect-detect conservation_broken)

# One stage marker is dropped while the stage still runs. The neighbouring span
# widens, so `interval_accounted_ns` still balances exactly; only the expected
# stage sequence sees it. Run end-to-end so that marks remain and the failure
# is the sequence rather than an empty interval.
add_mb1_control(stage_marker_dropped ";g3"
    --case 4 --impl rvv --mode end_to_end --inject skip_stage
    --expect-detect conservation_broken)

# ── G3 conservation: a mutation control per identity ─────────────────────────
#
# G3's last clause asks that every counter used in a conclusion have a
# mutation control. `dma_leg_bytes` has a real model-side one — the DMA-leg
# controls above skip a leg and require `conservation_broken` alongside the
# golden mismatch. The other three identities compare two honest observers of
# the same bytes, and nothing outside the model can make those observers
# disagree; what remains provable is that the identity would notice if they
# did. These perturb one side by a single byte and require the run to be
# rejected for that reason and no other.
#
# The runner refuses a mutation naming an identity the run does not evaluate,
# so a control cannot silently stop testing anything.
foreach(_identity tensor_footprint local_plane_bytes external_boundary_bytes
                  dma_leg_bytes interval_accounted_ns kernel_local_bytes
                  dma_local_bytes dma_external_bytes)
    add_test(NAME tpu_v3_g3_control_${_identity}_mutation
        COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
                --case 0 --impl rvv --mode end_to_end
                --inject-accounting ${_identity}
                --expect-detect conservation_broken)
    set_tests_properties(tpu_v3_g3_control_${_identity}_mutation PROPERTIES
        LABELS "tpu_v3;integration;microbench;d27;g3;negative_control"
        SKIP_RETURN_CODE 77
        TIMEOUT 900)
endforeach()

# ── G3: one mutation per counter source ──────────────────────────────────────
#
# Several identities sum more than one counter, and a mutation on the sum
# cannot tell whether each term is wired in: a term that was dropped, doubled
# or transposed leaves the aggregate mutation still detected and the defect
# still present. These name one term each.
#
# Every requester the fabric enumerates is a term, including the ones that are
# zero throughout MB1-MB4, and both halves of a read/write split are terms of
# their own.
#
# The runner refuses a name no identity reads, and fails when a perturbed
# counter goes unnoticed — that second case is the interesting one, because it
# means the counter appears in an identity without affecting it.
foreach(_counter
        "sram_debug_bytes;relu;rvv;end_to_end"
        "sram_bytes_read;relu;rvv;end_to_end"
        "sram_bytes_written;relu;rvv;end_to_end"
        "hart_external_bytes;relu;rvv;end_to_end"
        "dma_external_bytes;relu;rvv;end_to_end"
        "world_memory_read_bytes;relu;rvv;end_to_end"
        "world_memory_write_bytes;relu;rvv;end_to_end"
        "world_host_io_bytes;relu;rvv;end_to_end"
        "kernel_requester_bytes;relu;rvv;end_to_end"
        "dma_requester_bytes;relu;rvv;end_to_end"
        "dma_external_path_bytes;relu;rvv;end_to_end"
        "dma_local_path_bytes;relu;rvv;end_to_end"
        "fabric_cpu_bytes;relu;rvv;end_to_end"
        "fabric_dma_bytes;relu;rvv;end_to_end"
        "fabric_sa_bytes;gemm;mxu;kernel"
        # Zero in every current benchmark, and still terms of the local-plane
        # total that a conclusion rests on. A counter that is always zero is
        # the easiest one to stop summing by accident and the hardest to
        # notice, which is precisely why it needs the control.
        "fabric_transform_bytes;relu;rvv;end_to_end"
        "fabric_external_inbound_bytes;relu;rvv;end_to_end")
    list(GET _counter 0 _source)
    # Collected here rather than from a second list: one source of truth for
    # which counters have a control.
    list(APPEND BENCH_CONTROLLED_SOURCES "${_source}")
    list(GET _counter 1 _bench)
    list(GET _counter 2 _impl)
    list(GET _counter 3 _mode)
    if(_bench STREQUAL "gemm")
        set(_counter_elf "${BENCH_GEMM_ELF}")
    else()
        set(_counter_elf "${BENCH_RELU_ELF}")
    endif()
    add_test(NAME tpu_v3_g3_control_counter_${_source}
        COMMAND neo_core_bench_runner ${BENCH_COMMON_BASE}
                --elf "${_counter_elf}" --benchmark ${_bench}
                --case 4 --impl ${_impl} --mode ${_mode}
                --inject-counter ${_source}
                --expect-detect conservation_broken)
    set_tests_properties(tpu_v3_g3_control_counter_${_source} PROPERTIES
        LABELS "tpu_v3;integration;microbench;d27;g3;negative_control"
        SKIP_RETURN_CODE 77
        TIMEOUT 900)
endforeach()

# `dma_local_path_bytes` feeds only `dma_leg_bytes`, which exists in end-to-end
# mode alone, so naming it in kernel-only mode must be refused.
#
# `fabric_sa_bytes` would have been the obvious choice and is the wrong one:
# `local_plane_bytes` enumerates every attached requester, so the MXU's counter
# is read on an RVV run too — it is simply zero. Being unused is not the same
# as being unread, and this control asserts the latter.
# The matrix above must cover every source the runner reads. Checked against
# the runner's own enumeration rather than against a hand-kept list: the two
# gaps found in this area so far were both invisible to a green run, because a
# control that does not exist cannot fail.
add_test(NAME tpu_v3_g3_counter_control_matrix_is_complete
    COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:neo_core_bench_runner>"
            "-DRELU_ELF=${BENCH_RELU_ELF}"
            "-DGEMM_ELF=${BENCH_GEMM_ELF}"
            "-DREVISION_FILE=${BENCH_REVISION_FILE}"
            "-DBUILD_TYPE=$<CONFIG>"
            "-DCONTROLLED=${BENCH_CONTROLLED_SOURCES}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_counter_control_matrix.cmake")
set_tests_properties(tpu_v3_g3_counter_control_matrix_is_complete PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g3"
    TIMEOUT 900)

add_test(NAME tpu_v3_g3_control_counter_must_be_read
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 4 --impl rvv --mode kernel
            --inject-counter dma_local_path_bytes)
set_tests_properties(tpu_v3_g3_control_counter_must_be_read PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g3;negative_control"
    PASS_REGULAR_EXPRESSION "no identity in this run reads"
    TIMEOUT 900)

# Naming an identity this run never evaluates must be refused rather than
# quietly passing: `dma_leg_bytes` does not exist in kernel-only mode.
add_test(NAME tpu_v3_g3_control_mutation_must_name_a_live_identity
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 0 --impl rvv --mode kernel
            --inject-accounting dma_leg_bytes)
set_tests_properties(tpu_v3_g3_control_mutation_must_name_a_live_identity
    PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g3;negative_control"
    PASS_REGULAR_EXPRESSION "not an identity this run evaluates"
    TIMEOUT 900)

# ── D28 identity ─────────────────────────────────────────────────────────────

# A reserved profile identity may not label a machine that is not that profile.
# D28: "changing a manifest, a result-row default or a configuration ID while
# the component still uses another value is a configuration defect and must be
# refused". Without this, a reference-machine row could be pooled as promoted
# C1 or C2 evidence.
add_cli_refusal(reserved_profile_id_c1 "does not contain that profile"
    --config-id neo_lite_c1)
add_cli_refusal(reserved_profile_id_c2 "does not contain that profile"
    --config-id neo_lite_c2)

# The CPU identity in a row is a readback, and the readback is checked against
# what the build is supposed to contain. Asking for a VLEN this binary does not
# have must fail.
#
# Two things this control gets wrong if written carelessly, and it was written
# carelessly the first time:
#
#   * `--config-id reference` refuses `--expect-vlen-bits 256` during
#     configuration validation, because the reference profile *is* VLEN 512.
#     The runner then exits before elaboration and the guest never reads a CSR
#     — so the control passed under `WILL_FAIL` while the live-readback path it
#     names was never reached. It therefore runs under its own configuration
#     id;
#   * `WILL_FAIL` alone accepts any non-zero exit, including that one. The
#     assertion is the live diagnostic instead: only the readback path can
#     produce it.
add_test(NAME tpu_v3_mb1_control_vlen_readback_is_checked
    COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:neo_core_bench_runner>"
            "-DELF=${BENCH_RELU_ELF}"
            "-DREVISION_FILE=${BENCH_REVISION_FILE}"
            "-DRESULT_FILE=${BENCH_RESULTS}/control_vlen_rejected_row.json"
            "-DBUILD_TYPE=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_vlen_rejected_row.cmake")
set_tests_properties(tpu_v3_mb1_control_vlen_readback_is_checked PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;d28;negative_control"
    TIMEOUT 120)

# And the same control's own control: the diagnostic must not appear when the
# expected VLEN is the one the build contains.
add_test(NAME tpu_v3_mb1_control_vlen_readback_agrees
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 0 --impl scalar --mode kernel --expect-vlen-bits 512)
set_tests_properties(tpu_v3_mb1_control_vlen_readback_agrees PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;d28;golden"
    FAIL_REGULAR_EXPRESSION "but this build is configured for"
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

# The MXU identity in a row is a readback checked against the build, exactly as
# the CPU identity is. Asking for a geometry this binary does not contain must
# fail; it is how a mismatched Sauria profile would be caught.
# Asserted by its live diagnostic for the same reason as the VLEN control: a
# bare `WILL_FAIL` would accept a refusal issued before elaboration.
add_test(NAME tpu_v3_mb1_control_mxu_readback_is_checked
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 0 --impl rvv --mode kernel --expect-mxu-geometry 32x32)
set_tests_properties(tpu_v3_mb1_control_mxu_readback_is_checked PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;d28;negative_control"
    PASS_REGULAR_EXPRESSION
        "the instantiated MXU reports 64x64 but this build is configured for 32x32"
    TIMEOUT 120)

# ── Gate G5 / WP1: a build may not answer to a profile it is not ─────────────
#
# The comparison behind these runs is the only thing WP1 adds to the runner,
# and it is worth exactly as much as the evidence that it looks at every field.
# Two properties are asserted, and they need different tests:
#
#   * the machine is refused as C1 and as C2, naming precisely the fields that
#     disagree today. `--expect-profile-disagreements` compares sets, so a
#     field that quietly stopped being checked fails the test rather than
#     shortening a message nobody reads;
#   * every field is genuinely compared. Six of the eleven disagree on this
#     machine and are covered by the two runs above; the other five agree, so
#     a comparison that dropped them would look identical. Those need a
#     mutation: `--profile-mutate` swaps one field of the requested profile and
#     the expected set grows by exactly that name.
#
# The runner returns success here on a *refusal*, which reads backwards until
# you notice what is being tested: the subject is the comparison, not the run.
# It also proves it refused before `sc_start` — a truncated simulation would
# otherwise be indistinguishable from a decision.
set(WP1_C2_DISAGREEMENTS
    "dma_channels,external_axi_width_bits,backed_sram_bytes,local_bank_width_bits,local_bank_count,core_period_ns")
# Not C2's set plus the four: C1's local plane is 4 banks of 128 bits, which
# is what the reference machine already has, so those two fields agree and a
# composed set would be wrong. The lists are written out for that reason —
# each is the answer for one profile against one machine, not an expression.
set(WP1_C1_DISAGREEMENTS
    "vlenb,mxu_rows,mxu_columns,mxu_source_profile,dma_channels,external_axi_width_bits,backed_sram_bytes,core_period_ns")

foreach(_profile_case "neo_lite_c1;${WP1_C1_DISAGREEMENTS}"
                      "neo_lite_c2;${WP1_C2_DISAGREEMENTS}")
    list(GET _profile_case 0 _profile)
    list(GET _profile_case 1 _fields)
    add_test(NAME tpu_v3_wp1_control_${_profile}_is_refused
        COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
                --case 0 --impl scalar --mode kernel
                --profile ${_profile}
                --expect-profile-disagreements "${_fields}"
                --result "${BENCH_RESULTS}/control_${_profile}_refused.json")
    set_tests_properties(tpu_v3_wp1_control_${_profile}_is_refused PROPERTIES
        LABELS "tpu_v3;integration;microbench;d28;g5;wp1;negative_control"
        PASS_REGULAR_EXPRESSION
            "before any simulation time elapsed; no row was written"
        TIMEOUT 120)
endforeach()

# The five fields the reference machine already satisfies. Without these, a
# comparison that never looked at VLEN or the MXU would pass every other WP1
# test on this machine, and would go on passing until WP8 and WP9 changed the
# hardware it was supposed to have been guarding all along.
foreach(_mutation "vlenb=32" "mxu_rows=63" "mxu_columns=63"
                  "mxu_source_profile=int8_32x32" "dma_controllers=2")
    string(REGEX REPLACE "=.*$" "" _field "${_mutation}")
    add_test(NAME tpu_v3_wp1_control_mutate_${_field}
        COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
                --case 0 --impl scalar --mode kernel
                --profile neo_lite_c2
                --profile-mutate "${_mutation}"
                --expect-profile-disagreements
                    "${WP1_C2_DISAGREEMENTS},${_field}"
                --result "${BENCH_RESULTS}/control_mutate_${_field}.json")
    set_tests_properties(tpu_v3_wp1_control_mutate_${_field} PROPERTIES
        LABELS "tpu_v3;integration;microbench;d28;g5;wp1;negative_control"
        PASS_REGULAR_EXPRESSION
            "before any simulation time elapsed; no row was written"
        TIMEOUT 120)
endforeach()

# And the mutation control's own control, in the other direction: a swap that
# removes a disagreement must shrink the set. Together with the runs above this
# pins the comparison from both sides — it cannot pass by reporting everything,
# and it cannot pass by reporting nothing.
add_test(NAME tpu_v3_wp1_control_mutation_removes_a_disagreement
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 0 --impl scalar --mode kernel
            --profile neo_lite_c2
            --profile-mutate "backed_sram_bytes=16777216"
            --expect-profile-disagreements
                "dma_channels,external_axi_width_bits,local_bank_width_bits,local_bank_count,core_period_ns")
set_tests_properties(tpu_v3_wp1_control_mutation_removes_a_disagreement
    PROPERTIES
    LABELS "tpu_v3;integration;microbench;d28;g5;wp1;negative_control"
    PASS_REGULAR_EXPRESSION
        "before any simulation time elapsed; no row was written"
    TIMEOUT 120)

# An expectation that does not bite proves nothing, so it is tested too: the
# same run with one field missing from the expected set must fail. `WILL_FAIL`
# with `SKIP_RETURN_CODE` because an inverted control otherwise turns a skip
# into a pass.
add_test(NAME tpu_v3_wp1_control_expectation_bites
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 0 --impl scalar --mode kernel
            --profile neo_lite_c2
            --expect-profile-disagreements "dma_channels")
set_tests_properties(tpu_v3_wp1_control_expectation_bites PROPERTIES
    LABELS "tpu_v3;integration;microbench;d28;g5;wp1;negative_control"
    WILL_FAIL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

# A control flag with no profile to compare against would run the benchmark and
# assert nothing at all.
add_test(NAME tpu_v3_wp1_control_needs_a_profile
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 0 --impl scalar --mode kernel
            --profile none
            --expect-profile-disagreements "dma_channels")
set_tests_properties(tpu_v3_wp1_control_needs_a_profile PROPERTIES
    LABELS "tpu_v3;integration;microbench;d28;g5;wp1;negative_control"
    PASS_REGULAR_EXPRESSION "there is no D28 claim to check"
    TIMEOUT 120)

# `--profile none` is the D27 reference machine and must be unaffected: the
# gate may only refuse a claim that was made.
add_test(NAME tpu_v3_wp1_control_no_profile_still_runs
    COMMAND neo_core_bench_runner ${BENCH_COMMON_ARGS}
            --case 0 --impl scalar --mode kernel --profile none
            --result "${BENCH_RESULTS}/control_profile_none.json")
set_tests_properties(tpu_v3_wp1_control_no_profile_still_runs PROPERTIES
    LABELS "tpu_v3;integration;microbench;d28;g5;wp1"
    FAIL_REGULAR_EXPRESSION "this build cannot run as"
    SKIP_RETURN_CODE 77
    TIMEOUT 900)

# ── the testbench target's own contract ──────────────────────────────────────
#
# `bench_world` is only a testbench, which is exactly why it needs a gate: the
# runner generates well-formed traffic, so a permissive target never misbehaves
# in the runs anyone looks at.
add_executable(test_tpu_v3_bench_world tests/test_bench_world.cpp)
target_link_libraries(test_tpu_v3_bench_world PRIVATE
    cdc::components::tpu_v3_common
    SystemC::systemc)
target_include_directories(test_tpu_v3_bench_world PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${BENCH_FW_SRC}/common")
add_test(NAME tpu_v3_bench_world COMMAND test_tpu_v3_bench_world)
set_tests_properties(tpu_v3_bench_world PROPERTIES
    LABELS "tpu_v3;unit;microbench;d27"
    TIMEOUT 120)

# ── Gate G4: knob screening ──────────────────────────────────────────────────
#
# Added from here rather than from the microbench root because it needs the
# runner target, the firmware build directory and the revision file, all of
# which this file owns.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../screening"
                 "${CMAKE_CURRENT_BINARY_DIR}/screening")

# ── Gate G5 / WP0: Neo Lite feasibility ──────────────────────────────────────
#
# Added from here because it needs `TPU_V3_SAURIA_ROOT` and `BENCH_TOOLDIR`,
# which this file already has in scope.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../neo_lite"
                 "${CMAKE_CURRENT_BINARY_DIR}/neo_lite")
