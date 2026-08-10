# SPDX-License-Identifier: Apache-2.0
#
# Writes BUILD_MANIFEST.json into a TPU_V3 package.
#
# Run with `cmake -P`, so it executes at *package* time rather than at
# configure time.
#
# ## Two revisions, deliberately
#
# The revision compiled into the binary is captured when the platform is
# configured; this script runs later, when the package is assembled. Those are
# different moments and the tree can move between them. Recording one number
# and calling it "the revision" would let a manifest name a commit the binary
# was never built from, so both are recorded and compared:
#
#   build_revision    what the executable reports through --version
#   package_revision  what the working tree was at packaging time
#
# ## Unknown is `null`, not the word "unknown"
#
# A build outside a Git checkout — an exported tarball, a container without
# `git` — has no revision to record. Writing a bare `unknown` produced invalid
# JSON, so every consumer of the manifest failed on exactly the builds whose
# provenance most needed reading. JSON `null` is the honest encoding of "not
# available", and `revision_available` makes the distinction explicit for a
# reader that does not want to type-check.
#
# Required -D arguments:
#   MANIFEST_PATH      output file
#   SOURCE_DIR         CDC-VP source root (for the git query)
#   BUILD_REVISION     revision compiled into the binary, or "unknown"
#   PLATFORM           platform/executable name
#   BUILD_TYPE         CMAKE_BUILD_TYPE, or "unspecified"
#   CXX_COMPILER       full path
#   CXX_COMPILER_ID
#   CXX_COMPILER_VERSION
#   CMAKE_VERSION_USED
#   SYSTEMC_HOME
#   MXU_BACKEND        backend compiled into the binary
#   SPIKE_REVISION
#   SPIKE_LINKED       TRUE/FALSE
#   VPP_BASE_REVISION      RISC-V VP++ base commit
#   VPP_BACKPORT_REVISION  approved F11 backport commit applied on top
#   VPP_PATCH_NAME         patch filename under cpu_models/riscv_vp_plusplus/patches
#   VPP_PATCH_SHA256       content hash of that patch
#   VPP_LINKED             TRUE/FALSE
#   SAURIA_OPTION_ENABLED  repository-wide CMake option, TRUE/FALSE
#   SAURIA_LINKED          linked into this executable, TRUE/FALSE
#   SAURIA_SELECTABLE      runtime-selectable in this executable, TRUE/FALSE

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

# A JSON string literal, or the literal `null` when the value is absent or is
# the "unknown" sentinel the caller passes when Git was unavailable.
function(_json_string_or_null out value)
    if(value STREQUAL "" OR value STREQUAL "unknown")
        set(${out} "null" PARENT_SCOPE)
    else()
        _json_escape(_escaped "${value}")
        set(${out} "\"${_escaped}\"" PARENT_SCOPE)
    endif()
endfunction()

# ── revision at package time, and whether the tree was dirty ─────────────────
#
# A dirty tree is recorded rather than hidden. A package built from uncommitted
# work is a perfectly reasonable thing to make; one that claims to be a clean
# revision when it is not is how an unreproducible result gets believed.

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

# Only meaningful when both are known; otherwise `null` rather than a
# comparison between two absences.
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

_bool_json(_spike_linked_json "${SPIKE_LINKED}")
_bool_json(_sauria_option_enabled_json "${SAURIA_OPTION_ENABLED}")
_bool_json(_sauria_linked_json "${SAURIA_LINKED}")
_bool_json(_sauria_selectable_json "${SAURIA_SELECTABLE}")

_json_escape(_platform "${PLATFORM}")
_json_escape(_build_type "${BUILD_TYPE}")
_json_escape(_cxx "${CXX_COMPILER}")
_json_escape(_cxx_id "${CXX_COMPILER_ID}")
_json_escape(_cxx_version "${CXX_COMPILER_VERSION}")
_json_escape(_cmake_version "${CMAKE_VERSION_USED}")
_json_escape(_systemc "${SYSTEMC_HOME}")
_json_escape(_mxu_backend "${MXU_BACKEND}")
_json_escape(_spike_revision "${SPIKE_REVISION}")

