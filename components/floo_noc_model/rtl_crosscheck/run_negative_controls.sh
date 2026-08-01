#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Negative controls: prove the tests fail when the defects they exist to catch
# are put back.
#
# A test suite that has never been seen to fail is not evidence. Every fix in
# this component was accompanied by an injection that must be detected; this
# keeps those injections executable instead of leaving them in prose.
#
# ## What counts as detection
#
# A control is detected only when **all three** hold:
#
#   1. the mutated source still builds;
#   2. the named test runs; and
#   3. it fails, and its output contains the expected message.
#
# A build failure is a **control failure**, not a detection. An earlier version
# counted it as success, which meant a mutation that merely broke the syntax
# scored the same as one that broke behaviour — and four of six controls were in
# exactly that state without anyone noticing, because a malformed record had
# left the test name empty and CMake was printing its usage text.
#
# Requiring a specific message closes the other half: a test that fails for an
# unrelated reason is not evidence that it covers this defect.
#
# ## Where it runs
#
# In a private copy of the component under `/tmp`, never in the working tree.
# The tree is not touched at all, so an interrupt, a `SIGKILL`, or a lost
# machine cannot leave a mutated source behind, and two runs cannot collide.
#
# ## Scope
#
# These are the model-level controls, which run in seconds. The RTL cross-check
# controls remain **manual** and are documented in `docs/STATUS.md`: each needs
# Verilator and Bender and takes minutes, and several of them legitimately pass
# because the frozen RTL is redundant at that point — a judgement this script
# cannot make. See rule 9e in `AI_HANDOFF_CONTEXT.md`.

set -u -o pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
component_dir="$(cd "${script_dir}/.." && pwd)"

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/floo_noc_negative_controls.XXXXXX")"
echo "evidence directory: ${work_dir}"

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:${PATH}

echo "compiler: $(${CC} -dumpfullversion)"

# Deliberately not `$SYSTEMC_HOME`: a stale value in the environment is a known
# trap on this host. Override with `FLOO_SYSTEMC_HOME` if really needed.
systemc_home="${FLOO_SYSTEMC_HOME:-/opt/systemc-2.3.4}"

# ── the registry ─────────────────────────────────────────────────────────────
#
# Parallel arrays rather than delimited strings: the literals routinely span
# several lines, and `read` stops at the first one.
names=()
files=()
needles=()
replacements=()
tests=()
expects=()
whys=()
args=()

# Every field is required. Two are exceptions, and both for a stated reason:
#
#   * `replacement` — deleting a block is a legitimate mutation, so an empty
#     replacement is meaningful;
#   * `args` — most test executables take none. The trace-comparison targets do
#     take arguments (stimulus, output, expected), and without this they could
#     not be used as controls at all: run bare they exit on their usage message,
#     which is a failure for the wrong reason. Two placeholders are substituted,
#     `@SRC@` for the mutated source copy and `@EVIDENCE@` for this control's
#     evidence directory.
#
# The expected message in particular used to be optional, and two controls
# registered an empty one — so they were counted as detected whenever their test
# failed at all, for any reason. An optional check is a check that will
# eventually be skipped.
add_control() {
    local field
    local index=0
    for field in "$1" "$2" "$3" "$5" "$6" "$7"; do
        index=$((index + 1))
        if [[ -z "${field}" ]]; then
            echo "negative-control registry: entry '$1' has an empty required" >&2
            echo "field (position ${index} of name/file/needle/test/expect/why)." >&2
            exit 1
        fi
    done
    names+=("$1"); files+=("$2"); needles+=("$3"); replacements+=("$4")
    tests+=("$5"); expects+=("$6"); whys+=("$7"); args+=("${8:-}")
}

add_control \
    "lane-placement" \
    "include/floo_noc_model/axi_lanes.hpp" \
    'shape.lane_offset = static_cast<unsigned>(addr % bus_bytes);' \
    'shape.lane_offset = 0;' \
    "test_axi_lanes" \
    "lane offset" \
    "the first payload byte always went to lane 0, so a narrow access at a non-zero offset wrote the wrong half of the bus"

add_control \
    "byte-enables-ignored" \
    "include/floo_noc_model/axi_lanes.hpp" \
    '        if (!byte_enabled(enables, enable_length, index)) {
            continue;
        }
        const unsigned beat = shape.beat_of(index);
        const unsigned lane = shape.lane_of(index);
        view.data[beat]' \
    '        const unsigned beat = shape.beat_of(index);
        const unsigned lane = shape.lane_of(index);
        view.data[beat]' \
    "test_axi_lanes" \
    "disabled byte must clear its own strobe bit" \
    "byte enables were never read, so a partial write became a full one"

