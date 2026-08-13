// SPDX-License-Identifier: Apache-2.0
//
// NEO Local SRAM Fabric — the native local-data plane of one NEO-CORE
// (decision record D15, plan §11.7).
//
// ## What it owns
//
// Decode into the shared core SRAM, the mapping of one logical address space
// onto physically banked storage, deterministic per-bank round-robin
// arbitration, back-pressure on a bank conflict, independent progress on
// different banks, response ownership, and per-requester counters.
//
// It is deliberately **not** a full AXI data crossbar and must never become
// one, and it never hands out a pointer into the SRAM's backing store — the
// two prohibitions D15 states in as many words.
//
// ## Two timing modes, and why both exist
//
// `local_fabric_timing::annotated` is loosely timed: nothing ever blocks. Bank
// occupancy is tracked as a busy-until timestamp and the resulting
// serialisation is added to the caller's `delay`. This is the mode a target
// reachable from the detailed NoC must be behind, because a fabric that waits
// would stall the one process that advances the mesh clock
// (`INTERFACE_CONTRACT.md` §3), and it is the mode that keeps temporal
// decoupling worth having once a VP++ hart is attached in Phase 5.
//
// `local_fabric_timing::arbitrated` is approximately timed: requesters block
// on a real per-bank arbiter with rotating priority. Round-robin *fairness* is
// a behaviour in this mode rather than an estimate — with annotation alone,
// requests are processed in call order and round-robin is indistinguishable
// from first-come-first-served, so a "round-robin arbiter" would be an
// untested claim. The Phase 3 gate's fairness and back-pressure checks run
// here, under a watchdog.
//
// Both modes decode identically, split into identical beats, and return
// identical data and status. What differs is only how contention is charged.
//
// ## Ordering and outstanding requests
//
// Revision 1 is strictly in order with at most one request in flight per
// requester (D15), which a blocking call gives for free. Beats of one request
// are issued in ascending address order and completion is reported only after
// the last one, so a multi-beat access is *not* an atomic primitive against
// another requester — firmware must use the accelerator completion/status
// rules rather than depend on an undocumented wide-access atomicity
// (`INTERFACE_CONTRACT.md` §4.1).

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <systemc>

#include "tpu_v3/architecture_config.h"
#include "tpu_v3/sram/core_sram.h"
#include "tpu_v3/sram/native_port.h"

