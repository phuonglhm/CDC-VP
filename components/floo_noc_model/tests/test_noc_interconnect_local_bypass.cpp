// SPDX-License-Identifier: Apache-2.0
//
// Decision record D1: the owner-aware local bypass.
//
// A TPU chip is both a manager and a subordinate — it issues remote accesses
// and answers them — so its aperture has to be a target on the very node that
// hosts its upstream port. `floo_router` runs with `NoLoopback = 1`, which ties
// off the Eject-input to Eject-output crossbar leg, so a flit addressed to the
// node that injected it can never be delivered. That configuration does not
// fail; it hangs.
//
// D1's answer is to never create the flit: a target may name the port it is
// co-located with, and an access from that port is short-circuited to the
// target socket. The frozen router configuration is untouched.
//
// **The property that matters is negative, and that shapes this whole file.**
// "The access returned the right data" is equally true of a routed access, so
// it proves nothing about the bypass. What has to be shown is that *no flit
// existed*, and the only witness for that is the mesh's own passive counters:
// `accepted_flits` summed over both physical meshes, every router and every
// port. A bypassed access must leave that total bit-for-bit unchanged, while
// the very same address reached from a *different* port must move it.
//
// Configuration refusals live in `test_noc_interconnect_bad_config.cpp`, which
// is where this wrapper's refusals already live and which never starts a
// simulation: an out-of-range owner, an owner on another node, a co-located
// target with no owner at all, and an owner that tries to move off the node of
// what it owns. (An earlier version of this comment said that and it was not
// true yet -- the file contained no `local_owner` call.)

#include "floo_noc_model/noc_interconnect.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace {

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

constexpr std::uint64_t region_size = 0x1000;
/// Rounds each concurrent manager performs. Enough that their accesses
/// interleave many times rather than happening to miss each other.
constexpr unsigned concurrent_rounds = 24;
constexpr std::uint64_t local_base = 0x0000'0000;
constexpr std::uint64_t remote_base = 0x0001'0000;
/// A second co-located region, owned by the same port, with a slow target. Used
/// to hold a bypass open across the moment routed traffic drains.
constexpr std::uint64_t slow_local_base = 0x0002'0000;

/// Byte-addressed storage with a small access latency, so the bypass's
/// target-latency rounding is exercised rather than assumed away.
class memory_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<memory_target> socket;

    memory_target(sc_core::sc_module_name name,
                  sc_core::sc_time latency = sc_core::SC_ZERO_TIME)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(region_size, 0)
        , latency_(latency)
    {
        socket.register_b_transport(this, &memory_target::b_transport);
        socket.register_transport_dbg(this, &memory_target::transport_dbg);
    }

    std::uint64_t accesses = 0;

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        ++accesses;
        access(trans);
        delay += latency_;
        if (trans.get_response_status() == tlm::TLM_INCOMPLETE_RESPONSE) {
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        }
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans);
    }

    unsigned int access(tlm::tlm_generic_payload& trans)
    {
        const std::uint64_t offset = trans.get_address();
        const unsigned length = trans.get_data_length();
        if (offset + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return 0;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&storage_[offset], trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), &storage_[offset], length);
        }
        return length;
    }

    std::vector<unsigned char> storage_;
    sc_core::sc_time latency_;
};

/// One upstream port.
class driver : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<driver> socket;

    explicit driver(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    tlm::tlm_response_status access(tlm::tlm_command command,
                                    std::uint64_t address, unsigned char* data,
                                    unsigned length)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        if (delay != sc_core::SC_ZERO_TIME) {
            sc_core::wait(delay);
        }
        return trans.get_response_status();
    }

    unsigned int debug(std::uint64_t address, unsigned char* data,
                       unsigned length)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        return socket->transport_dbg(trans);
    }
};

/// Every flit either mesh accepted, on every router port.
///
/// This is the whole evidence base for "no flit was created". It reads the
/// passive RTL-signed counters rather than anything the bypass itself
/// maintains — a counter the bypass increments could be wrong in exactly the
/// way the bypass is wrong.
std::uint64_t total_accepted_flits(
    const cdc::components::noc_interconnect& noc)
{
    const auto snapshot = noc.detailed_counter_snapshot();
    std::uint64_t total = 0;
    for (const auto* mesh : {&snapshot.request, &snapshot.response}) {
        for (const auto& router : mesh->routers) {
            for (const auto& in : router.inputs) {
                total += in.accepted_flits;
            }
            for (const auto& out : router.outputs) {
                total += out.accepted_flits;
            }
        }
    }
    return total;
}

/// Drives one port repeatedly from its own process, so two managers can be in
/// flight against the same target at once.
class hammer : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(hammer);

    /// `go` is what keeps this off the sequential phase's toes: a mesh port has
    /// exactly one initiator socket, so these reuse the same `driver` objects
    /// and must not run while the scenario thread is using them.
    hammer(sc_core::sc_module_name name, driver& drv, sc_core::sc_event& go,
           std::uint64_t address, unsigned char tag, unsigned repeats)
        : sc_core::sc_module(name)
        , drv_(drv)
        , go_(go)
        , address_(address)
        , tag_(tag)
        , repeats_(repeats)
    {
        SC_THREAD(run);
    }

    unsigned completed = 0;
    unsigned mismatches = 0;
    bool finished = false;