# The VP++ source is base + one approved backport, so recording a single
# revision would describe source that was never compiled. All four fields are
# needed to reconstruct it exactly.
_json_escape(_vpp_base "${VPP_BASE_REVISION}")
_json_escape(_vpp_backport "${VPP_BACKPORT_REVISION}")
_json_escape(_vpp_patch_name "${VPP_PATCH_NAME}")
_json_escape(_vpp_patch_sha256 "${VPP_PATCH_SHA256}")
_bool_json(_vpp_linked_json "${VPP_LINKED}")

file(WRITE "${MANIFEST_PATH}"
"{
  \"schema\": \"cdc-vp/build-manifest/2\",
  \"generated_utc\": \"${_generated}\",
  \"platform\": \"${_platform}\",
  \"phase\": 1,
  \"phase_note\": \"Phase 1 skeleton: configuration, address map and packaging. No cores, MXUs, SVM or NoC are instantiated yet.\",
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
  \"configuration\": {
    \"mxu_backend\": \"${_mxu_backend}\",
    \"mxu_backend_note\": \"Compiled into the binary. A configuration selecting a different backend is refused at run time.\",
    \"mxu_arithmetic\": \"BF16 x BF16 with IEEE FP32 accumulation (decision record D6)\",
    \"max_chips\": 8,
    \"max_chips_reason\": \"Revision 1 backend limit: frozen FlooNoC chimney manager id is 3 bits; one aggregated NoC manager per chip\"
  },
  \"upstream\": {
    \"spike\": {
      \"repository\": \"https://github.com/riscv-software-src/riscv-isa-sim\",
      \"pinned_revision\": \"${_spike_revision}\",
      \"pin_status\": \"candidate\",
      \"linked\": ${_spike_linked_json},
      \"license\": \"BSD-3-Clause\"
    },
    \"riscv_vp_plusplus\": {
      \"repository\": \"https://github.com/ics-jku/riscv-vp-plusplus\",
      \"role\": \"runtime RV32GCV hart (scalar + RVV 1.0)\",
      \"base_revision\": \"${_vpp_base}\",
      \"base_revision_tag\": \"2025.09\",
      \"backports\": [
        {
          \"upstream_commit\": \"${_vpp_backport}\",
          \"summary\": \"vp: core: dbbcache: fixed random cycle counting bug (e.g. mcycles)\",
          \"reason\": \"TPU_V3 audit finding F11: without it the ISS banks ~2.29 s of simulated time before its first instruction and mcycle is wrong from reset\",
          \"patch_file\": \"cpu_models/riscv_vp_plusplus/patches/${_vpp_patch_name}\",
          \"patch_sha256\": \"${_vpp_patch_sha256}\"
        }
      ],
      \"effective_source\": \"${_vpp_base}+${_vpp_backport}\",
      \"linked\": ${_vpp_linked_json},
      \"license\": \"MIT\"
    },
    \"floo_noc\": {
      \"repository\": \"https://github.com/pulp-platform/FlooNoC.git\",
      \"pinned_revision\": \"9a6972a5f9b8117506d1df8a6505ce1da2bc9084\",
      \"linked\": false,
      \"license\": \"SHL-0.51\"
    },
    \"sauria\": {
      \"global_option_enabled\": ${_sauria_option_enabled_json},
      \"linked\": ${_sauria_linked_json},
      \"selectable\": ${_sauria_selectable_json},
      \"license\": \"Apache-2.0 WITH SHL-2.1\",
      \"note\": \"The global option may enable Sauria for another platform. Only linked/selectable describe this binary. A Sauria-linked TPU_V3 package is not authorized; see licenses/SAURIA.PROVENANCE.md.\"
    }
  }
}
")

message(STATUS
    "TPU_V3: wrote ${MANIFEST_PATH} "
    "(build ${_build_revision_json}, package ${_package_revision_json}, dirty ${_dirty_json})")
