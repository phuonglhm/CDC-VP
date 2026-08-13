# SPDX-License-Identifier: Apache-2.0
#
# Writes BUILD_MANIFEST.json into a `riscv_vpp_compiler_vp` package.
#
# Run with `cmake -P`, so it executes at *package* time rather than at configure
# time. The schema, the two-revision rule and the `null`-not-"unknown" rule are
# the ones the TPU_V3 manifest established; they are repeated here rather than
# shared because the two manifests describe different machines and a common
# generator would need a parameter for every field that differs, which is most
# of them.
#
# ## What this manifest is for
#
# The package is handed to a team who will report results from it. Every claim
# they could reasonably make — which ISA, which VLEN, which upstream source,
# what accuracy — has to be answerable from the bundle alone, months later,
# without this repository. In particular:
#
#   * the VP++ source is a base revision *plus a patch series*, so a single
#     revision would name source that was never compiled;
#   * `accuracy` is recorded as a field, because "we measured it on the VP" is
#     the claim this model must never be used to support;
#   * `absent` lists what is deliberately not in the binary. A reader cannot
#     verify an absence from a file listing, and `riscv_vpp_compiler_vp_independence`
#     is what actually enforces it — the field records the intent so a future
#     package that quietly added Sauria contradicts its own manifest.
#
# Required -D arguments:
#   MANIFEST_PATH        output file
#   SOURCE_DIR           CDC-VP source root (for the git query)
#   BUILD_REVISION       revision compiled into the binary, or "unknown"
#   PLATFORM             platform/executable name
#   BUILD_TYPE           CMAKE_BUILD_TYPE, or "unspecified"
#   CXX_COMPILER         full path
#   CXX_COMPILER_ID
#   CXX_COMPILER_VERSION
#   CMAKE_VERSION_USED
#   SYSTEMC_HOME
#   ISA_STRING           frozen -march for this package
#   ABI_STRING           frozen -mabi for this package
#   RAM_BASE             default RAM base, decimal or 0x
#   RAM_SIZE_DEFAULT     default RAM size in bytes
#   HOSTIO_BASE          host-I/O window base
#   HOSTIO_SIZE          host-I/O window size
#   VPP_BASE_REVISION    RISC-V VP++ base commit
#   VPP_PATCH_SERIES     one `file|kind|ref|sha|why` record per entry
#   FIRMWARE_INCLUDED    TRUE when the examples were built and bundled
#   CROSS_TOOLCHAIN      the toolchain that built them, or ""

cmake_minimum_required(VERSION 3.21)

function(_json_escape out value)
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(_bool_json out value)
    if(value)
        set(${out} "true" PARENT_SCOPE)
    else()
        set(${out} "false" PARENT_SCOPE)
    endif()
endfunction()

function(_json_string_or_null out value)
    if(value STREQUAL "" OR value STREQUAL "unknown")
        set(${out} "null" PARENT_SCOPE)
    else()
        _json_escape(_escaped "${value}")
        set(${out} "\"${_escaped}\"" PARENT_SCOPE)
    endif()
endfunction()

# ── revision at package time, and whether the tree was dirty ─────────────────

set(_package_revision "")
set(_dirty_json "null")

find_program(_git_executable git)
if(_git_executable AND EXISTS "${SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${_git_executable}" -C "${SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE _package_revision
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _rev_result
        ERROR_QUIET)
    if(NOT _rev_result EQUAL 0)
        set(_package_revision "")
    endif()

    execute_process(
        COMMAND "${_git_executable}" -C "${SOURCE_DIR}" status --porcelain
        OUTPUT_VARIABLE _status
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _status_result
        ERROR_QUIET)
    if(_status_result EQUAL 0)
        if(_status STREQUAL "")
            set(_dirty_json "false")
        else()
            set(_dirty_json "true")
        endif()
    endif()
endif()

_json_string_or_null(_package_revision_json "${_package_revision}")
_json_string_or_null(_build_revision_json "${BUILD_REVISION}")

if(_build_revision_json STREQUAL "null" OR _package_revision_json STREQUAL "null")
    set(_match_json "null")
    set(_available_json "false")
else()
    set(_available_json "true")
    if("${BUILD_REVISION}" STREQUAL "${_package_revision}")
        set(_match_json "true")
    else()
        set(_match_json "false")
    endif()
