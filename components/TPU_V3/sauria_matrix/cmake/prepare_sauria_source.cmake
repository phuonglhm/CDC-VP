# SPDX-License-Identifier: Apache-2.0
#
# Turn a pinned Sauria v4.2 tree into the exact source Phase 5 compiles.
#
# Four steps, each of which fails the configure rather than warning:
#
#   1. verify the pinned tree against its recorded content hash;
#   2. copy it into the build tree;
#   3. apply the recorded instrumentation-only hygiene patch and verify the
#      post-patch hash;
#   4. extract the `int8_64x64` profile from `sauria_targets.h` and check every
#      field against what Phase 5 requires.
#
# ## Why each of these is a hard failure
#
# **The pin.** `components/npu_tlm/models/` holds two copies of "v4.2" that are
# not the same source — 257 changed lines in the instruction decoder, and
# *different golden vectors* — and the shared component silently falls back to
# one of them while the NPU team's own working copy matches the other. A
# differential run against a source nobody chose answers a question nobody
# asked. See `TPU_V3_PHASE5_AUDIT.md` §1.
#
# **Copying before patching.** The pinned tree is shared with `npu_tlm`, whose
# consumers must not have their source edited underneath them. Phase 4.5 already
# paid for the other arrangement: building in the source tree let a Release and
# a Debug build clobber each other's artifacts.
#
# **The patch.** The source writes CSV traces through function-local
# `static std::ofstream` objects reached from the compute process, which
# `INTERFACE_CONTRACT.md` forbids between instances and which cost 4096 flushed
# writes per column-switch event at 64x64. Decision record D17 makes removing
# them a precondition for the adapter being contract-clean.
#
# **The profile.** The geometry is a C++ template parameter, not a runtime
# value, and the defaults are 32x32 (`NpuTop`) and 32x64 (`SystolicArray`). INT8
# needs no `-D` at all, so a build that selects nothing compiles cleanly and
# runs the wrong array. Reading the parameters out of the source and checking
# them is the only thing standing between "we built int8_64x64" and "we believe
# we built int8_64x64".
#
# Required: -DSAURIA_ROOT= -DWORK_DIR= -DPATCHES= -DBASE_HASH=
#           -DPATCHED_HASH= -DGOLDEN_HASH= -DPROFILE= -DGENERATED_HEADER=
#           -DTEMPLATE_HEADER=

foreach(_required SAURIA_ROOT WORK_DIR PATCHES BASE_HASH PATCHED_HASH GOLDEN_HASH
                  PROFILE GENERATED_HEADER TEMPLATE_HEADER)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR
            "prepare_sauria_source.cmake: -D${_required}= is required")
    endif()
endforeach()

# ── the reproducible tree hash ───────────────────────────────────────────────
#
# `sha256sum` prints the filename beside the digest, so hashing its output from
# two directories gives two answers for identical content. This hashes a sorted
# list of `<digest>  <relative path>` lines instead, which is stable wherever
# the tree happens to sit. The first version of this recipe was not, and it
# recorded a hash nobody else could reproduce.
function(_sauria_tree_hash root out_var)
    file(GLOB_RECURSE _headers RELATIVE "${root}" "${root}/*.h")
    list(FILTER _headers EXCLUDE REGEX "^npu_demo_clean/")
    list(SORT _headers)

    set(_listing "")
    foreach(_header ${_headers})
        file(SHA256 "${root}/${_header}" _digest)
        string(APPEND _listing "${_digest}  ${_header}\n")
    endforeach()

    string(SHA256 _tree "${_listing}")
    set(${out_var} "${_tree}" PARENT_SCOPE)
endfunction()

function(_sauria_golden_hash root out_var)
    set(_case "${root}/npu_demo_clean/cases/demo_gemm_64x64/sauria_tmp")
    set(_files
        "sauria_A_Mat_mvm_flat.txt"
        "sauria_B_Mat_mvm_flat.txt"
        "sauria_C_compute_mvm_flat.txt")
    set(_listing "")
    foreach(_file ${_files})
        if(NOT EXISTS "${_case}/${_file}")
            message(FATAL_ERROR "Pinned Sauria golden file is missing: ${_case}/${_file}")
        endif()
        file(SHA256 "${_case}/${_file}" _digest)
        string(APPEND _listing "${_digest}  sauria_tmp/${_file}\n")
    endforeach()
    string(SHA256 _golden "${_listing}")
    set(${out_var} "${_golden}" PARENT_SCOPE)
endfunction()