private:
    void run()
    {
        sc_core::wait(go_);
        for (unsigned i = 0; i < repeats_; ++i) {
            // Each manager owns its own four bytes of the target and writes a
            // pattern only it can produce, then reads them back. If the two
            // paths ever crossed a response or a payload, the reader would see
            // the other manager's tag.
            std::vector<unsigned char> out(4, tag_);
            if (drv_.access(tlm::TLM_WRITE_COMMAND, address_, out.data(), 4)
                != tlm::TLM_OK_RESPONSE) {
                ++mismatches;
                continue;
            }
            std::vector<unsigned char> in(4, 0);
            if (drv_.access(tlm::TLM_READ_COMMAND, address_, in.data(), 4)
                != tlm::TLM_OK_RESPONSE) {
                ++mismatches;
                continue;
            }
            if (in != out) {
                ++mismatches;
                continue;
            }
            ++completed;
        }
        finished = true;
    }

    driver& drv_;
    sc_core::sc_event& go_;
    std::uint64_t address_;
    unsigned char tag_;
    unsigned repeats_;
};

/// Two accesses on **one** upstream port, in the same AXI channel: an earlier
/// routed one and a later bypassed one.
///
/// This is the case the sequential and two-port checks cannot reach. Ordering
/// under `MaxUniqueIds = 1` is a promise about one port -- read completions
/// FIFO among reads, write completions FIFO among writes -- so a bypass that
/// returned the instant its target answered would hand its caller a response
/// before an access issued earlier on the same channel. Every functional check
/// still passes in that world, because each access on its own is correct.
class order_probe : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(order_probe);

    order_probe(sc_core::sc_module_name name, driver& drv,
                std::uint64_t address, int tag, sc_core::sc_time start,
                std::vector<int>& order)
        : sc_core::sc_module(name)
        , drv_(drv)
        , address_(address)
        , tag_(tag)
        , start_(start)
        , order_(order)
    {
        SC_THREAD(run);
    }

    bool finished = false;

private:
    void run()
    {
        if (start_ != sc_core::SC_ZERO_TIME) {
            sc_core::wait(start_);
        }
        std::vector<unsigned char> data(4, static_cast<unsigned char>(tag_));
        drv_.access(tlm::TLM_WRITE_COMMAND, address_, data.data(), 4);
        order_.push_back(tag_);
        finished = true;
    }

    driver& drv_;
    std::uint64_t address_;
    int tag_;
    sc_core::sc_time start_;
    std::vector<int>& order_;
};

/// What the R-P9-2 gate samples while a routed call is parked behind a bypass.
///
/// **Not** "how many callers are inside `b_transport`". A caller blocked in the
/// admission gate is inside `b_transport` too, so that number is 2 whether the
/// bound is honoured or not — it was the first thing this bench measured and it
/// cannot tell the two apart. What distinguishes them is the wrapper's own
/// accounting and what actually reached the mesh.
struct admission_watch {
    bool sampled = false;
    unsigned outstanding = 0;
    unsigned peak = 0;
    std::uint64_t remote_accesses = 0;
    bool mesh_quiescent = false;
    bool wrapper_idle = true;
    std::uint64_t cycles_at_first_sample = 0;
    std::uint64_t cycles_at_second_sample = 0;
    bool second_sampled = false;
};

/// Issues one access from its own process at a chosen instant.
class bound_probe : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(bound_probe);

    bound_probe(sc_core::sc_module_name name, driver& drv,
                std::uint64_t address, sc_core::sc_time start)
        : sc_core::sc_module(name)
        , drv_(drv)
        , address_(address)
        , start_(start)
    {
        SC_THREAD(run);
    }

    bool finished = false;
    sc_core::sc_time returned_at = sc_core::SC_ZERO_TIME;

private:
    void run()
    {
        if (start_ != sc_core::SC_ZERO_TIME) {
            sc_core::wait(start_);
        }
        std::vector<unsigned char> data(4, 0x5A);
        drv_.access(tlm::TLM_WRITE_COMMAND, address_, data.data(), 4);
        returned_at = sc_core::sc_time_stamp();
        finished = true;
    }

    driver& drv_;
    std::uint64_t address_;
    sc_core::sc_time start_;
};

/// Reads the wrapper's accounting twice inside the window where the only work
/// left is one routed caller's ordering wait.
class bound_sampler : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(bound_sampler);

    bound_sampler(sc_core::sc_module_name name,
                  cdc::components::noc_interconnect& noc,
                  const memory_target& remote, sc_core::sc_time first,
                  sc_core::sc_time second, admission_watch& watch)
        : sc_core::sc_module(name)
        , noc_(noc)
        , remote_(remote)
        , first_(first)
        , second_(second)
        , watch_(watch)
    {
        SC_THREAD(run);
    }