add_control \
    "target-delay-truncated" \
    "src/noc_interconnect.cpp" \
    '        if (static_cast<double>(ticks) < cycles) {
            ++ticks;
        }' \
    '' \
    "test_noc_interconnect" \
    "sub-cycle target latency must cost one cycle" \
    "a target latency shorter than one network cycle was rounded down to free"

add_control \
    "incoming-delay-dropped" \
    "src/noc_interconnect.cpp" \
    '    if (delay > sc_core::SC_ZERO_TIME) {
        sc_core::wait(delay);
        delay = sc_core::SC_ZERO_TIME;
    }' \
    '' \
    "test_noc_interconnect" \
    "incoming delay must be spent" \
    "the caller's annotated time was discarded instead of spent"

add_control \
    "offer-not-atomic" \
    "include/floo_noc_model/axi_endpoint.hpp" \
    '        if (txn.is_write ? buffer_.write_full() : buffer_.read_full()) {
            return false;
        }' \
    '' \
    "test_axi_endpoint" \
    "must refuse the offer, not throw" \
    "capacity was checked after the request flits had been queued, so a full metadata buffer threw with a burst already in flight"

add_control \
    "rlast-ignored" \
    "include/floo_noc_model/axi_chimney.hpp" \
    'r_rob_.i_rsp_last(i_r_pop_last);' \
    'r_rob_.i_rsp_last(const_true_);' \
    "test_axi_chimney_manager_response" \
    "non-final R beat" \
    "every beat of a read burst released a reorder-buffer counter, not just the last"

add_control \
    "region-decode-addition" \
    "src/noc_interconnect.cpp" \
    'if (addr >= entry.base && addr - entry.base < entry.size) {' \
    'if (addr >= entry.base && addr < entry.base + entry.size) {' \
    "test_noc_interconnect" \
    "region ending at UINT64_MAX must be writable" \
    "the mapped-region decode used an addition that wraps, making a region whose last byte is UINT64_MAX unreachable"

add_control \
    "axlen-truncated" \
    "include/floo_noc_model/axi_endpoint.hpp" \
    '        if (beats > axi_pkg::max_burst_beats) {' \
    '        if (false) {' \
    "test_axi_endpoint" \
    "257-beat read must be rejected" \
    "a burst longer than AxLEN can encode was narrowed instead of rejected, so 257 beats became ARLEN 0 and one beat was returned"

add_control \
    "tlm-mapping-slverr-decerr-swapped" \
    "src/noc_interconnect.cpp" \
    '    case axi_pkg::axi_resp::decerr:
        return tlm::TLM_ADDRESS_ERROR_RESPONSE;
    case axi_pkg::axi_resp::slverr:
        return tlm::TLM_GENERIC_ERROR_RESPONSE;' \
    '    case axi_pkg::axi_resp::decerr:
        return tlm::TLM_GENERIC_ERROR_RESPONSE;
    case axi_pkg::axi_resp::slverr:
        return tlm::TLM_ADDRESS_ERROR_RESPONSE;' \
    "test_noc_interconnect" \
    "SLVERR must map to TLM_GENERIC_ERROR_RESPONSE" \
    "the two AXI error codes were exchanged at the TLM boundary, so a decode failure looked like a target refusal and the reverse"

add_control \
    "tlm-mapping-exokay-as-error" \
    "src/noc_interconnect.cpp" \
    '    case axi_pkg::axi_resp::okay:
    case axi_pkg::axi_resp::exokay:
        return tlm::TLM_OK_RESPONSE;' \
    '    case axi_pkg::axi_resp::okay:
        return tlm::TLM_OK_RESPONSE;
    case axi_pkg::axi_resp::exokay:
        return tlm::TLM_GENERIC_ERROR_RESPONSE;' \
    "test_noc_interconnect" \
    "EXOKAY must map to TLM_OK_RESPONSE" \
    "EXOKAY, the success code for an exclusive access, was reported to the caller as a failure"

add_control \
    "b-response-discarded" \
    "include/floo_noc_model/axi_endpoint.hpp" \
    '            done.resp = flit.b.resp;' \
    '            done.resp = 0;' \
    "test_axi_endpoint" \
    "a B response must carry" \
    "the B response code was thrown away, so a failed write completed as OK"

# ---- found by the round-2 review -------------------------------------------

add_control \
    "sparse-beat-renumbering" \
    "src/noc_interconnect.cpp" \
    '        const std::uint64_t aw_addr = entry.addr;' \
    '        const std::uint64_t aw_addr = entry.addr
            - static_cast<std::uint64_t>(
                  (capture.strb.empty() || capture.strb[0] != 0)
                      ? 0
                      : bus_bytes);' \
    "test_noc_interconnect" \
    "not one beat earlier" \
    "a write whose leading beat was fully disabled had its data placed one beat too early, and still reported success"