# ── 1. the pin ───────────────────────────────────────────────────────────────

if(NOT EXISTS "${SAURIA_ROOT}/sauria_targets.h")
    message(FATAL_ERROR
        "TPU_V3_SAURIA_ROOT='${SAURIA_ROOT}' does not look like a Sauria v4.2 "
        "tree: sauria_targets.h is missing.")
endif()

_sauria_golden_hash("${SAURIA_ROOT}" _actual_golden)
if(NOT _actual_golden STREQUAL GOLDEN_HASH)
    message(FATAL_ERROR
        "The demo_gemm_64x64 oracle is not the corpus Phase 5 pinned.\n"
        "  expected  ${GOLDEN_HASH}\n"
        "  found     ${_actual_golden}\n"
        "A differential whose oracle can change outside the source pin is not "
        "reproducible. Update the recorded golden hash only after reviewing "
        "the NPU-team corpus change.")
endif()

_sauria_tree_hash("${SAURIA_ROOT}" _actual_base)

if(NOT _actual_base STREQUAL BASE_HASH)
    message(FATAL_ERROR
        "The Sauria source at ${SAURIA_ROOT} is not the revision Phase 5 is "
        "pinned to.\n"
        "  expected  ${BASE_HASH}\n"
        "  found     ${_actual_base}\n"
        "This is refused rather than accepted because there are two trees in "
        "this repository both called v4.2, they differ in the instruction "
        "decoder, the performance counters and their golden vectors, and only "
        "one of them matches the NPU team's working copy. A source-versus-"
        "adapter differential run against the other one proves nothing.\n"
        "Set -DTPU_V3_SAURIA_ROOT to the pinned tree recorded in "
        "components/TPU_V3/docs/TPU_V3_PHASE5_AUDIT.md section 1, or update the "
        "recorded hash there deliberately and re-run the Phase 5 gate.")
endif()

# ── 2 and 3. the build-tree copy, patched ────────────────────────────────────

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(COPY "${SAURIA_ROOT}/" DESTINATION "${WORK_DIR}")

find_program(_patch_program NAMES patch REQUIRED)
foreach(_patch ${PATCHES})
    execute_process(
        COMMAND "${_patch_program}" -p1 -s -i "${_patch}"
        WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE _patch_result
        OUTPUT_VARIABLE _patch_output
        ERROR_VARIABLE _patch_output)
    if(NOT _patch_result EQUAL 0)
        message(FATAL_ERROR
            "A Sauria hygiene patch did not apply to the pinned source:\n"
            "${_patch_output}\n"
            "Patch: ${_patch}")
    endif()
endforeach()

_sauria_tree_hash("${WORK_DIR}" _actual_patched)
if(NOT _actual_patched STREQUAL PATCHED_HASH)
    message(FATAL_ERROR
        "The patched Sauria source is not what was recorded.\n"
        "  expected  ${PATCHED_HASH}\n"
        "  found     ${_actual_patched}\n"
        "The patch applied, so this means the patch itself changed, or the base "
        "moved in a way the base-hash check should have caught. Either way the "
        "compiled source is not the audited one.")
endif()

# ── 4. the profile, read out of the source rather than believed ──────────────
#
# `sauria_targets.h` is generated from `sauria_targets.csv` and calls itself the
# single source of truth for geometry, element widths and index widths. So it is
# read, not transcribed: a table copied into this repository would be a second
# truth, and the failure mode is a build that names one geometry and compiles
# another.

file(READ "${WORK_DIR}/sauria_targets.h" _targets_text)

# `{"int8_64x64", 64, 64, 8, 8, 32, 0, 18, 18, 17, 1, 4, SAURIA_DT_INT8, ""}`
string(REGEX MATCH
       "\\{[ \t]*\"${PROFILE}\"[ \t]*,[^}]*\\}" _record "${_targets_text}")
if(_record STREQUAL "")
    string(REGEX MATCHALL "\"[A-Za-z0-9_]+\"[ \t]*," _names "${_targets_text}")
    message(FATAL_ERROR
        "The pinned Sauria source defines no profile named '${PROFILE}'.\n"
        "Profiles present: ${_names}\n"
        "Phase 5 requires ${PROFILE}: X=64, Y=64, INT8 operands, INT32 "
        "accumulation, index widths 18/18/17.")
endif()

string(REGEX REPLACE "^\\{[ \t]*\"${PROFILE}\"[ \t]*,[ \t]*" "" _fields "${_record}")
string(REGEX REPLACE "\\}$" "" _fields "${_fields}")
string(REPLACE "," ";" _fields "${_fields}")
list(TRANSFORM _fields STRIP)

