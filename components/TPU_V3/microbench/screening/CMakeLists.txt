# SPDX-License-Identifier: Apache-2.0
#
# Gate G4: one-factor-at-a-time screening of the supported knobs.
#
# The full screening is 170 points and takes about fifty seconds; CTest runs a
# reduced one over a single benchmark so the gate stays fast, and the audit
# records the full result. What the gate proves is that the screening is sound
# — every point valid, every conservation identity balanced, the summary
# well-formed — not what the screening concluded. A conclusion belongs in the
# audit, where it can be read with its limits.

find_package(Python3 COMPONENTS Interpreter QUIET)

if(NOT Python3_Interpreter_FOUND)
    message(STATUS
        "Python 3 not found; the G4 screening tests will not be registered")
    return()
endif()

set(NEO_SCREEN_TOOL "${CMAKE_CURRENT_SOURCE_DIR}/neo_core_screen.py")
set(NEO_SCREEN_OUT "${CMAKE_CURRENT_BINARY_DIR}/out")

set(NEO_SCREEN_COMMON
    --runner "$<TARGET_FILE:neo_core_bench_runner>"
    --firmware "${BENCH_FW_BUILD}"
    --revision-file "${BENCH_REVISION_FILE}"
    --build-type "$<CONFIG>")

add_test(NAME tpu_v3_g4_screening
    COMMAND ${Python3_EXECUTABLE} "${NEO_SCREEN_TOOL}" ${NEO_SCREEN_COMMON}
            --benchmark relu --out "${NEO_SCREEN_OUT}/relu")
set_tests_properties(tpu_v3_g4_screening PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g4"
    TIMEOUT 1800)

# The MXU path separately: it is the one where bank width matters, so a
# screening that only ever ran the hart would report the knob insensitive.
add_test(NAME tpu_v3_g4_screening_mxu
    COMMAND ${Python3_EXECUTABLE} "${NEO_SCREEN_TOOL}" ${NEO_SCREEN_COMMON}
            --benchmark gemm --knob local_bank_width_bits
            --knob fabric_pipeline_stages --out "${NEO_SCREEN_OUT}/gemm")
set_tests_properties(tpu_v3_g4_screening_mxu PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g4"
    TIMEOUT 1800)

