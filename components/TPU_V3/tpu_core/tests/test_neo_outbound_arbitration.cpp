// SPDX-License-Identifier: Apache-2.0
//
// The external bridge's outbound arbiter (Phase 8).
//
// A NEO-CORE has two named things that leave it — the hart and the DMA — and
// **one** external socket. Nothing arbitrated them until Phase 8, and nothing
// could notice: while every downstream target only annotates delay, a caller
// returns before the other one can run, so the two never overlap. The moment
// something downstream blocks they do, and the chip fabric refuses the second
// transaction because it treats a core as one initiator. That is how this was
// found — by running the chip composition gate with its fabric in `arbitrated`
// mode — and Phase 9's first real NoC hop would have found it again.
//
// So this file tests the arbiter under a target that actually waits, which is
// the only condition in which it does anything:
//
//   * one transaction at a time reaches the external socket. Measured **at the
//     target**, because that is the only place the invariant is observable;
//   * the two initiators alternate. Rotating priority is a behaviour here and
//     nothing anywhere else in the core would starve a hart behind a DMA;
//   * a reset while an initiator is blocked releases it with an error rather
//     than leaving it waiting for a grant no arbiter will issue -- **and does
//     not hand the port away underneath the initiator that is still inside the
//     downstream call**, which reset cannot unwind;
//   * the bridge still forwards after a reset. Stated as a transaction that
//     has to complete, not as an assertion on a counter: the first version of
//     this check read `outbound_requests() >= 0`, which is a tautology on an
//     unsigned type and would have passed a permanently wedged arbiter.
//
// Exit codes: 0 pass, 1 fail.

#include "tpu_v3/core/neo_external_bridge.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"
#include "tpu_v3/core/neo_local_sram_fabric.h"
#include "tpu_v3/sram/core_sram.h"

namespace tpu = cdc::components::tpu_v3;
namespace core = cdc::components::tpu_v3::core;
namespace sram = cdc::components::tpu_v3::sram;
namespace am = cdc::components::tpu_v3::address_map;

using core::neo_external_bridge;
using core::neo_local_sram_fabric;
using core::outbound_initiator;

namespace {

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ':'      \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__       \
                      << ':' << __LINE__ << '\n';                             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

constexpr tpu::chip_id_t kChip = 1;
constexpr tpu::core_id_t kCore = 0;

/// An external target that takes real time to answer.
///
/// Nothing about this is artificial: a contended SRAM bank, an `arbitrated`
/// chip fabric and a NoC hop all block inside `b_transport` the same way. It is
/// simply the condition under which the arbiter exists.
class slow_external : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<slow_external> socket;

    sc_core::sc_time service{40, sc_core::SC_NS};
    unsigned in_flight = 0;
    unsigned peak_in_flight = 0;
    /// Who was served, in order, read out of the payload rather than from a
    /// variable the caller set before it blocked.
    std::vector<int> order;
    /// When each one arrived. This is what makes "a contending request waits
    /// until it has actually arrived" observable from outside the arbiter.
    std::vector<sc_core::sc_time> arrivals;