# SauriaTarget: X, Y, ia_w, ib_w, oc_w, op_type, idx_a, idx_w, idx_o,
#               in_bytes, out_bytes, dtype, build_flags
list(LENGTH _fields _field_count)
if(_field_count LESS 13)
    message(FATAL_ERROR
        "The '${PROFILE}' record in sauria_targets.h has ${_field_count} fields, "
        "expected at least 13. The SauriaTarget layout has changed and this "
        "extraction no longer understands it:\n  ${_record}")
endif()

list(GET _fields 0 SAURIA_X)
list(GET _fields 1 SAURIA_Y)
list(GET _fields 2 SAURIA_IA_W)
list(GET _fields 3 SAURIA_IB_W)
list(GET _fields 4 SAURIA_OC_W)
list(GET _fields 5 SAURIA_OP_TYPE)
list(GET _fields 6 SAURIA_IDX_A)
list(GET _fields 7 SAURIA_IDX_W)
list(GET _fields 8 SAURIA_IDX_O)
list(GET _fields 9 SAURIA_IN_BYTES)
list(GET _fields 10 SAURIA_OUT_BYTES)
list(GET _fields 11 SAURIA_DTYPE)

# Phase 5 states these explicitly (plan §16). Checking the extracted values
# against them is what makes "record and enforce" more than a comment.
set(_expected
    "SAURIA_X|64|systolic array columns"
    "SAURIA_Y|64|systolic array rows"
    "SAURIA_IA_W|8|activation element width"
    "SAURIA_IB_W|8|weight element width"
    "SAURIA_OC_W|32|accumulator/output element width"
    "SAURIA_OP_TYPE|0|integer PE arithmetic"
    "SAURIA_IDX_A|18|packed-config activation index width"
    "SAURIA_IDX_W|18|packed-config weight index width"
    "SAURIA_IDX_O|17|packed-config output index width"
    "SAURIA_IN_BYTES|1|operand bytes in memory"
    "SAURIA_OUT_BYTES|4|result bytes in memory")

# Split on the separator rather than stripping around it with a regex.
#
# `string(REGEX REPLACE)` replaces *every* match, not the first, so the obvious
# `^[^|]*\|` strips through the **last** separator on a three-field record and
# silently yields the wrong field. (Single-separator records elsewhere in this
# repository are unaffected: there is only one match to make.)
foreach(_entry ${_expected})
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _want)
    list(GET _parts 2 _why)
    if(NOT "${${_name}}" STREQUAL "${_want}")
        message(FATAL_ERROR
            "Profile '${PROFILE}' does not match what Phase 5 requires.\n"
            "  ${_why}: expected ${_want}, source says ${${_name}}\n"
            "Phase 5 records and enforces X=64, Y=64, INT8 activation/weight, "
            "INT32 accumulation/output and index widths 18/18/17. A source that "
            "no longer provides them is a different engine.")
    endif()
endforeach()

if(NOT SAURIA_DTYPE STREQUAL "SAURIA_DT_INT8")
    message(FATAL_ERROR
        "Profile '${PROFILE}' declares dtype ${SAURIA_DTYPE}, expected "
        "SAURIA_DT_INT8.")
endif()

set(SAURIA_PROFILE_NAME "${PROFILE}")
set(SAURIA_BASE_HASH "${BASE_HASH}")
set(SAURIA_PATCHED_HASH "${PATCHED_HASH}")
set(SAURIA_GOLDEN_HASH "${GOLDEN_HASH}")
set(_patch_listing "")
set(_patch_names "")
foreach(_patch ${PATCHES})
    get_filename_component(_patch_name "${_patch}" NAME)
    file(SHA256 "${_patch}" _patch_hash)
    string(APPEND _patch_listing "${_patch_hash}  ${_patch_name}\n")
    list(APPEND _patch_names "${_patch_name}")
endforeach()
list(JOIN _patch_names "," SAURIA_PATCH_NAME)
string(SHA256 SAURIA_PATCH_HASH "${_patch_listing}")

configure_file("${TEMPLATE_HEADER}" "${GENERATED_HEADER}" @ONLY)

message(STATUS
    "TPU_V3 Sauria: ${PROFILE} ${SAURIA_X}x${SAURIA_Y} INT8/INT32 "
    "idx ${SAURIA_IDX_A}/${SAURIA_IDX_W}/${SAURIA_IDX_O}, patched source "
    "verified")
