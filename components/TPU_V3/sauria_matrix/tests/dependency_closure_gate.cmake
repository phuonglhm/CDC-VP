# SPDX-License-Identifier: Apache-2.0
#
# Proves the Phase 5 binary contains the selected matrix closure and none of the
# NPU-top blocks the architecture explicitly excludes.

foreach(_required SOURCE_DIR BINARY NM)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR
            "dependency_closure_gate.cmake: -D${_required}= is required")
    endif()
endforeach()

if(NOT EXISTS "${BINARY}" OR NOT EXISTS "${NM}")
    message(FATAL_ERROR "dependency closure input is absent")
endif()

set(_failures "")
string(ASCII 10 _newline)
file(GLOB_RECURSE _sources
    "${SOURCE_DIR}/include/*.h"
    "${SOURCE_DIR}/src/*.cpp")
foreach(_file ${_sources})
    file(READ "${_file}" _text)
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" _code "${_text}")
    string(REGEX REPLACE "//[^${_newline}]*" "" _code "${_code}")
    string(TOLOWER "${_code}" _code)
    foreach(_needle
            "npu_top\\.h"
            "sauria_dma\\.h"
            "instruction_decoder\\.h"
            "#include[^${_newline}]*obp"
            "#include[^${_newline}]*rce"
            "#include[^${_newline}]*reduction"
            "nputop[ \\t]*<"
            "sauriadma[ \\t]*<"
            "instructiondecoder[ \\t]*<"
            "obp[ \\t]*<"
            "rce[ \\t]*<"
            "reductionengine[ \\t]*<")
        if(_code MATCHES "${_needle}")
            list(APPEND _failures
                "${_file} includes/refers to excluded source '${_needle}'")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND "${NM}" -C "${BINARY}"
    RESULT_VARIABLE _nm_result
    OUTPUT_VARIABLE _symbols
    ERROR_VARIABLE _nm_error)
if(NOT _nm_result EQUAL 0)
    message(FATAL_ERROR "nm failed: ${_nm_error}")
endif()

foreach(_required_symbol
        "sauria::SystolicArray"
        "sauria::IfmapFeeder"
        "sauria::WeightFeeder"
        "sauria::Control"
        "sauria::Psm")
    string(FIND "${_symbols}" "${_required_symbol}" _present)
    if(_present EQUAL -1)
        list(APPEND _failures
            "built adapter has no ${_required_symbol}; the matrix closure is incomplete")
    endif()
endforeach()

foreach(_excluded_symbol
        "sauria::NpuTop"
        "sauria::SauriaDma"
        "sauria::InstructionDecoder"
        "sauria::Obp"
        "sauria::Rce"
        "sauria::ReductionEngine")
    string(FIND "${_symbols}" "${_excluded_symbol}" _present)
    if(NOT _present EQUAL -1)
        list(APPEND _failures
            "built adapter contains excluded ${_excluded_symbol}")
    endif()
endforeach()

if(DEFINED LINK_LIBRARIES)
    string(TOLOWER "${LINK_LIBRARIES}" _links)
    foreach(_excluded_link npu_tlm dma_tlm tpu_v3_core_sram tpu_v3_tpu_core)
        if(_links MATCHES "${_excluded_link}")
            list(APPEND _failures
                "Sauria library directly links excluded target '${_excluded_link}': ${LINK_LIBRARIES}")
        endif()
    endforeach()
endif()

if(_failures)
    foreach(_failure ${_failures})
        message(SEND_ERROR "FAIL: ${_failure}")
    endforeach()
    message(FATAL_ERROR "Phase 5 matrix dependency closure is not clean")
endif()

message(STATUS
    "Sauria dependency closure: PASS (array/feeders/control/PSM present; "
    "NpuTop/DMA/decoder/OBP/RCE/reduction absent)")