    explicit slow_external(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &slow_external::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        order.push_back(trans.get_data_length() > 0
                            ? static_cast<int>(trans.get_data_ptr()[0])
                            : -1);
        arrivals.push_back(sc_core::sc_time_stamp());
        ++in_flight;
        peak_in_flight = std::max(peak_in_flight, in_flight);
        sc_core::wait(service);
        --in_flight;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

/// Drives one of the bridge's outbound sockets from its own process.
class contender : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(contender);

    tlm_utils::simple_initiator_socket<contender> socket;

    unsigned completed = 0;
    unsigned errors = 0;
    bool finished = false;
    /// When the last request returned. The observable for "a reset woke it
    /// promptly" — a bare timed wait would return only when its quantum ran
    /// out, which is a time, not a state.
    sc_core::sc_time finished_at = sc_core::SC_ZERO_TIME;
    /// `sc_time_stamp()` at the instant `b_transport` returned, before this
    /// process consumes whatever delay came back. Kept separate from
    /// `finished_at` because a correctly returned remainder makes the two
    /// differ, and it is the *return* that says whether the reset was prompt.
    sc_core::sc_time returned_at = sc_core::SC_ZERO_TIME;

    /// **Set if this initiator's logical time ever moved backwards.**
    ///
    /// A temporally decoupled initiator's logical time is
    /// `sc_time_stamp() + delay`, and VP++ *sets* its quantum keeper from the
    /// delay a target returns rather than adding to it (`common/mem.h:105`).
    /// So a component that returns a smaller delay than the time it actually
    /// consumed does not merely lose accuracy — it moves the hart into its own
    /// past. Modelled here rather than trusted: the earlier version of this
    /// bench waited on the returned delay and would have passed against a
    /// component that returned zero from an interrupted catch-up.
    bool went_backwards = false;
    sc_core::sc_time worst_rollback = sc_core::SC_ZERO_TIME;

    /// `carry` is the TLM delay each request arrives with. Non-zero models a
    /// temporally decoupled initiator: VP++ passes its quantum-keeper local
    /// time in `delay` (`common/mem.h:105`), so a hart's requests routinely
    /// carry one.
    contender(sc_core::sc_module_name name, int id, unsigned repeats,
              sc_core::sc_time start,
              sc_core::sc_time carry = sc_core::SC_ZERO_TIME)
        : sc_core::sc_module(name)
        , socket("socket")
        , id_(id)
        , repeats_(repeats)
        , start_(start)
        , carry_(carry)
    {
        SC_THREAD(run);
    }

private:
    void run()
    {
        if (start_ != sc_core::SC_ZERO_TIME) {
            sc_core::wait(start_);
        }
        for (unsigned i = 0; i < repeats_; ++i) {
            // Its own index in every byte, so the target can say which
            // initiator it is serving.
            std::vector<unsigned char> data(4,
                                            static_cast<unsigned char>(id_));
            tlm::tlm_generic_payload trans;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(am::global_ram_base);
            trans.set_data_ptr(data.data());
            trans.set_data_length(4);
            trans.set_streaming_width(4);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            sc_core::sc_time delay = carry_;
            const sc_core::sc_time logical_before
                = sc_core::sc_time_stamp() + delay;

            socket->b_transport(trans, delay);

            returned_at = sc_core::sc_time_stamp();
            const sc_core::sc_time logical_after
                = sc_core::sc_time_stamp() + delay;
            if (logical_after < logical_before) {
                went_backwards = true;
                const auto rollback = logical_before - logical_after;
                if (rollback > worst_rollback) {
                    worst_rollback = rollback;
                }
            }

            if (trans.get_response_status() == tlm::TLM_OK_RESPONSE) {
                ++completed;
            } else {
                ++errors;
            }
            // A decoupled initiator consumes whatever delay comes back before
            // its next request, the way VP++'s quantum keeper does.
            if (delay != sc_core::SC_ZERO_TIME) {
                sc_core::wait(delay);
            }
        }
        finished = true;
        finished_at = sc_core::sc_time_stamp();
    }

    int id_;
    unsigned repeats_;
    sc_core::sc_time start_;
    sc_core::sc_time carry_;
};

/// Calls `reset()` at a chosen instant.
class resetter : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(resetter);

    resetter(sc_core::sc_module_name name, neo_external_bridge& bridge,
             sc_core::sc_time at)
        : sc_core::sc_module(name)
        , bridge_(bridge)
        , at_(at)
    {
        SC_THREAD(run);
    }

    bool fired = false;

private:
    void run()
    {
        sc_core::wait(at_);
        bridge_.reset();
        fired = true;
    }

    neo_external_bridge& bridge_;
    sc_core::sc_time at_;
};

sram::core_sram_config sram_config()
{
    sram::core_sram_config out;
    out.base_address = am::core_sram_base(kChip, kCore);
    out.window_bytes = am::core_sram_window;
    out.capacity_bytes = 64 * 1024;
    return out;
}