private:
    void run()
    {
        sc_core::wait(first_);
        watch_.outstanding = noc_.outstanding_transactions(0);
        watch_.peak = noc_.peak_outstanding_transactions(0);
        watch_.remote_accesses = remote_.accesses;
        watch_.mesh_quiescent = noc_.mesh_quiescent();
        watch_.wrapper_idle = noc_.wrapper_idle();
        watch_.cycles_at_first_sample = noc_.elapsed_cycles();
        watch_.sampled = true;

        sc_core::wait(second_ - first_);
        watch_.cycles_at_second_sample = noc_.elapsed_cycles();
        watch_.second_sampled = true;
    }

    cdc::components::noc_interconnect& noc_;
    const memory_target& remote_;
    sc_core::sc_time first_;
    sc_core::sc_time second_;
    admission_watch& watch_;
};

/// Issues one bypass to the slow co-located target when told to.
class slow_bypass : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(slow_bypass);

    slow_bypass(sc_core::sc_module_name name, driver& drv,
                sc_core::sc_event& go)
        : sc_core::sc_module(name)
        , drv_(drv)
        , go_(go)
    {
        SC_THREAD(run);
    }

    bool finished = false;

private:
    void run()
    {
        sc_core::wait(go_);
        std::vector<unsigned char> data{9, 9, 9, 9};
        drv_.access(tlm::TLM_WRITE_COMMAND, slow_local_base, data.data(), 4);
        finished = true;
    }

    driver& drv_;
    sc_core::sc_event& go_;
};

class scenario : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(scenario);

    scenario(sc_core::sc_module_name name,
             cdc::components::noc_interconnect& noc, driver& owner,
             driver& stranger, memory_target& local, memory_target& remote,
             cdc::components::noc_interconnect& fast_noc, driver& fast_owner,
             sc_core::sc_event& go, hammer& local_hammer,
             hammer& remote_hammer, sc_core::sc_event& slow_bypass_go,
             const slow_bypass& slow)
        : sc_core::sc_module(name)
        , noc_(noc)
        , owner_(owner)
        , stranger_(stranger)
        , local_(local)
        , remote_(remote)
        , fast_noc_(fast_noc)
        , fast_owner_(fast_owner)
        , go_(go)
        , local_hammer_(local_hammer)
        , remote_hammer_(remote_hammer)
        , slow_bypass_go_(slow_bypass_go)
        , slow_(slow)
    {
        SC_THREAD(run);
    }

    bool ran() const { return ran_; }

