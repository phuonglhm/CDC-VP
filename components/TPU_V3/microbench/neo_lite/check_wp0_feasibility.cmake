# SPDX-License-Identifier: Apache-2.0
#
# Re-checks the WP0 feasibility claims against the pinned sources.
#
# `NEO_LITE_WP0_FEASIBILITY.md` says C1 is source-proven. That claim rests on
# things outside this repository — a target row in the NPU team's manifest, a
# golden case beside it, a constant in the pinned VP++ checkout, a cross
# toolchain — and every one of them can move without anything here failing to
# build. A feasibility note that is not re-checked expires silently, and the
# expiry would be discovered by WP8 or WP9 spending days on an assumption that
# had already stopped holding.
#
# This checks the sources, not the prose: it asserts what must be true for C1
# to remain feasible, and names the work package that becomes blocked if it is
# not.

cmake_minimum_required(VERSION 3.16)

foreach(_required SAURIA_ROOT VPP_V_HEADER CROSS_GCC PROBE_DIR
                  SAURIA_TARGETS_SHA256 VPP_V_HEADER_SHA256
                  CAPTURE_INFO_SHA256)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} was not provided")
    endif()
endforeach()

# Conditions are written out at each site rather than passed through a helper.
# The first version took the success condition as a macro argument and tested
# `if(NOT ${_condition})`; with a condition that itself began with `NOT`, the
# double negation made every check report failure. It failed safe — a false
# alarm rather than a false pass — but a checker that cries wolf is a checker
# nobody reads.
set(_problems "")

# ── §2: the named int8_32x32 target, with its exact fields ───────────────────
set(_targets "${SAURIA_ROOT}/sauria_targets.h")
if(NOT EXISTS "${_targets}")
    list(APPEND _problems
        "the pinned Sauria manifest is missing: ${_targets}. WP8 has no source "
        "target to extract and C1 is blocked")
else()
    # Hash first. The substring checks below say the right rows are present;
    # only the hash says the file is the one this audit read. A manifest that
    # gained a row, lost an unrelated one, or had a width edited outside the
    # two rows checked here would otherwise pass unchanged.
    file(SHA256 "${_targets}" _targets_hash)
    if(NOT _targets_hash STREQUAL SAURIA_TARGETS_SHA256)
        list(APPEND _problems
            "the pinned Sauria manifest hashes ${_targets_hash}, but WP0 was audited against ${SAURIA_TARGETS_SHA256}. Re-read it before trusting any C1 source claim")
    endif()

    file(READ "${_targets}" _manifest)
    # X=32, Y=32, 8-bit operands, 32-bit accumulate, index widths 17/17/16.
    # Matched as the whole row: a target whose geometry survived but whose
    # index widths changed would produce plausible wrong results rather than an
    # error, which is the failure D28 singles out.
    string(FIND "${_manifest}"
        "{\"int8_32x32\", 32, 32, 8, 8, 32, 0, 17, 17, 16, 1, 4, SAURIA_DT_INT8"
        _row_at)
    if(_row_at EQUAL -1)
        list(APPEND _problems
            "the pinned manifest no longer carries the int8_32x32 row with index widths 17/17/16. WP8 must not fall through to a template default: that proves neither the target nor its index widths (D28)")
    endif()

    # C2's row must still be there too, or the current baseline is the thing
    # that moved.
    string(FIND "${_manifest}"
        "{\"int8_64x64\", 64, 64, 8, 8, 32, 0, 18, 18, 17, 1, 4, SAURIA_DT_INT8"
        _c2_at)
    if(_c2_at EQUAL -1)
        list(APPEND _problems
            "the pinned manifest no longer carries the int8_64x64 row the current baseline is built from")
    endif()
endif()

# ── §2: the golden case, and the index flags it records ──────────────────────
set(_case "${SAURIA_ROOT}/npu_demo_clean/cases/demo_gemm_32x32")
if(NOT EXISTS "${_case}/case.env")
    list(APPEND _problems
        "the int8_32x32 golden case is missing: ${_case}. WP8 would have a "
        "target with no independent evidence to check an extraction against")
