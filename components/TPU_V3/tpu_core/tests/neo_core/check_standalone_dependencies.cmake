# SPDX-License-Identifier: Apache-2.0
#
# Post-link half of the D27 dependency guard. CMake checks the named target
# graph at configure time; this checks what actually made it into the binary.

if(DEFINED FORCED_SYMBOLS)
    # Mutation-control input: proves the guard rejects a forbidden
    # implementation symbol instead of merely passing today's clean binary.
    set(_symbols "${FORCED_SYMBOLS}")
else()
    if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}")
        message(FATAL_ERROR
            "D27 dependency guard: binary does not exist: ${BINARY}")
    endif()

    if(NOT DEFINED NM OR NM STREQUAL "" OR NOT EXISTS "${NM}")
        message(FATAL_ERROR
            "D27 dependency guard: CMAKE_NM is unavailable: ${NM}")
    endif()

    execute_process(
        COMMAND "${NM}" -C "${BINARY}"
        RESULT_VARIABLE _nm_status
        OUTPUT_VARIABLE _symbols
        ERROR_VARIABLE _nm_error)
    if(NOT _nm_status EQUAL 0)
        message(FATAL_ERROR
            "D27 dependency guard: nm failed (${_nm_status}): ${_nm_error}")
    endif()
endif()

# Match implementation-class namespaces, not shared configuration types or
# diagnostic strings. `tpu_v3_common` legitimately contains the historical
# plain-data `tpu_chip_config`; that does not instantiate or link a chip.
if(_symbols MATCHES
   "(cdc::components::tpu_v3::chip::tpu_chip::|cdc::components::tpu_v3::noc::chip_noc_endpoint::|cdc::components::noc_interconnect::)")
    message(FATAL_ERROR
        "D27 dependency guard: chip/NoC implementation symbol found")
endif()

message(STATUS "D27 standalone dependency guard PASS")
