# SPDX-License-Identifier: Apache-2.0
#
# Every counter source the runner hooks must have a mutation control.
#
# This exists because the gap it closes has already been found twice by a
# reviewer and never by the suite: once as two model-side controls missing the
# `g3` label, once as three terms of a sum with no control at all. Both were
# invisible to a green test run, because a control that does not exist cannot
# fail.
#
# The runner enumerates its own hooks as the identities execute, so this
# compares the control matrix against what the code actually reads rather than
# against a list maintained by hand. Two runs are needed to see every source:
# an end-to-end RVV case reaches the DMA and hart terms, an MXU kernel case
# reaches the matrix engine's.

# `-P` script mode starts with no policy settings, and `IN_LIST` needs
# CMP0057. Without this the comparisons below fail to parse rather than
# comparing, which would look like the check finding a problem.
cmake_minimum_required(VERSION 3.16)

foreach(_required RUNNER RELU_ELF GEMM_ELF REVISION_FILE BUILD_TYPE CONTROLLED)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "${_required} was not provided")
    endif()
endforeach()

set(_hooked "")
foreach(_probe "${RELU_ELF};relu;rvv;end_to_end" "${GEMM_ELF};gemm;mxu;kernel")
    list(GET _probe 0 _elf)
    list(GET _probe 1 _benchmark)
    list(GET _probe 2 _impl)
    list(GET _probe 3 _mode)
    execute_process(
        COMMAND "${RUNNER}"
                --seed 20260825
                --build-type "${BUILD_TYPE}"
                --source-revision-file "${REVISION_FILE}"
                --config-id reference
                --elf "${_elf}" --benchmark ${_benchmark}
                --case 4 --impl ${_impl} --mode ${_mode}
                --list-counter-sources --quiet
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
        RESULT_VARIABLE _status)
    if(NOT _status EQUAL 0)
        message(FATAL_ERROR
            "enumerating counter sources failed for ${_benchmark}/${_impl}/"
            "${_mode} (${_status})\n${_stdout}\n${_stderr}")
    endif()
    string(REGEX MATCHALL "counter-source: [a-z_]+" _lines "${_stdout}")
    foreach(_line IN LISTS _lines)
        string(REPLACE "counter-source: " "" _name "${_line}")
        list(APPEND _hooked "${_name}")
    endforeach()
endforeach()

list(REMOVE_DUPLICATES _hooked)
list(LENGTH _hooked _hooked_count)
if(_hooked_count EQUAL 0)
    message(FATAL_ERROR
        "the runner reported no counter sources at all; this check would "
        "pass vacuously")
endif()

set(_missing "")
foreach(_name IN LISTS _hooked)
    if(NOT "${_name}" IN_LIST CONTROLLED)
        list(APPEND _missing "${_name}")
    endif()
endforeach()

set(_stale "")
foreach(_name IN LISTS CONTROLLED)
    if(NOT "${_name}" IN_LIST _hooked)
        list(APPEND _stale "${_name}")
    endif()
endforeach()

if(_missing)
    message(FATAL_ERROR
        "counter sources the runner reads with no mutation control: ${_missing}")
endif()
if(_stale)
    message(FATAL_ERROR
        "mutation controls naming counter sources the runner no longer reads: "
        "${_stale}")
endif()

message(STATUS
    "counter-source control matrix complete: ${_hooked_count} sources, "
    "${_hooked_count} controls")