namespace cdc::components::tpu_v3::core {

using sram::neo_command;
using sram::neo_local_request;
using sram::neo_local_response;
using sram::neo_local_sram_if;
using sram::neo_requester;
using sram::neo_requester_count;
using sram::neo_status;

enum class local_fabric_timing {
    /// Loosely timed. No process ever waits; contention is annotated onto the
    /// caller's delay. Safe behind any TLM target path.
    annotated,
    /// Approximately timed. Requesters block on a real per-bank round-robin
    /// arbiter. Callers must be SystemC processes that may `wait()`.
    arbitrated,
};

const char* to_string(local_fabric_timing timing) noexcept;

/// One requester's traffic, as D7 requires it to be named: what is counted is
/// stated, and no counter is a vector-instruction or hardware bus-transaction
/// count.
struct requester_counters {
    /// Logical requests accepted, **whatever their outcome**. This is the D7
    /// "TLM request" quantity at this boundary: one entry per `b_access`,
    /// however many beats it became and whether or not it succeeded.
    ///
    /// Refused requests are counted here as well as in `error_count`, so
    /// `request_count >= error_count` always and the failure rate is a ratio
    /// of two numbers that count the same thing. A counter that silently
    /// dropped failures would hide exactly the case worth looking at.
    std::uint64_t request_count = 0;
    /// Physical bank accesses those requests were split into. Never presented
    /// as the request count, and never as a hardware bus transaction count.
    std::uint64_t physical_beat_count = 0;
    /// Beats that found their bank occupied. This is the back-pressure.
    std::uint64_t bank_conflict_count = 0;
    /// Grants issued by an arbiter to this requester.
    std::uint64_t arbitration_event_count = 0;
    std::uint64_t transferred_bytes = 0;
    std::uint64_t error_count = 0;
    sc_core::sc_time total_latency = sc_core::SC_ZERO_TIME;
};

class neo_local_sram_fabric : public sc_core::sc_module,
                              public virtual neo_local_sram_if {
public:
    /// `requesters` is the closed list of ports that exist on this instance.
    /// An access from anything else is a model or integration defect and
    /// throws: unattributable traffic is worse than refused traffic, because
    /// it silently corrupts every counter that follows.
    ///
    /// `cycle` is the fabric's clock period. One beat occupies its bank for
    /// one cycle and the response path costs `pipeline_stages` cycles.
    neo_local_sram_fabric(sc_core::sc_module_name name,
                          local_sram_fabric_config config,
                          sram::core_sram& sram,
                          std::vector<neo_requester> requesters,
                          local_fabric_timing timing
                              = local_fabric_timing::annotated,
                          sc_core::sc_time cycle
                              = sc_core::sc_time(1, sc_core::SC_NS));

    /// The native port. Requester identity travels in the request, so one
    /// export serves every attached requester and every counter is
    /// attributable by construction (D15 permits either form).
    sc_core::sc_export<neo_local_sram_if> native_port;

    void b_access(const neo_local_request& request,
                  neo_local_response& response,
                  sc_core::sc_time& delay) override;

    std::uint32_t dbg_access(const neo_local_request& request) override;

    // ── introspection ────────────────────────────────────────────────────────

    const local_sram_fabric_config& config() const noexcept { return config_; }
    local_fabric_timing timing() const noexcept { return timing_; }
    sc_core::sc_time cycle() const noexcept { return cycle_; }

    bool is_attached(neo_requester requester) const noexcept;

    const requester_counters& counters(neo_requester requester) const;

    /// Grants a given bank has issued to a given requester. This is what makes
    /// round-robin fairness a measurable property rather than a claim.
    std::uint64_t bank_grants(unsigned bank, neo_requester requester) const;

    std::uint64_t total_requests() const noexcept;
    std::uint64_t total_beats() const noexcept;
    std::uint64_t total_bytes() const noexcept;
    std::uint64_t total_bank_conflicts() const noexcept;

    /// Which bank an absolute address maps to under the configured mapping.
    unsigned bank_of(std::uint64_t address) const noexcept;

    /// Beats `[address, size)` would be split into. Exposed so a test can
    /// state its expectation in terms of the configuration rather than
    /// recomputing the fabric's own arithmetic.
    std::uint32_t beats_for(std::uint64_t address,
                            std::uint32_t size) const noexcept;

    /// Synchronous reset: abandon in-flight work, release every bank, clear
    /// the counters.
    ///
    /// A requester blocked in `b_access` when this is called does **not** keep
    /// waiting. Reset bumps a generation counter and wakes every bank, and any
    /// request from an older generation returns `neo_status::aborted` with
    /// whatever beats had already completed. The first version of this method
    /// cleared `waiting[]` and `busy` without waking anyone, which left a
    /// blocked requester waiting for a grant no arbiter would ever issue —
    /// a hang that a reset in the middle of contention would produce and that
    /// nothing in the suite could have distinguished from a deadlock.
    ///
    /// `ARCHITECTURE.md` §6 requires exactly this: an in-flight job is
    /// abandoned rather than silently completed.
    void reset();

    /// Multi-line report. Every timing figure derived from these counts is
    /// labelled `VP++ element-wise granularity` (D7): with the current
    /// backend one request usually carries one active element, that is a
    /// property of the backend and not an invariant, and an arbitration-event
    /// count is never evidence of equivalence with TPU hardware.
    std::string report() const;

private:
    struct bank_state {
        // ── annotated mode ───────────────────────────────────────────────────
        sc_core::sc_time busy_until = sc_core::SC_ZERO_TIME;

        // ── arbitrated mode ──────────────────────────────────────────────────
        bool busy = false;
        bool waiting[neo_requester_count] = {};
        /// Rotating priority: the next grant starts searching one past this.
        unsigned last_granted = neo_requester_count - 1;
        /// Notified whenever `busy` clears or a new waiter appears, so a
        /// blocked requester re-evaluates rather than polling.
        sc_core::sc_event changed;

        std::uint64_t grants[neo_requester_count] = {};
    };

    void charge_annotated(const neo_local_request& request,
                          neo_local_response& response,
                          sc_core::sc_time& delay);
    void charge_arbitrated(const neo_local_request& request,
                           neo_local_response& response,
                           sc_core::sc_time& delay,
                           std::uint64_t request_generation);

    /// Move one beat's bytes. Returns the status; the caller has already
    /// classified the whole request, so a failure here is a defect.
    neo_status move_beat(const neo_local_request& request,
                         std::uint64_t address, std::uint32_t offset,
                         std::uint32_t bytes);

    /// Deterministic rotating-priority selection among the waiters on `bank`.
    unsigned select_waiter(const bank_state& bank) const noexcept;

    local_sram_fabric_config config_;
    sram::core_sram& sram_;
    local_fabric_timing timing_;
    sc_core::sc_time cycle_;

    bool attached_[neo_requester_count] = {};

    /// One request in flight per requester (D15), enforced rather than
    /// assumed.
    ///
    /// A blocking call gives it for free only when one process drives one
    /// requester. Two processes sharing a requester id overlap the moment the
    /// first one waits for a bank, and then their beats interleave under a
    /// single identity: the counters stay plausible, the arbiter sees one
    /// contender where there are two, and the ownership guarantee the response
    /// path relies on is gone. It is an integration defect, so it throws.
    bool in_flight_[neo_requester_count] = {};

    /// Bumped by `reset()`. A request carrying an older generation abandons
    /// itself at its next resume point instead of waiting for a grant that
    /// will never come.
    std::uint64_t generation_ = 0;

    requester_counters counters_[neo_requester_count];
    std::vector<std::unique_ptr<bank_state>> banks_;
};

} // namespace cdc::components::tpu_v3::core