private:
    void run()
    {
        the_owner_reaches_its_own_node_without_a_flit();
        another_port_reaches_the_same_target_through_the_mesh();
        the_owner_still_routes_to_everything_else();
        debug_access_reaches_a_co_located_target();
        fast_mode_bypasses_too();
        a_bypass_does_not_clock_an_idle_mesh();
        both_paths_run_concurrently_without_crossing();
        ran_ = true;
    }

    // ── the D1 property ──────────────────────────────────────────────────────

    void the_owner_reaches_its_own_node_without_a_flit()
    {
        const auto flits_before = total_accepted_flits(noc_);
        const auto completed_before = noc_.completed_transactions();
        const auto latency_before = noc_.total_latency_cycles();

        std::vector<unsigned char> out{0xDE, 0xAD, 0xBE, 0xEF};
        check(owner_.access(tlm::TLM_WRITE_COMMAND, local_base, out.data(), 4)
                  == tlm::TLM_OK_RESPONSE,
              "a bypassed write must succeed");

        std::vector<unsigned char> in(4, 0);
        check(owner_.access(tlm::TLM_READ_COMMAND, local_base, in.data(), 4)
                  == tlm::TLM_OK_RESPONSE,
              "a bypassed read must succeed");
        check(in == out,
              "a bypassed access must move the same bytes a routed one would");

        // **The negative property.** Stated first because everything else here
        // is equally true of an access that went the long way round.
        check(total_accepted_flits(noc_) == flits_before,
              "the owner's access to its own node injected a flit. With "
              "NoLoopback = 1 that flit is undeliverable, so this is a hang in "
              "the real system rather than a slow path (decision record D1)");

        check(noc_.local_bypass_transactions(0) == 2,
              "the two bypassed accesses were not counted as bypassed");
        check(noc_.completed_transactions() == completed_before,
              "a bypassed access was counted as a network transaction; the "
              "network counters describe traffic that entered the network");
        check(noc_.total_latency_cycles() == latency_before,
              "a zero-hop access added to the network latency total, which "
              "deflates every average taken from it");

        // It consumed no outstanding slot. The wrapper is constructed with a
        // budget of one, so a bypass that took a slot would still work here —
        // the peak counter is what makes the claim checkable.
        check(noc_.peak_outstanding_transactions(0) == 0,
              "a bypassed access consumed one of the port's outstanding slots");

        check(local_.accesses == 2, "the target was not actually reached");
    }

    void another_port_reaches_the_same_target_through_the_mesh()
    {
        const auto flits_before = total_accepted_flits(noc_);
        const auto completed_before = noc_.completed_transactions();

        // Same region, different port. Not self-addressed, so it routes — and
        // it must, or the bypass would have quietly become a global shortcut.
        std::vector<unsigned char> in(4, 0);
        check(stranger_.access(tlm::TLM_READ_COMMAND, local_base, in.data(), 4)
                  == tlm::TLM_OK_RESPONSE,
              "a non-owner must still reach a co-located target");
        check(in == std::vector<unsigned char>({0xDE, 0xAD, 0xBE, 0xEF}),
              "the routed read of the co-located target returned wrong data");

        check(total_accepted_flits(noc_) > flits_before,
              "a non-owner's access to the same target injected no flit, so "
              "the bypass is not owner-aware -- it is a shortcut for everyone");
        check(noc_.completed_transactions() == completed_before + 1,
              "the routed access was not counted as a network transaction");
        check(noc_.local_bypass_transactions(1) == 0,
              "a non-owner's access was counted as a bypass");
    }

    void the_owner_still_routes_to_everything_else()
    {
        const auto flits_before = total_accepted_flits(noc_);
        const auto bypassed_before = noc_.local_bypass_transactions(0);

        std::vector<unsigned char> out{1, 2, 3, 4};
        check(owner_.access(tlm::TLM_WRITE_COMMAND, remote_base, out.data(), 4)
                  == tlm::TLM_OK_RESPONSE,
              "the owner must still reach a remote target");

        check(total_accepted_flits(noc_) > flits_before,
              "the owner's access to a *different* node injected no flit; the "
              "bypass is keyed on the target, not on the port");
        check(noc_.local_bypass_transactions(0) == bypassed_before,
              "a routed access from the owner was counted as a bypass");
        check(remote_.accesses == 1, "the remote target was not reached");
    }

    void debug_access_reaches_a_co_located_target()
    {
        const auto flits_before = total_accepted_flits(noc_);
        const auto bypassed_before = noc_.local_bypass_transactions(0);

        std::vector<unsigned char> in(4, 0);
        check(owner_.debug(local_base, in.data(), 4) == 4,
              "debug access to a co-located target must work");
        check(in == std::vector<unsigned char>({0xDE, 0xAD, 0xBE, 0xEF}),
              "the debug read returned wrong data");
        check(total_accepted_flits(noc_) == flits_before,
              "a debug access injected a flit");
        check(noc_.local_bypass_transactions(0) == bypassed_before,
              "a debug access moved the bypass counter; debug is free of side "
              "effects (it never entered b_transport at all)");
    }

    /// A bypass must not hold the mesh clock open.
    ///
    /// `wrapper_idle()` and the clock gate are different questions, and the
    /// first version answered them with one counter: a bypass whose target took
    /// time kept `network_idle()` false, so the network thread went on calling
    /// `step_once()` on a quiescent mesh and added `counted_cycles` to every
    /// router. The bypass creates no flit and must not move a mesh counter —
    /// including the one counting *cycles*, which is the denominator of every
    /// utilisation figure taken from the others.
    ///
    /// **The bypass has to overlap routed traffic**, and the first version of
    /// this check did not arrange that. On a quiet mesh the network thread is
    /// already parked in `wait(work)` and a bypass never notifies it, so
    /// nothing steps whether the counter is shared or not — the control passed
    /// against the defect. The routed access here is what wakes the thread; the
    /// window measured is what happens after that access drains and only the
    /// bypass is left.
    void a_bypass_does_not_clock_an_idle_mesh()
    {
        slow_bypass_go_.notify(sc_core::SC_ZERO_TIME);

        // Wakes the network thread and drains quickly, leaving only the bypass.
        std::vector<unsigned char> probe(4, 0);
        check(stranger_.access(tlm::TLM_READ_COMMAND, remote_base,
                               probe.data(), 4)
                  == tlm::TLM_OK_RESPONSE,
              "the routed probe must succeed");

        // Let the mesh drain the response, then measure a window in which only
        // the bypass is outstanding.
        wait(sc_core::sc_time(100, sc_core::SC_NS));
        const auto before = noc_.detailed_counter_snapshot();
        std::uint64_t cycles_before = 0;
        for (const auto& router : before.request.routers) {
            cycles_before += router.counted_cycles;
        }

        wait(sc_core::sc_time(200, sc_core::SC_NS));

        const auto after = noc_.detailed_counter_snapshot();
        std::uint64_t cycles_after = 0;
        for (const auto& router : after.request.routers) {
            cycles_after += router.counted_cycles;
        }

        check(!slow_bypass_done_(),
              "the slow bypass finished before the measured window closed, so "
              "the window did not contain a bypass-only interval");
        check(cycles_after == cycles_before,
              "the mesh was clocked for "
                  + std::to_string(cycles_after - cycles_before)
                  + " router-cycles while only a bypass was running. No flit "
                    "existed, so every utilisation figure derived from these "
                    "counters was diluted by a transaction that never entered "
                    "the network");

        // Drain it before the next scenario, which counts bypasses exactly.
        for (unsigned i = 0; i < 2000 && !slow_.finished; ++i) {
            wait(sc_core::sc_time(50, sc_core::SC_NS));
        }
        check(slow_.finished, "the slow bypass never completed");
    }

    void fast_mode_bypasses_too()
    {
        // The mapping is a fact about the topology, not about the timing
        // backend. Without this, fast mode would charge Manhattan hops for a
        // path that does not exist.
        std::vector<unsigned char> out{0x5A, 0x5A, 0x5A, 0x5A};
        check(fast_owner_.access(tlm::TLM_WRITE_COMMAND, local_base,
                                 out.data(), 4)
                  == tlm::TLM_OK_RESPONSE,
              "a bypassed write must succeed in fast mode");
        check(fast_noc_.local_bypass_transactions(0) == 1,
              "fast mode did not take the bypass");
        check(fast_noc_.completed_transactions() == 0,
              "fast mode counted a bypassed access as a network transaction");
    }

    /// Both paths aimed at the same target at the same time.
    ///
    /// Everything above runs one access at a time, and sequential accesses
    /// cannot show the failure that matters here: a bypass replay and a
    /// network-thread replay in flight together, where a response or a payload
    /// could be handed to the wrong manager. The two managers write disjoint
    /// words with tags only they produce, so a crossed response is visible as
    /// the *other* manager's tag coming back.
    void both_paths_run_concurrently_without_crossing()
    {
        const auto bypassed_before = noc_.local_bypass_transactions(0);
        const auto completed_before = noc_.completed_transactions();

        go_.notify(sc_core::SC_ZERO_TIME);

        // Bounded: a crossed response could deadlock, and a deadlock has to
        // fail rather than stall the suite.
        //
        // The poll also samples for **overlap**. Two managers that happened to
        // take turns would satisfy every check below without ever having been
        // concurrent, so the run has to show at least one instant where a
        // routed transaction was in flight while the bypassing manager still
        // had work left. Without that this whole scenario proves nothing it
        // did not already prove sequentially.
        bool overlapped = false;
        for (unsigned i = 0;
             i < 20000 && !(local_hammer_.finished && remote_hammer_.finished);
             ++i) {
            if (!local_hammer_.finished
                && noc_.outstanding_transactions(1) > 0) {
                overlapped = true;
            }
            sc_core::wait(sc_core::sc_time(50, sc_core::SC_NS));
        }

        check(overlapped,
              "no routed transaction was ever in flight while the bypassing "
              "manager still had work to do, so the two paths took turns and "
              "this scenario measured nothing about concurrency");

        check(local_hammer_.finished && remote_hammer_.finished,
              "a concurrent manager never finished; the bypass and the network "
              "path deadlocked against each other");
        check(local_hammer_.mismatches == 0,
              "the bypassing manager read back data it did not write while the "
              "network path was live on the same target");
        check(remote_hammer_.mismatches == 0,
              "the routed manager read back data it did not write while the "
              "bypass was live on the same target");
        check(local_hammer_.completed == concurrent_rounds
                  && remote_hammer_.completed == concurrent_rounds,
              "a concurrent manager did not complete every round");

        // Each path stayed on its own side of the boundary throughout.
        check(noc_.local_bypass_transactions(0)
                  == bypassed_before + 2 * concurrent_rounds,
              "the bypassing manager's concurrent accesses were not all "
              "bypassed");
        check(noc_.completed_transactions()
                  == completed_before + 2 * concurrent_rounds,
              "the routed manager's concurrent accesses were not all routed");
    }

    cdc::components::noc_interconnect& noc_;
    driver& owner_;
    driver& stranger_;
    memory_target& local_;
    memory_target& remote_;
    cdc::components::noc_interconnect& fast_noc_;
    driver& fast_owner_;
    sc_core::sc_event& go_;
    hammer& local_hammer_;
    hammer& remote_hammer_;
    sc_core::sc_event& slow_bypass_go_;
    const slow_bypass& slow_;
    bool slow_bypass_done_() const { return slow_.finished; }
    bool ran_ = false;
};

} // namespace

