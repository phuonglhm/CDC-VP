# SPDX-License-Identifier: Apache-2.0
#
# The Phase 4 independence gate.
#
# Decision record D14 says the NEO DMA is owned by TPU_V3 and is independent of
# Sauria; plan §11.5 adds that it is not the shared PL330-style DMA component
# under another name, and that it never takes a direct pointer to core-SRAM or
# global-memory backing.
#
# All three are easy to state and easy to violate later by including one
# convenient header. This script is what makes them hold: it reads the DMA's
# own sources with comments stripped, and it reads the symbols the compiler
# actually emitted. Checking the sources alone would miss a dependency arriving
# through a transitive header; checking the symbols alone would miss a
# copy-pasted register map that happens not to link anything.
#
# Required: -DSOURCE_DIR=, -DARCHIVE=, -DNM=

foreach(_required SOURCE_DIR ARCHIVE)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "check_dma_independence.cmake: -D${_required}= is required")
    endif()
endforeach()

set(_failures "")

# ── 1. the sources ────────────────────────────────────────────────────────────
#
# Comments are stripped first, because the header comments have to be able to
# *name* what they must not use — "not Sauria's DMA" is the clearest way to say
# it, and a guard that forbade saying so would push the reasoning out of the
# code and into a document nobody reads next to it.

file(GLOB_RECURSE _sources
    "${SOURCE_DIR}/include/*.h"
    "${SOURCE_DIR}/src/*.cpp")

if(_sources STREQUAL "")
    message(FATAL_ERROR
        "check_dma_independence.cmake: no sources under ${SOURCE_DIR}; the "
        "guard would pass vacuously")
endif()

# Identifiers that must not appear anywhere in code.
set(_forbidden_tokens
    "dma_tlm|the shared PL330-style DMA component (plan §11.5 reuse boundary)"
    "sauria|Sauria (decision record D14 forbids any Sauria DMA dependency)"
    "npu_tlm|the NPU model tree"
    "DMAMOV|a PL330 channel-program opcode"
    "DMALD|a PL330 channel-program opcode"
    "DMAST|a PL330 channel-program opcode"
    "get_direct_mem_ptr|DMI, which would bypass arbitration, bounds checks and every counter"
    "set_dmi_ptr|DMI, which would bypass arbitration, bounds checks and every counter"
)

# Headers that must not be included.
#
# Separate from the token list because the distinction matters: naming
# `address_map::core_sram_window` is exactly right — the window is a fact about
# the map — while including `core_sram.h` would mean the DMA had reached past
# the native port to the storage behind it. A guard that banned the identifier
# would forbid the correct code along with the wrong code, and the usual next
# step is to weaken or delete the guard.
set(_forbidden_includes
    "core_sram\\.h|the SRAM implementation; the DMA must reach storage only through the native port interface"
    "dma_tlm|the shared PL330-style DMA component"
    "sauria|any Sauria header"
    "npu_tlm|the NPU model tree"
)

# A real newline, built rather than written. Two CMake details make the
# obvious spellings wrong, and both cost a debugging session to find:
#
#   * `[^\n]` in a CMake regex is "not backslash, not n", so a whole-file
#     `//[^\n]*` strips almost nothing;
#   * `file(STRINGS)` splits its input at bytes it does not consider text,
#     including the middle of a UTF-8 em dash. A comment containing one comes
#     back as two fragments and the second has lost its `//`, so the guard
#     trips on its own documentation.
#
# Reading the file whole and cutting at an actual newline character avoids
# both.
string(ASCII 10 _newline)

foreach(_file ${_sources})
    file(READ "${_file}" _text)
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" _code "${_text}")
    string(REGEX REPLACE "//[^${_newline}]*" "" _code "${_code}")
    string(TOLOWER "${_code}" _lower)

    get_filename_component(_name "${_file}" NAME)
    foreach(_entry ${_forbidden_tokens})
        string(REGEX REPLACE "\\|.*" "" _token "${_entry}")
        string(REGEX REPLACE "^[^|]*\\|" "" _why "${_entry}")
        string(TOLOWER "${_token}" _needle)
        if(_lower MATCHES "${_needle}")
            list(APPEND _failures
                "${_name} refers to '${_token}' in code: ${_why}")
        endif()
    endforeach()

    # Includes, checked on the directive rather than on the identifier.
    string(REGEX MATCHALL "#include[^${_newline}]*" _includes "${_lower}")
    foreach(_entry ${_forbidden_includes})
        string(REGEX REPLACE "\\|.*" "" _pattern "${_entry}")
        string(REGEX REPLACE "^[^|]*\\|" "" _why "${_entry}")
        string(TOLOWER "${_pattern}" _needle)
        foreach(_include ${_includes})
            if(_include MATCHES "${_needle}")
                list(APPEND _failures
                    "${_name} has '${_include}': ${_why}")
            endif()
        endforeach()
    endforeach()
endforeach()

# ── 2. the built archive ──────────────────────────────────────────────────────
#
# Undefined symbols are what a transitive dependency actually looks like once
# the compiler is done with it.

if(NOT EXISTS "${ARCHIVE}")
    message(FATAL_ERROR
        "check_dma_independence.cmake: ${ARCHIVE} was not built")
endif()

if(NM AND EXISTS "${NM}")
    execute_process(COMMAND "${NM}" --defined-only -C "${ARCHIVE}"
                    OUTPUT_VARIABLE _defined ERROR_QUIET)
    execute_process(COMMAND "${NM}" -u -C "${ARCHIVE}"
                    OUTPUT_VARIABLE _undefined ERROR_QUIET)
    string(TOLOWER "${_defined}${_undefined}" _symbols)

    foreach(_needle sauria dma_tlm pl330)
        if(_symbols MATCHES "${_needle}")
            list(APPEND _failures
                "the built archive references a symbol containing '${_needle}'")
        endif()
    endforeach()
else()
    message(STATUS
        "check_dma_independence: no usable nm, symbol check skipped "
        "(the source scan still ran)")
endif()

# ── 3. the CMake link interface ───────────────────────────────────────────────

if(DEFINED LINK_LIBRARIES)
    string(TOLOWER "${LINK_LIBRARIES}" _links)
    foreach(_needle dma_tlm sauria npu_tlm core_sram)
        if(_links MATCHES "${_needle}")
            list(APPEND _failures
                "tpu_v3_neo_dma links a target matching '${_needle}': ${LINK_LIBRARIES}")
        endif()
    endforeach()
endif()

# ── verdict ───────────────────────────────────────────────────────────────────

if(_failures)
    foreach(_failure ${_failures})
        message(SEND_ERROR "FAIL: ${_failure}")
    endforeach()
    message(FATAL_ERROR
        "the NEO DMA is not independent. Plan §11.5 and decision record D14 "
        "require it to be implemented and owned under TPU_V3, reaching core "
        "SRAM only through the native port interface.")
endif()

message(STATUS "neo_dma independence: PASS")