else()
    file(READ "${_case}/case.env" _env)
    foreach(_claim "EVAL_X=32" "EVAL_Y=32" "VERSION=\"int8_32x32\""
                   "-DSAURIA_ACT_IDX_W=17" "-DSAURIA_WEI_IDX_W=17"
                   "-DSAURIA_OUT_IDX_W=16")
        string(FIND "${_env}" "${_claim}" _at)
        if(_at EQUAL -1)
            list(APPEND _problems
                "the int8_32x32 golden case no longer records ${_claim}")
        endif()
    endforeach()
    # The payload, not merely the directory. Empty `stimuli/` and `config/`
    # directories used to pass this check, which would have let WP8 extract
    # against a golden case containing no golden.
    #
    # `CAPTURE_INFO.txt` records a SHA256 for each stimulus file, so the corpus
    # carries its own reference and this audit does not have to invent one.
    # The recorded paths are the capture machine's, so only the basename is
    # matched.
    if(NOT EXISTS "${_case}/CAPTURE_INFO.txt")
        list(APPEND _problems
            "the int8_32x32 golden case has no CAPTURE_INFO.txt, so its stimuli carry no reference hash")
    else()
        # Pin the reference itself. Without this, a payload and the file that
        # records its hashes could be edited together and still agree — the
        # check would confirm the corpus is self-consistent, which is not the
        # claim WP0 makes about it.
        file(SHA256 "${_case}/CAPTURE_INFO.txt" _capture_hash)
        if(NOT _capture_hash STREQUAL CAPTURE_INFO_SHA256)
            list(APPEND _problems
                "the int8_32x32 golden case's CAPTURE_INFO.txt hashes ${_capture_hash}, but WP0 was audited against ${CAPTURE_INFO_SHA256}. Its recorded stimulus hashes are the reference the payload is checked against, so a change here re-opens the whole golden claim")
        endif()

        # Each digest must be bound to *its own* file, not merely present.
        #
        # The first version hashed each stimulus and searched for the digest
        # anywhere in CAPTURE_INFO.txt. Swapping `initial_dram.txt` with
        # `gold_dram.txt` passed: both digests were still somewhere in the
        # file. A golden case whose input and expected output are exchanged is
        # exactly the corpus defect this check exists to catch.
        file(STRINGS "${_case}/CAPTURE_INFO.txt" _capture_lines)
        foreach(_stimulus initial_dram.txt gold_dram.txt GoldenStimuli.txt)
            set(_path "${_case}/stimuli/${_stimulus}")
            if(NOT EXISTS "${_path}")
                list(APPEND _problems
                    "the int8_32x32 golden case is missing stimuli/${_stimulus}")
                continue()
            endif()

            file(SIZE "${_path}" _stimulus_size)
            if(_stimulus_size EQUAL 0)
                list(APPEND _problems
                    "the int8_32x32 golden stimulus ${_stimulus} is empty")
            endif()

            # The recorded paths are the capture machine's, so the line is
            # matched on the trailing basename and the digest read from the
            # front of that same line.
            set(_recorded "")
            foreach(_line IN LISTS _capture_lines)
                if(_line MATCHES "^([0-9a-f]+)[ \t]+.*/${_stimulus}$")
                    set(_recorded "${CMAKE_MATCH_1}")
                    break()
                endif()
            endforeach()
            if(_recorded STREQUAL "")
                list(APPEND _problems
                    "CAPTURE_INFO.txt records no hash line for stimuli/${_stimulus}")
                continue()
            endif()

            file(SHA256 "${_path}" _stimulus_hash)
            if(NOT _stimulus_hash STREQUAL _recorded)
                list(APPEND _problems
                    "stimuli/${_stimulus} hashes ${_stimulus_hash}, but CAPTURE_INFO.txt records ${_recorded} for that filename; the golden payload does not match its own manifest")
            endif()
        endforeach()
    endif()

    if(NOT EXISTS "${_case}/config/demo_manifest.json")
        list(APPEND _problems
            "the int8_32x32 golden case has no config/demo_manifest.json")
    else()
        # The corpus disagrees with itself, and WP0 names which file wins.
        #
        # `case.env`, `CAPTURE_INFO.txt` and the manifest's own
        # `sauria_shapes_flat` all describe a 32x32 case. The manifest's
        # `compile_time` block says EVAL_X=16, EVAL_Y=8 — which is exactly the
        # `int8_8x16` target's geometry, so that block is stale from another
        # capture rather than an alternative reading of this one.
        #
        # Both halves are asserted. The shapes must keep agreeing, and the
        # stale block must keep being the *known* stale value: if the corpus is
        # ever corrected, this fails and the note gets updated rather than
        # carrying a caveat that has quietly stopped applying.
        file(READ "${_case}/config/demo_manifest.json" _demo)
        foreach(_shape "\"A_Mat_mvm\": \"1 32 64\"" "\"B_Mat_mvm\": \"1 64 32\""
                       "\"C_compute_mvm\": \"1 32 32\"")
            string(FIND "${_demo}" "${_shape}" _shape_at)
            if(_shape_at EQUAL -1)
                list(APPEND _problems
                    "the golden case manifest no longer records ${_shape}; the authoritative 32x32 shapes have changed")
            endif()
        endforeach()
        string(FIND "${_demo}" "\"EVAL_X\": 16" _stale_x)
        string(FIND "${_demo}" "\"EVAL_Y\": 8" _stale_y)
        if(_stale_x EQUAL -1 OR _stale_y EQUAL -1)
            list(APPEND _problems
                "config/demo_manifest.json no longer carries the known-stale compile_time EVAL_X=16/EVAL_Y=8. If the corpus was corrected, remove this check and the caveat in NEO_LITE_WP0_FEASIBILITY.md section 2; if it changed to some third value, the corpus needs re-auditing before WP8")
        endif()
    endif()