endif()

string(TIMESTAMP _generated "%Y-%m-%dT%H:%M:%SZ" UTC)

_json_escape(_platform "${PLATFORM}")
_json_escape(_build_type "${BUILD_TYPE}")
_json_escape(_cxx "${CXX_COMPILER}")
_json_escape(_cxx_id "${CXX_COMPILER_ID}")
_json_escape(_cxx_version "${CXX_COMPILER_VERSION}")
_json_escape(_cmake_version "${CMAKE_VERSION_USED}")
_json_escape(_systemc "${SYSTEMC_HOME}")
_json_escape(_isa "${ISA_STRING}")
_json_escape(_abi "${ABI_STRING}")
_json_escape(_vpp_base "${VPP_BASE_REVISION}")
_json_escape(_toolchain "${CROSS_TOOLCHAIN}")
_bool_json(_firmware_json "${FIRMWARE_INCLUDED}")

# Emitted as decimal integers so a consumer does not have to parse hex out of a
# string it was told is a number.
math(EXPR _ram_base "${RAM_BASE}" OUTPUT_FORMAT DECIMAL)
math(EXPR _ram_size "${RAM_SIZE_DEFAULT}" OUTPUT_FORMAT DECIMAL)
math(EXPR _hostio_base "${HOSTIO_BASE}" OUTPUT_FORMAT DECIMAL)
math(EXPR _hostio_size "${HOSTIO_SIZE}" OUTPUT_FORMAT DECIMAL)