tpu::local_sram_fabric_config fabric_config()
{
    tpu::local_sram_fabric_config out;
    out.data_width_bits = 128;
    out.bank_count = 4;
    out.mapping = tpu::bank_mapping::low_order_interleaved;
    out.pipeline_stages = 1;
    out.max_outstanding_per_requester = 1;
    out.arbitration = tpu::arbitration_policy::round_robin;
    return out;
}

core::core_aperture_spec aperture()
{
    core::core_aperture_spec spec;
    spec.core_base = am::core_base(kChip, kCore);
    spec.core_size = am::core_aperture_stride;
    spec.sram_base = am::core_sram_base(kChip, kCore);
    spec.sram_window = am::core_sram_window;
    return spec;
}

/// Answers nothing. It exists because SystemC refuses to elaborate an unbound
/// port and a socket cannot be created outside a module; this bench never sends
/// anything down the inbound control path.
class null_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<null_target> socket;

    explicit null_target(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &null_target::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }
};

/// Sends nothing. `bridge.inbound` is a target socket and SystemC refuses to
/// elaborate one with no initiator on it; this bench drives only the outbound
/// side.
class null_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<null_master> socket;

    explicit null_master(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }
};

/// A bridge with its mandatory fabric, and nothing else.
struct bench {
    sram::core_sram store;
    neo_local_sram_fabric fabric;
    neo_external_bridge bridge;
    slow_external outside;
    null_target control;
    null_master remote;

    explicit bench(const std::string& prefix)
        : store((prefix + "_sram").c_str(), sram_config())
        , fabric((prefix + "_fabric").c_str(), fabric_config(), store,
                 {sram::neo_requester::cpu,
                  sram::neo_requester::external_inbound})
        , bridge((prefix + "_bridge").c_str(), aperture(), fabric)
        , outside((prefix + "_outside").c_str())
        , control((prefix + "_control").c_str())
        , remote((prefix + "_remote").c_str())
    {
        bridge.external.bind(outside.socket);
        bridge.inbound_control.bind(control.socket);
        remote.socket.bind(bridge.inbound);
    }
};

} // namespace