# The aggregation's own maths, driven with constructed rows. It reaches the
# cases a simulation cannot: a knob with no baseline point, a metric absent
# from some points, a baseline of zero.
# `verify_row()` has no coverage from a real run, because a real run never
# produces a row that disagrees with the request. That is why it needs a test
# rather than why it does not.
# The comparator's own failure modes. Two of these are cases the first version
# could not see: it compared aggregated min/max per knob, and those survive
# both a permutation of the interior points and any change to how an interval
# was spent.
add_test(NAME tpu_v3_g4_screening_report_unit
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_screening_report.py")
set_tests_properties(tpu_v3_g4_screening_report_unit PROPERTIES
    LABELS "tpu_v3;unit;microbench;d27;g4"
    TIMEOUT 300)

add_test(NAME tpu_v3_g4_screening_verify_unit
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_screening_verify.py")
set_tests_properties(tpu_v3_g4_screening_verify_unit PROPERTIES
    LABELS "tpu_v3;unit;microbench;d27;g4"
    TIMEOUT 300)

add_test(NAME tpu_v3_g4_screening_summary_unit
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_screening_summary.py")
set_tests_properties(tpu_v3_g4_screening_summary_unit PROPERTIES
    LABELS "tpu_v3;unit;microbench;d27;g4"
    TIMEOUT 300)

# The full screening the audit quotes, as a reproducible target rather than a
# command someone once ran.
#
# It carries `tpu_v3`, so it **is** in the default gate: two screenings of 170
# points each take about a minute of wall time in parallel, and the whole
# `tpu_v3` label runs in roughly 52 seconds against about 10 without them. That
# is the right trade — the audit's central table is generated from these runs,
# and evidence that is not regenerated is evidence that rots. The extra
# `g4_full` label exists to run or exclude them by name, not to keep them out
# of the gate.
add_test(NAME tpu_v3_g4_screening_full
    COMMAND ${Python3_EXECUTABLE} "${NEO_SCREEN_TOOL}" ${NEO_SCREEN_COMMON}
            --out "${NEO_SCREEN_OUT}/full")
set_tests_properties(tpu_v3_g4_screening_full PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g4;g4_full"
    FIXTURES_SETUP g4_full_screening
    TIMEOUT 3600)

# The same screening under `arbitrated`. It is what supports the claim that the
# two timing modes agree on elapsed time here, and it must be run rather than
# asserted.
add_test(NAME tpu_v3_g4_screening_full_arbitrated
    COMMAND ${Python3_EXECUTABLE} "${NEO_SCREEN_TOOL}" ${NEO_SCREEN_COMMON}
            --timing arbitrated --out "${NEO_SCREEN_OUT}/full_arbitrated")
set_tests_properties(tpu_v3_g4_screening_full_arbitrated PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g4;g4_full"
    FIXTURES_SETUP g4_full_screening
    TIMEOUT 3600)

# The comparator, and the reason the audit may say its table is generated.
#
# Running both screenings proves they run. It does not check that either one
# still supports what the audit says about them, and the last G4 defect lived
# exactly there: the measurement was complete and the presentation was not.
# This regenerates the table from the summary, requires the audit to contain it
# verbatim, and re-asserts every numeric claim the surrounding prose makes.
add_test(NAME tpu_v3_g4_report_matches_the_audit
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/neo_core_screen_report.py"
            --annotated "${NEO_SCREEN_OUT}/full"
            --arbitrated "${NEO_SCREEN_OUT}/full_arbitrated"
            --audit "${CMAKE_SOURCE_DIR}/components/TPU_V3/docs/TPU_V3_STANDALONE_DSE_AUDIT.md"
            --write-table "${NEO_SCREEN_OUT}/screening_table.md")
set_tests_properties(tpu_v3_g4_report_matches_the_audit PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g4;g4_full"
    FIXTURES_REQUIRED g4_full_screening
    TIMEOUT 600)

# ── controls ─────────────────────────────────────────────────────────────────
#
# A screening that averaged a rejected run would produce a sensitivity figure
# for a machine whose bytes did not reconcile. This proves it stops instead.
add_test(NAME tpu_v3_g4_control_invalid_point_aborts
    COMMAND ${Python3_EXECUTABLE} "${NEO_SCREEN_TOOL}" ${NEO_SCREEN_COMMON}
            --benchmark relu --knob local_bank_count
            --fault-inject-counter sram_bytes_read
            --out "${NEO_SCREEN_OUT}/control_invalid")
set_tests_properties(tpu_v3_g4_control_invalid_point_aborts PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g4;negative_control"
    PASS_REGULAR_EXPRESSION "screening point failed"
    TIMEOUT 1800)

# A knob name the tool does not know must be refused rather than screened as
# an empty set, which would report every knob insensitive.
# Mixed with a valid name, so it also covers the case that used to pass: a
# typo alongside a real knob was filtered away, and the screening reported the
# missing knob exactly as it reports a knob it screened and found inert.
add_test(NAME tpu_v3_g4_control_unknown_knob_refused
    COMMAND ${Python3_EXECUTABLE} "${NEO_SCREEN_TOOL}" ${NEO_SCREEN_COMMON}
            --benchmark relu --knob local_bank_count --knob no_such_knob
            --out "${NEO_SCREEN_OUT}/control_knob")
set_tests_properties(tpu_v3_g4_control_unknown_knob_refused PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g4;negative_control"
    PASS_REGULAR_EXPRESSION "no known knob"
    TIMEOUT 300)

add_test(NAME tpu_v3_g4_control_unknown_benchmark_refused
    COMMAND ${Python3_EXECUTABLE} "${NEO_SCREEN_TOOL}" ${NEO_SCREEN_COMMON}
            --benchmark relu --benchmark no_such_benchmark
            --out "${NEO_SCREEN_OUT}/control_bench")
set_tests_properties(tpu_v3_g4_control_unknown_benchmark_refused PROPERTIES
    LABELS "tpu_v3;integration;microbench;d27;g4;negative_control"
    PASS_REGULAR_EXPRESSION "no known benchmark"
    TIMEOUT 300)