endif()

# ── §3: the VP++ constant a VLEN patch would change ──────────────────────────
if(NOT EXISTS "${VPP_V_HEADER}")
    list(APPEND _problems "the pinned VP++ vector header is missing: ${VPP_V_HEADER}")
else()
    file(SHA256 "${VPP_V_HEADER}" _vheader_hash)
    if(NOT _vheader_hash STREQUAL VPP_V_HEADER_SHA256)
        list(APPEND _problems
            "the pinned VP++ vector header hashes ${_vheader_hash}, but WP0 was audited against ${VPP_V_HEADER_SHA256}. WP9's patch is written against the audited text")
    endif()

    file(READ "${VPP_V_HEADER}" _vheader)
    string(FIND "${_vheader}" "constexpr unsigned VLEN = 512" _vlen_at)
    if(_vlen_at EQUAL -1)
        list(APPEND _problems
            "the pinned VP++ no longer declares `constexpr unsigned VLEN = 512` in the shape WP9's patch targets; the patch must be rewritten against whatever replaced it before C1's VLEN256 build can be trusted")
    endif()
endif()

# ── §3: the cross toolchain still builds C1's ISA ────────────────────────────
# Not skippable. "The cross toolchain builds rv32gcv_zvl256b" is one of the
# three things WP0 concludes, and a checker that skipped it still printed the
# sentence claiming it. A machine that cannot answer the question must not
# report the answer.
if(NOT EXISTS "${CROSS_GCC}")
    list(APPEND _problems
        "the pinned cross toolchain is absent at ${CROSS_GCC}, so WP0's claim that C1's firmware ISA builds cannot be checked. This is a failure rather than a skip: the claim is part of the conclusion")
else()
    set(_probe "${PROBE_DIR}/wp0_vsetvli_probe.c")
    file(WRITE "${_probe}"
        "int main(void){int x=0;__asm__ volatile(\"vsetvli %0, x0, e32, m1, ta, ma\":\"=r\"(x));return x;}\n")
    foreach(_isa rv32gcv_zvl256b rv32gcv_zvl512b)
        execute_process(
            COMMAND "${CROSS_GCC}" -march=${_isa} -mabi=ilp32d -O1
                    -ffreestanding -nostdlib -c "${_probe}"
                    -o "${PROBE_DIR}/wp0_${_isa}.o"
            RESULT_VARIABLE _isa_status
            OUTPUT_QUIET ERROR_VARIABLE _isa_error)
        if(NOT _isa_status EQUAL 0)
            list(APPEND _problems
                "the pinned cross toolchain cannot build ${_isa}: ${_isa_error}")
        endif()
    endforeach()
endif()

if(_problems)
    foreach(_problem IN LISTS _problems)
        message(SEND_ERROR "WP0 feasibility: ${_problem}")
    endforeach()
    message(FATAL_ERROR
        "the WP0 feasibility claims no longer hold against the pinned sources")
endif()

message(STATUS
    "WP0 feasibility: int8_32x32 target and golden present with index widths "
    "17/17/16, VP++ VLEN constant unchanged, cross toolchain builds both ISAs")