int sc_main(int, char*[])
{
    // ── fairness ─────────────────────────────────────────────────────────────
    bench fair("fair");
    contender fair_cpu("fair_cpu", 0, 6, sc_core::SC_ZERO_TIME);
    contender fair_dma("fair_dma", 1, 6, sc_core::SC_ZERO_TIME);
    fair_cpu.socket.bind(
        fair.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::cpu)]);
    fair_dma.socket.bind(
        fair.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::dma)]);

    // ── reset while blocked, and reset while somebody is downstream ──────────
    //
    // One 1 us transaction from the hart, a DMA request that queues behind it
    // at 100 ns, and a reset at 300 ns — while the hart is still inside the
    // target. The DMA issues **twice**: the first is the queued request the
    // reset abandons, and the second goes out immediately afterwards, into the
    // window where the hart still owns the port. If reset released the port on
    // the hart's behalf, that second request walks straight into the target
    // alongside it.
    bench abandon("abandon");
    abandon.outside.service = sc_core::sc_time(1, sc_core::SC_US);
    contender abandon_cpu("abandon_cpu", 0, 2, sc_core::SC_ZERO_TIME);
    contender abandon_dma("abandon_dma", 1, 2,
                          sc_core::sc_time(100, sc_core::SC_NS));
    abandon_cpu.socket.bind(
        abandon.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::cpu)]);
    abandon_dma.socket.bind(
        abandon.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::dma)]);
    resetter reset_at("reset_at", abandon.bridge,
                      sc_core::sc_time(300, sc_core::SC_NS));

    // ── a contender arrives when it says it arrives ──────────────────────────
    //
    // The DMA takes the port at t = 0 and holds it for 200 ns. The hart issues
    // at t = 0 too, but carrying a 1 us quantum — in simulated time it has not
    // reached t = 0 + 1 us yet. It must therefore not be served at 200 ns when
    // the port frees; it has to wait until it has actually arrived.
    //
    // The DMA is constructed **first** on purpose: SystemC runs threads in
    // creation order, and the initiator that runs first takes a free port on
    // the uncontended fast path without consuming its delay. Letting the hart
    // go first would measure that documented shortcut instead of the queueing
    // rule under test.
    bench arrival("arrival");
    arrival.outside.service = sc_core::sc_time(200, sc_core::SC_NS);
    contender arrival_dma("arrival_dma", 1, 1, sc_core::SC_ZERO_TIME);
    contender arrival_cpu("arrival_cpu", 0, 1, sc_core::SC_ZERO_TIME,
                          sc_core::sc_time(1, sc_core::SC_US));
    arrival_dma.socket.bind(
        arrival.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::dma)]);
    arrival_cpu.socket.bind(
        arrival.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::cpu)]);

    // ── a reset reaches a request that is catching up on its quantum ────────
    //
    // The third population. The DMA takes the port at t = 0 and holds it for
    // 5 us. The hart arrives at t = 0 carrying a 3 us quantum, finds the port
    // busy, and starts waiting out that quantum — at which point it is
    // registered nowhere: not queued on the arbiter, not downstream. The reset
    // at 500 ns has to reach it anyway. A bare `wait(delay)` cannot be woken by
    // an event, so it would return at 3 us instead.
    bench late("late");
    late.outside.service = sc_core::sc_time(5, sc_core::SC_US);
    contender late_dma("late_dma", 1, 1, sc_core::SC_ZERO_TIME);
    contender late_cpu("late_cpu", 0, 1, sc_core::SC_ZERO_TIME,
                       sc_core::sc_time(3, sc_core::SC_US));
    late_dma.socket.bind(
        late.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::dma)]);
    late_cpu.socket.bind(
        late.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::cpu)]);
    resetter late_reset("late_reset", late.bridge,
                        sc_core::sc_time(500, sc_core::SC_NS));

    // Bounded, because a deadlock has to fail rather than hang the suite
    // (`INTERFACE_CONTRACT.md` §5).
    sc_core::sc_start(sc_core::sc_time(100, sc_core::SC_US));

    // ── fairness results ─────────────────────────────────────────────────────

    CHECK_MSG(fair_cpu.finished && fair_dma.finished,
              "an initiator never finished; the arbiter deadlocked or starved "
              "it");
    CHECK(fair_cpu.completed == 6 && fair_dma.completed == 6);

    CHECK_MSG(fair.outside.peak_in_flight == 1,
              "two transactions were inside the core's one external socket at "
              "the same time. The chip fabric treats a core as one initiator "
              "and refuses the second, so this is a model defect downstream, "
              "not a slow path");

    CHECK_MSG(fair.bridge.outbound_conflicts() > 0,
              "neither initiator ever found the port busy, so this run "
              "exercised no arbitration and the alternation below would be an "
              "accident");

    CHECK(fair.bridge.outbound_grants(outbound_initiator::cpu) == 6);
    CHECK(fair.bridge.outbound_grants(outbound_initiator::dma) == 6);

    unsigned alternations = 0;
    for (std::size_t i = 1; i < fair.outside.order.size(); ++i) {
        if (fair.outside.order[i] != fair.outside.order[i - 1]) {
            ++alternations;
        }
    }
    std::cout << "outbound port: " << alternations << " alternations in "
              << fair.outside.order.size() << " grants, "
              << fair.bridge.outbound_conflicts() << " conflicts\n";
    CHECK_MSG(alternations >= 9,
              "the external port did not alternate between the hart and the "
              "DMA: " + std::to_string(alternations) + " changes in "
                  + std::to_string(fair.outside.order.size())
                  + " grants, which is fixed priority rather than rotating. A "
                    "hart starved behind a bulk DMA is what that means in a "
                    "real core");

    // ── reset while blocked ──────────────────────────────────────────────────

    CHECK_MSG(reset_at.fired, "the reset never ran");

    CHECK_MSG(abandon_dma.finished,
              "the initiator blocked in the arbiter never returned. A reset "
              "that clears the waiting flags without waking anyone leaves it "
              "waiting for a grant no arbiter will ever issue");
    CHECK_MSG(abandon_dma.errors == 1,
              "the queued request was not abandoned with an error. Reset "
              "abandons in-flight work; completing it silently afterwards "
              "would be worse than either alternative (ARCHITECTURE.md §6)");

    CHECK_MSG(abandon_cpu.finished,
              "the initiator that held the port when the reset arrived never "
              "returned");
    CHECK_MSG(abandon_cpu.completed == 2 && abandon_cpu.errors == 0,
              "a transaction already inside the target when the reset arrived "
              "did not complete. Reset abandons what is queued; it cannot "
              "un-issue what a target has already been handed");

    // **The port is not handed away underneath its owner.**
    //
    // The DMA's second request goes out right after the reset, while the hart
    // is still inside its 1 us downstream call. If `reset()` had cleared the
    // busy flag on the hart's behalf — which it cannot honour, because it
    // cannot unwind the hart's blocked call — that request would enter the
    // target alongside it and this counter would read 2.
    CHECK_MSG(abandon.outside.peak_in_flight == 1,
              "two transactions were inside the external socket at once after "
              "a reset: the port was released on behalf of an initiator that "
              "was still inside its downstream call");

    // **The bridge still forwards, stated as a transaction rather than as an
    // assertion on a counter.** The DMA's second request is issued after the
    // reset and has to complete; an arbiter left wedged by the reset would
    // leave it unfinished, which `finished` and this count both notice.
    CHECK_MSG(abandon_dma.completed == 1,
              "the request issued after the reset never completed, so the "
              "arbiter did not recover");

    // ── arrival-time results ─────────────────────────────────────────────────

    CHECK_MSG(arrival_dma.completed == 1 && arrival_cpu.completed == 1,
              "an arrival-order contender did not complete");
    CHECK(arrival.outside.arrivals.size() == 2);
    if (arrival.outside.arrivals.size() == 2) {
        std::cout << "arrival order: dma at " << arrival.outside.arrivals[0]
                  << ", cpu at " << arrival.outside.arrivals[1] << '\n';
        CHECK_MSG(arrival.outside.order[0] == 1,
                  "the DMA did not take the free port first, so the hart never "
                  "contended and the check below measures nothing");
        CHECK_MSG(arrival.outside.arrivals[1]
                      >= sc_core::sc_time(1, sc_core::SC_US),
                  "a contending request was served before it had arrived: it "
                  "was still carrying a 1 us quantum and took its place in the "
                  "queue at an instant it had not reached, ahead of anything "
                  "that really was there");
    }

    // ── the reset reached the request catching up on its quantum ─────────────

    CHECK_MSG(late_reset.fired, "the late reset never ran");
    CHECK_MSG(late_cpu.finished,
              "the initiator waiting out its quantum never returned");
    CHECK_MSG(late_cpu.errors == 1,
              "the abandoned request did not report an error");

    std::cout << "quantum catch-up abandoned at " << late_cpu.returned_at
              << " (reset at 500 ns, quantum would have expired at 3 us)\n";
    CHECK_MSG(late_cpu.returned_at < sc_core::sc_time(1, sc_core::SC_US),
              "the reset did not reach a request that was waiting out its "
              "quantum: it returned at "
                  + late_cpu.finished_at.to_string()
                  + " rather than promptly after the 500 ns reset, so it slept "
                    "out the whole 3 us instead of being woken. That is a third "
                    "reset population -- neither queued on the arbiter nor "
                    "inside a downstream call -- and it holds its caller for a "
                    "full quantum after the reset");

    // **And it did not pay for that promptness with the hart's clock.**
    //
    // Waking early is only half the contract: the unelapsed part of the
    // quantum has to come back in `delay`, or a keeper that *sets* from it
    // moves the initiator into its own past.
    for (const auto* who : {&late_cpu, &late_dma, &arrival_cpu, &arrival_dma,
                            &abandon_cpu, &abandon_dma, &fair_cpu, &fair_dma}) {
        CHECK_MSG(!who->went_backwards,
                  std::string("an initiator's logical time moved backwards by ")
                      + who->worst_rollback.to_string()
                      + ". `sc_time_stamp() + delay` must never decrease across "
                        "a b_transport, and VP++ sets its quantum keeper from "
                        "the returned delay rather than adding to it");
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_neo_outbound_arbitration: all checks passed\n";
    return 0;
}
