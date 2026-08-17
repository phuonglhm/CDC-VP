# SPDX-License-Identifier: Apache-2.0
#
# Every internal signal of `matrix_composition` must have a driver.
#
# ## Why this is a source-level gate and not a test
#
# SystemC requires *ports* to be bound and diagnoses a dangling one (E109), so
# `test_matrix_composition.cpp` reaching `sc_start` proves all 429 bindings
# resolved. It proves nothing about drivers: an `sc_signal` that no process ever
# writes is perfectly legal and simply holds its default value.
#
# That is not a hypothetical. The composition's bindings were generated from
# `npu_top.h` by filtering to the kept module *instances*, which keeps every
# binding a signal appears in and drops every `SC_METHOD` of `NpuTop` that wrote
# one. Eight signals ended up with a reader and no writer, four of them the
# array's operand inputs — the engine was multiplying zeros, and the elaboration
# test passed.
#
# Nothing in the SystemC API exposes "does this signal have a writer", so the
# check has to read the source. It is coarse by design: it does not attempt to
# understand the code, only to insist that each declared `s_*` name appears
# somewhere as the argument of an output-port binding or as the target of a
# `.write()`. A signal that passes this and is still wrong is a wiring mistake,
# which is the differential's job; a signal that fails it cannot be right.

# Script mode starts with no policies set, so `IN_LIST` below is not an operator
# unless CMP0057 is asked for. Without this the gate fails to *parse*, which ctest
# reports as a failure — loud, but for the wrong reason.
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0057 NEW)

if(NOT DEFINED HEADER)
    message(FATAL_ERROR "driver_coverage_gate.cmake requires -DHEADER=<path>")
endif()
if(NOT EXISTS "${HEADER}")
    message(FATAL_ERROR "driver coverage: no such file: ${HEADER}")
endif()

file(STRINGS "${HEADER}" _lines)

set(_declared "")
set(_driven "")
set(_read "")

foreach(_line IN LISTS _lines)
    # A comment line mentioning a signal name must not count as a driver, and a
    # commented-out binding must not either. Cheap and strict: drop anything from
    # the first `//` onwards before looking for bindings.
    string(REGEX REPLACE "//.*$" "" _code "${_line}")

    # ── declarations: `sc_signal<...> s_name{"s_name"};` ─────────────────────
    if(_code MATCHES "sc_signal<.*>[ \t]+(s_[A-Za-z0-9_]+)[ \t]*\\{")
        list(APPEND _declared "${CMAKE_MATCH_1}")
    endif()

    # ── drivers: `.o_xxx(s_name)` and `s_name.write(` ────────────────────────
    #
    # MATCHALL because several bindings share a line in the generated text.
    string(REGEX MATCHALL "\\.[ \t]*o_[A-Za-z0-9_]+[ \t]*\\([ \t]*s_[A-Za-z0-9_]+[ \t]*\\)"
           _out_bindings "${_code}")
    foreach(_binding IN LISTS _out_bindings)
        string(REGEX REPLACE "^.*\\([ \t]*(s_[A-Za-z0-9_]+)[ \t]*\\)$" "\\1"
               _name "${_binding}")
        list(APPEND _driven "${_name}")
    endforeach()

    string(REGEX MATCHALL "(s_[A-Za-z0-9_]+)[ \t]*\\.[ \t]*write[ \t]*\\("
           _writes "${_code}")
    foreach(_write IN LISTS _writes)
        string(REGEX REPLACE "^(s_[A-Za-z0-9_]+).*$" "\\1" _name "${_write}")
        list(APPEND _driven "${_name}")
    endforeach()

    # ── readers, for the diagnostic only ─────────────────────────────────────
    string(REGEX MATCHALL "\\.[ \t]*i_[A-Za-z0-9_]+[ \t]*\\([ \t]*s_[A-Za-z0-9_]+[ \t]*\\)"
           _in_bindings "${_code}")
    foreach(_binding IN LISTS _in_bindings)
        string(REGEX REPLACE "^.*\\([ \t]*(s_[A-Za-z0-9_]+)[ \t]*\\)$" "\\1"
               _name "${_binding}")
        list(APPEND _read "${_name}")
    endforeach()
endforeach()

list(REMOVE_DUPLICATES _declared)
list(REMOVE_DUPLICATES _driven)
list(REMOVE_DUPLICATES _read)

list(LENGTH _declared _declared_count)
if(_declared_count EQUAL 0)
    # The gate parsed nothing. That is a broken gate, not a clean result — the
    # distinction the earlier hygiene gates got wrong twice by reporting success
    # for a check that could not fail.
    message(FATAL_ERROR
        "driver coverage: no signal declarations were found in ${HEADER}. The "
        "gate's declaration pattern no longer matches the source, so it is "
        "checking nothing.")
endif()

set(_undriven "")
foreach(_signal IN LISTS _declared)
    if(NOT _signal IN_LIST _driven)
        list(APPEND _undriven "${_signal}")
    endif()
endforeach()

list(LENGTH _undriven _undriven_count)
if(NOT _undriven_count EQUAL 0)
    set(_report "")
    foreach(_signal IN LISTS _undriven)
        if(_signal IN_LIST _read)
            string(APPEND _report
                   "\n  ${_signal}  - bound to a module input, so it feeds a "
                   "kept module with its default value")
        else()
            string(APPEND _report "\n  ${_signal}  - never read either; dead")
        endif()
    endforeach()
    message(FATAL_ERROR
        "driver coverage: ${_undriven_count} of ${_declared_count} signals in "
        "${HEADER} have no driver.${_report}\n\n"
        "A signal with a reader and no writer holds its default value for the "
        "whole simulation. SystemC does not diagnose it and elaboration does "
        "not fail, so the engine runs and produces plausible wrong numbers. If "
        "a signal is genuinely meant to be constant, drive it explicitly from a "
        "process so the intent is in the source rather than in its absence.")
endif()

message(STATUS
    "driver coverage: ${_declared_count} signals declared, all driven")