add_control \
    "widened-read-policy-removed" \
    "src/noc_interconnect.cpp" \
    '        && impl_->targets[static_cast<std::size_t>(first_slot)].kind
               != target_kind::memory) {' \
    '        && false) {' \
    "test_noc_interconnect" \
    "widened read of an MMIO target must be refused" \
    "a read wider than the request reached an MMIO target, where a neighbouring register may clear on read"

add_control \
    "r-burst-error-lost" \
    "include/floo_noc_model/axi_endpoint.hpp" \
    '            done.resp = burst_resp_;
            read_beats_.clear();' \
    '            done.resp = flit.r.resp;
            read_beats_.clear();' \
    "test_axi_endpoint" \
    "a later OKAY beat must not erase an earlier SLVERR" \
    "an error on an intermediate R beat was erased by a later OKAY"

# ---- found by the round-3 review -------------------------------------------
#
# Three checks that the round-3 acceptance criteria named and the control set
# did not have. Two of them were written against tests that a mutation walked
# straight through, which is the whole reason the controls are mandatory: a test
# that never observes the defect is indistinguishable from one that does until
# something breaks the code underneath it.

# R3-F3. There is no `placed` flag to restore any more, so the mutation
# reproduces its *observable* rule instead: skip the conflict check when the
# target is going onto node (0,0), which is where every unplaced port sits. The
# defect this brings back is the one R3-F3 removed — a target on an unplaced
# port's default node accepted during configuration and refused later, from
# `end_of_elaboration()`.
add_control \
    "self-node-check-skips-default" \
    "src/noc_interconnect.cpp" \
    '        if (impl_->index_of(impl_->initiator_nodes[port]) != node_index) {
            continue;
        }' \
    '        if (impl_->index_of(impl_->initiator_nodes[port]) != node_index
            || node_index == 0) {
            continue;
        }' \
    "test_noc_interconnect_bad_config" \
    "refused by add_target itself" \
    "a target on an unplaced port's documented default (0,0) was accepted during configuration and rejected only at end_of_elaboration, which is the late failure the check was moved forward to avoid"

