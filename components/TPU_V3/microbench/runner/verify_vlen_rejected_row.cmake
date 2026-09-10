# SPDX-License-Identifier: Apache-2.0
#
# Integration control for the rejected-row contract. This executes the real
# runner, not the serializer in isolation: a verdict moved back ahead of the
# live identity gates must make this test fail.

foreach(_required RUNNER ELF REVISION_FILE RESULT_FILE BUILD_TYPE)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} was not provided")
    endif()
endforeach()

file(REMOVE "${RESULT_FILE}")
execute_process(
    COMMAND "${RUNNER}"
            --seed 20260825
            --build-type "${BUILD_TYPE}"
            --source-revision-file "${REVISION_FILE}"
            --elf "${ELF}"
            --benchmark relu
            --config-id bringup_vlen_control
            --case 0
            --impl scalar
            --mode kernel
            --expect-vlen-bits 256
            --result "${RESULT_FILE}"
    RESULT_VARIABLE _status
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)

set(_log "${_stdout}\n${_stderr}")
if(NOT _status EQUAL 1)
    message(FATAL_ERROR
        "the mismatched VLEN run returned ${_status}, expected 1\n${_log}")
endif()

set(_diagnostic
    "the guest reports vlenb = 64 (512-bit VLEN) but this build is configured for 256-bit")
string(FIND "${_log}" "${_diagnostic}" _diagnostic_at)
if(_diagnostic_at EQUAL -1)
    message(FATAL_ERROR
        "the run did not reach the live VLEN diagnostic\n${_log}")
endif()
if(NOT EXISTS "${RESULT_FILE}")
    message(FATAL_ERROR "the rejected run did not write ${RESULT_FILE}")
endif()

file(READ "${RESULT_FILE}" _row)
foreach(_claim IN ITEMS
        "\"run_valid\": false"
        "\"kind\": \"harness\""
        "\"passed\": true"
        "\"has_mismatch\": false"
        "\"mismatch_count\": 0"
        "all 3 output elements match the independently computed host golden")
    string(FIND "${_row}" "${_claim}" _claim_at)
    if(_claim_at EQUAL -1)
        message(FATAL_ERROR
            "the rejected row does not contain '${_claim}'\n${_row}")
    endif()
endforeach()

message(STATUS
    "live VLEN mismatch rejected the run while preserving golden correctness")