set(_vpp_patch_json "")
set(_vpp_effective "${VPP_BASE_REVISION}")
set(_vpp_first TRUE)
foreach(_record IN LISTS VPP_PATCH_SERIES)
    string(REPLACE "|" ";" _fields "${_record}")
    list(LENGTH _fields _n)
    if(NOT _n EQUAL 5)
        message(FATAL_ERROR "malformed VPP_PATCH_SERIES record: ${_record}")
    endif()
    list(GET _fields 0 _pfile)
    list(GET _fields 1 _pkind)
    list(GET _fields 2 _pref)
    list(GET _fields 3 _psha)
    list(GET _fields 4 _pwhy)
    _json_escape(_pfile "${_pfile}")
    _json_escape(_pkind "${_pkind}")
    _json_escape(_pref "${_pref}")
    _json_escape(_psha "${_psha}")
    _json_escape(_pwhy "${_pwhy}")
    if(NOT _vpp_first)
        string(APPEND _vpp_patch_json ",")
    endif()
    set(_vpp_first FALSE)
    string(APPEND _vpp_patch_json "
        {
          \"kind\": \"${_pkind}\",
          \"reference\": \"${_pref}\",
          \"reason\": \"${_pwhy}\",
          \"patch_file\": \"cpu_models/riscv_vp_plusplus/patches/${_pfile}\",
          \"patch_sha256\": \"${_psha}\"
        }")
    if(_pkind STREQUAL "upstream-backport")
        string(APPEND _vpp_effective "+${_pref}")
    else()
        get_filename_component(_pstem "${_pfile}" NAME_WE)
        string(APPEND _vpp_effective "+${_pstem}")
    endif()
endforeach()
_json_escape(_vpp_effective "${_vpp_effective}")

file(WRITE "${MANIFEST_PATH}"
"{
  \"schema\": \"cdc-vp/build-manifest/2\",
  \"generated_utc\": \"${_generated}\",
  \"platform\": \"${_platform}\",
  \"phase\": 4.5,
  \"phase_note\": \"Phase 4.5: the RISC-V VP++ Compiler Enablement VP. One architectural RV32GCV hart -- scalar and RVV 1.0 execution share one PC, one register file, one CSR set and one TLM memory path -- with program/data RAM and a simulator-only host-I/O target. No NoC, no accelerator and no NEO composition.\",
  \"source\": {
    \"repository\": \"CDC-VP\",
    \"revision_available\": ${_available_json},
    \"build_revision\": ${_build_revision_json},
    \"package_revision\": ${_package_revision_json},
    \"revision_matches_binary\": ${_match_json},
    \"dirty_at_package_time\": ${_dirty_json}
  },
  \"build\": {
    \"type\": \"${_build_type}\",
    \"cxx_compiler\": \"${_cxx}\",
    \"cxx_compiler_id\": \"${_cxx_id}\",
    \"cxx_compiler_version\": \"${_cxx_version}\",
    \"cmake_version\": \"${_cmake_version}\",
    \"cxx_standard\": 17
  },
  \"runtime\": {
    \"systemc_home\": \"${_systemc}\",
    \"bundled_systemc\": true,
    \"rpath\": \"$ORIGIN\"
  },
  \"machine\": {
    \"harts\": 1,
    \"xlen\": 32,
    \"isa\": \"${_isa}\",
    \"abi\": \"${_abi}\",
    \"rvv_version\": \"1.0\",
    \"vlen_bits\": 512,
    \"elen_bits\": 64,
    \"vlenb\": 64,
    \"vector_registers\": 32,
    \"execution_profile\": \"bare-metal / freestanding\",
    \"accuracy\": \"functional instruction-set model with loosely-timed TLM\",
    \"accuracy_note\": \"Not pipeline- or cycle-accurate, and not a microarchitecture. It models no TPU pipeline timing, no memory bandwidth and no NoC latency; no such number may be published from it.\",
    \"scalar_and_vector_share_one_hart\": true
  },
  \"memory_map\": {
    \"ram\": { \"base\": ${_ram_base}, \"default_size\": ${_ram_size}, \"configurable_size\": true },
    \"host_io\": { \"base\": ${_hostio_base}, \"size\": ${_hostio_size}, \"simulator_only\": true },
    \"host_io_note\": \"Not a TPU_V3 architectural peripheral. It exists so a freestanding image can print and exit without a libc, and it must not appear in full-SoC firmware. The exit-protocol words are the Phase 2 contract at the same offsets; everything Phase 4.5 adds is at 0x400 and above.\",
    \"unmapped_policy\": \"refused with a TLM address error, which the hart takes as an access fault chosen by access origin (decision record D13)\"
  },
  \"contents\": {
    \"firmware_examples\": ${_firmware_json},
    \"cross_toolchain\": \"${_toolchain}\",
    \"sdk\": \"sdk/ carries the startup, linker script, host-I/O shim, the shared map header and the Makefile that built the bundled examples\"
  },
  \"absent\": {
    \"note\": \"Deliberately not in this binary or package. Enforced by the riscv_vpp_compiler_vp_independence gate against the sources, the emitted symbols, the link interface and the package listing -- not by this field.\",
    \"floo_noc\": false,
    \"sauria\": false,
    \"neo_dma\": false,
    \"core_sram\": false,
    \"image_transform\": false,
    \"spike\": false,
    \"qt_vnc_gui\": false,
    \"upstream_vpp_platforms\": false
  },
  \"upstream\": {
    \"riscv_vp_plusplus\": {
      \"repository\": \"https://github.com/ics-jku/riscv-vp-plusplus\",
      \"role\": \"the RV32GCV hart: scalar and RVV 1.0 execution in one ISS\",
      \"base_revision\": \"${_vpp_base}\",
      \"base_revision_tag\": \"2025.09\",
      \"patch_series\": [${_vpp_patch_json}
      ],
      \"patch_series_note\": \"Applied in filename order. 'upstream-backport' entries vanish when the pin moves past them; 'downstream-conformance' entries have no upstream counterpart and must be re-checked against any new base.\",
      \"effective_source\": \"${_vpp_effective}\",
      \"linked\": true,
      \"license\": \"MIT\"
    },
    \"berkeley_softfloat\": {
      \"role\": \"scalar and vector floating point, vendored inside VP++\",
      \"release\": \"3d\",
      \"linked\": true,
      \"license\": \"BSD-3-Clause\"
    },
    \"spike\": {
      \"repository\": \"https://github.com/riscv-software-src/riscv-isa-sim\",
      \"role\": \"standalone development oracle only (decision record D3)\",
      \"linked\": false,
      \"license\": \"BSD-3-Clause\"
    }
  }
}
")

message(STATUS
    "riscv_vpp_compiler_vp: wrote ${MANIFEST_PATH} "
    "(build ${_build_revision_json}, package ${_package_revision_json}, dirty ${_dirty_json})")