# Commit the new position before the conflict loop, so a refused placement still
# moves the port. The previous version of this test re-placed the port at its
# original node and asserted the call succeeded, which this mutation passes.
add_control \
    "place-initiator-not-atomic" \
    "src/noc_interconnect.cpp" \
    '    const unsigned node_index = impl_->index_of(where);
    for (const auto& entry : impl_->targets) {
        if (!entry.mapped || entry.node != node_index) {' \
    '    const unsigned node_index = impl_->index_of(where);
    impl_->initiator_nodes[index] = where;
    for (const auto& entry : impl_->targets) {
        if (!entry.mapped || entry.node != node_index) {' \
    "test_noc_interconnect_bad_config" \
    "must still occupy its old node" \
    "a rejected placement moved the port anyway, so the throw reported a failure that had already been committed"

# Delete the beat-frame guard. Reaching it needs a region whose end is not
# bus-aligned; with the aligned regions the tests used before, the whole-range
# decode rejected every overrunning access first and this mutation was invisible.
add_control \
    "beat-frame-guard-removed" \
    "src/noc_interconnect.cpp" \
    '        const std::uint64_t frame_end = shape.beat0_addr + (frame_bytes - 1);
        if (impl_->decode(shape.beat0_addr) != first_slot
            || impl_->decode(frame_end) != first_slot) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }' \
    '' \
    "test_noc_interconnect" \
    "beat frame leaves the region must be refused" \
    "a full-width transfer whose beat frame leaves its target's region was accepted; the read then fetched bytes from outside the mapping, and the write was accepted describing an AXI burst that crosses the region boundary"

# ---- Step A-1: the manager-side response path, against RTL ------------------
#
# These three run against `chimney_mgr_rsp_trace_sc`, which replays the shared
# stimulus and compares every cycle to `chimney_mgr_rsp_expected.csv` — a trace
# produced by the unmodified frozen `floo_axi_chimney.sv`, not by this model.
# That makes them the strongest controls in this file: a divergence is measured
# against hardware rather than against another opinion of the same author.
#
# `rlast-ignored` above reinjects the same first defect against the unit test.
# Both are kept deliberately. The unit test says the module's own contract
# broke; this one says the RTL disagrees.

A1_ARGS="@SRC@/tests/data/chimney_mgr_rsp_stimulus.csv @EVIDENCE@/model.csv @SRC@/tests/data/chimney_mgr_rsp_expected.csv"

add_control \
    "rlast-ignored-vs-rtl" \
    "include/floo_noc_model/axi_chimney.hpp" \
    'r_rob_.i_rsp_last(i_r_pop_last);' \
    'r_rob_.i_rsp_last(const_true_);' \
    "chimney_mgr_rsp_trace_sc" \
    "trace mismatch" \
    "every beat of a read burst released a reorder-buffer counter instead of only RLAST, and the RTL counter disagrees from the first beat of the burst" \
    "${A1_ARGS}"

add_control \
    "rsp-ready-channel-swapped" \
    "include/floo_noc_model/axi_chimney.hpp" \
    '        if (channel == axi_channel::b) {
            ready = i_b_rob_ready.read();
        } else if (channel == axi_channel::r) {
            ready = i_r_rob_ready.read();
        }' \
    '        if (channel == axi_channel::b) {
            ready = i_r_rob_ready.read();
        } else if (channel == axi_channel::r) {
            ready = i_b_rob_ready.read();
        }' \
    "chimney_mgr_rsp_trace_sc" \
    "trace mismatch" \
    "floo_rsp_o.ready was selected by the wrong channel, so a B flit followed the R manager's ready and the reverse" \
    "${A1_ARGS}"

# The response *payload*, not just its handshake. The first version of the A-1
# trace cast `RDATA` to 32 bits and omitted `BUSER`/`RUSER` entirely, so these
# three mutations all PASSed: the B/R channel was signed only from bit 31 down,
# with no user field at all. Each is separated from the others so a detection
# names exactly which field stopped being compared.
add_control \
    "rdata-upper-word-truncated" \
    "include/floo_noc_model/axi_chimney.hpp" \
    '        o_axi_r.write(flit.r);' \
    '        axi_r_chan r_truncated = flit.r;
        r_truncated.data &= 0xFFFFFFFFull;
        o_axi_r.write(r_truncated);' \
    "chimney_mgr_rsp_trace_sc" \
    "trace mismatch" \
    "the upper 32 bits of RDATA were dropped, which the first version of this cross-check could not see because it traced only the low word" \
    "${A1_ARGS}"

add_control \
    "ruser-dropped" \
    "include/floo_noc_model/axi_chimney.hpp" \
    '        o_axi_r.write(flit.r);' \
    '        axi_r_chan r_nouser = flit.r;
        r_nouser.user = 0;
        o_axi_r.write(r_nouser);' \
    "chimney_mgr_rsp_trace_sc" \
    "trace mismatch" \
    "RUSER was not forwarded to the manager, which no trace covered until the payload was traced in full" \
    "${A1_ARGS}"

add_control \
    "buser-dropped" \
    "include/floo_noc_model/axi_chimney.hpp" \
    '        o_axi_b.write(flit.b);' \
    '        axi_b_chan b_nouser = flit.b;
        b_nouser.user = 0;
        o_axi_b.write(b_nouser);' \
    "chimney_mgr_rsp_trace_sc" \
    "trace mismatch" \
    "BUSER was not forwarded to the manager" \
    "${A1_ARGS}"

add_control \
    "r-pop-id-from-b-payload" \
    "include/floo_noc_model/axi_chimney.hpp" \
    'o_r_pop_id.write(static_cast<unsigned>(flit.r.id));' \
    'o_r_pop_id.write(static_cast<unsigned>(flit.b.id));' \
    "chimney_mgr_rsp_trace_sc" \
    "trace mismatch" \
    "the R counter was released by the id in the B payload rather than the R payload, so a burst decremented the wrong per-id counter" \
    "${A1_ARGS}"

# ---- Steps A-2/A-3: integrated signal-driven datapath ----------------------
#
# A-2 assembled the three chimney quadrants and both physical meshes into one
# per-node composition. Swapping these two private ready links is syntactically
# valid and leaves every standalone chimney test untouched; only a test that
# really traverses `axi_chimney_node` can observe that B follows the R ordering
# gate and R follows the B gate.
add_control \
    "chimney-node-rob-ready-swapped" \
    "include/floo_noc_model/axi_noc.hpp" \
    '        manager_response_.i_b_rob_ready(b_rob_ready_);
        manager_response_.i_r_rob_ready(r_rob_ready_);' \
    '        manager_response_.i_b_rob_ready(r_rob_ready_);
        manager_response_.i_r_rob_ready(b_rob_ready_);' \
    "test_axi_noc_chimney" \
    "stalled R must restore ID and preserve every field" \
    "the composed node crossed the private B/R reorder-buffer ready links, so an R response followed B's ordering state and the stalled R payload was corrupted"

# A-3 replaced the endpoint method calls with per-cycle AXI signal driving.
# Shift only the AW signal payload, leaving destination decode and the TLM
# request unchanged. The transaction still builds, routes and returns OKAY, so
# detection requires the integrated wrapper test to inspect where the bytes
# actually landed rather than merely seeing a completed response.
add_control \
    "a3-manager-aw-address-shifted" \
    "src/noc_interconnect.cpp" \
    '                    manager.aw.write(request.aw);' \
    '                    axi_aw_chan shifted_aw = request.aw;
                    shifted_aw.addr += bus_bytes;
                    manager.aw.write(shifted_aw);' \
    "test_noc_interconnect" \
    "the enabled bytes must land, in the right order" \
    "the A-3 manager adapter drove an AW address one bus beat away from the original TLM request, so writes completed but changed the wrong target bytes"

# ── run them ─────────────────────────────────────────────────────────────────
detected=0
missed=0

for index in "${!names[@]}"; do
    name="${names[${index}]}"
    evidence="${work_dir}/${name}"
    mkdir -p "${evidence}"

    # A private copy per control: nothing below can reach the working tree.
    source_copy="${evidence}/src"
    cp -r "${component_dir}" "${source_copy}"
    rm -rf "${source_copy}/build"

    target_file="${source_copy}/${files[${index}]}"
    if ! NEEDLE="${needles[${index}]}" REPLACEMENT="${replacements[${index}]}" \
         python3 - "${target_file}" <<'PY'
import os, sys
path = sys.argv[1]
needle = os.environ["NEEDLE"]
replacement = os.environ["REPLACEMENT"]
text = open(path).read()
if needle not in text:
    sys.exit(2)
open(path, "w").write(text.replace(needle, replacement, 1))
PY
    then
        echo "MISSED ${name}: the injection point no longer exists in ${files[${index}]}." >&2
        echo "        The code changed; update this control or remove it." >&2
        missed=$((missed + 1))
        continue
    fi

    build_dir="${evidence}/build"
    if ! cmake -S "${source_copy}" -B "${build_dir}" \
              -DSYSTEMC_HOME="${systemc_home}" \
              -DFLOO_NOC_MODEL_BUILD_TESTS=ON \
              >"${evidence}/configure.log" 2>&1; then
        echo "MISSED ${name}: the mutated source does not configure." >&2
        echo "        See ${evidence}/configure.log" >&2
        missed=$((missed + 1))
        continue
    fi

    test_name="${tests[${index}]}"
    if ! cmake --build "${build_dir}" --target "${test_name}" --parallel \
              >"${evidence}/build.log" 2>&1; then
        # Not a detection. A mutation that will not compile proves nothing about
        # whether any test covers the behaviour it was meant to break.
        echo "MISSED ${name}: the mutated source does not build." >&2
        echo "        A build failure is not detection. See ${evidence}/build.log" >&2
        missed=$((missed + 1))
        continue
    fi

    # Placeholders resolve to this control's private copy, never the tree.
    test_args=()
    if [[ -n "${args[${index}]}" ]]; then
        resolved="${args[${index}]//@SRC@/${source_copy}}"
        resolved="${resolved//@EVIDENCE@/${evidence}}"
        read -r -a test_args <<<"${resolved}"
    fi

    if "${build_dir}/tests/${test_name}" "${test_args[@]}" \
         >"${evidence}/test.log" 2>&1; then
        echo "MISSED ${name}: ${test_name} still passes." >&2
        echo "        Defect: ${whys[${index}]}" >&2
        echo "        See ${evidence}/test.log" >&2
        missed=$((missed + 1))
        continue
    fi

    expect="${expects[${index}]}"
    if ! grep -qF "${expect}" "${evidence}/test.log"; then
        echo "MISSED ${name}: ${test_name} failed, but not for the expected" >&2
        echo "        reason. Wanted output containing: ${expect}" >&2
        echo "        See ${evidence}/test.log" >&2
        missed=$((missed + 1))
        continue
    fi

    echo "detected ${name} (${test_name} fails as expected)"
    detected=$((detected + 1))
    # Keep only failing evidence; a detected control's copy is large and dull.
    rm -rf "${source_copy}" "${build_dir}"
done

echo
echo "negative controls: ${detected} detected, ${missed} missed"
echo "evidence kept under ${work_dir}"
if [[ ${missed} -ne 0 ]]; then
    echo "FAIL: every control must build, run, and fail for its own reason" >&2
    exit 1
fi
echo "negative controls PASS"
