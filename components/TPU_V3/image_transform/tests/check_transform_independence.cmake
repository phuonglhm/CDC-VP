# SPDX-License-Identifier: Apache-2.0
# Prove the Phase 6 component reaches data only through native_port and did not
# drag an NPU top, matrix engine, DMA, external AXI master or SRAM backing into
# the extracted transform.

foreach(_required SOURCE_DIR ARCHIVE)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "-${_required}= is required")
    endif()
endforeach()

file(GLOB_RECURSE _sources
    "${SOURCE_DIR}/include/*.h"
    "${SOURCE_DIR}/src/*.cpp")
if(NOT _sources)
    message(FATAL_ERROR "no ImageTransform sources found; guard would be vacuous")
endif()

set(_failures "")
string(ASCII 10 _newline)
foreach(_file IN LISTS _sources)
    file(READ "${_file}" _text)
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" _code "${_text}")
    string(REGEX REPLACE "//[^${_newline}]*" "" _code "${_code}")
    string(TOLOWER "${_code}" _lower)
    get_filename_component(_name "${_file}" NAME)

    string(REGEX MATCHALL "#include[^${_newline}]*" _includes "${_lower}")
    foreach(_include IN LISTS _includes)
        if(_include MATCHES "core_sram\\.h|npu_top|sauria|dma_tlm|neo_dma")
            list(APPEND _failures "${_name} has forbidden include ${_include}")
        endif()
    endforeach()
    foreach(_token get_direct_mem_ptr set_dmi_ptr simple_initiator_socket)
        if(_lower MATCHES "${_token}")
            list(APPEND _failures "${_name} uses forbidden token ${_token}")
        endif()
    endforeach()
endforeach()

if(NM AND EXISTS "${NM}")
    execute_process(COMMAND "${NM}" -u -C "${ARCHIVE}"
                    OUTPUT_VARIABLE _undefined ERROR_QUIET)
    string(TOLOWER "${_undefined}" _symbols)
    foreach(_token nputop sauria neo_dma dma_tlm core_sram)
        if(_symbols MATCHES "${_token}")
            list(APPEND _failures "archive references ${_token}")
        endif()
    endforeach()
endif()

if(DEFINED LINK_LIBRARIES)
    string(TOLOWER "${LINK_LIBRARIES}" _links)
    foreach(_token sauria npu_tlm neo_dma dma_tlm core_sram tpu_v3_tpu_core)
        if(_links MATCHES "${_token}")
            list(APPEND _failures
                "image transform links ${_token}: ${LINK_LIBRARIES}")
        endif()
    endforeach()
endif()

if(_failures)
    foreach(_failure IN LISTS _failures)
        message(SEND_ERROR "FAIL: ${_failure}")
    endforeach()
    message(FATAL_ERROR "ImageTransform dependency boundary failed")
endif()

message(STATUS
    "ImageTransform independence: PASS (native port only; no external master, "
    "SRAM backing, DMA or matrix/NPU top dependency)")