int sc_main(int, char**)
{
    using cdc::components::noc_interconnect;

    // A 2x2 mesh with the owner at (0,0) and a second manager at (1,1). One
    // outstanding slot per port, so "the bypass consumed no slot" is a claim
    // the peak counter can settle.
    // Exactly two target slots: SystemC requires every pre-created socket to be
    // bound, so the constructor's `num_targets` is a count, not a budget.
    noc_interconnect noc{"noc",  2, 2, 3, 2, sc_core::sc_time(1, sc_core::SC_NS),
                         1, noc_interconnect::timing_mode::detailed};
    memory_target local_memory{"local_memory",
                               sc_core::sc_time(2, sc_core::SC_NS)};
    memory_target remote_memory{"remote_memory"};
    // Slow enough to still be running after routed traffic has drained.
    memory_target slow_local_memory{"slow_local_memory",
                                    sc_core::sc_time(500, sc_core::SC_NS)};
    driver owner{"owner"};
    driver stranger{"stranger"};

    noc.place_initiator(0, {0, 0});
    noc.place_initiator(1, {1, 1});
    owner.socket.bind(noc.target_socket);
    stranger.socket.bind(noc.cpu_port(1));

    // The co-located target, authorised by naming the port it shares a node
    // with. Without the last argument this call throws.
    noc.add_target(local_base, region_size, {0, 0},
                   noc_interconnect::target_kind::memory, /*local_owner=*/0)
        .bind(local_memory.socket);
    noc.add_target(remote_base, region_size, {1, 0},
                   noc_interconnect::target_kind::memory)
        .bind(remote_memory.socket);
    // A second target on the owner's own node, owned by the same port.
    noc.add_target(slow_local_base, region_size, {0, 0},
                   noc_interconnect::target_kind::memory, /*local_owner=*/0)
        .bind(slow_local_memory.socket);

    // Fast mode, same topology.
    noc_interconnect fast{"fast", 2, 2, 1, 1,
                          sc_core::sc_time(1, sc_core::SC_NS), 1,
                          noc_interconnect::timing_mode::fast};
    memory_target fast_memory{"fast_memory"};
    driver fast_owner{"fast_owner"};
    fast.place_initiator(0, {0, 0});
    fast_owner.socket.bind(fast.target_socket);
    fast.add_target(local_base, region_size, {0, 0},
                    noc_interconnect::target_kind::memory, /*local_owner=*/0)
        .bind(fast_memory.socket);

    // The two concurrent managers, both aimed at the co-located target: the
    // owner reaches it by bypass, the stranger through the mesh. Disjoint
    // words, tags only each can produce.
    sc_core::sc_event go;
    hammer local_hammer{"local_hammer", owner, go, local_base + 0x100, 0xA1,
                        concurrent_rounds};
    hammer remote_hammer{"remote_hammer", stranger, go, local_base + 0x200,
                         0xB2, concurrent_rounds};

    sc_core::sc_event slow_go;
    slow_bypass slow{"slow_bypass", owner, slow_go};

    scenario checks{"checks",       noc,           owner, stranger,
                    local_memory,   remote_memory, fast,  fast_owner,
                    go,             local_hammer,  remote_hammer,
                    slow_go,        slow};

    // ── same port, same channel, routed before bypassed ─────────────────────
    //
    // Its own interconnect, because it needs two managers on **one** port and
    // the bench above deliberately puts them on two. `remote` is a mesh hop
    // away and slow; `owned` is co-located and instant. Issue the routed write
    // first, the bypassed one a delta later, and require them to complete in
    // that order.
    std::vector<int> completion_order;
    noc_interconnect ordered{"ordered", 2, 2, 2, 1,
                             sc_core::sc_time(1, sc_core::SC_NS), 4,
                             noc_interconnect::timing_mode::detailed};
    memory_target ordered_local{"ordered_local"};
    memory_target ordered_remote{"ordered_remote",
                                 sc_core::sc_time(40, sc_core::SC_NS)};
    driver ordered_port{"ordered_port"};
    ordered.place_initiator(0, {0, 0});
    ordered_port.socket.bind(ordered.target_socket);
    ordered.add_target(local_base, region_size, {0, 0},
                       noc_interconnect::target_kind::memory,
                       /*local_owner=*/0)
        .bind(ordered_local.socket);
    ordered.add_target(remote_base, region_size, {1, 1},
                       noc_interconnect::target_kind::memory)
        .bind(ordered_remote.socket);

    order_probe routed_first{"routed_first", ordered_port, remote_base, 0,
                             sc_core::SC_ZERO_TIME, completion_order};
    order_probe bypass_second{"bypass_second", ordered_port, local_base, 1,
                              sc_core::sc_time(1, sc_core::SC_NS),
                              completion_order};

    // ── same port, same channel, one slow bypass then TWO routed ────────────
    //
    // The mirror of the scenario above. There a bypass must not overtake an
    // earlier routed access, which `await_bypass_turn()` enforces. Here a
    // **bypass is issued first and held open**, and two routed accesses queue
    // behind it in `await_earlier_bypass()`:
    //
    //     while (!outstanding_bypass[slot].empty()
    //            && *outstanding_bypass[slot].begin() < ticket)
    //         wait(*order_ready[slot]);
    //
    // When the bypass erases its ticket, **every** waiter passes that loop in
    // the same delta, and nothing after it re-imposes their relative order —
    // routed-versus-routed is deliberately left to the deques, so that
    // `same-port-request-order-reversed` can still see a mesh that injected
    // out of order.
    //
    // **What this pins, and what it does not.** `MaxUniqueIds = 1` promises
    // write completions FIFO per port, and the outcome here is FIFO. But no
    // code in `await_earlier_bypass()` produces that: the waiters are released
    // together and their relative order is the kernel's resumption order for
    // dynamic waiters, which the LRM does not specify and which happens to
    // follow the order they began waiting. So this is a **characterisation**
    // check, not a control for a fix — there is no mutation of this file that
    // makes it fail, because the ordering it observes is not performed here.
    // It earns its place by failing if that kernel behaviour ever changes,
    // which is the only way this would become visible at all.
    //
    // Adding an explicit ticket-order release was tried and reverted: it
    // changed no observable outcome, and forcing routed completions into
    // ticket order is what an earlier draft of this file measured as blinding
    // `same-port-request-order-reversed`.
    //
    // **The 100/150 ns separation is load-bearing.** An earlier version
    // started these at 1 ns and 2 ns and measured nothing: both were still
    // blocked in `ensure_started()` when the mesh came up, both took their
    // completion tickets in the *same* instant at 4 ns, and the kernel decided
    // which got ticket 1 — the probe that started later held the earlier
    // ticket. Asserting on tag order then reported a reversal the wrapper had
    // never promised to avoid. Issue order *is* ticket order, and ticket order
    // is only observable when the two calls are genuinely separated in time.
    //
    // The local target is slow and the remote one fast, so both routed
    // accesses have finished in the mesh and are parked in that loop before
    // the bypass releases them.
    std::vector<int> release_order;
    noc_interconnect released{"released", 2, 2, 2, 1,
                              sc_core::sc_time(1, sc_core::SC_NS), 4,
                              noc_interconnect::timing_mode::detailed};
    memory_target released_local{"released_local",
                                 sc_core::sc_time(400, sc_core::SC_NS)};
    memory_target released_remote{"released_remote",
                                  sc_core::sc_time(5, sc_core::SC_NS)};
    driver released_port{"released_port"};
    released.place_initiator(0, {0, 0});
    released_port.socket.bind(released.target_socket);
    released.add_target(local_base, region_size, {0, 0},
                        noc_interconnect::target_kind::memory,
                        /*local_owner=*/0)
        .bind(released_local.socket);
    released.add_target(remote_base, region_size, {1, 1},
                        noc_interconnect::target_kind::memory)
        .bind(released_remote.socket);

    // Named for what it does here rather than `slow_bypass`, which is already
    // the SystemC name of the instance at the top of this file: a duplicate
    // makes the kernel rename one of them, and a renamed object is what any
    // report taken from this bench would then show.
    order_probe holding_bypass{"holding_bypass", released_port, local_base, 0,
                               sc_core::SC_ZERO_TIME, release_order};
    // **Separated by far more than a delta, and deliberately.** An earlier
    // draft started these at 1 ns and 2 ns and measured nothing: both were
    // still blocked in `ensure_started()` when the mesh came up, both took
    // their completion tickets in the *same* instant, and the kernel decided
    // which got ticket 1. The probe that started later held the earlier
    // ticket, so an assertion on tag order was asserting something the
    // wrapper never promised. Issue order is ticket order, and ticket order is
    // only observable if the two calls are genuinely separated in time.
    order_probe routed_a{"routed_a", released_port, remote_base, 1,
                         sc_core::sc_time(100, sc_core::SC_NS), release_order};
    order_probe routed_b{"routed_b", released_port, remote_base, 2,
                         sc_core::sc_time(150, sc_core::SC_NS), release_order};

    // ── R-P9-2: a routed call owns its admission slot until it returns ──────
    //
    // `max_outstanding_per_port` documents itself as bounding "concurrent
    // `b_transport` calls admitted on one tagged upstream socket"
    // (`noc_interconnect.h:236`). The slot used to be released inside
    // `complete_manager_transaction()` -- on the mesh side, when the final AXI
    // response landed -- and only then did the caller return to
    // `await_earlier_bypass()` and park. So with a bound of one, a second
    // routed call was admitted and injected while the first was still inside
    // `b_transport`, and `outstanding_transactions()` read zero for both.
    //
    // Bound of one, a slow bypass holding the ordering, routed A at 100 ns and
    // routed B at 150 ns. In the window between A's response landing and the
    // bypass releasing it, the only work left in the whole wrapper is A's
    // ordering wait, and that is what the samples below are taken in.
    admission_watch watch;
    noc_interconnect bound{"bound", 2, 2, 2, 1,
                           sc_core::sc_time(1, sc_core::SC_NS),
                           /*max_outstanding_per_port=*/1,
                           noc_interconnect::timing_mode::detailed};
    memory_target bound_local{"bound_local",
                              sc_core::sc_time(400, sc_core::SC_NS)};
    memory_target bound_remote{"bound_remote",
                               sc_core::sc_time(5, sc_core::SC_NS)};
    driver bound_port{"bound_port"};
    bound.place_initiator(0, {0, 0});
    bound_port.socket.bind(bound.target_socket);
    bound.add_target(local_base, region_size, {0, 0},
                     noc_interconnect::target_kind::memory,
                     /*local_owner=*/0)
        .bind(bound_local.socket);
    bound.add_target(remote_base, region_size, {1, 1},
                     noc_interconnect::target_kind::memory)
        .bind(bound_remote.socket);

    bound_probe bound_bypass{"bound_bypass", bound_port, local_base,
                             sc_core::SC_ZERO_TIME};
    bound_probe bound_routed_a{"bound_routed_a", bound_port, remote_base,
                               sc_core::sc_time(100, sc_core::SC_NS)};
    bound_probe bound_routed_b{"bound_routed_b", bound_port, remote_base,
                               sc_core::sc_time(150, sc_core::SC_NS)};
    // Both samples sit inside the hold: A has long since had its response and
    // the bypass does not finish until about 404 ns.
    bound_sampler bound_watch{"bound_watch", bound, bound_remote,
                              sc_core::sc_time(250, sc_core::SC_NS),
                              sc_core::sc_time(350, sc_core::SC_NS), watch};

    // Bounded: a bypass that wedged an input FIFO would hang, and a hang has
    // to fail rather than stall the suite.
    sc_core::sc_start(sc_core::sc_time(500, sc_core::SC_US));

    check(watch.sampled && watch.second_sampled,
          "the R-P9-2 samples were never taken");
    std::cout << "R-P9-2 while routed A is parked: outstanding="
              << watch.outstanding << " peak=" << watch.peak
              << " remote_accesses=" << watch.remote_accesses
              << " mesh_quiescent=" << watch.mesh_quiescent
              << " wrapper_idle=" << watch.wrapper_idle
              << " cycles " << watch.cycles_at_first_sample << "->"
              << watch.cycles_at_second_sample << '\n';

    check(watch.outstanding == 1,
          "a routed caller parked in await_earlier_bypass() did not hold its "
          "admission slot. It is inside b_transport() and the bound counts "
          "concurrent admitted calls, so releasing the slot on the mesh side "
          "lets the next caller in while this one has not returned (R-P9-2)");
    check(watch.peak == 1,
          "the peak admitted count exceeded the bound of one");
    check(watch.remote_accesses == 1,
          "the remote target was entered more than once while the bound was "
          "one and the first caller had not returned; routed B was admitted "
          "and injected out of turn");
    check(bound_routed_b.returned_at >= bound_routed_a.returned_at,
          "routed B returned before routed A, which held the only slot");

    // The two idle predicates answer different questions, and R-P9-2 is why
    // they had to be separated: the mesh really is empty during the hold, so
    // it must be free to clock-gate, while the wrapper still owes a caller its
    // return.
    check(watch.mesh_quiescent,
          "the mesh was not quiescent during the ordering hold, so something "
          "was still in flight when the scenario assumes nothing is");
    check(!watch.wrapper_idle,
          "wrapper_idle() reported idle while a routed caller was parked "
          "holding an admission slot. It must consult admission slots, or a "
          "testbench polling it concludes the wrapper is quiescent when a "
          "call has not returned");
    check(watch.cycles_at_second_sample == watch.cycles_at_first_sample,
          "the mesh advanced cycles during a window whose only remaining work "
          "is an ordering wait. network_idle() must not consult admission "
          "slots, or the clock gate keeps stepping an empty mesh and every "
          "counted-cycle metric inflates");

    check(bound_bypass.finished && bound_routed_a.finished
              && bound_routed_b.finished,
          "an R-P9-2 probe never finished");
    check(bound.outstanding_transactions(0) == 0,
          "an admission slot was still held after every caller returned");
    check(bound.wrapper_idle(),
          "the wrapper did not return to idle once every caller had returned");

    check(checks.ran(), "the scenario did not finish");

    check(routed_first.finished && bypass_second.finished,
          "an ordering probe never finished");
    check(completion_order.size() == 2, "both probes must have completed");
    if (completion_order.size() == 2) {
        std::cout << "same-port completion order: " << completion_order[0]
                  << " then " << completion_order[1] << '\n';
        check(completion_order[0] == 0,
              "the bypassed access completed before a routed access issued "
              "earlier on the same port and channel. MaxUniqueIds = 1 makes "
              "write completions FIFO per port, so this is a protocol-visible "
              "reordering -- and every functional check still passes, because "
              "each access on its own is correct");
    }

    // ── R-P9-3: the ordering fallback is live ───────────────────────────────
    //
    // Both routed calls were parked behind the bypass, so both must have been
    // caught by `await_earlier_bypass()` and sent through the ticket-order
    // fallback. This asserts the **path**, not the outcome, and the difference
    // is the whole honesty of the check: on Accellera 2.3.4 the completion
    // order above is `0 1 2` with the fallback and without it, because the
    // kernel resumes dynamic waiters in the order they began waiting. No test
    // on this kernel can distinguish the two. What it can do is refuse to let
    // the fallback become dead code that rots unnoticed.
    check(released.ordering_holds() >= 2,
          "the two routed calls parked behind the bypass did not reach the "
          "ticket-order fallback; R-P9-3's ordering step is not on the path it "
          "was written for, so the FIFO promise is back to resting on the "
          "kernel's unspecified waiter-resumption order");

    check(holding_bypass.finished && routed_a.finished && routed_b.finished,
          "a release-order probe never finished");
    check(release_order.size() == 3,
          "all three release-order probes must have completed");
    if (release_order.size() == 3) {
        std::cout << "slow-bypass release order: " << release_order[0] << " "
                  << release_order[1] << " " << release_order[2] << '\n';
        check(release_order[0] == 0,
              "a routed access completed before the bypass issued earlier on "
              "the same port and channel");
        check(release_order[1] == 1 && release_order[2] == 2,
              "two routed accesses released together by a bypass completed "
              "out of issue order. MaxUniqueIds = 1 makes write completions "
              "FIFO per port. Nothing in await_earlier_bypass() enforces this "
              "-- every waiter is released in one delta -- so what just "
              "changed is the kernel's resumption order for dynamic waiters, "
              "and the wrapper now needs to order them itself");
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_noc_interconnect_local_bypass: all checks passed\n";
    return 0;
}
